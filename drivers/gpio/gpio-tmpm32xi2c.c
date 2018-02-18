/*
 * drivers/gpio/gpio-tmpm32xi2c.c
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#ifdef CONFIG_OF_GPIO
#include <linux/of_platform.h>
#endif
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include "gpio-tmpm32xi2c.h"

#define TMPM_MAX_PORT 112
#define TMPM_MAX_INTR_PORT 8
#define TMPM_BANK_SZ 8

#define TMPM_MAX_BANK (TMPM_MAX_PORT / TMPM_BANK_SZ)
#define TMPM_MAX_INTR_BANK (TMPM_MAX_INTR_PORT / TMPM_BANK_SZ)

#if ((TMPM_MAX_PORT % TMPM_BANK_SZ) || (TMPM_MAX_INTR_PORT % TMPM_BANK_SZ))
#error "Please set TMPM_MAX_PORT with BANK size as a unit!"
#endif

#define TMPM_GET_BANK(_A, _pos) (_A[(_pos) / TMPM_BANK_SZ])
#define TMPM_GET_BANKOFFSET(_pos) ((_pos) % TMPM_BANK_SZ)

#define TMPM_SET_BIT(_A, _pos) \
	(_A[(_pos) / TMPM_BANK_SZ] |= (1 << TMPM_GET_BANKOFFSET(_pos)))
#define TMPM_CLR_BIT(_A, _pos) \
	(_A[(_pos) / TMPM_BANK_SZ] &= ~(1 << TMPM_GET_BANKOFFSET(_pos)))
#define TMPM_TEST_BIT(_A, _pos) \
	(_A[(_pos) / TMPM_BANK_SZ] & (1 << TMPM_GET_BANKOFFSET(_pos)))

struct tmpm32xi2c_intr_map {
	/* index: intr-num, data: gpio-num */
	const uint32_t iidg[TMPM_MAX_INTR_PORT];
	/* index: gpio-num, data: intr-num */
	uint32_t igdi[TMPM_MAX_PORT];
};

/* Currently MCU firmware supports only 8 interrupt sources
   including one reserved */

/*
 * *---------------------------------------*
 * | intr source |      GPIO offset        |
 * *---------------------------------------*
 * |     0       | 24 - TMPM32X_GPIO(D, 0) |
 * |     1       | 25 - TMPM32X_GPIO(D, 1) |
 * |     2       | 35 - TMPM32X_GPIO(H, 6) |
 * |     3       | 27 - TMPM32X_GPIO(D, 3) |
 * |     4       | 28 - TMPM32X_GPIO(D, 4) |
 * |     5       | 29 - TMPM32X_GPIO(D, 5) |
 * |     6       | 81 - TMPM32X_GPIO(K, 1) |
 * |     7       | N/A                     |
 * *---------------------------------------*
 */

static struct tmpm32xi2c_intr_map intr_map = {
	{ 24, 25, 35, 27, 28, 29, 81, ~0 }, { ~0, }
};

#define TMPM_GET_GPIO_NUM(index) (intr_map.iidg[(index)])
#define TMPM_GET_INTR_NUM(index) (intr_map.igdi[(index)])


