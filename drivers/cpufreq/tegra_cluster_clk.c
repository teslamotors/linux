/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <asm/cputype.h>
#include <asm/cpu.h>

/*
 * Note: These enums should be aligned to the regs mentioned in the
 * device tree
*/
enum cluster_clk {
	SYS_CLK,
	A57_CLUSTER_CLK_PUB,
	A57_CLUSTER_CLK_PRIV,
	D_CLUSTER_CLK_PUB,
	D_CLUSTER_CLK_PRIV,
	MAX_CLUSTER_CLK,
};

#define REF_CLK_KHZ 38400

#define NUM_CLUSTER_CLK_REG	23
#define NUM_CLUSTER_CLK_REG_PUB	102

struct tcl_clk_drv_data {
	void __iomem **clk_regs;
	struct platform_device *pdev;
#ifdef CONFIG_DEBUG_FS
	struct dentry *clk_root;
	enum cluster_clk cl_clk[MAX_CLUSTER_CLK];
#endif
};

static struct tcl_clk_drv_data *cl_clk_drv_data;

#ifdef CONFIG_DEBUG_FS

#define OFFSET_CLOCKS_PRIVATE_EDVD_MONITOR_CTRL  0x5
#define OFFSET_CLOCKS_PRIVATE_EDVD_MONITOR_DATA  0x6
#define OFFSET_SYS_CLK_MMCRAB_M_NAFLL_LUT_DEBUG2 0x44
#define OFFSET_CLOCKS_PRIVATE_EDVD_VINDEX  0x16
#define OFFSET_SYS_CLOCKS_PLLX_BASE 0xc800 /* 0x803200 */
#define OFFSET_SYS_CLOCKS_M_ADC_MONITOR  0x091 /* 0x803291 */
#define OFFSET_SYS_CLOCKS_B_ADC_MONITOR  0x191 /* 0x803391 */
#define OFFSET_SYS_CLOCKS_M_NAFLL_LUT_DEBUG2  0x044 /* 0x803244 */
#define OFFSET_SYS_CLOCKS_B_NAFLL_LUT_DEBUG2  0x144 /* 0x803344 */
/* 0x803246 */
#define OFFSET_SYS_CLOCKS_M_NAFLL_LUT_SW_FREQ_REQ  0x046
 /* 0x803346 */
#define OFFSET_SYS_CLOCKS_B_NAFLL_LUT_SW_FREQ_REQ  0x146

#define OFFSET_CLOCKS_PUB_EDVD_CORE0_VOLT_FREQ  0x48
#define OFFSET_CLOCKS_PUB_EDVD_CORE1_VOLT_FREQ  0x49
#define OFFSET_CLOCKS_PUB_EDVD_CORE2_VOLT_FREQ  0x4a
#define OFFSET_CLOCKS_PUB_EDVD_CORE3_VOLT_FREQ  0x4b

#define BUFF_SIZE		50

#define REG_SIZE		0x4
/* Max reg offset can be updated using debugfs nodes */
#define MAX_SYS_CLK_OFFSET	0x194
#define MAX_PRIV_CLK_OFFSET	0x17
#define MAX_PUB_CLK_OFFSET	0x66

#define RW_MODE		(S_IWUSR | S_IRUGO)
#define RO_MODE		(S_IRUGO)

enum volt_rail {
	CPU_RAIL,
	SRAM_RAIL,
};

struct denver_creg {
	unsigned long offset;
	const char *name;
	u64 val;
};

static struct denver_creg denver_cregs[] = {
	{0x00001005, "CREG_DPMU_PCLUSTER0_PWR_CTRL", 0},
	{0x00001006, "CREG_DPMU_PCLUSTER1_PWR_CTRL", 0},
};

#define NR_CREGS (sizeof(denver_cregs) / sizeof(struct denver_creg))
enum creg_command {
	CREG_INDEX = 1,
	CREG_READ,
	CREG_WRITE
};
static struct cpumask denver_cpumask;



