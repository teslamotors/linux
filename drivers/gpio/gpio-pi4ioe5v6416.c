// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for the Pericom PI4IOE5V6416 GPIO Expander.
 * https://www.diodes.com/assets/Datasheets/PI4IOE5V6416.pdf
 *
 * Copyright (C) 2020 Tesla Motors, Inc.
 */
#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>

// Registers
#define PI4IO16_INPUT_PORT0	0x00
#define PI4IO16_INPUT_PORT1	0x01
#define PI4IO16_OUTPUT_PORT0	0x02
#define PI4IO16_OUTPUT_PORT1	0x03
#define PI4IO16_POLARITY0	0x04
#define PI4IO16_POLARITY1	0x05
#define PI4IO16_CONFIG_PORT0	0x06
#define PI4IO16_CONFIG_PORT1	0x07
#define PI4IO16_OUTPUT_DRIVE0_0 0x40
#define PI4IO16_OUTPUT_DRIVE0_1 0x41
#define PI4IO16_OUTPUT_DRIVE1_0 0x42
#define PI4IO16_OUTPUT_DRIVE1_1 0x43
#define PI4IO16_INPUT_LATCH0	0x44
#define PI4IO16_INPUT_LATCH1	0x45
#define PI4IO16_PULLUP_ENB0	0x46
#define PI4IO16_PULLUP_ENB1	0x47
#define PI4IO16_PULLUP_SEL0	0x48
#define PI4IO16_PULLUP_SEL1	0x49
#define PI4IO16_INTMASK_REG0	0x4A
#define PI4IO16_INTMASK_REG1	0x4B
#define PI4IO16_INT_STATUS0	0x4C
#define PI4IO16_INT_STATUS1	0x4D
#define PI4IO16_OUTPUT_CONFIG	0x4F

#define PI4IO16_N_GPIO 16
/*
 * The datasheet calls for a minimum of 30s pulse, and 600ns recovery time.
 * to safe, round to 1ms.
 */
#define PI4IO16_RESET_DELAY_MS 1

/*
 * 0<->GPIOF_DIR_OUT
 * 1<->GPIOF_DIR_IN
 */
#define PI4IO16_DIRECTION_TO_GPIOD(x) ((x) ? GPIOF_DIR_IN : GPIOF_DIR_OUT)
#define GPIOD_DIRECTION_TO_PI4IO16(x) ((x) == GPIOF_DIR_OUT ? 0 : 1)

//#define PI4IO16_IRQ /* TODO: Enable irqchip */

static bool pi4io16_readable_reg(struct device *dev, unsigned int reg)
{
	// registers are readable.
	switch (reg) {
	case PI4IO16_INPUT_PORT0 ... PI4IO16_CONFIG_PORT1:
	case PI4IO16_OUTPUT_DRIVE0_0 ... PI4IO16_OUTPUT_CONFIG:
		return true;
	default:
		return false;
	}
	return false;
}

static bool pi4io16_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case PI4IO16_OUTPUT_PORT0 ... PI4IO16_CONFIG_PORT1:
	case PI4IO16_OUTPUT_DRIVE0_0 ... PI4IO16_INTMASK_REG1:
	case PI4IO16_OUTPUT_CONFIG:
		return true;
	default:
		return false;
	}
	return false;
}

static bool pi4io16_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case PI4IO16_INPUT_PORT0:
	case PI4IO16_INPUT_PORT1:
	case PI4IO16_INPUT_LATCH0:
	case PI4IO16_INPUT_LATCH1:
	case PI4IO16_INT_STATUS0:
	case PI4IO16_INT_STATUS1:
		return true;
	default:
		return false;
	}
	return false;
}

static const struct regmap_config pi4io16_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = PI4IO16_OUTPUT_CONFIG,
	.writeable_reg = pi4io16_writeable_reg,
	.readable_reg = pi4io16_readable_reg,
	.volatile_reg = pi4io16_volatile_reg,
};

struct pi4io16_priv {
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct gpio_chip gpio;
	struct gpio_desc *reset_gpio;
#ifdef CONFIG_GPIO_PI4IOE5V6416_IRQ
	struct irq_chip irq_chip;
	struct mutex irq_lock;
	uint16_t irq_mask;
#endif
};