static const struct i2c_device_id tmpm32xi2c_id[] = {
	{ "tmpm32xi2c", TMPM_MAX_PORT, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tmpm32xi2c_id);

struct tmpm32xi2c_chip {
	struct mutex i2c_lock;
	struct i2c_client *client;
	struct gpio_chip gpio_chip;
	const char *const *names;
	uint8_t irq_available[TMPM_MAX_BANK];

	/* for interrupt */
	struct mutex irq_lock;
	uint8_t reg_direction_intr[TMPM_MAX_INTR_BANK];
	uint8_t irq_mask[TMPM_MAX_INTR_BANK];
	uint8_t irq_trig_raise[TMPM_MAX_INTR_BANK];
	uint8_t irq_trig_fall[TMPM_MAX_INTR_BANK];
	unsigned long irq_flags;

	/* for direction output */
	uint8_t dir_output[TMPM_MAX_PORT];
	uint8_t output_val[TMPM_MAX_PORT];

	uint8_t cmd_ver;
	uint8_t fw_ver[2];
};

static void tmpm32xi2c_gpio_set_value(struct gpio_chip *gc, unsigned offset,
		int val);


static inline struct tmpm32xi2c_chip *to_tmpm32xi2c(struct gpio_chip *gc)
{
	return container_of(gc, struct tmpm32xi2c_chip, gpio_chip);
}

static int tmpm32xi2c_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct tmpm32xi2c_chip *tmpm_chip = to_tmpm32xi2c(chip);

	if (TMPM_TEST_BIT(tmpm_chip->irq_available, offset))
		return irq_find_mapping(chip->irqdomain, offset);

	dev_dbg(&tmpm_chip->client->dev,
			"offset[%u] is not available for intr\n", offset);
	return 0;
}

static inline int tmpm32xi2c_write_read(struct tmpm32xi2c_chip *chip,
		u8 *tx_buf, int tx_size, u8 *rx_buf, int rx_size)
{
	struct i2c_msg msg[2];
	int msg_num = 0;
	int ret = 0;

	if (tx_size > 0) {
		msg[msg_num].addr = chip->client->addr;
		msg[msg_num].flags = chip->client->flags & I2C_M_TEN;
		msg[msg_num].len = tx_size;
		msg[msg_num].buf = tx_buf;
		msg_num++;
	}

	if (rx_size > 0) {
		msg[msg_num].addr = chip->client->addr;
		msg[msg_num].flags = chip->client->flags & I2C_M_TEN;
		msg[msg_num].flags |= I2C_M_RD;
		msg[msg_num].len = rx_size;
		msg[msg_num].buf = rx_buf;
		msg_num++;
	}

	ret = i2c_transfer(chip->client->adapter, msg, msg_num);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"failed to transfer data, CMD[0x%02x]\n",
			tx_buf ? tx_buf[0] : 0);
		return ret;
	}

	return 0;
}

static int __tmpm32xi2c_gpio_direction_input(struct tmpm32xi2c_chip *chip,
		unsigned offset)
{
	int ret;
	uint32_t intr_num;
	u8 tx_buf[] = { CMD_PIN_IN, 0 /*pin*/ };

	dev_dbg(&chip->client->dev, "(%s:%d) offset[%u]\n",
			__func__, __LINE__, offset);
	tx_buf[1] = (u8)offset;
	ret = tmpm32xi2c_write_read(chip, tx_buf, sizeof(tx_buf), NULL, 0);
	if (ret)
		goto exit;

	TMPM_CLR_BIT(chip->dir_output, offset);
	intr_num = TMPM_GET_INTR_NUM(offset);
	if (intr_num != ~0)
		TMPM_SET_BIT(chip->reg_direction_intr, intr_num);

	ret = 0;

exit:
	return ret;
}

static int tmpm32xi2c_gpio_direction_input(struct gpio_chip *gc,
		unsigned offset)
{
	int ret;
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);

	mutex_lock(&chip->i2c_lock);
	ret = __tmpm32xi2c_gpio_direction_input(chip, offset);
	mutex_unlock(&chip->i2c_lock);
	return ret;
}

static int __tmpm32xi2c_gpio_direction_output(struct tmpm32xi2c_chip *chip,
		unsigned offset, int val)
{
	int ret;
	uint32_t intr_num;
	u8 tx_buf[] = { CMD_PIN_OUT, 0 /*pin*/, 0 /*val*/ };

	dev_dbg(&chip->client->dev, "(%s:%d) offset[%u], val[%d]\n",
			__func__, __LINE__, offset, val);
	tx_buf[1] = (u8)offset;
	tx_buf[2] = val ? 1 : 0;

	/* set output level */
	ret = tmpm32xi2c_write_read(chip, tx_buf, sizeof(tx_buf), NULL, 0);
	if (ret)
		goto exit;

	TMPM_SET_BIT(chip->dir_output, offset);
	intr_num = TMPM_GET_INTR_NUM(offset);
	if (intr_num != ~0)
		TMPM_CLR_BIT(chip->reg_direction_intr, intr_num);

	ret = 0;

exit:
	return ret;
}