static inline uint32_t cl_clk_readl(enum cluster_clk cl_clk, uint32_t offset)
{
	struct device *dev = &cl_clk_drv_data->pdev->dev;

	dev_dbg(dev, "%s:Addr: cl_clk_drv_data->clk_regs[cl_clk: %d]0x%p + (REG_SIZE * offset): 0x%x: 0x%p\n",
		__func__, cl_clk,
		(void __iomem *)cl_clk_drv_data->clk_regs[cl_clk],
		REG_SIZE * offset,
		cl_clk_drv_data->clk_regs[cl_clk] + (REG_SIZE * offset));
	return readl(cl_clk_drv_data->clk_regs[cl_clk] +
		(REG_SIZE * offset));
}

static inline uint32_t cl_clk_writel(enum cluster_clk cl_clk, uint32_t offset,
	 uint32_t value)
{
	struct device *dev = &cl_clk_drv_data->pdev->dev;

	dev_dbg(dev, "%s:Addr: cl_clk_drv_data->clk_regs[cl_clk: %d] + (REG_SIZE * offset: 0x%x) Value:%u\n",
		__func__, cl_clk, REG_SIZE * offset, value);
	writel(value,
		cl_clk_drv_data->clk_regs[cl_clk] + (REG_SIZE * offset));
	return 0;
}
/* To update a register
 * echo "value_in_hex : reg_offset_in_hex" > sys_clk
 * echo "value_in_hex : reg_offset_in_hex" > b_cluster_clk_priv
 * echo "value_in_hex : reg_offset_in_hex" > m_cluster_clk_priv
 * echo "value_in_hex : reg_offset_in_hex" > b_cluster_clk_pub
 * echo "value_in_hex : reg_offset_in_hex" > m_cluster_clk_pub
 */
static ssize_t cl_clk_write(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct device *dev = &cl_clk_drv_data->pdev->dev;
	enum cluster_clk *cl_clk = file->f_path.dentry->d_inode->i_private;
	uint32_t offset_allowed = 0;
	char buf[BUFF_SIZE];
	int32_t offset = -1;
	uint32_t val;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (strncpy_from_user(buf, user_buf, count) <= 0)
		return -EFAULT;

	switch (*cl_clk) {
	case SYS_CLK:
		offset_allowed = MAX_SYS_CLK_OFFSET;
		break;
	case A57_CLUSTER_CLK_PUB:
		offset_allowed = MAX_PUB_CLK_OFFSET;
		break;
	case A57_CLUSTER_CLK_PRIV:
		offset_allowed = MAX_PRIV_CLK_OFFSET;
		break;
	case D_CLUSTER_CLK_PUB:
		offset_allowed = MAX_PUB_CLK_OFFSET;
		break;
	case D_CLUSTER_CLK_PRIV:
		offset_allowed = MAX_PRIV_CLK_OFFSET;
		break;
	case MAX_CLUSTER_CLK:
		offset_allowed = MAX_SYS_CLK_OFFSET;
		dev_err(dev, "\nMAX_CLUSTER_CLK\n");
		break;
	}

	/* terminate buffer and trim - white spaces may be appended
	 * at the end when invoked from shell command line */
	buf[count] = '\0';
	strim(buf);

	if (sscanf(buf, "%x:%x", &offset, &val) != 2)
		return -EINVAL;

	if ((offset < 0) || (offset >= offset_allowed)) {
		dev_err(dev, "Incorrect Args\n");
		return -EFAULT;
	}

	cl_clk_writel(*cl_clk, offset, val);
	return count;

}

static int get_edvd_monitor_data(enum cluster_clk cl_clk,
	int ctrl_select)
{
	cl_clk_writel(cl_clk, OFFSET_CLOCKS_PRIVATE_EDVD_MONITOR_CTRL,
		0xF & ctrl_select);
	return 0xFFFF & cl_clk_readl(cl_clk,
		OFFSET_CLOCKS_PRIVATE_EDVD_MONITOR_DATA);
}

