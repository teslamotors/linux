/* drivers/soc/tegra/fuse/legacy-fuse-odm.c
 *
 * Legacy tegra fuse access (ODM fuses)
 *
 * Copyright 2016 Ben Dooks <ben.dooks@codethink.co.uk>
*/

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/module.h>

#include <soc/tegra/fuse.h>

static struct kobject *fuse_kobj;
static struct mutex fuse_lock;

#define BIT_ADDR(__byte, __bit) ((__byte) * 32 + (__bit))

struct fuse_info {
	const char	*name;
	unsigned int	bit_addr;
	unsigned int	bit_size;
	struct kobj_attribute attr;
};

static struct fuse_info fuse_devkey = {
	.name		= "device_key",
	.bit_addr	= BIT_ADDR(0x16, 22),
	.bit_size	= 32,
};

static struct fuse_info fuse_jtag_disable = {
	.name		= "jtag_disable",
	.bit_addr	= 24,
	.bit_size	= 1,
};

static struct fuse_info fuse_odm_prodmode = {
	.name		= "odm_production_mode",
	.bit_addr	= 23, .bit_size = 1 };

static struct fuse_info fuse_boot_devcfg = {
	.name		= "sec_boot_dev_cfg",
	.bit_addr	= BIT_ADDR(0x18, 22),
	.bit_size	= 16,
};

static struct fuse_info fuse_boot_devsel = {
	.name		= "sec_boot_dev_sel",
	.bit_addr	= BIT_ADDR(0x1A, 6),
	.bit_size	= 3,
};

static struct fuse_info fuse_secbootkey = {
	.name		= "secure_boot_key",
	.bit_addr	= BIT_ADDR(0x0E, 22),
	.bit_size	= 128,
};

static struct fuse_info fuse_sw_reserved = {
	.name		= "sw_reserved",
	.bit_addr	= BIT_ADDR(0x1A, 10),
	.bit_size	= 4,
};

static struct fuse_info fuse_ign_devselstrap = {
	.name		= "ignore_dev_sel_straps",
	.bit_addr	= BIT_ADDR(0x1A, 9),
	.bit_size	= 1,
};

static struct fuse_info fuse_odm_reserved = {
	.name		= "odm_reseved",
	.bit_addr	= BIT_ADDR(0x1A, 14),
	.bit_size	= 256,
};

static struct fuse_info *fuses[] = {
	&fuse_devkey,
	&fuse_jtag_disable,
	&fuse_odm_prodmode,
	&fuse_boot_devcfg,
	&fuse_boot_devsel,
	&fuse_secbootkey,
	&fuse_sw_reserved,
	&fuse_ign_devselstrap,
	&fuse_odm_reserved,
	NULL,
};

#define kattr_to_fuse(__k) container_of(__k, struct fuse_info, attr)

/* copied from nvidia driver */
#define FUSE_CTRL		0x000
#define FUSE_REG_ADDR		0x004
#define FUSE_REG_READ		0x008
#define FUSE_REG_WRITE		0x00C
#define FUSE_TIME_PGM		0x01C
#define FUSE_PRIV2INTFC		0x020
#define FUSE_DIS_PGM		0x02C
#define FUSE_WRITE_ACCESS	0x030
#define FUSE_PWR_GOOD_SW	0x034

#define FUSE_READ	0x1
#define FUSE_WRITE	0x2
#define FUSE_SENSE	0x3
#define FUSE_CMD_MASK	0x3

#define STATE_IDLE	(0x4 << 16)
#define STATE_MASK	(0xF << 16)

#define FUSE_BEGIN	(0x100)	/* fuse base to normal fuse registers */

/* wrapper functions to use the standard fuse driver
 * code, which adds FUSE_BEGIN to the base we want to
 * access
 */
static void legacy_fuse_readl(unsigned offset, u32 *reg)
{
	tegra_fuse_readl(offset - FUSE_BEGIN, reg);
}

static void legacy_fuse_writel(unsigned offset, u32 reg)
{
	tegra_fuse_writel(offset - FUSE_BEGIN, reg);
}

static void fuse_wait_for_idle(void)
{
	u32 reg;
	int timeout = 10000;

	while (true) {
		udelay(1);
		legacy_fuse_readl(FUSE_CTRL, &reg);
		if ((reg & STATE_MASK) == STATE_IDLE)
			break;

		if (--timeout == 0) {
			pr_err("%s: timeout waiting for idle\n", __func__);
			break;
		}
	}
}

static u32 fuse_cmd_read(unsigned addr)
{
	u32 reg;

	fuse_wait_for_idle();
	legacy_fuse_writel(FUSE_REG_ADDR, addr);
	legacy_fuse_readl(FUSE_CTRL, &reg);
	reg &= ~FUSE_CMD_MASK;
	reg |= FUSE_READ;
	legacy_fuse_writel(FUSE_CTRL, reg);

	fuse_wait_for_idle();
	legacy_fuse_readl(FUSE_REG_READ, &reg);
	return reg;
}