static int tmpm32xi2c_gpio_direction_output(struct gpio_chip *gc,
		unsigned offset, int val)
{
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);
	int ret;

	mutex_lock(&chip->i2c_lock);
	ret = __tmpm32xi2c_gpio_direction_output(chip, offset, val);
	mutex_unlock(&chip->i2c_lock);

	/* WAR: CMD_PIN_OUT doesn't set the output value with latest F/W */
	tmpm32xi2c_gpio_set_value(gc, offset, val);

	return ret;
}

static int __tmpm32xi2c_gpio_get_value(struct tmpm32xi2c_chip *chip,
		unsigned offset)
{
	int ret;

	dev_dbg(&chip->client->dev, "(%s:%d) offset[%u]\n",
			__func__, __LINE__, offset);
	if (TMPM_TEST_BIT(chip->dir_output, offset)) {
		ret = TMPM_TEST_BIT(chip->output_val, offset) ? 1 : 0;
	} else {
		u8 tx_buf[] = { CMD_PIN_RD, 0 /*pin*/ };
		u8 rx_buf[] = { 0 /*val*/ };

		tx_buf[1] = (u8)offset;
		ret = tmpm32xi2c_write_read(chip, tx_buf, sizeof(tx_buf),
				rx_buf, sizeof(rx_buf));
		if (ret < 0)
			return 0;

		ret = rx_buf[0] ? 1 : 0;
	}

	return ret;
}

static int tmpm32xi2c_gpio_get_value(struct gpio_chip *gc, unsigned offset)
{
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);
	int ret;

	mutex_lock(&chip->i2c_lock);
	ret = __tmpm32xi2c_gpio_get_value(chip, offset);
	mutex_unlock(&chip->i2c_lock);

	return ret;
}

static void __tmpm32xi2c_gpio_set_value(struct tmpm32xi2c_chip *chip,
		unsigned offset, int val)
{
	u8 tx_buf[] = { CMD_PIN_WR, 0 /*pin*/, 0 /*val*/};

	dev_dbg(&chip->client->dev, "(%s:%d) offset[%u], val[%d]\n",
			__func__, __LINE__, offset, val);
	tx_buf[1] = (u8)offset;
	tx_buf[2] = val ? 1 : 0;

	if (tmpm32xi2c_write_read(chip, tx_buf, sizeof(tx_buf), NULL, 0) < 0)
		return;

	if (val)
		TMPM_SET_BIT(chip->output_val, offset);
	else
		TMPM_CLR_BIT(chip->output_val, offset);
}

static void tmpm32xi2c_gpio_set_value(struct gpio_chip *gc,
		unsigned offset, int val)
{
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);

	mutex_lock(&chip->i2c_lock);
	__tmpm32xi2c_gpio_set_value(chip, offset, val);
	mutex_unlock(&chip->i2c_lock);
}