static int dump_cl_avfs_priv(struct seq_file *file,
	enum cluster_clk cl_clk)
{
	uint32_t i;
	uint32_t edvd_vindex = cl_clk_readl(cl_clk,
		OFFSET_CLOCKS_PRIVATE_EDVD_VINDEX);

	seq_printf(file, "cycle_int   = %d\n",
		 get_edvd_monitor_data(cl_clk, 1));
	seq_printf(file, "pro_term    = %d\n",
		 get_edvd_monitor_data(cl_clk, 2));
	seq_printf(file, "int_term    = %d\n",
		 get_edvd_monitor_data(cl_clk, 3));
	seq_printf(file, "output_int  = 0x%02x\n",
		 get_edvd_monitor_data(cl_clk, 4));
	seq_printf(file, "vindex_sent = 0x%02x\n",
		get_edvd_monitor_data(cl_clk, 7));

	mdelay(5);
	seq_printf(file, "freq        = %u mhz\n",
		((REF_CLK_KHZ * get_edvd_monitor_data(cl_clk, 6) / 4) / 1000));

	seq_printf(file, "clocks_private.edvd_vindex                  0x%08x\n",
		edvd_vindex);
	seq_printf(file,
		"clocks_private.edvd_vindex.VINDEX_REQ         0x%02x  %d mV\n",
		0xFF & (edvd_vindex >> 24),
		450 + (10 * (0xFF & (edvd_vindex >> 24))));
	seq_printf(file,
		"clocks_private.edvd_vindex.VINDEX_SENT       0x%02x  %d mV\n",
		0xFF & (edvd_vindex >> 0),
		450 + (10 * (0xFF & (edvd_vindex >> 0))));

	for (i = 0; i < NUM_CLUSTER_CLK_REG; i++)
		seq_printf(file, "clock_private_%u: 0x%08x\n", i,
		cl_clk_readl(cl_clk, i));

	return 0;
}

static int dump_cl_avfs_pub(struct seq_file *file,
	enum cluster_clk cl_clk)
{

	uint32_t edvd_vf0 = cl_clk_readl(cl_clk,
		OFFSET_CLOCKS_PUB_EDVD_CORE0_VOLT_FREQ);
	uint32_t edvd_vf1 = cl_clk_readl(cl_clk,
		OFFSET_CLOCKS_PUB_EDVD_CORE1_VOLT_FREQ);
	uint32_t edvd_vf2 = cl_clk_readl(cl_clk,
		OFFSET_CLOCKS_PUB_EDVD_CORE2_VOLT_FREQ);
	uint32_t edvd_vf3 = cl_clk_readl(cl_clk,
		OFFSET_CLOCKS_PUB_EDVD_CORE3_VOLT_FREQ);
	uint32_t i;

	seq_printf(file, "%s_cpu0 : Ndiv: %d\tVindex:%d\n",
		(cl_clk == A57_CLUSTER_CLK_PUB) ? "b" : "m",
		(edvd_vf0 & 0x1FF), 0xFF & (edvd_vf0 >> 0x10));

	seq_printf(file, "%s_cpu1 : Ndiv: %d\tVindex:%d\n",
		(cl_clk == A57_CLUSTER_CLK_PUB) ? "b" : "m",
		(edvd_vf1 & 0x1FF), 0xFF & (edvd_vf1 >> 0x10));

	seq_printf(file, "%s_cpu2 : Ndiv: %d\tVindex:%d\n",
		(cl_clk == A57_CLUSTER_CLK_PUB) ? "b" : "m",
		(edvd_vf2 & 0x1FF), 0xFF & (edvd_vf2 >> 0x10));

	seq_printf(file, "%s_cpu3 : Ndiv: %d\tVindex:%d\n",
		(cl_clk == A57_CLUSTER_CLK_PUB) ? "b" : "m",
		(edvd_vf3 & 0x1FF), 0xFF & (edvd_vf3 >> 0x10));

	for (i = 0; i < NUM_CLUSTER_CLK_REG_PUB; i++)
		seq_printf(file, "clock_pub_%u: 0x%08x\n", i,
		cl_clk_readl(cl_clk, i));

