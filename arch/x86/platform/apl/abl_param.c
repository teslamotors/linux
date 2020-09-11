#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <soc/apl/abl.h>

static struct kobject *sysfs_obj;
LIST_HEAD(params_list);

struct abl_param {
	struct list_head list;
	struct attribute attr;
	const char *name;
	const char *value;
};

#ifdef CONFIG_ABL_PARAM_MAX
#define MAX_ABL_PARAMS CONFIG_ABL_PARAM_MAX
#else
#define MAX_ABL_PARAMS 32
#endif
/** When __setup macro will be triggered by kernel memory
 * allocation is still not initialized, so use statically
 * defined array to keep parsed params.
 */
static struct abl_param params[MAX_ABL_PARAMS];
static unsigned int num_params;

/* device and user seed */
#define ABL_SEED		"seed"
#define ABL_SEED_LIST		"seed_list"
#define ABL_SEED_FORMAT		"%x,%x"
#define ABL_RET_DSEED_USEED	2

#define ABL_MANIFEST		"oemkm"
#define ABL_MANIFEST_FORMAT	"%x@%x"
#define ABL_RET_SIZE_MANIFEST	2

#define ABL_HWVER		"hwver"
#define ABL_HWVER_FORMAT	"%x,%d,%d,%x,%d,%d"
#define ABL_RET_SIZE_HWVER	6

static int get_apl_offsets(const char* tag, struct seed_offset *ksoff)
{
	struct abl_param *param;
	int res = -ENODATA;

	if (!ksoff) {
		pr_err(KBUILD_MODNAME ": null ptr - ksoff: %p\n", ksoff);
		return -EFAULT;
	}

	list_for_each_entry(param, &params_list, list) {
		if (0 == strcmp(tag, param->name)) {
			res = sscanf(param->value, ABL_SEED_FORMAT,
					&ksoff->device_seed,
					&ksoff->user_seed);

			if (res == ABL_RET_DSEED_USEED)
				res = 0;
			break;
		}
	}
	if ( (0 == strcmp(tag,ABL_SEED_LIST) && (res == -ENODATA) ) )
	{
		ksoff->device_seed = 0;
		ksoff->user_seed = 0;
		res = 0;
	}

	return res;
}


int get_apl_seed_offsets(struct seed_offset *ksoff)
{
	return get_apl_offsets(ABL_SEED, ksoff);
}
EXPORT_SYMBOL_GPL(get_apl_seed_offsets);

int get_apl_seed_list_offsets(struct seed_offset *ksoff)
{
	return get_apl_offsets(ABL_SEED_LIST, ksoff);
}
EXPORT_SYMBOL_GPL(get_apl_seed_list_offsets);

int get_apl_manifest_offsets(struct manifest_offset *moff)
{
	struct abl_param *param;
	int res = -ENODATA;

	if (!moff) {
		pr_err(KBUILD_MODNAME ": null ptr - moff: %p\n", moff);
		return -EFAULT;
	}

	list_for_each_entry(param, &params_list, list) {
		if (0 == strcmp(ABL_MANIFEST, param->name)) {
			res = sscanf(param->value, ABL_MANIFEST_FORMAT,
					&moff->size,
					&moff->offset);

			if (res == ABL_RET_SIZE_MANIFEST)
				res = 0;
			break;
		}
	}

	return res;
}
EXPORT_SYMBOL_GPL(get_apl_manifest_offsets);

int get_apl_hwver(struct hwver *hwver)
{
	struct abl_param *param;
	int res = -ENODATA;

	if (!hwver) {
		pr_err(KBUILD_MODNAME ": null ptr - hwver: %p\n", hwver);
		return -EFAULT;
	}

	list_for_each_entry(param, &params_list, list) {
		if (0 == strcmp(ABL_HWVER, param->name)) {
			res = sscanf(param->value, ABL_HWVER_FORMAT,
					&hwver->soc_stepping,
					&hwver->cpu_cores,
					&hwver->cpu_freq,
					&hwver->platform_id,
					&hwver->sku,
					&hwver->memory_total);

			if (res == ABL_RET_SIZE_HWVER)
				res = 0;
			break;
		}
	}
	return res;
}
EXPORT_SYMBOL_GPL(get_apl_hwver);

/**
 * Generic show function that will print ABL param value
 */
static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct abl_param *param;

	list_for_each_entry(param, &params_list, list) {
		if (0 == strcmp(attr->name, param->name))
			return snprintf(buf, PAGE_SIZE, "%s\n", param->value);
	}

	return -EIO;
}

static const struct sysfs_ops abl_sysfs_ops = {
	.show   = show,
};

static struct kobj_type abl_ktype = {
	.sysfs_ops = &abl_sysfs_ops
};

static int init_sysfs(void)
{
	int ret = 0;

	sysfs_obj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!sysfs_obj)
		return -ENOMEM;

	kobject_init(sysfs_obj, &abl_ktype);

	ret = kobject_add(sysfs_obj, kernel_kobj, "abl");
	if (ret) {
		pr_err(KBUILD_MODNAME ": Cannot add ABL kobject to sysfs\n");
		return ret;
	}

	return ret;
}

static int __init abl_cmdline_init(void)
{
	int ret = 0;
	struct abl_param *param;

	/* Register ABL in sysfs under /sys/kernel */
	ret = init_sysfs();
	if (ret) {
		pr_err(KBUILD_MODNAME ": Cannot initialize ABL sysfs\n");
		return ret;
	}

	/* Expose all ABL params in sysfs */
	list_for_each_entry(param, &params_list, list) {
		param->attr.name = param->name;
		param->attr.mode = 0444;
		ret = sysfs_create_file(sysfs_obj, &param->attr);
		if (ret)
			pr_err(KBUILD_MODNAME ": Cannot create sysfs file for param %s\n",
					param->name);
	}

	pr_info(KBUILD_MODNAME ": Expose ABL parameters to sysfs (/sys/kernel/abl)\n");
	return 0;
}

static void __exit abl_cmdline_exit(void)
{

}

static inline int __init setup_abl_params(char *arg)
{
	struct abl_param *param;
	char *name;
	char *value;

	if (!arg)
		return -EINVAL;

	/* All ABL params are in form of ABL.<name>=<value> */
	name = strsep(&arg, "=");
	if (!name)
		return -ENODATA;

	value = strsep(&arg, "=");
	if (!value)
		return -ENODATA;

	if (num_params < MAX_ABL_PARAMS) {
		param = &params[num_params];
		num_params++;

		INIT_LIST_HEAD(&param->list);
		param->name = name;
		param->value = value;

		list_add(&param->list, &params_list);
	} else {
		pr_warning(KBUILD_MODNAME ":Exceeded max number of ABL parameters - %d\n", MAX_ABL_PARAMS);
	}

	return 0;
}
__setup("ABL.", setup_abl_params);

module_init(abl_cmdline_init);
module_exit(abl_cmdline_exit);
MODULE_LICENSE("GPL");