static int pi4io16_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	int ret, io_dir, direction;
	struct pi4io16_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	/*
	 * The Configuration registers configure the direction of the I/O pins.
	 * If a bit in these regsiters is set to 1 the corresponding port pin is
	 * enabled as a high-impedence input if a bit in these registers is cleared
	 * to 0, the corresponding port pin is enabled as an output.
	 */
	/* GPIO pins 0-7 -> PORT0, pins 8->15 PORT1 */
	if (offset < 8) {
		ret = regmap_read(pi4io->regmap, PI4IO16_CONFIG_PORT0, &io_dir);
	} else {
		ret = regmap_read(pi4io->regmap, PI4IO16_CONFIG_PORT1, &io_dir);
		offset = offset % 8;
	}

	if (ret) {
		dev_err(dev, "Failed to read I/O direction: %d", ret);
		return ret;
	}

	direction = PI4IO16_DIRECTION_TO_GPIOD((io_dir >> offset) & 1);
	dev_dbg(dev, "get_direction : offset=%u, direction=%s, reg=0x%X",
		offset, (direction == GPIOF_DIR_IN) ? "input" : "output",
		io_dir);

	return direction;
}

static int pi4io16_gpio_set_direction(struct gpio_chip *chip, unsigned offset,
				      int direction)
{
	int ret, reg;
	struct pi4io16_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	dev_dbg(dev, "set_direction : offset=%u, direction=%s", offset,
		(direction == GPIOF_DIR_IN) ? "input" : "output");

	if (offset < 8) {
		reg = PI4IO16_CONFIG_PORT0;
	} else {
		reg = PI4IO16_CONFIG_PORT1;
		offset = offset % 8;
	}
	ret = regmap_update_bits(pi4io->regmap, reg, 1 << offset,
				 GPIOD_DIRECTION_TO_PI4IO16(direction) <<
				 offset);

	if (ret) {
		dev_err(dev, "Failed to set direction: %d", ret);
		return ret;
	}

	return ret;
}

static int pi4io16_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	int ret, out, reg;
	struct pi4io16_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	if (offset < 8) {
		reg = PI4IO16_INPUT_PORT0;
	} else {
		reg = PI4IO16_INPUT_PORT1;
		offset = offset % 8;
	}

	ret = regmap_read(pi4io->regmap, reg, &out);
	if (ret) {
		dev_err(dev, "Failed to read output: %d", ret);
		return ret;
	}

	dev_dbg(dev, "gpio_get : offset=%u, val=%s reg=0x%X", offset,
		out & (1 << (offset % 8)) ? "1" : "0", out);

	if (out & (1 << (offset % 8))) {
		return 1;
	}
	return 0;
}

static void pi4io16_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	int ret, reg;
	struct pi4io16_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	dev_dbg(dev, "gpio_set : offset=%u, val=%s", offset, value ? "1" : "0");

	if (offset < 8) {
		reg = PI4IO16_OUTPUT_PORT0;
	} else {
		reg = PI4IO16_OUTPUT_PORT1;
		offset = offset % 8;
	}

	ret = regmap_update_bits(pi4io->regmap, reg, 1 << offset,
				 value << offset);
	if (ret) {
		dev_err(dev, "Failed to write output: %d", ret);
	}
}

static int pi4io16_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	return pi4io16_gpio_set_direction(chip, offset, GPIOF_DIR_IN);
}

static int pi4io16_gpio_direction_output(struct gpio_chip *chip,
					 unsigned offset, int value)
{
	int ret;
	struct pi4io16_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	ret = pi4io16_gpio_set_direction(chip, offset, GPIOF_DIR_OUT);
	if (ret) {
		dev_err(dev, "Failed to set direction: %d", ret);
		return ret;
	}

	pi4io16_gpio_set(chip, offset, value);
	return 0;
}