	return 0;
}

static uint32_t get_voltage_millivolts(uint32_t offset,
	enum volt_rail rail)
{
	uint32_t adc_monitor_data;
	uint32_t sampled = 0;
	uint32_t voltage;

	while (!sampled) {
		adc_monitor_data = cl_clk_readl(SYS_CLK, offset);
		if (((adc_monitor_data >> 1) & 1) == rail) {
			sampled = 1;
			/*
			 * ADC monitored voltage
			 * = ADC_out * 10mV + 450mV
			 * Where 450mV is caliberation data and 10mv is voltage
			 * multiplier
			 */
			voltage = 10 * (adc_monitor_data >> 2) + 450;
		}
	}
	return voltage;
}

static void dump_cl_sys_clocks(struct seq_file *file, enum cluster_clk cl_clk)
{
	uint32_t m_nafll_lut_debug2 = cl_clk_readl(cl_clk,
		OFFSET_SYS_CLOCKS_M_NAFLL_LUT_DEBUG2);
	uint32_t b_nafll_lut_debug2 = cl_clk_readl(cl_clk,
		OFFSET_SYS_CLOCKS_B_NAFLL_LUT_DEBUG2);
	uint32_t m_nafll_lut_sw_freq_req = cl_clk_readl(cl_clk,
		OFFSET_SYS_CLOCKS_M_NAFLL_LUT_SW_FREQ_REQ);
	uint32_t b_nafll_lut_sw_freq_req = cl_clk_readl(cl_clk,
		OFFSET_SYS_CLOCKS_B_NAFLL_LUT_SW_FREQ_REQ);

	seq_printf(file, "mcpu.cpu_rail  %d mV\n",
		get_voltage_millivolts(OFFSET_SYS_CLOCKS_M_ADC_MONITOR,
		CPU_RAIL));
	seq_printf(file, "bcpu.cpu_rail  %d mV\n",
		get_voltage_millivolts(OFFSET_SYS_CLOCKS_B_ADC_MONITOR,
		CPU_RAIL));
	seq_printf(file, "mcpu.sram_rail %d mV\n",
		get_voltage_millivolts(OFFSET_SYS_CLOCKS_M_ADC_MONITOR,
		SRAM_RAIL));
	seq_printf(file, "bcpu.sram_rail %d mV\n",
		get_voltage_millivolts(OFFSET_SYS_CLOCKS_B_ADC_MONITOR,
		SRAM_RAIL));

	seq_printf(file, "sys_clocks.m_nafll_lut_debug2                  0x%08x\n",
		m_nafll_lut_debug2);
	seq_printf(file, "sys_clocks.m_nafll_lut_debug2.NDIV		 0x%03x		%d mhz\n",
		m_nafll_lut_debug2 >> 8,
			 (REF_CLK_KHZ *
				(m_nafll_lut_debug2 >> 8)) / 1000);
	seq_printf(file, "sys_clocks.m_nafll_lut_debug2.VFGAIN              0x%01x\n",
		0xF & (m_nafll_lut_debug2 >> 4));
	seq_printf(file, "sys_clocks.b_nafll_lut_debug2                     0x%08x\n",
		b_nafll_lut_debug2);
	seq_printf(file, "sys_clocks.b_nafll_lut_debug2.NDIV                0x%03x  %d mhz\n",
		b_nafll_lut_debug2 >> 8,
			(REF_CLK_KHZ *
			 (b_nafll_lut_debug2 >> 8)) / 1000);
	seq_printf(file, "sys_clocks.b_nafll_lut_debug2.VFGAIN              0x%01x\n",
		 0xF & (b_nafll_lut_debug2 >> 4));
	seq_printf(file, "sys_clocks.m_nafll_lut_sw_freq_req                0x%08x\n",
		 m_nafll_lut_sw_freq_req);
	seq_printf(file, "sys_clocks.m_nafll_lut_sw_freq_req.VFGAIN         0x%01x\n",
		 0xF & (m_nafll_lut_sw_freq_req >> 20));
	seq_printf(file, "sys_clocks.m_nafll_lut_sw_freq_req.NDIV           0x%03x  %d mhz\n",
		0x1FF & (m_nafll_lut_sw_freq_req >> 4),
			(REF_CLK_KHZ *
				(0x1FF & (m_nafll_lut_sw_freq_req >> 4)))
					/ 1000);
	seq_printf(file, "sys_clocks.m_nafll_lut_sw_freq_req.SW_OVERRIDE_LUT 0x%01x\n",
		0x3 & (m_nafll_lut_sw_freq_req >> 2));
	seq_printf(file, "sys_clocks.b_nafll_lut_sw_freq_req                0x%08x\n",
		b_nafll_lut_sw_freq_req);
	seq_printf(file, "sys_clocks.b_nafll_lut_sw_freq_req.VFGAIN         0x%01x\n",
		0xF & (b_nafll_lut_sw_freq_req >> 20));
	seq_printf(file, "sys_clocks.b_nafll_lut_sw_freq_req.NDIV           0x%03x  %d mhz\n",
		 0x1FF & (b_nafll_lut_sw_freq_req >> 4),
			(REF_CLK_KHZ *
				(0x1FF & (b_nafll_lut_sw_freq_req >> 4)))
					/ 1000);
	seq_printf(file, "sys_clocks.b_nafll_lut_sw_freq_req.SW_OVERRIDE_LUT 0x%01x\n",
		 0x3 & (b_nafll_lut_sw_freq_req >> 2));
}

