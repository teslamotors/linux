/*
 * tegra_hv_comm.c: TTY over Tegra HV
 *
 * Copyright (c) 2014-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * Very loosely based on altera_jtaguart.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#undef DEBUG
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/tegra-soc.h>

#define DRV_NAME "tegra_hv_comm"

#include <linux/tegra-ivc.h>

/* frame format is
* 0000: <size>
* 0004: <flags>
* 0008: data
*/

#define HDR_SIZE	8

struct tegra_hv_comm {
	struct uart_port port;
	struct platform_device *pdev;
	struct tegra_hv_ivc_cookie *ivck;
	unsigned int sigs;
	unsigned long imr;
	unsigned int tx_en:1;
	struct timer_list tx_timer;
	unsigned int rx_en:1;
	void *rx_frame;
	u8 *rx_buf;	/* always points to rx_frame + HDR_SIZE */
	void *tx_frame;
	u8 *tx_buf;	/* always points to tx_frame + HDR_SIZE */
	void *buf;
	int rx_in, rx_cnt;
};

#define to_tegra_hv_comm(_x) \
	container_of(_x, struct tegra_hv_comm, port)

static unsigned int tegra_hv_comm_tx_empty(struct uart_port *port)
{
	struct tegra_hv_comm *pp = to_tegra_hv_comm(port);
	struct device *dev = &pp->pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	return tegra_hv_ivc_tx_empty(pp->ivck) ? TIOCSER_TEMT : 0;
}

static unsigned int tegra_hv_comm_get_mctrl(struct uart_port *port)
{
	/* fake it */
	return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}

static void tegra_hv_comm_set_mctrl(struct uart_port *port, unsigned int sigs)
{
	struct tegra_hv_comm *pp = to_tegra_hv_comm(port);
	struct device *dev = &pp->pdev->dev;

	/* nothing */
	dev_dbg(dev, "%s\n", __func__);
}

static int xmit_active(struct tegra_hv_comm *pp)
{
	struct uart_port *port = &pp->port;
	struct circ_buf *xmit = &port->state->xmit;

	return port->x_char != 0 || uart_circ_chars_pending(xmit) > 0;
}

static void xmit_timer_setup(struct tegra_hv_comm *pp, int in_timer)
{
	if (xmit_active(pp)) {
		if (!pp->tx_en) {
			pp->tx_en = 1;
			mod_timer(&pp->tx_timer, jiffies + HZ / 8);
		}
	} else {
		if (pp->tx_en) {
			pp->tx_en = 0;
			if (!in_timer)
				del_timer(&pp->tx_timer);
		}
	}
}

static void tegra_hv_comm_tx_chars(struct tegra_hv_comm *pp)
{
	struct uart_port *port = &pp->port;
	struct device *dev = &pp->pdev->dev;
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int pending, count;
	char *s;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	if (!tegra_hv_ivc_can_write(pp->ivck))
		return;

	if (port->x_char) {
		/* Send special char - probably flow control */

		((u32 *)pp->tx_frame)[0] = 1;	/* count */
		((u32 *)pp->tx_frame)[1] = 0;	/* flags */
		s = pp->tx_frame + HDR_SIZE;
		*s++ = port->x_char;

		count = (void *)s - pp->tx_frame + HDR_SIZE;
		memset(pp->tx_frame + count, 0, pp->ivck->frame_size - count);
		ret = tegra_hv_ivc_write(pp->ivck, pp->tx_frame, count);
		if (ret != count) {
			/* error */
			dev_err(dev, "%s: failed to write x_char\n", __func__);
			return;
		}

		port->x_char = 0;
		port->icount.tx++;
		return;
	}

	pending = uart_circ_chars_pending(xmit);
	if (pending > 0) {
		count = pp->ivck->frame_size - HDR_SIZE;
		if (count > pending)
			count = pending;
		if (count > 0) {
			pending -= count;

			((u32 *)pp->tx_frame)[0] = count;	/* count */
			((u32 *)pp->tx_frame)[1] = 0;		/* flags */
			s = pp->tx_frame + HDR_SIZE;
			while (count-- > 0) {
				*s++ = xmit->buf[xmit->tail];
				xmit->tail = (xmit->tail + 1) &
					(UART_XMIT_SIZE - 1);
				port->icount.tx++;
			}

			/* count is now from start of frame */
			count = (void *)s - pp->tx_frame;
			if (count > 0)
				memset(pp->tx_frame + count, 0,
						pp->ivck->frame_size - count);

			ret = tegra_hv_ivc_write(pp->ivck, pp->tx_frame, count);
			if (ret != count) {
				/* error */
				dev_err(dev, "%s: failed to write normal data\n",
						__func__);
				return;
			}

			if (pending < WAKEUP_CHARS)
				uart_write_wakeup(port);
		}
	}

}

