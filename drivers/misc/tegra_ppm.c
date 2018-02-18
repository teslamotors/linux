/*
 * Copyright (c) 2011-2015, NVIDIA CORPORATION. All Rights Reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/hashtable.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/tegra_ppm.h>

#define for_each_fv_entry(fv, fve) \
	for (fve = fv->table + fv->size - 1; fve >= fv->table; fve--)

struct fv_relation {
	struct mutex lock;
	struct clk *c;
	int (*lookup_voltage)(struct clk*, long unsigned int);

	ssize_t max_size;
	int freq_step;

	ssize_t size;
	struct fv_entry {
		unsigned int freq;
		int voltage_mv;
	} *table;
};

struct maxf_cache_entry {
	u32 budget;
	s16 temp_c;
	u8 cores;
	u8 units;
	unsigned maxf;
	struct hlist_node nib; /* next in bucket*/
};

struct tegra_ppm {
	const char *name;

	struct fv_relation *fv;
	struct tegra_ppm_params *params;
	int iddq_ma;

	DECLARE_HASHTABLE(maxf_cache, 7);

	struct mutex lock;

	struct dentry *debugfs_dir;

	struct {
		s32 temp_c;
		u32 volt_mv;
		u32 freq_hz;
		u32 cores;
		u32 iddq_ma;
	} model_query;

	struct {
		s32 temp_c;
		u32 cores;
		u32 iddq_ma;
		u32 budget;
	} cap_query;
};

static int fv_relation_update(struct fv_relation *fv)
{
	int ret = 0;
	unsigned long f, maxf;
	struct fv_entry *fve;

	mutex_lock(&fv->lock);

	maxf = clk_round_rate(fv->c, ULONG_MAX);
	if (IS_ERR_VALUE(maxf))
		return -ENODATA;

	fv->size = maxf / fv->freq_step + 1;

	if (fv->size > fv->max_size) {
		pr_warn("%s: fv->table ought to be bigger (%zu > %zu)\n",
			__func__, fv->size, fv->max_size);
		fv->size = fv->max_size;
	}

	f = maxf;
	for_each_fv_entry(fv, fve) {
		fve->freq = f;
		fve->voltage_mv = fv->lookup_voltage(fv->c, fve->freq);

		if (fve->voltage_mv < 0) {
			int mv = (f == maxf ? INT_MAX : fve[1].voltage_mv);
			pr_warn("%s: failure %d. guessing %dmV for %uHz\n",
				__func__, fve->voltage_mv, mv, fve->freq);
			fve->voltage_mv = mv;
			ret = -ENODATA;
		}
		f -= fv->freq_step;
	}

	mutex_unlock(&fv->lock);

	return ret;
}

/**
 * fv_relation_create() - build a voltage/frequency table for a clock
 * @c : the subject clock domain
 * @freq_step : step size between frequency points in Hz
 * @max_size : max number of frequency/voltage entries
 * @lookup_voltage : callback to get the minimium voltage for a frequency
 *
 * fv_relation_create constructs a voltage/frequency table for a given
 * clock. The table has evenly spaced frequencies from 0Hz to the maximum
 * rate of the clock.
 *
 * Return: pointer to the the newly created &struct fv_relation on
 *  success. -%ENOMEM or -%EINVAL for the usual reasons. -%ENODATA if
 *  a call to @lookup_voltage or clk_round_rate fails
 */
struct fv_relation *fv_relation_create(struct clk *c, int freq_step,
				       ssize_t max_size, int (*lookup_voltage)(
					       struct clk *, long unsigned int))
{
	int ret = 0;
	struct fv_relation *result;
	struct fv_entry *table;

	if (WARN_ON(!c || !lookup_voltage || freq_step <= 0))
		return ERR_PTR(-EINVAL);

	result = kzalloc(sizeof(struct fv_relation), GFP_KERNEL);
	table = kzalloc(sizeof(struct fv_entry[max_size]), GFP_KERNEL);