#ifdef CONFIG_DEBUG_FS
static int __tmpm32xi2c_power_get_value(struct tmpm32xi2c_chip *chip,
		unsigned cmd, unsigned channel)
{
	int ret, i, max_bytes;
	u8 tx_buf[] = { 0 /*cmd*/, 0 /*channel*/ };
	u8 rx_buf[] = { 0 /*val*/ };
	uint32_t val = 0;
	bool err_flag = false;

	if ((cmd != CMD_RD_VOLT) && (cmd != CMD_RD_ADC)) {
		dev_err(&chip->client->dev, "(%s:%d) invalid cmd[%u]\n",
			__func__, __LINE__, cmd);
		return 0;
	}

	dev_dbg(&chip->client->dev, "(%s:%d) cmd[%u] ch[%u]\n",
		__func__, __LINE__, cmd, channel);

	/* send channel info */
	tx_buf[0] = (u8)cmd;
	tx_buf[1] = (u8)channel;
	ret = tmpm32xi2c_write_read(chip, tx_buf,
			sizeof(tx_buf), NULL, 0);
	if (ret) {
		dev_err(&chip->client->dev, "(%s:%d) cmd[%u] ch[%u] ret[%d]\n",
			__func__, __LINE__, cmd, channel, ret);
		return 0;
	}

	/* set byte length for reading */
	if (cmd == CMD_RD_VOLT)
		max_bytes = 4;
	else
		max_bytes = 2;

	for (i = 0; i < max_bytes; i++) {
		/* send cmd only */
		ret = tmpm32xi2c_write_read(chip, tx_buf, 1,
				rx_buf, sizeof(rx_buf));
		if (ret < 0) {
			dev_err(&chip->client->dev, "(%s:%d) cmd[%u] ch[%u] ret[%d]\n",
				__func__, __LINE__, cmd, channel, ret);
			err_flag = true;
		}
		val <<= 8;
		val |= rx_buf[0];
	}

	if (err_flag)
		return 0;
	else
		return val;
}

static int tmpm32xi2c_ina3221_get_value(struct gpio_chip *gc, unsigned channel)
{
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);
	int ret;

	mutex_lock(&chip->i2c_lock);
	ret = __tmpm32xi2c_power_get_value(chip, CMD_RD_VOLT, channel);
	mutex_unlock(&chip->i2c_lock);

	return ret;
}

static int tmpm32xi2c_adc_get_value(struct gpio_chip *gc, unsigned channel)
{
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);
	int ret;

	mutex_lock(&chip->i2c_lock);
	ret = __tmpm32xi2c_power_get_value(chip, CMD_RD_ADC, channel);
	mutex_unlock(&chip->i2c_lock);

	return ret;
}
#endif /* CONFIG_DEBUG_FS */

static void tmpm32xi2c_setup_gpio(struct tmpm32xi2c_chip *chip, int gpios)
{
	struct gpio_chip *gc;

	gc = &chip->gpio_chip;

	gc->direction_input  = tmpm32xi2c_gpio_direction_input;
	gc->direction_output = tmpm32xi2c_gpio_direction_output;
	gc->get = tmpm32xi2c_gpio_get_value;
	gc->set = tmpm32xi2c_gpio_set_value;
	gc->can_sleep = true;

	gc->base = -1;
	gc->ngpio = gpios;
	gc->label = chip->client->name;
	gc->dev = &chip->client->dev;
	gc->owner = THIS_MODULE;
	gc->names = chip->names;
	gc->can_sleep = true;
#ifdef CONFIG_OF_GPIO
	gc->of_node = chip->client->dev.of_node;
#endif
}


static uint8_t tmpm32xi2c_irq_pending(struct tmpm32xi2c_chip *chip,
		uint8_t *pending)
{
	int ret = -1;
	int i;
	uint8_t tx_buf[] = { CMD_INT_REG, 0 /* dummy */ };
	uint8_t rx_buf[TMPM_MAX_INTR_BANK * 2] = { 0, };
	uint8_t pendings = 0;

	mutex_lock(&chip->i2c_lock);
	ret = tmpm32xi2c_write_read(chip, tx_buf, sizeof(tx_buf),
			rx_buf, sizeof(rx_buf));
	mutex_unlock(&chip->i2c_lock);
	if (ret)
		return 0;

	mutex_lock(&chip->irq_lock);
	for (i = 0; i < TMPM_MAX_INTR_BANK; i++) {
		uint8_t cur_stat;
		uint8_t trig_raise, trig_fall;
		uint8_t irq_trig_raise, irq_trig_fall;

		cur_stat = (rx_buf[i * 2] & chip->reg_direction_intr[i]) &
			   chip->irq_mask[i];
		trig_raise = cur_stat & rx_buf[i * 2 + 1];
		trig_fall = cur_stat & ~(rx_buf[i * 2 + 1]);
		irq_trig_raise = cur_stat & chip->irq_trig_raise[i];
		irq_trig_fall = cur_stat & chip->irq_trig_fall[i];

		if (!trig_raise && !trig_fall)
			continue;

		if (irq_trig_raise || irq_trig_fall) {
			/* Check pending irq based on configured irq type. */
			pending[i] = (trig_raise & irq_trig_raise) |
				     (trig_fall & irq_trig_fall);
		} else {
			/* If there was no configured irq type, consider any
			 * triggered irq types as pending irq. */
			pending[i] = (trig_raise | trig_fall);
		}
		pendings += (pending[i] ? 1 : 0);
	}
	mutex_unlock(&chip->irq_lock);

	return pendings;
}