static void tegra_hv_tx_timer_expired(unsigned long data)
{
	struct tegra_hv_comm *pp = (void *)data;
	struct uart_port *port = &pp->port;

	spin_lock(&port->lock);
	tegra_hv_comm_tx_chars(pp);
	xmit_timer_setup(pp, 1);
	spin_unlock(&port->lock);
}

static void tegra_hv_comm_start_tx(struct uart_port *port)
{
	struct tegra_hv_comm *pp = to_tegra_hv_comm(port);
	struct device *dev = &pp->pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	tegra_hv_comm_tx_chars(pp);
	xmit_timer_setup(pp, 0);
}

static void tegra_hv_comm_stop_tx(struct uart_port *port)
{
	struct tegra_hv_comm *pp = to_tegra_hv_comm(port);
	struct device *dev = &pp->pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	if (pp->tx_en) {
		pp->tx_en = 0;
		del_timer_sync(&pp->tx_timer);
	}
}

static void tegra_hv_comm_stop_rx(struct uart_port *port)
{
	struct tegra_hv_comm *pp = to_tegra_hv_comm(port);
	struct device *dev = &pp->pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	pp->rx_en = 0;
}

static void tegra_hv_comm_break_ctl(struct uart_port *port, int break_state)
{
	/* nothing */
}

static void tegra_hv_comm_enable_ms(struct uart_port *port)
{
	/* nothing */
}

static void tegra_hv_comm_set_termios(struct uart_port *port,
					struct ktermios *termios,
					struct ktermios *old)
{
	struct tegra_hv_comm *pp = to_tegra_hv_comm(port);
	struct device *dev = &pp->pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	/* Just copy the old termios settings back */
	if (old)
		tty_termios_copy_hw(termios, old);
}

static void tegra_hv_comm_rx_chars(struct tegra_hv_comm *pp)
{
	struct uart_port *port = &pp->port;
	struct device *dev = &pp->pdev->dev;
	unsigned char ch, flag;
	u32 cnt, flags;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	for (;;) {
		/* no more data, get more */
		while (pp->rx_in >= pp->rx_cnt &&
				tegra_hv_ivc_can_read(pp->ivck)) {

			ret = tegra_hv_ivc_read(pp->ivck, pp->rx_frame,
					pp->ivck->frame_size);

			/* error */
			if (ret <= 0) {
				dev_err(dev, "%s: ivc_read failed (ret=%d)\n",
						__func__, ret);
				break;
			}

			/* not enough data */
			if (ret < HDR_SIZE) {
				dev_err(dev, "%s: ivc_read short read (ret=%d)\n",
						__func__, ret);
				break;
			}

			cnt = ((u32 *)pp->rx_frame)[0];
			flags = ((u32 *)pp->rx_frame)[1];

			/* validate */
			if (cnt > pp->ivck->frame_size - HDR_SIZE) {
				dev_err(dev, "%s: ivc_read bogus frame (cnt=%d)\n",
						__func__, cnt);
				break;
			}

			pp->rx_cnt = cnt;
			pp->rx_in = 0;
		}

		if (pp->rx_in >= pp->rx_cnt)
			break;

		/* next character */
		ch = pp->rx_buf[pp->rx_in++];

		flag = TTY_NORMAL;
		port->icount.rx++;

		if (uart_handle_sysrq_char(port, ch))
			continue;
		uart_insert_char(port, 0, 0, ch, flag);
	}

	tty_flip_buffer_push(&port->state->port);
}

static irqreturn_t tegra_hv_comm_interrupt(int irq, void *data)
{
	struct uart_port *port = data;
	struct tegra_hv_comm *pp = to_tegra_hv_comm(port);
	struct device *dev = &pp->pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	spin_lock(&port->lock);

	/* until this function returns 0, the channel is unusable */
	if (tegra_hv_ivc_channel_notified(pp->ivck) == 0) {
		if (pp->rx_en && tegra_hv_ivc_can_read(pp->ivck))
			tegra_hv_comm_rx_chars(pp);

		tegra_hv_comm_tx_chars(pp);
		xmit_timer_setup(pp, 0);
	}

	spin_unlock(&port->lock);

	return IRQ_HANDLED;
}

static void tegra_hv_comm_config_port(struct uart_port *port, int flags)
{
	port->type = PORT_TEGRA_HV;
}