static int pi4io16_gpio_configure_input_latch(struct gpio_chip *chip,
					      uint16_t irq_mask)
{
	int ret;
	struct pi4io16_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	ret = regmap_update_bits(pi4io->regmap, PI4IO16_INPUT_LATCH0,
				 0xFF, irq_mask & 0xFF);
	if (ret) {
		dev_err(dev, "Failed to configure INPUT_LATCH0: %d", ret);
		return ret;
	}

	ret = regmap_update_bits(pi4io->regmap, PI4IO16_INPUT_LATCH1,
				 0xFF, (irq_mask >> 8) & 0xFF);
	if (ret) {
		dev_err(dev, "Failed to configure INPUT_LATCH1: %d", ret);
		return ret;
	}
	return ret;
}

static int pi4io16_gpio_unmask_interrupts(struct gpio_chip *chip,
					  uint16_t irq_mask)
{
	int ret;
	struct pi4io16_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	ret = regmap_update_bits(pi4io->regmap, PI4IO16_INTMASK_REG0,
				 0xFF, ~(irq_mask & 0xFF));
	if (ret) {
		dev_err(dev, "Failed to configure INTMASK_REG0: %d", ret);
		return ret;
	}

	ret = regmap_update_bits(pi4io->regmap, PI4IO16_INTMASK_REG1,
				 0xFF, ~((irq_mask >> 8) & 0xFF));
	if (ret) {
		dev_err(dev, "Failed to configure INTMASK_REG1: %d", ret);
		return ret;
	}
	return ret;
}

static int pi4io16_gpio_setup(struct pi4io16_priv *pi4io)
{
	int ret;
	struct device *dev = &pi4io->i2c->dev;
	struct gpio_chip *gc = &pi4io->gpio;

	gc->ngpio = PI4IO16_N_GPIO;
	gc->label = pi4io->i2c->name;
	gc->parent = &pi4io->i2c->dev;
	gc->owner = THIS_MODULE;
	gc->base = -1;
	gc->can_sleep = true;

	gc->get_direction = pi4io16_gpio_get_direction;
	gc->direction_input = pi4io16_gpio_direction_input;
	gc->direction_output = pi4io16_gpio_direction_output;
	gc->get = pi4io16_gpio_get;
	gc->set = pi4io16_gpio_set;

	ret = devm_gpiochip_add_data(dev, gc, pi4io);
	if (ret) {
		dev_err(dev, "devm_gpiochip_add_data failed: %d", ret);
		return ret;
	}
	return 0;
}

static int pi4io16_reset_setup(struct pi4io16_priv *pi4io)
{
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;
	struct gpio_desc *gpio;

	gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(gpio))
		return PTR_ERR(gpio);

	if (gpio) {
		dev_info(dev, "Reset pin=%d\n", desc_to_gpio(gpio));
		pi4io->reset_gpio = gpio;
	}

	return 0;
}

static void pi4io16_reset(struct pi4io16_priv *pi4io)
{
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;

	if (pi4io->reset_gpio) {
		dev_info(dev, "Resetting\n");
		gpiod_set_value(pi4io->reset_gpio, 0);
		msleep(PI4IO16_RESET_DELAY_MS);
		gpiod_set_value(pi4io->reset_gpio, 1);
		msleep(PI4IO16_RESET_DELAY_MS);
		gpiod_set_value(pi4io->reset_gpio, 0);
		msleep(PI4IO16_RESET_DELAY_MS);
	}
}

#ifdef CONFIG_GPIO_PI4IOE5V6416_IRQ
static void pi4io16_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pi4io16_priv *pi4io = gpiochip_get_data(gc);
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;

	dev_dbg(dev, "update irq_mask=0x%X & ~%lX\n", pi4io->irq_mask,
		d->hwirq);

	pi4io->irq_mask &= ~(1 << d->hwirq);
}

static void pi4io16_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pi4io16_priv *pi4io = gpiochip_get_data(gc);
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;

	dev_dbg(dev, "update irq_mask=0x%X | %lX\n", pi4io->irq_mask, d->hwirq);

	pi4io->irq_mask |= (1 << d->hwirq);
}

static void pi4io16_irq_bus_lock(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pi4io16_priv *pi4io = gpiochip_get_data(gc);

	mutex_lock(&pi4io->irq_lock);
}

