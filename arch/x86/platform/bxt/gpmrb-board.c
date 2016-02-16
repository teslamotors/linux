#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/i2c.h>

static const struct i2c_board_info i2c_bus_info[] = {

 { I2C_BOARD_INFO("tdf8532", 0xD8>>1) },

};

static int __init gpmrb_board_init(void)
{
  int ret;
  printk("Init gpmrb board\n");
  ret = i2c_register_board_info(4, &i2c_bus_info[0], 1);
  printk("Init gpmrb board: i2c_register function returned: %d\n",ret);
  return ret;
}

arch_initcall(gpmrb_board_init);
MODULE_LICENSE(GPL);