static int tegra_hv_comm_startup(struct uart_port *port)
{
	struct tegra_hv_comm *pp = to_tegra_hv_comm(port);
	struct device *dev = &pp->pdev->dev;
	unsigned long flags;
	int ret, irq;

	dev_dbg(dev, "%s\n", __func__);

	irq = pp->ivck->irq;
	ret = devm_request_irq(dev, irq,
			tegra_hv_comm_interrupt, 0,
			dev_name(dev), port);
	if (ret) {
		pr_err(DRV_NAME ": unable to request #%d irq=%d\n",
				port->line, irq);
		return ret;
	}

	spin_lock_irqsave(&port->lock, flags);
	pp->rx_en = 1;
	if (tegra_hv_ivc_can_read(pp->ivck))
		tegra_hv_comm_rx_chars(pp);
	spin_unlock_irqrestore(&port->lock, flags);

	return 0;
}

static void tegra_hv_comm_shutdown(struct uart_port *port)
{
	struct tegra_hv_comm *pp = to_tegra_hv_comm(port);
	struct device *dev = &pp->pdev->dev;
	unsigned long flags;
	int irq;

	dev_dbg(dev, "%s\n", __func__);

	irq = pp->ivck->irq;

	devm_free_irq(&pp->pdev->dev, irq, port);

	spin_lock_irqsave(&port->lock, flags);
	pp->rx_en = 0;
	if (pp->tx_en) {
		pp->tx_en = 0;
		del_timer(&pp->tx_timer);
	}
	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *tegra_hv_comm_type(struct uart_port *port)
{
	return (port->type == PORT_TEGRA_HV) ? "Tegra HV" : NULL;
}

static int tegra_hv_comm_request_port(struct uart_port *port)
{
	/* UARTs always present */
	return 0;
}

static void tegra_hv_comm_release_port(struct uart_port *port)
{
	/* Nothing to release... */
}

static int tegra_hv_comm_verify_port(struct uart_port *port,
				       struct serial_struct *ser)
{
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_TEGRA_HV)
		return -EINVAL;
	return 0;
}

/*
 *	Define the basic serial functions we support.
 */
static struct uart_ops tegra_hv_comm_ops = {
	.tx_empty	= tegra_hv_comm_tx_empty,
	.get_mctrl	= tegra_hv_comm_get_mctrl,
	.set_mctrl	= tegra_hv_comm_set_mctrl,
	.start_tx	= tegra_hv_comm_start_tx,
	.stop_tx	= tegra_hv_comm_stop_tx,
	.stop_rx	= tegra_hv_comm_stop_rx,
	.enable_ms	= tegra_hv_comm_enable_ms,
	.break_ctl	= tegra_hv_comm_break_ctl,
	.startup	= tegra_hv_comm_startup,
	.shutdown	= tegra_hv_comm_shutdown,
	.set_termios	= tegra_hv_comm_set_termios,
	.type		= tegra_hv_comm_type,
	.request_port	= tegra_hv_comm_request_port,
	.release_port	= tegra_hv_comm_release_port,
	.config_port	= tegra_hv_comm_config_port,
	.verify_port	= tegra_hv_comm_verify_port,
};

#define TEGRA_HV_COMM_MAJOR	204
#define TEGRA_HV_COMM_MINOR	213
#define TEGRA_HV_COMM_MAXPORTS 1
static struct tegra_hv_comm tegra_hv_comm_ports[TEGRA_HV_COMM_MAXPORTS];

static struct uart_driver tegra_hv_comm_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= "tegra_hv_comm",
	.dev_name	= "ttyTGRHVC",
	.major		= TEGRA_HV_COMM_MAJOR,
	.minor		= TEGRA_HV_COMM_MINOR,
	.nr		= TEGRA_HV_COMM_MAXPORTS,
	.cons		= NULL,	/* no console supported yet */
};

