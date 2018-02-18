#include <linux/types.h>
#include <asm/cputype.h>
#include <asm/cpu.h>
#include <asm/smp_plat.h>

static inline u8 tegra18_logical_to_cluster(u8 cpu) {
	return MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
}

static inline u8 tegra18_logical_to_cpu(u8 cpu) {
	return MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 0);
}

static inline int tegra18_is_cpu_denver(u8 cpu)
{
	return tegra18_logical_to_cluster(cpu) == 0;
}

static inline int tegra18_is_cpu_arm(u8 cpu)
{
	return tegra18_logical_to_cluster(cpu) == 1;
}

static inline int tegra18_logical_to_physical_cpu(u8 cpu)
{
	return (tegra18_logical_to_cluster(cpu) << 2) +
		tegra18_logical_to_cpu(cpu);
}