static irqreturn_t tmpm32xi2c_irq_handler(int irq, void *devid)
{
	struct tmpm32xi2c_chip *chip = (struct tmpm32xi2c_chip *)devid;
	uint8_t pending[TMPM_MAX_BANK] = { 0, };
	unsigned int nhandled = 0;
	unsigned int gpio_irq = 0;
	uint8_t level;
	int i;

	if (!tmpm32xi2c_irq_pending(chip, pending))
		return IRQ_NONE;

	for (i = 0; i < TMPM_MAX_INTR_BANK; i++) {
		while (pending[i]) {
			level = __ffs(pending[i]);
			gpio_irq = irq_find_mapping(chip->gpio_chip.irqdomain,
					TMPM_GET_GPIO_NUM(level));
			handle_nested_irq(gpio_irq);
			pending[i] &= ~(1 << level);
			nhandled++;
		}
	}

	return (nhandled > 0) ? IRQ_HANDLED : IRQ_NONE;
}

static void tmpm32xi2c_update_interrupt_mask_reg(struct tmpm32xi2c_chip *chip)
{
	uint8_t tx_mask[] = { CMD_INT_MASK, 0x0 /*value*/ };
	uint8_t level;
	uint8_t new_irqs;
	int ret;
	int i;

	for (i = 0; i < TMPM_MAX_INTR_BANK; i++) {
		new_irqs = chip->irq_mask[i] & ~chip->reg_direction_intr[i];

		mutex_lock(&chip->i2c_lock);
		while (new_irqs) {
			level = __ffs(new_irqs);
			__tmpm32xi2c_gpio_direction_input(chip,
					TMPM_GET_GPIO_NUM(level));
			new_irqs &= ~(1 << level);
		}

		tx_mask[1] = chip->irq_mask[i];
		ret = tmpm32xi2c_write_read(chip, tx_mask, sizeof(tx_mask),
				NULL, 0);
		mutex_unlock(&chip->i2c_lock);
	}
}

static void tmpm32xi2c_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);
	uint32_t intr_num = TMPM_GET_INTR_NUM(d->hwirq);

	TMPM_CLR_BIT(chip->irq_mask, intr_num);
}

static void tmpm32xi2c_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);
	uint32_t intr_num = TMPM_GET_INTR_NUM(d->hwirq);

	TMPM_SET_BIT(chip->irq_mask, intr_num);
}

static void tmpm32xi2c_irq_bus_lock(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);

	mutex_lock(&chip->irq_lock);
}

static void tmpm32xi2c_irq_bus_sync_unlock(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);

	tmpm32xi2c_update_interrupt_mask_reg(chip);
	mutex_unlock(&chip->irq_lock);
}

static int tmpm32xi2c_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct tmpm32xi2c_chip *chip = to_tmpm32xi2c(gc);
	uint32_t intr_num;

	if (!TMPM_TEST_BIT(chip->irq_available, d->hwirq))
		return 0;

	intr_num = TMPM_GET_INTR_NUM(d->hwirq);
	if (type & ~IRQ_TYPE_SENSE_MASK)
		return -EINVAL;

	/* FIXME: LEVEL type is not supported yet in current MCU firmware. */
	if (!(type & IRQ_TYPE_EDGE_BOTH) && (type & IRQ_TYPE_LEVEL_MASK))
		return 0;

	if (type & IRQ_TYPE_EDGE_FALLING)
		TMPM_SET_BIT(chip->irq_trig_fall, intr_num);
	else
		TMPM_CLR_BIT(chip->irq_trig_fall, intr_num);

	if (type & IRQ_TYPE_EDGE_RISING)
		TMPM_SET_BIT(chip->irq_trig_raise, intr_num);
	else
		TMPM_CLR_BIT(chip->irq_trig_raise, intr_num);

	return 0;
}