	if (!result || !table) {
		kfree(result);
		kfree(table);
		return ERR_PTR(-ENOMEM);
	}

	mutex_init(&result->lock);
	result->c = c;
	result->lookup_voltage = lookup_voltage;
	result->freq_step = freq_step;
	result->max_size = max_size;
	result->table = table;

	ret = fv_relation_update(result);
	if (ret) {
		kfree(result->table);
		kfree(result);
		result = ERR_PTR(ret);
	}
	return result;
}
EXPORT_SYMBOL_GPL(fv_relation_create);

/**
 * fv_relation_destroy() - inverse of fv_relation_create
 * @fv : pointer to the &struct fv_relation to be destroyed
 *
 * Free the resources created by a previous call to fv_relation_create
 */
void fv_relation_destroy(struct fv_relation *fv)
{
	if (fv)
		kfree(fv->table);
	kfree(fv);
}
EXPORT_SYMBOL_GPL(fv_relation_destroy);

static inline s64 _pow(s64 val, int pwr)
{
	s64 retval = 1;

	while (pwr) {
		if (pwr & 1)
			retval *= val;
		pwr >>= 1;
		if (pwr)
			val *= val;
	}

	return retval;
}

static s64 calc_leakage_calc_step(struct tegra_ppm_params *common,
				  int iddq_ma, int temp_c, unsigned voltage_mv,
				  int i, int j, int k)
{
	s64 leakage_calc_step;

	leakage_calc_step = common->leakage_consts_ijk[i][j][k];

	/* iddq raised to i */
	for (; i; i--) {
		leakage_calc_step *= iddq_ma;

		/* Convert (mA) to (A) */
		leakage_calc_step = div64_s64(leakage_calc_step, 1000);
	}

	leakage_calc_step = div64_s64(leakage_calc_step, _pow(1000, i));

	/* voltage raised to j */
	leakage_calc_step *= _pow(voltage_mv, j);

	/* Convert (mV)^j to (V)^j */
	leakage_calc_step = div64_s64(leakage_calc_step, _pow(1000, j));

	/* temp raised to k */
	leakage_calc_step *= _pow(temp_c, k);

	/* Convert (C)^k to (dC)^k */
	leakage_calc_step = div64_s64(leakage_calc_step,
					_pow(10, k));

	return leakage_calc_step;
}

static s64 calc_leakage_ma(struct tegra_ppm_params *common,
			   int iddq_ma, int temp_c,
			   unsigned int voltage_mv, int cores)
{
	int i, j, k;
	s64 leakage_ma = 0;

	for (i = 0; i <= 3; i++)
		for (j = 0; j <= 3; j++)
			for (k = 0; k <= 3; k++)
				leakage_ma +=
					calc_leakage_calc_step(
						common, iddq_ma,
						temp_c, voltage_mv, i, j, k);

	/* leakage model coefficients were pre-scaled */
	leakage_ma = div64_s64(leakage_ma, common->ijk_scaled);

	/* scale leakage based on number of cores */
	leakage_ma *= common->leakage_consts_n[cores - 1];
	leakage_ma = div64_s64(leakage_ma, 1000);

	/* set floor for leakage current */
	if (leakage_ma <= common->leakage_min)
		leakage_ma = common->leakage_min;

	return leakage_ma;
}

static s64 calc_dynamic_ma(struct tegra_ppm_params *common,
			   unsigned int voltage_mv, int cores,
			   unsigned int freq_khz)
{
	s64 dyn_ma;

	/* Convert freq to MHz */
	dyn_ma = voltage_mv * freq_khz / 1000;

	/* Convert mV to V */
	dyn_ma = div64_s64(dyn_ma, 1000);
	dyn_ma *= common->dyn_consts_n[cores - 1];

	/* dyn_const_n was in fF, convert it to nF */
	dyn_ma = div64_s64(dyn_ma, 1000000);

	return dyn_ma;
}