static int tegra_hv_comm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *dn, *hv_dn;
	struct tegra_hv_comm *pp;
	struct uart_port *port;
	int i = pdev->id;
	int ret;
	u32 id;

	if (!is_tegra_hypervisor_mode()) {
		dev_info(dev, "Hypervisor is not present\n");
		return -ENODEV;
	}

	/* -1 emphasizes that the platform must have one port, no .N suffix */
	if (i == -1)
		i = 0;

	if (i >= TEGRA_HV_COMM_MAXPORTS)
		return -EINVAL;

	dn = dev->of_node;
	if (dn == NULL) {
		dev_err(dev, "No OF data\n");
		return -EINVAL;
	}

	hv_dn = of_parse_phandle(dn, "ivc", 0);
	if (hv_dn == NULL) {
		dev_err(dev, "Failed to parse phandle of ivc prop\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(dn, "ivc", 1, &id);
	if (ret != 0) {
		dev_err(dev, "Failed to read IVC property ID\n");
		of_node_put(hv_dn);
		return ret;
	}

	/* zero out */
	pp = &tegra_hv_comm_ports[i];
	memset(pp, 0, sizeof(*pp));

	port = &pp->port;

	pp->ivck = tegra_hv_ivc_reserve(hv_dn, id, NULL);

	of_node_put(hv_dn);

	if (IS_ERR_OR_NULL(pp->ivck)) {
		dev_err(dev, "Failed to reserve IVC channel %d\n", id);
		ret = PTR_ERR(pp->ivck);
		pp->ivck = NULL;
		return ret;
	}

	/* make sure the frame size is sufficient */
	if (pp->ivck->frame_size <= HDR_SIZE) {
		dev_err(dev, "frame size too small to support COMM\n");
		ret = -EINVAL;
		goto out_unreserve;
	}

	dev_info(dev, "Reserved IVC channel #%d - frame_size=%d\n",
			id, pp->ivck->frame_size);

	/* allocate temporary frames */
	pp->buf = devm_kzalloc(dev, pp->ivck->frame_size * 2, GFP_KERNEL);
	if (pp->buf == NULL) {
		dev_err(dev, "Failed to allocate buffers\n");
		ret = -ENOMEM;
		goto out_unreserve;
	}
	pp->rx_frame = pp->buf;
	pp->rx_buf = pp->rx_frame + HDR_SIZE;
	pp->tx_frame = pp->rx_frame + pp->ivck->frame_size;
	pp->tx_buf = pp->tx_frame + HDR_SIZE;

	init_timer(&pp->tx_timer);
	pp->tx_timer.function = tegra_hv_tx_timer_expired;
	pp->tx_timer.data = (unsigned long)pp;

	pp->pdev = pdev;
	platform_set_drvdata(pdev, pp);

	port->line = i;
	port->type = PORT_TEGRA_HV;
	port->iotype = SERIAL_IO_MEM;
	port->ops = &tegra_hv_comm_ops;
	port->flags = UPF_BOOT_AUTOCONF;

	/*
	 * start the channel reset process asynchronously. until the reset
	 * process completes, any attempt to use the ivc channel will return
	 * an error (e.g., all transmits will fail.)
	 */
	tegra_hv_ivc_channel_reset(pp->ivck);

	ret = uart_add_one_port(&tegra_hv_comm_driver, port);
	if (ret != 0) {
		dev_err(dev, "uart_add_one_port failed\n");
		goto out_unreserve;
	}

	dev_info(dev, "ready\n");

	return 0;

out_unreserve:
	tegra_hv_ivc_unreserve(pp->ivck);
	return ret;
}

static int tegra_hv_comm_remove(struct platform_device *pdev)
{
	struct tegra_hv_comm *pp = platform_get_drvdata(pdev);
	struct uart_port *port;

	port = &pp->port;

	tegra_hv_ivc_unreserve(pp->ivck);

	uart_remove_one_port(&tegra_hv_comm_driver, port);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id tegra_hv_comm_match[] = {
	{ .compatible = "nvidia,tegra-hv-comm", },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_hv_comm_match);
#endif /* CONFIG_OF */

static struct platform_driver tegra_hv_comm_platform_driver = {
	.probe	= tegra_hv_comm_probe,
	.remove	= tegra_hv_comm_remove,
	.driver	= {
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(tegra_hv_comm_match),
	},
};

static int tegra_hv_comm_init(void)
{
	int rc;

	rc = uart_register_driver(&tegra_hv_comm_driver);
	if (rc)
		return rc;
	rc = platform_driver_register(&tegra_hv_comm_platform_driver);
	if (rc)
		uart_unregister_driver(&tegra_hv_comm_driver);
	return rc;
}

static void tegra_hv_comm_exit(void)
{
	platform_driver_unregister(&tegra_hv_comm_platform_driver);
	uart_unregister_driver(&tegra_hv_comm_driver);
}

module_init(tegra_hv_comm_init);
module_exit(tegra_hv_comm_exit);

MODULE_AUTHOR("Pantelis Antoniou <pantoniou@nvidia.com>");
MODULE_DESCRIPTION("TTY over Tegra Hypervisor IVC channel");
MODULE_LICENSE("GPL");