static int tegra_legacy_fuse_read(struct fuse_info *info, u32 *out)
{
	unsigned int bitsz = info->bit_size;
	unsigned int addr = info->bit_addr;
	unsigned int dstbit = 0;
	unsigned int toread, start_bit, bit;
	u32 val;

	pr_debug("%s: read %d,%d to %p\n", __func__, addr, bitsz, out);

	while (bitsz > 0) {
		start_bit = addr % 32;
		toread = min(bitsz, 32 - start_bit);
		val = fuse_cmd_read(addr / 32);

		for (bit = start_bit; bit < (start_bit + toread); bit++) {
			if (val & BIT(bit))
				*out |= BIT(dstbit);
			dstbit++;
			if (dstbit == 32) {
				out++;
				dstbit = 0;
			}
		}
		bitsz -= toread;
		addr += toread;
		addr += 32;
	}
	return 0;
}

static bool fuse_odm_prod_mode(void)
{
	int ret;
	u32 data;

	ret = tegra_legacy_fuse_read(&fuse_odm_prodmode, &data);
	if (ret != 0)
		return false;

	return data != 0;
}

static ssize_t tegra_legacy_fuse_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	struct fuse_info *info = kattr_to_fuse(attr);
	char *ptr = buf;
	u32 data[8];
	int ret, i;

	if (info == &fuse_secbootkey && fuse_odm_prod_mode()) {
		pr_err("%s: device is locked, cannot access sbk\n", __func__);
		return -EACCES;
	}

	memset(data, 0, sizeof(data));
	ret = tegra_legacy_fuse_read(info, data);
	if (ret) {
		pr_err("%s: read failed (%d)\n", __func__, ret);
		return ret;
	}

	*ptr++ = '0';
	*ptr++ = 'x';

	for (i = DIV_ROUND_UP(info->bit_size, 32) - 1; i >= 0; i--) {
		sprintf(ptr, "%08x", data[i]);
		ptr += 8;
	}

	*ptr++ = '\n';
	*ptr++ = '\0';
	return ptr - buf;
}

static ssize_t tegra_legacy_fuse_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	return -EINVAL;
}

static int tegra_legacy_fuse_init(void)
{
	struct fuse_info **fuse;
	struct fuse_info *info;
	int rc = -EINVAL;
	unsigned default_mode;

	pr_info("Tegra2/3 legacy fuse userspace driver\n");

	mutex_init(&fuse_lock);

	/* todo - do we need to get the fuse-tegra clocks? */

	fuse_kobj = kobject_create_and_add("fuse", firmware_kobj);
	if (!fuse_kobj) {
		pr_err("%s: failed to create fuse kobject\n", __func__);
		goto exit_err;
	}

	if (!fuse_odm_prod_mode())
		default_mode = 0640;
	else
		default_mode = 0440;

	if (tegra_get_chip_id() == TEGRA20) {
		pr_debug("%s: is tegra20.. altering fuses\n", __func__);
		fuse_devkey.bit_addr = BIT_ADDR(0x12, 8);
		fuse_boot_devcfg.bit_addr = BIT_ADDR(0x14, 8);
		fuse_boot_devsel.bit_addr = BIT_ADDR(0x14, 24);
		fuse_secbootkey.bit_addr = BIT_ADDR(0x0A, 8);
		fuse_sw_reserved.bit_addr = BIT_ADDR(0x14, 8);
		fuse_ign_devselstrap.bit_addr = BIT_ADDR(0x14, 27);
		fuse_odm_reserved.bit_addr = BIT_ADDR(0x16, 4);
	}

	/* add all the fuse files */
	for (fuse = fuses; (info = *fuse) != NULL; fuse++) {
		info->attr.attr.name = info->name;
		info->attr.attr.mode = default_mode;
		info->attr.show = tegra_legacy_fuse_show;
		info->attr.store = tegra_legacy_fuse_store;
		rc = sysfs_create_file(fuse_kobj, &info->attr.attr);
		if (rc) {
			pr_err("%s: failed to add attr '%s': %d\n",
			       __func__, info->name, rc);
			goto err_attr;
		}
	}

	return 0;
err_attr:
	for (fuse = fuses; (info = *fuse) != NULL; info++)
		sysfs_remove_file(fuse_kobj, &info->attr.attr);
exit_err:
	return rc;
}

static void tegra_legacy_fuse_exit(void)
{
	struct fuse_info **fuse;

	for (fuse = fuses; *fuse != NULL; fuse++)
		sysfs_remove_file(fuse_kobj, &(*fuse)->attr.attr);

	kobject_del(fuse_kobj);
}

module_init(tegra_legacy_fuse_init);
module_exit(tegra_legacy_fuse_exit);

MODULE_DESCRIPTION("Tegra legacy ODM fuse driver");
MODULE_AUTHOR("Ben Dooks <ben.dooks@codethink.co.uk>");
MODULE_LICENSE("GPL v2");