static struct irq_chip tmpm32xi2c_irq_chip = {
	.name			= "tmpm32xi2c",
	.irq_mask		= tmpm32xi2c_irq_mask,
	.irq_unmask		= tmpm32xi2c_irq_unmask,
	.irq_bus_lock		= tmpm32xi2c_irq_bus_lock,
	.irq_bus_sync_unlock	= tmpm32xi2c_irq_bus_sync_unlock,
	.irq_set_type		= tmpm32xi2c_irq_set_type,
	.flags			= IRQCHIP_SKIP_SET_WAKE,
};

static int tmpm32xi2c_setup_gpio_irq(struct tmpm32xi2c_chip *chip,
		const struct i2c_device_id *id)
{
	struct i2c_client *client = chip->client;
	int ret;

	if (client->irq) {
		int i;
		uint32_t gpio_num = 0;

		mutex_init(&chip->irq_lock);
		for (i = 0; i < ARRAY_SIZE(intr_map.iidg); i++) {
			gpio_num = TMPM_GET_GPIO_NUM(i);
			if (gpio_num != ~0) {
				TMPM_SET_BIT(chip->irq_available, gpio_num);
				intr_map.igdi[gpio_num] = i;
			}
		}

		ret = devm_request_threaded_irq(&client->dev,
				client->irq,
				NULL,
				tmpm32xi2c_irq_handler,
				chip->irq_flags | IRQF_ONESHOT | IRQF_SHARED,
				dev_name(&client->dev), chip);
		if (ret) {
			dev_err(&client->dev, "failed to request irq %d\n",
				client->irq);
			return ret;
		}

		ret =  gpiochip_irqchip_add(&chip->gpio_chip,
				&tmpm32xi2c_irq_chip,
				0,
				handle_simple_irq,
				IRQ_TYPE_EDGE_BOTH);
		if (ret) {
			dev_err(&client->dev,
					"could not connect irqchip to gpiochip\n");
			return ret;
		}
	} else {
		dev_err(&client->dev, "invalid client->irq\n");
		return -EINVAL;
	}

	return 0;
}

static int tmpm32xi2c_device_init(struct tmpm32xi2c_chip *chip)
{
	int ret = -1;
	u8 tx[] = { CMD_VERSION, 0 /*ver type*/ };

	/* read version */
	mutex_lock(&chip->i2c_lock);
	tx[1] = V_FW;
	ret = tmpm32xi2c_write_read(chip, tx, sizeof(tx),
			chip->fw_ver, 2);
	if (ret)
		goto exit;
	dev_info(&chip->client->dev, "(%s:%d) FW version [0x%x][0x%x]\n",
			__func__, __LINE__, chip->fw_ver[0], chip->fw_ver[1]);

	tx[1] = V_CMD;
	ret = tmpm32xi2c_write_read(chip, tx, sizeof(tx),
			&(chip->cmd_ver), 1);
	if (ret)
		goto exit;
	dev_info(&chip->client->dev, "(%s:%d) CMD version [0x%x]\n",
			__func__, __LINE__, chip->cmd_ver);

exit:
	mutex_unlock(&chip->i2c_lock);

	return ret;
}

static int tmpm32xi2c_parse_dt(struct tmpm32xi2c_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct device_node *np = client->dev.of_node;
	int ret;

	if (!np)
		return -ENOENT;

	ret = of_property_read_u32(client->dev.of_node, "tmpm32xi2c,irq_flags",
					(unsigned int *)&chip->irq_flags);
	if (ret) {
		dev_warn(&client->dev, "Using default irq_flags");
		chip->irq_flags = IRQF_TRIGGER_LOW;
	}

	return 0;
}

