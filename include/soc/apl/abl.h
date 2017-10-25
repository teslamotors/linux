#ifndef __SOC_APL_ABL_H__
#define __SOC_APL_ABL_H__

enum apl_soc_stepping {
	APL_SOC_STEPPING_A0 = 0x50,
	APL_SOC_STEPPING_B0,
	APL_SOC_STEPPING_B1,
	APL_SOC_STEPPING_D0
};

/**
 * struct seed_offset - ABL SEED address holder
 * @seed:  Array holding the SEED offset locations
 */
struct seed_offset {
	unsigned int    device_seed;
	unsigned int    user_seed;
};

/**
 * struct manifest_offset - ABL MANIFEST address and size holder
 * @size: Size of manifest
 * @manifest: Offset for manifest
 */
struct manifest_offset {
	unsigned int    size;
	unsigned int    offset;
};

/**
 * struct hvwer - ABL hardware info
 * @soc_stepping: Stepping of SoC
 * @cpu_cores: Number of CPU cores
 * @cpu_freq: Frequency of CPU
 * @platform_id: ???
 * @sku: ???
 * @memory_total: Total size of available system memory
 */
struct hwver {
	unsigned int	soc_stepping;
	unsigned int	cpu_cores;
	unsigned int	cpu_freq;
	unsigned int	platform_id;
	unsigned int	sku;
	unsigned int	memory_total;
};

int get_apl_hwver(struct hwver *hwver);
int get_apl_manifest_offsets(struct manifest_offset *moff);
int get_apl_seed_offsets(struct seed_offset *ksoff);
int get_apl_seed_list_offsets(struct seed_offset *ksoff);

#endif /* __SOC_APL_ABL_H__ */