static int dump_cl_clk_reg(struct seq_file *file, void *data)
{
	struct device *dev = &cl_clk_drv_data->pdev->dev;
	enum cluster_clk *cl_clk = file->private;

	switch (*cl_clk) {
	case SYS_CLK:
		seq_printf(file, "\nsys_clk:\n");
		dump_cl_sys_clocks(file, *cl_clk);
		break;
	case A57_CLUSTER_CLK_PUB:
		seq_printf(file, "\na57_clk_pub:\n");
		dump_cl_avfs_pub(file, *cl_clk);
		break;
	case A57_CLUSTER_CLK_PRIV:
		seq_printf(file, "\na57_clk_priv\n");
		dump_cl_avfs_priv(file, *cl_clk);
		break;
	case D_CLUSTER_CLK_PUB:
		seq_printf(file, "\nd_clk_pub\n");
		dump_cl_avfs_pub(file, *cl_clk);
		break;
	case D_CLUSTER_CLK_PRIV:
		seq_printf(file, "\nd_clk_priv\n");
		dump_cl_avfs_priv(file, *cl_clk);
		break;
	case MAX_CLUSTER_CLK:
		dev_err(dev, "\nMAX_CLUSTER_CLK\n");
		break;
	}

	return 0;
}

static int cl_clk_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_cl_clk_reg, inode->i_private);
}

static const struct file_operations cl_clk_fops = {
	.open = cl_clk_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.write = cl_clk_write,
	.release = single_release,
};

static void __denver_creg_get(void *data)
{
	struct denver_creg *reg = (struct denver_creg *)data;
	struct device *dev = &cl_clk_drv_data->pdev->dev;

	asm volatile (
	"	sys 0, c11, c0, 1, %1\n"
	"	sys 0, c11, c0, 0, %2\n"
	"	sys 0, c11, c0, 0, %3\n"
	"	sysl %0, 0, c11, c0, 0\n"
	: "=r" (reg->val)
	: "r" (reg->offset), "r" (CREG_INDEX), "r" (CREG_READ)
	);

	dev_dbg(dev, "CREG: read %s @ 0x%lx =  0x%llx\n",
			reg->name, reg->offset, reg->val);
}