#ifdef CONFIG_DEBUG_FS
typedef union {
	uint32_t raw;
	struct {
		uint16_t mv; /* low 2B */
		uint16_t ma; /* high 2B */
	} w;
} ina_data;

static int show_tmpm32xi2c_power_stats(struct seq_file *s, void *data)
{
	struct gpio_chip *gc = s->private;
	uint32_t sys_9v_mv, vbat_mv, vdd_ddr_1v8_ma;
	ina_data vdd_gpu, vdd_bcpu, vdd_mcpu;
	ina_data vdd_soc, vdd_sram, vdd_ddr;
	char *pwr_str_fmt = "%12s\t%6s\t%6s\t%6s\n";
	char *pwr_val_fmt = "%12s\t%6d\t%6d\t%6d\n";

	sys_9v_mv = tmpm32xi2c_adc_get_value(gc, R_SYS_9V);
	vbat_mv = tmpm32xi2c_adc_get_value(gc, R_VBAT);
	vdd_ddr_1v8_ma = tmpm32xi2c_adc_get_value(gc, R_VDD_DDR_1V8);
	vdd_gpu.raw = tmpm32xi2c_ina3221_get_value(gc, R_VDD_GPU);
	vdd_bcpu.raw = tmpm32xi2c_ina3221_get_value(gc, R_VDD_BCPU);
	vdd_mcpu.raw = tmpm32xi2c_ina3221_get_value(gc, R_VDD_MCPU);
	vdd_soc.raw = tmpm32xi2c_ina3221_get_value(gc, R_VDD_SOC);
	vdd_sram.raw = tmpm32xi2c_ina3221_get_value(gc, R_VDD_SRAM);
	vdd_ddr.raw = tmpm32xi2c_ina3221_get_value(gc, R_VDD_DDR);

	seq_printf(s, pwr_str_fmt, "PWR RAIL", "mA", "mV", "mW");
	/* TMPM32X ADC */
	seq_printf(s, pwr_val_fmt, "SYS_9V", 0, sys_9v_mv, 0);
	seq_printf(s, pwr_val_fmt, "VBAT_MAIN", 0, vbat_mv, 0);
	seq_printf(s, pwr_val_fmt, "VDD_1V8_DDR",  vdd_ddr_1v8_ma, 0, 0);
	/* INA3221 */
	seq_printf(s, pwr_val_fmt, "VDD_GPU",
		vdd_gpu.w.ma, vdd_gpu.w.mv,
		(vdd_gpu.w.ma * vdd_gpu.w.mv) / 1000);
	seq_printf(s, pwr_val_fmt, "VDD_BCPU",
		vdd_bcpu.w.ma, vdd_bcpu.w.mv,
		(vdd_bcpu.w.ma * vdd_bcpu.w.mv) / 1000);
	seq_printf(s, pwr_val_fmt, "VDD_MCPU",
		vdd_mcpu.w.ma, vdd_mcpu.w.mv,
		(vdd_mcpu.w.ma * vdd_mcpu.w.mv) / 1000);
	seq_printf(s, pwr_val_fmt, "VDD_SOC",
		vdd_soc.w.ma, vdd_soc.w.mv,
		(vdd_soc.w.ma * vdd_soc.w.mv) / 1000);
	seq_printf(s, pwr_val_fmt, "VDD_SRAM",
		vdd_sram.w.ma, vdd_sram.w.mv,
		(vdd_sram.w.ma * vdd_sram.w.mv) / 1000);
	seq_printf(s, pwr_val_fmt, "VDD_DDR",
		vdd_ddr.w.ma, vdd_ddr.w.mv,
		(vdd_ddr.w.ma * vdd_ddr.w.mv) / 1000);

	return 0;
}

static int tmpm32xi2c_power_stats_dump(struct inode *inode, struct file *file)
{
	return single_open(file, show_tmpm32xi2c_power_stats, inode->i_private);
}

