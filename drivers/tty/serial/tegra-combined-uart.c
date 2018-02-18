/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/serial_core.h>
#include <linux/console.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/io.h>

#define PKT_RCVD_BIT		26
#define NUM_BYTES_FIELD_BIT	27
#define LAST_PKT_BIT		29
#define FIRST_PKT_BIT		30
#define INTR_TRIGGER_BIT	31

static volatile u32 __iomem *top0_mbox_reg;
static volatile u32 __iomem *spe_mbox_reg;

static struct uart_port tegra_combined_uart_port;
static struct console tegra_combined_uart_console;
static struct uart_driver tegra_combined_uart_driver;

static void tegra_combined_uart_console_write(struct console *co,
						const char *s,
						unsigned int count);

/*
 * This function does nothing. This function is used to fill in the function
 * pointers in struct uart_ops tegra_combined_uart_ops, which we don't
 * implement.
 */
static int uart_null_func(struct uart_port *port)
{
	return 0;
}

static void tegra_combined_uart_start_tx(struct uart_port *port)
{
	unsigned long tail;
	unsigned long count;
	struct circ_buf *xmit = &port->state->xmit;

	tail = (unsigned long)&xmit->buf[xmit->tail];
	count = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
	if (!count)
		return;

	tegra_combined_uart_console_write(NULL, (char *)tail, count);
}

static int __init tegra_combined_uart_console_init(void)
{
	register_console(&tegra_combined_uart_console);

	return 0;
}
console_initcall(tegra_combined_uart_console_init);

/*
 * This function splits the string to be printed (const char *s) into multiple
 * packets. Each packet contains a max of 3 characters. Packets are sent to the
 * SPE-based combined UART server for printing. Communication with SPE is done
 * through mailbox registers which can generate interrupts for SPE and Linux.
 */
static void tegra_combined_uart_console_write(struct console *co,
						const char *s,
						unsigned int count)
{
	int num_packets, curr_packet_bytes, last_packet_bytes;
	u32 reg_val;
	int i, j;

	num_packets = count / 3;
	if (count % 3 != 0) {
		last_packet_bytes = count % 3;
		num_packets += 1;
	} else {
		last_packet_bytes = 3;
	}

	/* Loop for processing each 3 char packet */
	for (i = 0; i < num_packets; i++) {
		reg_val = BIT(INTR_TRIGGER_BIT);
		if (i == 0)
			reg_val |= BIT(FIRST_PKT_BIT);

		if (i == num_packets - 1) {
			reg_val |= BIT(LAST_PKT_BIT);
			curr_packet_bytes = last_packet_bytes;
		} else {
			curr_packet_bytes = 3;
		}

		/*
		 * Extract the current 3 chars from the
		 * string buffer (const char *s) and store them in the mailbox
		 * register value.
		 */
		reg_val |= curr_packet_bytes << NUM_BYTES_FIELD_BIT;
		for (j = 0; j < curr_packet_bytes; j++)
			reg_val |= s[i*3 + j] << (j * 8);

		/* Send current packet to SPE */
		*spe_mbox_reg = reg_val;

		/*
		 * Wait for SPE to ACK that it received the current packet.
		 * TODO: Add support for interrupt based operation.
		 */
		while ((*top0_mbox_reg & BIT(PKT_RCVD_BIT)) == 0) {
		}

		/* Clear the ACK */
		*top0_mbox_reg = 0;
	}
}

static int __init tegra_combined_uart_console_setup(struct console *co,
							char *options)
{
	int baud = 115200;
	int parity = 'n';
	int bits = 8;
	int flow = 'n';

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(&tegra_combined_uart_port, co, baud, parity,
				bits, flow);
}

static int tegra_combined_uart_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;

	if (!np)
		return -ENODEV;

	top0_mbox_reg =	(u32 *)(of_io_request_and_map(np, 0,
				"Tegra Combined UART TOP0_HSP Linux mailbox"));
	if (IS_ERR(top0_mbox_reg))
		return PTR_ERR(top0_mbox_reg);

	spe_mbox_reg = (u32 *)(of_io_request_and_map(np, 1,
					"Tegra Combined UART SPE mailbox"));
	if (IS_ERR(spe_mbox_reg))
		return PTR_ERR(spe_mbox_reg);

	ret = uart_register_driver(&tegra_combined_uart_driver);
	if (ret < 0) {
		pr_err("%s: Could not register driver\n", __func__);
		return ret;
	}

	ret = uart_add_one_port(&tegra_combined_uart_driver,
				&tegra_combined_uart_port);
	if (ret < 0) {
		pr_err("%s: Failed to add uart port\n", __func__);
		return ret;
	}

	return 0;
}