static void __denver_creg_set(void *data)
{
	struct denver_creg *reg = (struct denver_creg *)data;
	struct device *dev = &cl_clk_drv_data->pdev->dev;

	dev_dbg(dev, "CREG: write %s @ 0x%lx =  0x%llx\n",
			reg->name, reg->offset, reg->val);

	asm volatile (
	"	sys 0, c11, c0, 1, %0\n"
	"	sys 0, c11, c0, 0, %1\n"
	"	sys 0, c11, c0, 1, %2\n"
	"	sys 0, c11, c0, 0, %3\n"
	:
	: "r" (reg->offset), "r" (CREG_INDEX),
	  "r" (reg->val), "r" (CREG_WRITE)
	);
}

static int denver_creg_get(void *data, u64 *val)
{
	struct denver_creg *reg = (struct denver_creg *)data;
	smp_call_function_any(&denver_cpumask, __denver_creg_get, reg, 1);
	*val = reg->val;

	return 0;
}

static int denver_creg_set(void *data, u64 val)
{
	struct denver_creg *reg = (struct denver_creg *)data;

	reg->val = val;
	smp_call_function_any(&denver_cpumask, __denver_creg_set, reg, 1);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(denver_creg_fops,
	denver_creg_get, denver_creg_set, "%llu\n");

static void set_denver_cpumask(void)
{
	int cpu_number;

	cpumask_clear(&denver_cpumask);

	for_each_possible_cpu(cpu_number) {
		struct cpuinfo_arm64 *cpuinfo = &per_cpu(cpu_data, cpu_number);
		u32 midr = cpuinfo->reg_midr;
		if (MIDR_IMPLEMENTOR(midr) != ARM_CPU_IMP_ARM)
			cpumask_set_cpu(cpu_number, &denver_cpumask);
	}
}

static int __init tcl_clk_debug_init(struct platform_device *pdev)
{
	struct tcl_clk_drv_data *drv_data = platform_get_drvdata(pdev);
	struct dentry *tcl_clk_root = drv_data->clk_root;
	struct device *dev = &pdev->dev;
	struct denver_creg *reg;
	int i;

	tcl_clk_root = debugfs_create_dir("tegra_cluster_clk", NULL);
	if (!tcl_clk_root)
		return -ENOMEM;

	drv_data->cl_clk[SYS_CLK] = SYS_CLK;
	if (!debugfs_create_file("sys_clk", RW_MODE, tcl_clk_root,
		 &drv_data->cl_clk[SYS_CLK], &cl_clk_fops))
		goto err_out;

	drv_data->cl_clk[A57_CLUSTER_CLK_PUB] = A57_CLUSTER_CLK_PUB;
	if (!debugfs_create_file("b_cluster_clk_pub", RW_MODE, tcl_clk_root,
		 &drv_data->cl_clk[A57_CLUSTER_CLK_PUB], &cl_clk_fops))
		goto err_out;

	drv_data->cl_clk[A57_CLUSTER_CLK_PRIV] = A57_CLUSTER_CLK_PRIV;
	if (!debugfs_create_file("b_cluster_clk_priv", RW_MODE, tcl_clk_root,
		 &drv_data->cl_clk[A57_CLUSTER_CLK_PRIV], &cl_clk_fops))
		goto err_out;

	drv_data->cl_clk[D_CLUSTER_CLK_PUB] = D_CLUSTER_CLK_PUB;
	if (!debugfs_create_file("m_cluster_clk_pub", RW_MODE, tcl_clk_root,
		 &drv_data->cl_clk[D_CLUSTER_CLK_PUB], &cl_clk_fops))
		goto err_out;

	drv_data->cl_clk[D_CLUSTER_CLK_PRIV] = D_CLUSTER_CLK_PRIV;
	if (!debugfs_create_file("m_cluster_clk_priv", RW_MODE, tcl_clk_root,
		 &drv_data->cl_clk[D_CLUSTER_CLK_PRIV], &cl_clk_fops))
		goto err_out;

	set_denver_cpumask();

	for (i = 0; i < NR_CREGS; ++i) {
		reg = &denver_cregs[i];
		if (!debugfs_create_file(
			reg->name, S_IRUGO, tcl_clk_root,
			reg, &denver_creg_fops))
			goto err_out;
	}

	return 0;
err_out:
	dev_err(dev, "Unable to create debugfs nodes\n");
	debugfs_remove_recursive(tcl_clk_root);
	return -ENOMEM;
}
#endif

static int __init tcl_clk_parse_dt(struct platform_device *pdev)
{
	struct tcl_clk_drv_data *drv_data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	struct resource *res = NULL;
	void __iomem *clk_reg;
	enum cluster_clk clclk;
	int ret = 0;

	drv_data->clk_regs =
		devm_kzalloc(dev, sizeof(void *) * MAX_CLUSTER_CLK,
							GFP_KERNEL);
	if (!drv_data->clk_regs) {
		dev_err(dev, "Failed to allocate regs\n");
		return -ENOMEM;
	}

	for (clclk = 0; clclk < MAX_CLUSTER_CLK; clclk++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, clclk);
		if (!res) {
			dev_err(dev,
			"Failed to get resource with ID %d\n",
							clclk);
			ret = -EINVAL;
			goto err_out;
		}

		clk_reg = devm_ioremap_resource(dev, res);
		if (IS_ERR(clk_reg)) {
			dev_err(dev, "Failed to iomap resource reg[%d]\n",
				clclk);
			ret = PTR_ERR(clk_reg);
			goto err_out;
		}
		drv_data->clk_regs[clclk] = clk_reg;
	}

	return 0;
err_out:
	for (clclk = 0; clclk < MAX_CLUSTER_CLK; clclk++)
		if (drv_data->clk_regs[clclk])
			devm_iounmap(dev, drv_data->clk_regs[clclk]);

	devm_kfree(dev, drv_data->clk_regs);
	return ret;
}
static int __init tcl_clk_probe(struct platform_device *pdev)
{
	struct tcl_clk_drv_data *drv_data;
	struct device *dev = &pdev->dev;
	int ret = 0;

	dev_info(dev, "in probe()...\n");

	drv_data = devm_kzalloc(dev, sizeof(*drv_data),
				GFP_KERNEL);
	if (!drv_data) {
		dev_err(&pdev->dev, "Failed to allocate driver data\n");
		ret = -ENOMEM;
		goto err_out;
	}

	platform_set_drvdata(pdev, drv_data);

	ret = tcl_clk_parse_dt(pdev);
	if (ret)
		goto err_out;
	cl_clk_drv_data = drv_data;
	cl_clk_drv_data->pdev = pdev;

#ifdef CONFIG_DEBUG_FS
	ret = tcl_clk_debug_init(pdev);
	if (ret)
		goto err_out;
#endif
	dev_info(dev, "passed\n");

	return 0;
err_out:
	dev_info(dev, "failed\n");
	devm_kfree(dev, drv_data);
	return ret;
}