static s64 calc_total_ma(struct tegra_ppm_params *params,
			int iddq_ma, int temp_c,
			unsigned int voltage_mv, int cores,
			unsigned int freq_khz)
{
	s64 leak = calc_leakage_ma(params, iddq_ma,
				   temp_c, voltage_mv, cores);
	s64 dyn = calc_dynamic_ma(params, voltage_mv,
				  cores, freq_khz);
	return leak + dyn;
}

static s64 calc_total_mw(struct tegra_ppm_params *params,
			int iddq_ma, int temp_c,
			unsigned int voltage_mv, int cores,
			unsigned int freq_khz)
{
	s64 cur_ma = calc_total_ma(params, iddq_ma, temp_c,
				   voltage_mv, cores, freq_khz);
	return div64_s64(cur_ma * voltage_mv, 1000);
}

static unsigned int calculate_maxf(
	struct tegra_ppm_params *params,
	struct fv_relation *fv, int cores,
	unsigned int budget, int units, int temp_c, int iddq_ma)
{
	unsigned int voltage_mv, freq_khz = 0;
	struct fv_entry *fve;
	s64 val = 0;

	mutex_lock(&fv->lock);
	for_each_fv_entry(fv, fve) {
		freq_khz = fve->freq / 1000;
		voltage_mv = fve->voltage_mv;

		if (units == TEGRA_PPM_UNITS_MILLIWATTS)
			val = calc_total_mw(params, iddq_ma, temp_c,
					    voltage_mv, cores, freq_khz);
		else if (units == TEGRA_PPM_UNITS_MILLIAMPS)
			val = calc_total_ma(params, iddq_ma, temp_c,
					    voltage_mv, cores, freq_khz);

		if (val <= budget)
			goto end;

		freq_khz = 0;
	}
 end:
	mutex_unlock(&fv->lock);
	return freq_khz;
}

#define make_key(budget, units, temp_c, ncores) \
	((ncores<<24) ^ (units<<23) ^ (budget << 8) ^ temp_c)

static unsigned get_maxf_locked(struct tegra_ppm *ctx, unsigned limit,
				int units, int temp_c, int cores)
{
	unsigned maxf;
	struct maxf_cache_entry *me;
	u32 key = make_key(limit, units, temp_c, cores);

	if ((WARN(cores < 0 || cores > ctx->params->n_cores,
		  "power model can't handle %d cores", cores))
	    || (WARN(units != TEGRA_PPM_UNITS_MILLIAMPS &&
		     units != TEGRA_PPM_UNITS_MILLIWATTS,
		     "illegal value for units (%d)", units)))
		return 0;

	/* check cache */
	hash_for_each_possible(ctx->maxf_cache, me, nib, key)
		if (me->budget == limit && me->temp_c == temp_c &&
		    me->cores == cores && me->units == units) {
			maxf = me->maxf;
			return maxf;
		}

	/* service a miss */
	maxf = calculate_maxf(ctx->params,
			      ctx->fv,
			      cores,
			      limit,
			      units,
			      temp_c,
			      ctx->iddq_ma);

	/* best effort to cache the result */
	me = kzalloc(sizeof(*me), GFP_KERNEL);
	if (!IS_ERR_OR_NULL(me)) {
		me->budget = limit;
		me->units = units;
		me->temp_c = temp_c;
		me->maxf = maxf;
		me->cores = cores;
		hash_add(ctx->maxf_cache, &me->nib, key);
	}
	return maxf;
}

/**
 * tegra_ppm_get_maxf() - query maximum allowable frequency given a budget
 * @ctx : the power model to query
 * @units: %TEGRA_PPM_MILLIWATTS or %TEGRA_PPM_MILLIAMPS
 * @limit : the budget
 * @temp_c : the temperature in degrees C
 * @cores : the number of "cores" consuming power
 *
 * If the result has not been previously memoized, compute and memoize
 * the maximum allowable frequency give a power model (@ctx), a budget
 * (@limit, in mA or mW as specified by @units), a temperature
 * (temp_c, in degrees Celcius), and the number of active cores
 * (@cores).
 *
 * Return: If the value of cores is outside the model's expected range, 0.
 *    Otherwise, the (previously) computed frequency in Hz.
 */