static void pi4io16_irq_bus_sync_unlock(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pi4io16_priv *pi4io = gpiochip_get_data(gc);
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;
	uint16_t new_irqs;
	int level;

	new_irqs = pi4io->irq_mask;

	dev_dbg(dev, "syncing update new_mask=0x%X\n", new_irqs);

	/* Enable input latch on newly masked registers */
	pi4io16_gpio_configure_input_latch(&pi4io->gpio, pi4io->irq_mask);

	/* for every new irq that got re-configured set to an input */
	while (new_irqs) {
		level = __ffs(new_irqs);
		pi4io16_gpio_direction_input(&pi4io->gpio, level);
		new_irqs &= ~(1 << level);
	}

	/* unmask the interrupts */
	pi4io16_gpio_unmask_interrupts(&pi4io->gpio, pi4io->irq_mask);

	mutex_unlock(&pi4io->irq_lock);
}

static int pi4io16_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pi4io16_priv *pi4io = gpiochip_get_data(gc);
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;
	uint16_t irq = BIT(d->hwirq);

	dev_dbg(dev, "set_type irq=%d, type=%d\n", irq, type);

	/* The pi4io16 only supports generating interrupts when value changes
	 * from its default state.
	 */
	if (!(type & IRQ_TYPE_EDGE_BOTH)) {
		dev_err(dev, "irq %d: unsupported type %d\n", d->irq, type);
		return -EINVAL;
	}

	return 0;
}

static uint16_t pi4io16_irq_pending(struct pi4io16_priv *pi4io)
{
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;
	uint16_t irq_status;
	int irq_status_reg;
	int ret;

	/* Interrupt status from both banks */
	ret = regmap_read(pi4io->regmap, PI4IO16_INT_STATUS0, &irq_status_reg);
	if (ret < 0) {
		dev_err(dev, "Failed to read INT_STATUS0 rc=%d\n", ret);
		return 0;
	}

	irq_status = irq_status_reg;

	ret = regmap_read(pi4io->regmap, PI4IO16_INT_STATUS1, &irq_status_reg);
	if (ret < 0) {
		dev_err(dev, "Failed to read INT_STATUS1 rc=%d\n", ret);
		return 0;
	}

	irq_status |= (irq_status_reg << 8);

	/* return 16 bit representation of irq status, msB = bankb */
	return irq_status;
}

static irqreturn_t pi4io16_irq_handler(int irq, void *data)
{
	struct pi4io16_priv *pi4io = data;
	uint16_t pending;
	uint16_t level;

	/* Get the current state of the ISR of both banks, NOTE:
	   does not clear mask yet. */
	pending = pi4io16_irq_pending(pi4io);
	if (pending == 0) {
		/* Spurious interrupt ? */
		pr_warn("No pending nested irqs in irq handler\n");
		return IRQ_NONE;
	}

	/* Process each each irq sequentially */
	while (pending) {
		level = __ffs(pending);	// first pin
		handle_nested_irq(irq_find_mapping
				  (pi4io->gpio.irq.domain, level)
		    );
		/* clear pending bit */
		pending &= ~(1 << level);
	}
	return IRQ_HANDLED;
}