static const struct file_operations tmpm32xi2c_power_stats_fops = {
	.open		= tmpm32xi2c_power_stats_dump,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void tmpm32xi2c_debugfs_init(struct gpio_chip *gc)
{
	struct dentry *root;

	root = debugfs_create_dir("tmpm32x", NULL);

	if (!debugfs_create_file("pwr_stat", S_IRUGO, root, gc,
		&tmpm32xi2c_power_stats_fops))
		goto err_root;

	return;

err_root:
	debugfs_remove_recursive(root);

	return;
}
#endif /* CONFIG_DEBUG_FS */

static int tmpm32xi2c_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct tmpm32xi2c_chip *chip;
	int ret;

	chip = devm_kzalloc(&client->dev,
			sizeof(struct tmpm32xi2c_chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

	chip->client = client;
	mutex_init(&chip->i2c_lock);

	tmpm32xi2c_parse_dt(chip);

	tmpm32xi2c_setup_gpio(chip, id->driver_data);
	ret = gpiochip_add(&chip->gpio_chip);
	if (ret)
		return ret;

	ret = tmpm32xi2c_setup_gpio_irq(chip, id);
	if (ret) {
		gpiochip_remove(&chip->gpio_chip);
		return ret;
	}

	chip->gpio_chip.to_irq = tmpm32xi2c_to_irq;

	ret = tmpm32xi2c_device_init(chip);
	if (ret) {
		gpiochip_remove(&chip->gpio_chip);
		return ret;
	}

	i2c_set_clientdata(client, chip);
	device_init_wakeup(&client->dev, true);

#ifdef CONFIG_DEBUG_FS
	tmpm32xi2c_debugfs_init(&chip->gpio_chip);
#endif

	return 0;
}

static int tmpm32xi2c_remove(struct i2c_client *client)
{
	struct tmpm32xi2c_chip *chip = i2c_get_clientdata(client);

	device_init_wakeup(&client->dev, false);
	gpiochip_remove(&chip->gpio_chip);

	return 0;
}

static const struct of_device_id tmpm32xi2c_dt_ids[] = {
	{ .compatible = "toshiba,tmpm32xi2c" },
	{ .compatible = "nvidia,tmpm32xi2c" },
	{ }
};

MODULE_DEVICE_TABLE(of, tmpm32xi2c_dt_ids);

#ifdef CONFIG_PM
static int tmpm32xi2c_suspend(struct device *dev)
{
	struct tmpm32xi2c_chip *chip = dev_get_drvdata(dev);
	int ret = 0;

	if (device_may_wakeup(dev)) {
		ret = enable_irq_wake(chip->client->irq);
		if (ret < 0)
			dev_err(dev, "Failed to enable irq wake for irq=%d, ret=%d\n",
				chip->client->irq, ret);
	}

	return ret;
}

static int tmpm32xi2c_resume(struct device *dev)
{
	struct tmpm32xi2c_chip *chip = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(chip->client->irq);

	return 0;
}

static const struct dev_pm_ops tmpm32xi2c_pm = {
	.suspend_late = tmpm32xi2c_suspend,
	.resume_early = tmpm32xi2c_resume,
};
#endif

static struct i2c_driver tmpm32xi2c_driver = {
	.driver = {
		.name	= "tmpm32xi2c",
		.of_match_table = tmpm32xi2c_dt_ids,
#ifdef CONFIG_PM
		.pm	= &tmpm32xi2c_pm,
#endif
	},
	.probe		= tmpm32xi2c_probe,
	.remove		= tmpm32xi2c_remove,
	.id_table	= tmpm32xi2c_id,
};

static int __init tmpm32xi2c_init(void)
{
	return i2c_add_driver(&tmpm32xi2c_driver);
}
/* register after i2c postcore initcall and before
 * subsys initcalls that may rely on these GPIOs
 */
subsys_initcall(tmpm32xi2c_init);

static void __exit tmpm32xi2c_exit(void)
{
	i2c_del_driver(&tmpm32xi2c_driver);
}
module_exit(tmpm32xi2c_exit);

MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("I2C GPIO expander driver for TMPM32x");
MODULE_LICENSE("GPL");