unsigned tegra_ppm_get_maxf(struct tegra_ppm *ctx, unsigned int limit,
			    int units, int temp_c, int cores)
{
	unsigned ret;
	mutex_lock(&ctx->lock);

	ret = get_maxf_locked(ctx, limit, units, temp_c, cores);

	mutex_unlock(&ctx->lock);
	return ret;
}

/**
 * tegra_ppm_drop_cache() - eliminate memoized data for a struct tegra_ppm
 * @ctx
 *
 * Discards previously memoized results from tegra_ppm_get_maxf. Also,
 * recomputes the v/f curve for the &struct fv_relation used during the creation
 * of @ctx. Typically called when by the holder of a &struct tegra_ppm pointer
 * when the underlying v/f operating curve has changed.
 *
 */
void tegra_ppm_drop_cache(struct tegra_ppm *ctx)
{
	int bucket;
	struct hlist_node *tmp;
	struct maxf_cache_entry *me;

	mutex_lock(&ctx->lock);
	hash_for_each_safe(ctx->maxf_cache, bucket, tmp, me, nib) {
		hash_del(&me->nib);
		kfree(me);
	}

	WARN_ON(fv_relation_update(ctx->fv));
	mutex_unlock(&ctx->lock);
}
EXPORT_SYMBOL_GPL(tegra_ppm_drop_cache);

#ifdef CONFIG_DEBUG_FS