static int tegra_combined_uart_remove(struct platform_device *pdev)
{
	uart_remove_one_port(&tegra_combined_uart_driver,
				&tegra_combined_uart_port);
	uart_unregister_driver(&tegra_combined_uart_driver);
	iounmap(spe_mbox_reg);
	iounmap(top0_mbox_reg);
	return 0;
}

static struct of_device_id tegra_combined_uart_of_match[] = {
	{
		.compatible     = "nvidia,tegra186-combined-uart",
	}, {
	},
};

MODULE_DEVICE_TABLE(of, tegra_combined_uart_of_match);

static struct uart_ops tegra_combined_uart_ops = {
	.pm		= (void (*)(struct uart_port *,
					unsigned int,
					unsigned int)) &uart_null_func,
	.tx_empty	=
		(unsigned int (*)(struct uart_port *)) &uart_null_func,
	.get_mctrl	=
		(unsigned int (*)(struct uart_port *)) &uart_null_func,
	.set_mctrl	= (void (*)(struct uart_port *,
					unsigned int)) &uart_null_func,
	.stop_tx	= (void (*)(struct uart_port *)) &uart_null_func,
	.start_tx	= tegra_combined_uart_start_tx,
	.stop_rx	= (void (*)(struct uart_port *)) &uart_null_func,
	.break_ctl	= (void (*)(struct uart_port *, int)) &uart_null_func,
	.startup	= uart_null_func,
	.shutdown	= (void (*)(struct uart_port *)) &uart_null_func,
	.set_termios	= (void (*)(struct uart_port *,
					struct ktermios *,
					struct ktermios *)) &uart_null_func,
	.type		=
		(const char * (*)(struct uart_port *)) &uart_null_func,
	.release_port	= (void (*)(struct uart_port *)) &uart_null_func,
	.request_port	= uart_null_func,
	.config_port	= (void (*)(struct uart_port *, int)) &uart_null_func,
	.verify_port	= (int (*)(struct uart_port *,
				struct serial_struct *)) &uart_null_func,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char = uart_null_func,
	.poll_put_char = (void (*)(struct uart_port *,
					unsigned char)) &uart_null_func,
#endif
};

static struct uart_port tegra_combined_uart_port = {
	.lock		= __SPIN_LOCK_UNLOCKED(tegra_combined_uart_port.lock),
	.iotype		= UPIO_MEM,
	.uartclk	= 0,
	.fifosize	= 16,
	.flags		= UPF_BOOT_AUTOCONF,
	.line		= 0,
	.type		= 111,
	.ops		= &tegra_combined_uart_ops,
};

static struct console tegra_combined_uart_console = {
	.name		= "ttyTCU",
	.device		= uart_console_device,
	.flags		= CON_PRINTBUFFER | CON_ANYTIME,
	.index		= -1,
	.write		= tegra_combined_uart_console_write,
	.setup		= tegra_combined_uart_console_setup,
	.data		= &tegra_combined_uart_driver,
};

static struct uart_driver tegra_combined_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= "tegra-combined-uart",
	.dev_name	= "ttyTCU",
	.major		= TTY_MAJOR,
	.minor		= 70,
	.cons		= &tegra_combined_uart_console,
	.nr		= 1,
};

static struct platform_driver tegra_combined_uart_platform_driver = {
	.probe		= tegra_combined_uart_probe,
	.remove		= tegra_combined_uart_remove,
	.driver		= {
		.name	= "tegra-combined-uart",
		.of_match_table = of_match_ptr(tegra_combined_uart_of_match),
	},
};

static int __init tegra_combined_uart_init(void)
{
	int ret;

	ret = platform_driver_register(&tegra_combined_uart_platform_driver);
	if (ret < 0) {
		pr_err("%s: Platform driver register failed!\n", __func__);
		return ret;
	}

	return 0;
}

static void __exit tegra_combined_uart_exit(void)
{
	pr_info("Unloading Tegra combined UART driver\n");
	platform_driver_unregister(&tegra_combined_uart_platform_driver);
}

module_init(tegra_combined_uart_init);
module_exit(tegra_combined_uart_exit);

MODULE_ALIAS("tegra-combined-uart");
MODULE_DESCRIPTION(
	"Linux client driver for Tegra combined UART");
MODULE_AUTHOR("Adeel Raza <araza@nvidia.com>");
MODULE_LICENSE("GPL v2");