static int pi4io16_irq_setup(struct pi4io16_priv *pi4io)
{
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;
	struct gpio_desc *gpio;
	int ret;
	int irq;
	int irq_base = 0;

	gpio = devm_gpiod_get_optional(dev, "irq", GPIOD_IN);
	if (IS_ERR(gpio))
		return PTR_ERR(gpio);

	if (!gpio) {
		dev_warn(dev, "No irq pin for gpio chip");
		return 0;
	}

	mutex_init(&pi4io->irq_lock);
	irq = gpiod_to_irq(gpio);
	if (irq < 0) {
		dev_err(dev, "No irq for gpio=%d rc=%d",
			desc_to_gpio(gpio), irq);
		return irq;
	}

	ret = devm_request_threaded_irq(dev, irq,
					NULL, pi4io16_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING |
					IRQF_SHARED, dev_name(dev), pi4io);
	if (ret < 0) {
		dev_err(dev,
			"Failed to request irq for gpio=%d, irq=%d, rc=%d\n",
			desc_to_gpio(gpio), irq, ret);
		return ret;
	}

	/* Setup per-instance of device irq_chip */
	memset(&pi4io->irq_chip, 0, sizeof(struct irq_chip));
	pi4io->irq_chip.name = "pi4io16";
	pi4io->irq_chip.irq_mask = pi4io16_irq_mask;
	pi4io->irq_chip.irq_unmask = pi4io16_irq_unmask;
	pi4io->irq_chip.irq_bus_lock = pi4io16_irq_bus_lock;
	pi4io->irq_chip.irq_bus_sync_unlock = pi4io16_irq_bus_sync_unlock;
	pi4io->irq_chip.irq_set_type = pi4io16_irq_set_type;

	ret = gpiochip_irqchip_add_nested(&pi4io->gpio,
					  &pi4io->irq_chip,
					  irq_base,
					  handle_simple_irq, IRQ_TYPE_NONE);
	if (ret) {
		dev_err(dev, "could not connect irqchip to gpiochip\n");
		return ret;
	}

	gpiochip_set_nested_irqchip(&pi4io->gpio, &pi4io->irq_chip, irq);
	dev_info(dev, "Successfully setup nested irq_handlers\n");
	return 0;
}

#else
static int pi4io16_irq_setup(struct pi4io16_priv *pi4io)
{
	struct i2c_client *client = pi4io->i2c;

	dev_warn(&client->dev, "IRQ not supported with current config");
	return 0;
}
#endif

static int pi4io16_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int ret;
	struct device *dev = &client->dev;
	struct pi4io16_priv *pi4io;

	dev_info(dev, "pi4io16 probe()\n");

	pi4io = devm_kzalloc(dev, sizeof(struct pi4io16_priv), GFP_KERNEL);
	if (!pi4io) {
		return -ENOMEM;
	}

	i2c_set_clientdata(client, pi4io);
	pi4io->i2c = client;

	pi4io->regmap = devm_regmap_init_i2c(client, &pi4io16_regmap);

	ret = pi4io16_reset_setup(pi4io);
	if (ret < 0) {
		dev_err(dev, "failed to configure reset-gpio: %d", ret);
		return ret;
	}

	/* Reset chip to nice state. */
	pi4io16_reset(pi4io);

	ret = pi4io16_gpio_setup(pi4io);
	if (ret < 0) {
		dev_err(dev, "Failed to setup GPIOs: %d", ret);
		return ret;
	}

	ret = pi4io16_irq_setup(pi4io);
	if (ret < 0) {
		dev_err(dev, "Failed to setup IRQ: %d", ret);
		return ret;
	}
	dev_dbg(dev, "probe finished");

	return 0;
}

static int pi4io16_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id pi4io16_id_table[] = {
	{"pi4ioe5v6416", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, pi4io16_id_table);

#ifdef CONFIG_OF
static const struct of_device_id pi4io16_of_match[] = {
	{.compatible = "pericom,pi4ioe5v6416"},
	{},
};

MODULE_DEVICE_TABLE(of, pi4io16_of_match);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id pi4io16_acpi_match_table[] = {
	{"PI4IO16", 0},
	{},
};
#endif

static struct i2c_driver pi4io16_driver = {
	.driver = {
		   .name = "pi4io16-gpio",
		   .of_match_table = of_match_ptr(pi4io16_of_match),
#ifdef CONFIG_ACPI
		   .acpi_match_table = ACPI_PTR(pi4io16_acpi_match_table),
#endif
	},
	.probe = pi4io16_probe,
	.remove = pi4io16_remove,
	.id_table = pi4io16_id_table,
};

static int __init pi4io16_init(void)
{
	return i2c_add_driver(&pi4io16_driver);
}

/* NOTE: not using module_i2c_driver macro in order to enumerate
 *	 gpio expander earlier.
 */
subsys_initcall(pi4io16_init);

static void __exit pi4io16_exit(void)
{
	i2c_del_driver(&pi4io16_driver);
}

module_exit(pi4io16_exit);

MODULE_DESCRIPTION("PI4IOE5V6416 16-bit I2C GPIO expander driver");
MODULE_LICENSE("GPL v2");