static int total_ma_show(void *data, u64 *val)
{
	struct tegra_ppm *ctx = data;
	int cores = ctx->model_query.cores;

	if (cores <= 0 || cores > ctx->params->n_cores)
		return -EINVAL;

	mutex_lock(&ctx->lock);
	*val = calc_total_ma(ctx->params, ctx->model_query.iddq_ma,
			     ctx->model_query.temp_c, ctx->model_query.volt_mv,
			     ctx->model_query.cores,
			     ctx->model_query.freq_hz / 1000);
	mutex_unlock(&ctx->lock);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(total_ma_fops, total_ma_show, NULL, "%llu\n");

static int total_mw_show(void *data, u64 *val)
{
	struct tegra_ppm *ctx = data;
	int cores = ctx->model_query.cores;

	if (cores <= 0 || cores > ctx->params->n_cores)
		return -EINVAL;

	mutex_lock(&ctx->lock);
	*val = calc_total_mw(ctx->params, ctx->model_query.iddq_ma,
			     ctx->model_query.temp_c, ctx->model_query.volt_mv,
			     ctx->model_query.cores,
			     ctx->model_query.freq_hz / 1000);
	mutex_unlock(&ctx->lock);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(total_mw_fops, total_mw_show, NULL, "%llu\n");

static int show_ma_limited_hz(void *data, u64 *val)
{
	struct tegra_ppm *ctx = data;
	int cores = ctx->cap_query.cores;

	if (cores <= 0 || cores > ctx->params->n_cores)
		return -EINVAL;

	*val = tegra_ppm_get_maxf(ctx, ctx->cap_query.budget,
				  TEGRA_PPM_UNITS_MILLIAMPS,
				  ctx->cap_query.temp_c,
				  ctx->cap_query.cores);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(ma_limited_hz_fops, show_ma_limited_hz, NULL, "%llu\n");

static int show_mw_limited_hz(void *data, u64 *val)
{
	struct tegra_ppm *ctx = data;
	int cores = ctx->cap_query.cores;

	if (cores <= 0 || cores > ctx->params->n_cores)
		return -EINVAL;

	*val = tegra_ppm_get_maxf(ctx, ctx->cap_query.budget,
				  TEGRA_PPM_UNITS_MILLIWATTS,
				  ctx->cap_query.temp_c,
				  ctx->cap_query.cores);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(mw_limited_hz_fops, show_mw_limited_hz, NULL, "%llu\n");

static int ppm_cache_show(struct seq_file *s, void *data)
{
	int bucket;
	struct maxf_cache_entry *me;
	struct tegra_ppm *ctx = s->private;

	seq_printf(s, "%10s %5s %10s %10s %10s\n",
		   "budget", "units", "temp['C]", "cores[#]", "fmax [Hz]");

	hash_for_each(ctx->maxf_cache, bucket, me, nib)
		seq_printf(s, "%10d %5s %10d %10d %10d\n",
			   me->budget,
			   me->units == TEGRA_PPM_UNITS_MILLIAMPS ? "mA" : "mW",
			   me->temp_c, me->cores, me->maxf);

	return 0;
}

static ssize_t ppm_cache_poison(struct file *f, const char __user *buf,
			    size_t sz, loff_t *off)
{
	struct tegra_ppm *ctx = f->f_inode->i_private;
	tegra_ppm_drop_cache(ctx);

	return sz;
}

static int ppm_cache_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppm_cache_show, inode->i_private);
}

static const struct file_operations ppm_cache_fops = {
	.open = ppm_cache_open,
	.read = seq_read,
	.write = ppm_cache_poison,
	.release = single_release,
};

static struct dentry *ppm_debugfs_dir(void)
{
	static struct mutex lock = __MUTEX_INITIALIZER(lock);
	static struct dentry *base_dir;

	mutex_lock(&lock);
	if (IS_ERR_OR_NULL(base_dir))
		base_dir = debugfs_create_dir("tegra_ppm", NULL);
	mutex_unlock(&lock);

	return base_dir;
}

static int model_debugfs_init(struct tegra_ppm *ctx, struct dentry *parent)
{
	parent = debugfs_create_dir("query_model", parent);
	if (IS_ERR_OR_NULL(parent))
		return PTR_ERR(parent);

	debugfs_create_u32("temp_c", S_IRUSR | S_IWUSR, parent,
			   &ctx->model_query.temp_c);
	debugfs_create_u32("volt_mv", S_IRUSR | S_IWUSR, parent,
			   &ctx->model_query.volt_mv);
	debugfs_create_u32("freq_hz", S_IRUSR | S_IWUSR, parent,
			   &ctx->model_query.freq_hz);
	debugfs_create_u32("cores", S_IRUSR | S_IWUSR, parent,
			   &ctx->model_query.cores);
	debugfs_create_u32("iddq_ma", S_IRUSR | S_IWUSR, parent,
			   &ctx->model_query.iddq_ma);
	debugfs_create_file("total_ma", S_IRUSR, parent, ctx,
			    &total_ma_fops);
	debugfs_create_file("total_mw", S_IRUSR, parent, ctx,
			    &total_mw_fops);

	return 0;
}

static int cap_debugfs_init(struct tegra_ppm *ctx, struct dentry *parent)
{
	parent = debugfs_create_dir("query_cap", parent);
	if (IS_ERR_OR_NULL(parent))
		return PTR_ERR(parent);

	debugfs_create_u32("temp_c", S_IRUSR | S_IWUSR, parent,
			   &ctx->cap_query.temp_c);
	debugfs_create_u32("cores", S_IRUSR | S_IWUSR, parent,
			   &ctx->cap_query.cores);
	debugfs_create_u32("iddq_ma", S_IRUSR | S_IWUSR, parent,
			   &ctx->cap_query.iddq_ma);
	debugfs_create_u32("budget", S_IRUSR | S_IWUSR, parent,
			   &ctx->cap_query.budget);
	debugfs_create_file("ma_limited_hz", S_IRUSR, parent, ctx,
			    &ma_limited_hz_fops);
	debugfs_create_file("mw_limited_hz", S_IRUSR, parent, ctx,
			    &mw_limited_hz_fops);

	return 0;
}

static int ppm_debugfs_init(struct tegra_ppm *ctx,
			    struct dentry *parent)
{
	if (!parent) {
		parent = ppm_debugfs_dir();
		if (IS_ERR_OR_NULL(parent))
			return PTR_ERR(parent);
		parent = debugfs_create_dir(ctx->name, parent);
	} else {
		char buf[32];
		snprintf(buf, sizeof(buf), "ppm.%s", ctx->name);
		parent = debugfs_create_dir(buf, parent);
	}

	if (IS_ERR_OR_NULL(parent))
		return PTR_ERR(parent);

	ctx->debugfs_dir = parent;

	debugfs_create_file("ppm_cache", S_IRUSR | S_IWUSR, parent,
			    ctx, &ppm_cache_fops);
	debugfs_create_u32_array("vf_lut", S_IRUSR, parent,
				 (u32 *)ctx->fv->table, 2*ctx->fv->size);
	debugfs_create_u32("iddq_ma", S_IRUSR, parent,
			   &ctx->iddq_ma);

	model_debugfs_init(ctx, parent);
	cap_debugfs_init(ctx, parent);

	return 0;
}

#else
static int ppm_debugfs_init(struct tegra_ppm *ctx
			    struct dentry *parent)
{ return 0; }
#endif /* CONFIG_DEBUG_FS */

/**
 * of_read_tegra_ppm_params() - read PPM parameters from device tree
 * @np : the device tree node containing the PPM information
 *
 * Allocate a &struct tegra_ppm_params. Populate it according to the
 * device tree properties in *@np.
 *
 * If this function succeeds, the caller is responsible for
 * (eventually) calling kfree on the returned result.
 *
 * Return: on success, a pointer to thew new &struct
 * tegra_ppm_params. -%EINVAL or -%EDOM for device tree content
 * errors. %NULL or other errors for a kzalloc failure.
 */
struct tegra_ppm_params *of_read_tegra_ppm_params(struct device_node *np)
{
	int ret;
	int n_dyn, n_leak, n_coeff;
	struct tegra_ppm_params *params;

	if (!np)
		return ERR_PTR(-EINVAL);

	n_dyn = of_property_count_u32(np, "nvidia,tegra-ppm-cdyn");
	if (n_dyn <= 0) {
		pr_warn("%s: missing required property nvidia,tegra-ppm-cdyn\n",
			__func__);
		return ERR_PTR(-EINVAL);
	} else if (n_dyn > TEGRA_PPM_MAX_CORES) {
		pr_warn("%s: can't handle nvidia,tegra-ppm-cdyn of length %d\n",
			__func__, n_dyn);
		return ERR_PTR(-EDOM);
	}

	n_coeff = of_property_count_u32(np, "nvidia,tegra-ppm-leakage_coeffs");
	if (n_coeff <= 0) {
		pr_warn("%s: missing required property %s\n",
			__func__, "nvidia,tegra-ppm-leakage_coeffs");
		return ERR_PTR(-EINVAL);
	} else if (n_coeff != 64) {
		pr_warn("%s: expected nvidia,tegra-ppm-cdyn length 64, not %d\n",
			__func__, n_coeff);
		return ERR_PTR(-EDOM);
	}

	n_leak = of_property_count_u32(np, "nvidia,tegra-ppm-leakage_weights");
	if ((n_dyn == 1) ? (n_leak > 1) : (n_leak != n_dyn)) {
		pr_warn("__func__: nvidia,tegra-ppm-leakage_weights required but invalid\n");
		return ERR_PTR(-EINVAL);
	}

	params = kzalloc(sizeof(struct tegra_ppm_params), GFP_KERNEL);
	if (IS_ERR_OR_NULL(params))
		return params;
	params->n_cores = n_dyn;

	ret = (of_property_read_u32_array(
		       np, "nvidia,tegra-ppm-cdyn",
		       (u32 *)&params->dyn_consts_n, params->n_cores)
	       || of_property_read_u32_array(
		       np, "nvidia,tegra-ppm-leakage_coeffs",
		       (u32 *)&params->leakage_consts_ijk, 4 * 4 * 4));
	WARN_ON(ret); /* this shouldn't happen */
	if (ret)
		goto err;

	if (n_leak < 0)
		params->leakage_consts_n[0] = 1000;
	else
		ret = of_property_read_u32_array(
			np, "nvidia,tegra-ppm-leakage_weights",
			(u32 *)&params->leakage_consts_n, params->n_cores);
	WARN_ON(ret); /* this shouldn't happen */
	if (ret)
		goto err;

	if (of_property_read_u32(np, "nvidia,tegra-ppm-min_leakage",
				 &params->leakage_min))
		params->leakage_min = 0;

	if (of_property_read_u32(np, "nvidia,tegra-ppm-coeff_scale",
				 &params->ijk_scaled))
		params->ijk_scaled = 100000;

	return params;
err:
	kfree(params);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(of_read_tegra_ppm_params);


/**
 * tegra_ppm_create() - build a processor power model
 * @name : a name for this model to use in debugfs
 * @fv : relation between frequency and minimum allowable voltage
 * @params : parameters for the active and leakage power calculations
 * @iddq_ma : the quiescent current of the voltage domain
 * @debugfs_dir : optional location for debugfs nodes
 *
 * Construct a processor power model which supports querying the maximum
 * allowable frequency (for a given temperature) within a current (mA)
 * budget.
 *
 * Return: pointer to the the newly created &struct tegra_ppm on
 * success. -%EINVAL if name, fv, or params doesn't point to anything.
 * -%ENOMEM on memory allocation failure.
 */
struct tegra_ppm *tegra_ppm_create(const char *name,
				   struct fv_relation *fv,
				   struct tegra_ppm_params *params,
				   int iddq_ma,
				   struct dentry *debugfs_dir)
{
	struct tegra_ppm *result;

	if (IS_ERR_OR_NULL(name) || IS_ERR_OR_NULL(fv) ||
	    IS_ERR_OR_NULL(params))
		return ERR_PTR(-EINVAL);

	result = kzalloc(sizeof(struct tegra_ppm), GFP_KERNEL);
	if (!result)
		return ERR_PTR(-ENOMEM);

	mutex_init(&result->lock);

	result->name = name;
	result->fv = fv;
	result->params = params;
	result->iddq_ma = iddq_ma;

	ppm_debugfs_init(result, debugfs_dir);

	return result;
}
EXPORT_SYMBOL_GPL(tegra_ppm_create);

/**
 * tegra_ppm_destroy() - inverse of tegra_ppm_create
 * @doomed : pointer to struct to destroy
 * @pfv : receptable for @doomed's fv_relation pointer
 * @pparams : receptable for @doomed's tegra_ppm_params pointer
 *
 * Reverse the operations done by tegra_ppm create resulting in the
 * deallocation of struct *@doomed. If @doomed is NULL, nothing is
 * deallocated.
 *
 * If @pfv is non-NULL *@pfv is set to value passed to
 * tegra_ppm_create as fv. Callers may use that value to destroy an
 * fv_relation. Similarly for @pparams and the value passed to
 * tegra_ppm_create as params.
 *
 * if @doomed is %NULL, *@pfv and *@pparams will still be set to %NULL
 *
 */
void tegra_ppm_destroy(struct tegra_ppm *doomed,
		       struct fv_relation **pfv,
		       struct tegra_ppm_params **pparams)
{
	if (pfv)
		*pfv = doomed ? doomed->fv : NULL;
	if (pparams)
		*pparams = doomed ? doomed->params : NULL;

	if (!doomed)
		return;

	debugfs_remove_recursive(doomed->debugfs_dir);
	kfree(doomed);
}
EXPORT_SYMBOL_GPL(tegra_ppm_destroy);