static int tcl_clk_remove(struct platform_device *pdev)
{
	struct tcl_clk_drv_data *drv_data = platform_get_drvdata(pdev);
#ifdef CONFIG_DEBUG_FS
	struct dentry *tcl_clk_root = drv_data->clk_root;
#endif
	struct device *dev = &pdev->dev;
	enum cluster_clk clclk;

	for (clclk = 0; clclk < MAX_CLUSTER_CLK; clclk++)
		if (drv_data->clk_regs[clclk])
			devm_iounmap(dev, drv_data->clk_regs[clclk]);
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(tcl_clk_root);
#endif
	devm_kfree(dev, drv_data);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tcl_clk_of_match[] = {
	{ .compatible = "nvidia,t18x-cluster-clk-priv", .data = NULL, },
	{},
};
#endif

static struct platform_driver tegra_cluster_clk_driver __refdata = {
	.driver	= {
		.name	= "tegra_cluster_clk",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcl_clk_of_match),
	},
	.probe		= tcl_clk_probe,
	.remove		= tcl_clk_remove,
};
module_platform_driver(tegra_cluster_clk_driver);

MODULE_AUTHOR("Puneet Saxena <puneets@nvidia.com>");
MODULE_DESCRIPTION("cluster clock driver for Nvidia Tegra18x");
MODULE_LICENSE("GPL");
