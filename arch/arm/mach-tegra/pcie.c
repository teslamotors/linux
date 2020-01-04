/*
 * arch/arm/mach-tegra/pcie.c
 *
 * PCIe host controller driver for TEGRA SOCs
 *
 * Copyright (c) 2010, CompuLab, Ltd.
 * Author: Mike Rapoport <mike@compulab.co.il>
 *
 * Based on NVIDIA PCIe driver
 * Copyright (c) 2008-2011, NVIDIA Corporation.
 *
 * Bits taken from arch/arm/mach-dove/pcie.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/msi.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <asm/sizes.h>
#include <asm/mach/pci.h>

#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/clk.h>
#include <mach/powergate.h>
#include <mach/pci.h>

#define MSELECT_CONFIG_0_ENABLE_PCIE_APERTURE			5

#define PINMUX_AUX_PEX_L0_RST_N_0				0x33bc
#define PINMUX_AUX_PEX_L0_RST_N_0_E_INPUT			5
#define PINMUX_AUX_PEX_L0_RST_N_0_E_INPUT_ENABLE		1

#define PINMUX_AUX_PEX_L1_RST_N_0				0x33cc
#define PINMUX_AUX_PEX_L1_RST_N_0_E_INPUT			5
#define PINMUX_AUX_PEX_L1_RST_N_0_E_INPUT_ENABLE		1

#define PINMUX_AUX_PEX_L2_RST_N_0				0x33d8
#define PINMUX_AUX_PEX_L2_RST_N_0_E_INPUT			5
#define PINMUX_AUX_PEX_L2_RST_N_0_E_INPUT_ENABLE		1
#define AFI_PEX0_CTRL_0_PEX0_CLKREQ_EN				1
#define NV_PCIE2_PADS_REFCLK_CFG1				0x000000cc
#define NV_PCIE2_RP_CTL_3					0x00000FC0
#define APBDEV_PMC_SCRATCH42_0_PCX_CLAMP_MASK			0x1


#define AFI_MSI_VEC0_0						0x6c
#define AFI_MSI_VEC1_0						0x70
#define AFI_MSI_VEC2_0						0x74
#define AFI_MSI_VEC3_0						0x78
#define AFI_MSI_VEC4_0						0x7c
#define AFI_MSI_VEC5_0						0x80
#define AFI_MSI_VEC6_0						0x84
#define AFI_MSI_VEC7_0						0x88

#define AFI_MSI_EN_VEC0_0					0x8c
#define AFI_MSI_EN_VEC1_0					0x90
#define AFI_MSI_EN_VEC2_0					0x94
#define AFI_MSI_EN_VEC3_0					0x98
#define AFI_MSI_EN_VEC4_0					0x9c
#define AFI_MSI_EN_VEC5_0					0xa0
#define AFI_MSI_EN_VEC6_0					0xa4
#define AFI_MSI_EN_VEC7_0					0xa8

#define AFI_MSI_FPCI_BAR_ST_0					0x64
#define AFI_MSI_BAR_SZ_0					0x60
#define AFI_MSI_AXI_BAR_ST_0					0x68
#define AFI_INTR_MASK_0						0xb4
#define AFI_INTR_MASK_0_INT_MASK				0
#define AFI_INTR_MASK_0_MSI_MASK				8


#define AFI_PEXBIAS_CTRL_0					0x168


/* register definitions */
#define AFI_OFFSET						0x3800
#define PADS_OFFSET						0x3000
#define RP0_OFFSET						0x0000
#define RP1_OFFSET						0x1000
#define RP2_OFFSET						0x4000

#define AFI_AXI_BAR0_SZ						0x00
#define AFI_AXI_BAR1_SZ						0x04
#define AFI_AXI_BAR2_SZ						0x08
#define AFI_AXI_BAR3_SZ							0x0c
#define AFI_AXI_BAR4_SZ						0x10
#define AFI_AXI_BAR5_SZ						0x14

#define AFI_AXI_BAR0_START					0x18
#define AFI_AXI_BAR1_START					0x1c
#define AFI_AXI_BAR2_START					0x20
#define AFI_AXI_BAR3_START					0x24
#define AFI_AXI_BAR4_START					0x28
#define AFI_AXI_BAR5_START					0x2c

#define AFI_FPCI_BAR0						0x30
#define AFI_FPCI_BAR1						0x34
#define AFI_FPCI_BAR2						0x38
#define AFI_FPCI_BAR3						0x3c
#define AFI_FPCI_BAR4						0x40
#define AFI_FPCI_BAR5						0x44

#define AFI_CACHE_BAR0_SZ					0x48
#define AFI_CACHE_BAR0_ST					0x4c
#define AFI_CACHE_BAR1_SZ					0x50
#define AFI_CACHE_BAR1_ST					0x54

#define AFI_MSI_BAR_SZ						0x60
#define AFI_MSI_FPCI_BAR_ST					0x64
#define AFI_MSI_AXI_BAR_ST					0x68

#define AFI_CONFIGURATION					0xac
#define AFI_CONFIGURATION_EN_FPCI				(1 << 0)

#define AFI_FPCI_ERROR_MASKS					0xb0

#define AFI_INTR_MASK						0xb4
#define AFI_INTR_MASK_INT_MASK					(1 << 0)
#define AFI_INTR_MASK_MSI_MASK					(1 << 8)

#define AFI_INTR_CODE						0xb8
#define  AFI_INTR_CODE_MASK					0xf
#define  AFI_INTR_MASTER_ABORT					4
#define  AFI_INTR_LEGACY					6

#define AFI_INTR_SIGNATURE					0xbc
#define AFI_SM_INTR_ENABLE					0xc4

#define AFI_AFI_INTR_ENABLE					0xc8
#define AFI_INTR_EN_INI_SLVERR					(1 << 0)
#define AFI_INTR_EN_INI_DECERR					(1 << 1)
#define AFI_INTR_EN_TGT_SLVERR					(1 << 2)
#define AFI_INTR_EN_TGT_DECERR					(1 << 3)
#define AFI_INTR_EN_TGT_WRERR					(1 << 4)
#define AFI_INTR_EN_DFPCI_DECERR				(1 << 5)
#define AFI_INTR_EN_AXI_DECERR					(1 << 6)
#define AFI_INTR_EN_FPCI_TIMEOUT				(1 << 7)
#define AFI_INTR_EN_PRSNT_SENSE					(1 << 8)

#define AFI_PCIE_CONFIG						0x0f8
#define AFI_PCIE_CONFIG_PCIEC0_DISABLE_DEVICE			(1 << 1)
#define AFI_PCIE_CONFIG_PCIEC1_DISABLE_DEVICE			(1 << 2)
#define AFI_PCIE_CONFIG_PCIEC2_DISABLE_DEVICE			(1 << 3)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK		(0xf << 20)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_SINGLE		(0x0 << 20)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_DUAL		(0x1 << 20)
#define AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_411			(0x2 << 20)

#define AFI_FUSE						0x104
#define AFI_FUSE_PCIE_T0_GEN2_DIS				(1 << 2)

#define AFI_PEX0_CTRL						0x110
#define AFI_PEX1_CTRL						0x118
#define AFI_PEX2_CTRL						0x128
#define AFI_PEX_CTRL_RST					(1 << 0)
#define AFI_PEX_CTRL_REFCLK_EN					(1 << 3)

#define RP_VEND_XP						0x00000F00
#define RP_VEND_XP_DL_UP					(1 << 30)

#define RP_LINK_CONTROL_STATUS					0x00000090
#define RP_LINK_CONTROL_STATUS_LINKSTAT_MASK			0x3fff0000

#define PADS_CTL_SEL						0x0000009C

#define PADS_CTL						0x000000A0
#define PADS_CTL_IDDQ_1L					(1 << 0)
#define PADS_CTL_TX_DATA_EN_1L					(1 << 6)
#define PADS_CTL_RX_DATA_EN_1L					(1 << 10)

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define  PADS_PLL_CTL						0x000000B8
#else
#define  PADS_PLL_CTL						0x000000B4
#endif
#define  PADS_PLL_CTL_RST_B4SM					(1 << 1)
#define  PADS_PLL_CTL_LOCKDET					(1 << 8)
#define  PADS_PLL_CTL_REFCLK_MASK				(0x3 << 16)
#define  PADS_PLL_CTL_REFCLK_INTERNAL_CML			(0 << 16)
#define  PADS_PLL_CTL_REFCLK_INTERNAL_CMOS			(1 << 16)
#define  PADS_PLL_CTL_REFCLK_EXTERNAL				(2 << 16)
#define  PADS_PLL_CTL_TXCLKREF_MASK				(0x1 << 20)
#define  PADS_PLL_CTL_TXCLKREF_BUF_EN				(1 << 22)
#define  PADS_PLL_CTL_TXCLKREF_DIV10				(0 << 20)
#define  PADS_PLL_CTL_TXCLKREF_DIV5				(1 << 20)

/* PMC access is required for PCIE xclk (un)clamping */
#define PMC_SCRATCH42						0x144
#define PMC_SCRATCH42_PCX_CLAMP					(1 << 0)

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
/*
 * Tegra2 defines 1GB in the AXI address map for PCIe.
 *
 * That address space is split into different regions, with sizes and
 * offsets as follows:
 *
 * 0x80000000 - 0x80003fff - PCI controller registers
 * 0x80004000 - 0x80103fff - PCI configuration space
 * 0x80104000 - 0x80203fff - PCI extended configuration space
 * 0x80203fff - 0x803fffff - unused
 * 0x80400000 - 0x8040ffff - downstream IO
 * 0x80410000 - 0x8fffffff - unused
 * 0x90000000 - 0x9fffffff - non-prefetchable memory
 * 0xa0000000 - 0xbfffffff - prefetchable memory
 */
#define TEGRA_PCIE_BASE		0x80000000

#define PCIE_REGS_SZ		SZ_16K
#define PCIE_CFG_OFF		PCIE_REGS_SZ
#define PCIE_CFG_SZ		SZ_1M
#define PCIE_EXT_CFG_OFF	(PCIE_CFG_SZ + PCIE_CFG_OFF)
#define PCIE_EXT_CFG_SZ		SZ_1M
#define PCIE_IOMAP_SZ		(PCIE_REGS_SZ + PCIE_CFG_SZ + PCIE_EXT_CFG_SZ)

#define MMIO_BASE		(TEGRA_PCIE_BASE + SZ_4M)
#define MMIO_SIZE		SZ_64K
#define MEM_BASE_0		(TEGRA_PCIE_BASE + SZ_256M)
#define MEM_SIZE		SZ_256M
#define PREFETCH_MEM_BASE_0	(MEM_BASE_0 + MEM_SIZE)
#define PREFETCH_MEM_SIZE	SZ_512M

#else

/*
 * AXI address map for the PCIe aperture , defines 1GB in the AXI
 *  address map for PCIe.
 *
 *  That address space is split into different regions, with sizes and
 *  offsets as follows. Exepct for the Register space, SW is free to slice the
 *  regions as it chooces.
 *
 *  The split below seems to work fine for now.
 *
 *  0x0000_0000 to 0x00ff_ffff - Register space          16MB.
 *  0x0100_0000 to 0x01ff_ffff - Config space            16MB.
 *  0x0200_0000 to 0x02ff_ffff - Extended config space   16MB.
 *  0x0300_0000 to 0x03ff_ffff - Downstream IO space
 *   ... Will be filled with other BARS like MSI/upstream IO etc.
 *  0x1000_0000 to 0x1fff_ffff - non-prefetchable memory aperture
 *  0x2000_0000 to 0x3fff_ffff - Prefetchable memory aperture
 *
 *  Config and Extended config sizes are choosen to support
 *  maximum of 256 devices,
 *  which is good enough for all the current use cases.
 *
 */
#define TEGRA_PCIE_BASE	0x00000000

#define PCIE_REGS_SZ		SZ_16M
#define PCIE_CFG_OFF		PCIE_REGS_SZ
#define PCIE_CFG_SZ		SZ_16M
#define PCIE_EXT_CFG_OFF	(PCIE_CFG_SZ + PCIE_CFG_OFF)
#define PCIE_EXT_CFG_SZ		SZ_16M
/* During the boot only registers/config and extended config apertures are
 * mapped. Rest are mapped on demand by the PCI device drivers.
 */
#define PCIE_IOMAP_SZ		(PCIE_REGS_SZ + PCIE_CFG_SZ + PCIE_EXT_CFG_SZ)

#define MMIO_BASE				(TEGRA_PCIE_BASE + SZ_48M)
#define MMIO_SIZE							SZ_1M
#define MEM_BASE_0				(TEGRA_PCIE_BASE + SZ_256M)
#define MEM_SIZE							SZ_256M
#define PREFETCH_MEM_BASE_0			(MEM_BASE_0 + MEM_SIZE)
#define PREFETCH_MEM_SIZE						SZ_512M
#endif

#define  PCIE_CONF_BUS(b)					((b) << 16)
#define  PCIE_CONF_DEV(d)					((d) << 11)
#define  PCIE_CONF_FUNC(f)					((f) << 8)
#define  PCIE_CONF_REG(r)	\
	(((r) & ~0x3) | (((r) < 256) ? PCIE_CFG_OFF : PCIE_EXT_CFG_OFF))

struct tegra_pcie_port {
	int			index;
	u8			root_bus_nr;
	void __iomem		*base;

	bool			link_up;

	char			io_space_name[16];
	char			mem_space_name[16];
	char			prefetch_space_name[20];
	struct resource		res[3];
};

struct tegra_pcie_info {
	struct tegra_pcie_port	port[MAX_PCIE_SUPPORTED_PORTS];
	int			num_ports;

	void __iomem		*reg_clk_base;
	void __iomem		*regs;
	struct resource		res_mmio;
	int			power_rails_enabled;
	int			pcie_power_enabled;
	int			power_gpio;

	struct regulator	*regulator_hvdd;
	struct regulator	*regulator_pexio;
	struct regulator	*regulator_avdd_plle;
	struct clk		*clk_afi;
	struct clk		*clk_pcie;
	struct clk		*pcie_xclk;
	struct clk		*pll_e;
	struct tegra_pci_platform_data *plat_data;
};

#define pmc_writel(value, reg) \
	__raw_writel(value, (u32)reg_pmc_base + (reg))
#define pmc_readl(reg) \
	__raw_readl((u32)reg_pmc_base + (reg))

static void __iomem *reg_pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);

static struct tegra_pcie_info tegra_pcie = {
	.res_mmio = {
		.name = "PCI IO",
		.start = MMIO_BASE,
		.end = MMIO_BASE + MMIO_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct resource pcie_io_space;
static struct resource pcie_mem_space;
static struct resource pcie_prefetch_mem_space;

unsigned int tegra_pcie_io_size;
unsigned int tegra_pcie_mem_size;
unsigned int tegra_pcie_prefetch_mem_size;

void __iomem *tegra_pcie_io_base;
EXPORT_SYMBOL(tegra_pcie_io_base);

static inline void afi_writel(u32 value, unsigned long offset)
{
	writel(value, offset + AFI_OFFSET + tegra_pcie.regs);
}

static inline u32 afi_readl(unsigned long offset)
{
	return readl(offset + AFI_OFFSET + tegra_pcie.regs);
}

static inline void pads_writel(u32 value, unsigned long offset)
{
	writel(value, offset + PADS_OFFSET + tegra_pcie.regs);
}

static inline u32 pads_readl(unsigned long offset)
{
	return readl(offset + PADS_OFFSET + tegra_pcie.regs);
}

static struct tegra_pcie_port *bus_to_port(int bus)
{
	int i;

	for (i = tegra_pcie.num_ports - 1; i >= 0; i--) {
		int rbus = tegra_pcie.port[i].root_bus_nr;
		if (rbus != -1 && rbus == bus)
			break;
	}

	return i >= 0 ? tegra_pcie.port + i : NULL;
}

static int tegra_pcie_read_conf(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *val)
{
	struct tegra_pcie_port *pp = bus_to_port(bus->number);
	void __iomem *addr;

	if (pp) {
		if (devfn != 0) {
			*val = 0xffffffff;
			return PCIBIOS_DEVICE_NOT_FOUND;
		}

		addr = pp->base + (where & ~0x3);
	} else {
		addr = tegra_pcie.regs + (PCIE_CONF_BUS(bus->number) +
					  PCIE_CONF_DEV(PCI_SLOT(devfn)) +
					  PCIE_CONF_FUNC(PCI_FUNC(devfn)) +
					  PCIE_CONF_REG(where));
	}

	*val = readl(addr);

	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int tegra_pcie_write_conf(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	struct tegra_pcie_port *pp = bus_to_port(bus->number);
	void __iomem *addr;

	u32 mask;
	u32 tmp;
	/* pcie core is supposed to enable bus mastering and io/mem responses
	 * if its not setting then enable corresponding bits in pci_command
	 */
	if (where == PCI_COMMAND) {
		if (!(val & PCI_COMMAND_IO))
			val |= PCI_COMMAND_IO;
		if (!(val & PCI_COMMAND_MEMORY))
			val |= PCI_COMMAND_MEMORY;
		if (!(val & PCI_COMMAND_MASTER))
			val |= PCI_COMMAND_MASTER;
		if (!(val & PCI_COMMAND_SERR))
			val |= PCI_COMMAND_SERR;
	}

	if (pp) {
		if (devfn != 0)
			return PCIBIOS_DEVICE_NOT_FOUND;

		addr = pp->base + (where & ~0x3);
	} else {
		addr = tegra_pcie.regs + (PCIE_CONF_BUS(bus->number) +
					  PCIE_CONF_DEV(PCI_SLOT(devfn)) +
					  PCIE_CONF_FUNC(PCI_FUNC(devfn)) +
					  PCIE_CONF_REG(where));
	}

	if (size == 4) {
		writel(val, addr);
		return PCIBIOS_SUCCESSFUL;
	}

	if (size == 2)
		mask = ~(0xffff << ((where & 0x3) * 8));
	else if (size == 1)
		mask = ~(0xff << ((where & 0x3) * 8));
	else
		return PCIBIOS_BAD_REGISTER_NUMBER;

	tmp = readl(addr) & mask;
	tmp |= val << ((where & 0x3) * 8);
	writel(tmp, addr);

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops tegra_pcie_ops = {
	.read	= tegra_pcie_read_conf,
	.write	= tegra_pcie_write_conf,
};

static void __devinit tegra_pcie_fixup_bridge(struct pci_dev *dev)
{
	u16 reg;

	if ((dev->class >> 16) == PCI_BASE_CLASS_BRIDGE) {
		pci_read_config_word(dev, PCI_COMMAND, &reg);
		reg |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
			PCI_COMMAND_MASTER | PCI_COMMAND_SERR);
		pci_write_config_word(dev, PCI_COMMAND, reg);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_ANY_ID, PCI_ANY_ID, tegra_pcie_fixup_bridge);

/* Tegra PCIE root complex wrongly reports device class */
static void __devinit tegra_pcie_fixup_class(struct pci_dev *dev)
{
	dev->class = PCI_CLASS_BRIDGE_PCI << 8;
}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0bf0, tegra_pcie_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0bf1, tegra_pcie_fixup_class);
#else
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e1c, tegra_pcie_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e1d, tegra_pcie_fixup_class);
#endif

/* Tegra PCIE requires relaxed ordering */
static void __devinit tegra_pcie_relax_enable(struct pci_dev *dev)
{
	u16 val16;
	int pos = pci_find_capability(dev, PCI_CAP_ID_EXP);

	if (pos <= 0) {
		dev_err(&dev->dev, "skipping relaxed ordering fixup\n");
		return;
	}

	pci_read_config_word(dev, pos + PCI_EXP_DEVCTL, &val16);
	val16 |= PCI_EXP_DEVCTL_RELAX_EN;
	pci_write_config_word(dev, pos + PCI_EXP_DEVCTL, val16);
}
DECLARE_PCI_FIXUP_FINAL(PCI_ANY_ID, PCI_ANY_ID, tegra_pcie_relax_enable);

static void __init tegra_pcie_preinit(void)
{
	pcie_io_space.name = "PCIe I/O Space";
	pcie_io_space.start = PCIBIOS_MIN_IO;
	pcie_io_space.end = IO_SPACE_LIMIT;
	pcie_io_space.flags = IORESOURCE_IO;
	if (request_resource(&ioport_resource, &pcie_io_space))
		panic("can't allocate PCIe I/O space");

	pcie_mem_space.name = "PCIe MEM Space";
	pcie_mem_space.start = MEM_BASE_0;
	pcie_mem_space.end = MEM_BASE_0 + MEM_SIZE - 1;
	pcie_mem_space.flags = IORESOURCE_MEM;
	if (request_resource(&iomem_resource, &pcie_mem_space))
		panic("can't allocate PCIe MEM space");

	pcie_prefetch_mem_space.name = "PCIe PREFETCH MEM Space";
	pcie_prefetch_mem_space.start = PREFETCH_MEM_BASE_0;
	pcie_prefetch_mem_space.end = PREFETCH_MEM_BASE_0 + PREFETCH_MEM_SIZE
					- 1;
	pcie_prefetch_mem_space.flags = IORESOURCE_MEM | IORESOURCE_PREFETCH;
	if (request_resource(&iomem_resource, &pcie_prefetch_mem_space))
		panic("can't allocate PCIe PREFETCH MEM space");

	switch (tegra_pcie.num_ports) {
	case 1: {
			tegra_pcie_io_size = IO_SPACE_LIMIT -
				PCIBIOS_MIN_IO + 1;
			break;
		}
	case 2: {
			tegra_pcie_io_size = (IO_SPACE_LIMIT -
					PCIBIOS_MIN_IO + 1) / 2;
			break;
		}
	case 3: {
			tegra_pcie_io_size = (IO_SPACE_LIMIT -
					PCIBIOS_MIN_IO + 1) / 3;
			break;
		}
	default:
		pr_err("PCIe: %d ports not supported\n", tegra_pcie.num_ports);
	}
}

static int tegra_pcie_setup(int nr, struct pci_sys_data *sys)
{
	struct tegra_pcie_port *pp;

	if (nr >= tegra_pcie.num_ports)
		return 0;

	pp = tegra_pcie.port + nr;
	pp->root_bus_nr = sys->busnr;

	/*
	 * IORESOURCE_IO
	 */
	snprintf(pp->io_space_name, sizeof(pp->io_space_name),
		 "PCIe %d I/O", pp->index);
	pp->io_space_name[sizeof(pp->io_space_name) - 1] = 0;
	pp->res[0].name = pp->io_space_name;
	pp->res[0].start = PCIBIOS_MIN_IO + (tegra_pcie_io_size * nr);
	pp->res[0].end = pp->res[0].start + tegra_pcie_io_size - 1;
	pp->res[0].flags = IORESOURCE_IO;
	if (request_resource(&pcie_io_space, &pp->res[0])) {
		pr_err("Request PCIe IO resource failed\n");
		/* return failure */
		return -EBUSY;
	}
	sys->resource[0] = &pp->res[0];
	sys->resource[1] = &pcie_mem_space;
	sys->resource[2] = &pcie_prefetch_mem_space;

	return 1;
}

static int tegra_pcie_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	return INT_PCIE_INTR;
}

static struct pci_bus __init *tegra_pcie_scan_bus(int nr,
						  struct pci_sys_data *sys)
{
	struct tegra_pcie_port *pp;

	if (nr >= tegra_pcie.num_ports)
		return 0;

	pp = tegra_pcie.port + nr;
	pp->root_bus_nr = sys->busnr;

	return pci_scan_bus(sys->busnr, &tegra_pcie_ops, sys);
}

static struct hw_pci tegra_pcie_hw __initdata = {
	.nr_controllers	= MAX_PCIE_SUPPORTED_PORTS,
	.preinit	= tegra_pcie_preinit,
	.setup		= tegra_pcie_setup,
	.scan		= tegra_pcie_scan_bus,
	.swizzle	= pci_std_swizzle,
	.map_irq	= tegra_pcie_map_irq,
};


static irqreturn_t tegra_pcie_isr(int irq, void *arg)
{
	const char *err_msg[] = {
		"Unknown",
		"AXI slave error",
		"AXI decode error",
		"Target abort",
		"Master abort",
		"Invalid write",
		"Response decoding error",
		"AXI response decoding error",
		"Transcation timeout",
	};

	u32 code, signature;

	code = afi_readl(AFI_INTR_CODE) & AFI_INTR_CODE_MASK;
	signature = afi_readl(AFI_INTR_SIGNATURE);
	afi_writel(0, AFI_INTR_CODE);

	if (code == AFI_INTR_LEGACY)
		return IRQ_NONE;

	if (code >= ARRAY_SIZE(err_msg))
		code = 0;

	/*
	 * do not pollute kernel log with master abort reports since they
	 * happen a lot during enumeration
	 */
	if (code == AFI_INTR_MASTER_ABORT)
		pr_debug("PCIE: %s, signature: %08x\n",
				err_msg[code], signature);
	else
		pr_err("PCIE: %s, signature: %08x\n", err_msg[code], signature);

	return IRQ_HANDLED;
}

/*
 *  PCIe support functions
 */
static void tegra_pcie_setup_translations(void)
{
	u32 fpci_bar;
	u32 size;
	u32 axi_address;

	/* Bar 0: config Bar */
	fpci_bar = ((u32)0xfdff << 16);
	size = PCIE_CFG_SZ;
	axi_address = TEGRA_PCIE_BASE + PCIE_CFG_OFF;
	afi_writel(axi_address, AFI_AXI_BAR0_START);
	afi_writel(size >> 12, AFI_AXI_BAR0_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR0);

	/* Bar 1: extended config Bar */
	fpci_bar = ((u32)0xfe1 << 20);
	size = PCIE_EXT_CFG_SZ;
	axi_address = TEGRA_PCIE_BASE + PCIE_EXT_CFG_OFF;
	afi_writel(axi_address, AFI_AXI_BAR1_START);
	afi_writel(size >> 12, AFI_AXI_BAR1_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR1);

	/* Bar 2: downstream IO bar */
	fpci_bar = ((__u32)0xfdfc << 16);
	size = MMIO_SIZE;
	axi_address = MMIO_BASE;
	afi_writel(axi_address, AFI_AXI_BAR2_START);
	afi_writel(size >> 12, AFI_AXI_BAR2_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR2);

	/* Bar 3: prefetchable memory BAR */
	fpci_bar = (((PREFETCH_MEM_BASE_0 >> 12) & 0x0fffffff) << 4) | 0x1;
	size =  PREFETCH_MEM_SIZE;
	axi_address = PREFETCH_MEM_BASE_0;
	afi_writel(axi_address, AFI_AXI_BAR3_START);
	afi_writel(size >> 12, AFI_AXI_BAR3_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR3);

	/* Bar 4: non prefetchable memory BAR */
	fpci_bar = (((MEM_BASE_0 >> 12)	& 0x0FFFFFFF) << 4) | 0x1;
	size = MEM_SIZE;
	axi_address = MEM_BASE_0;
	afi_writel(axi_address, AFI_AXI_BAR4_START);
	afi_writel(size >> 12, AFI_AXI_BAR4_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR4);

	/* Bar 5: NULL out the remaining BAR as it is not used */
	fpci_bar = 0;
	size = 0;
	axi_address = 0;
	afi_writel(axi_address, AFI_AXI_BAR5_START);
	afi_writel(size >> 12, AFI_AXI_BAR5_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR5);

	/* map all upstream transactions as uncached */
	afi_writel(PHYS_OFFSET, AFI_CACHE_BAR0_ST);
	afi_writel(0, AFI_CACHE_BAR0_SZ);
	afi_writel(0, AFI_CACHE_BAR1_ST);
	afi_writel(0, AFI_CACHE_BAR1_SZ);

	/* No MSI */
	afi_writel(0, AFI_MSI_FPCI_BAR_ST);
	afi_writel(0, AFI_MSI_BAR_SZ);
	afi_writel(0, AFI_MSI_AXI_BAR_ST);
	afi_writel(0, AFI_MSI_BAR_SZ);
}

#define TEGRA_PLLE_LOCK_TIMEOUT         300

static int tegra_pcie_enable_controller(void)
{
	u32 val, reg;
	int i;
	int timeout = TEGRA_PLLE_LOCK_TIMEOUT;

	void __iomem *reg_apb_misc_base;
	void __iomem *reg_mselect_base;

	reg_apb_misc_base = IO_ADDRESS(TEGRA_APB_MISC_BASE);
	reg_mselect_base = IO_ADDRESS(TEGRA_MSELECT_BASE);

	/* select the PCIE APERTURE in MSELECT config */
	reg = readl(reg_mselect_base);
	reg |= 1 << MSELECT_CONFIG_0_ENABLE_PCIE_APERTURE;
	writel(reg, reg_mselect_base);

	/* Enable slot clock and pulse the reset signals */
	for (i = 0, reg = AFI_PEX0_CTRL; i < MAX_PCIE_SUPPORTED_PORTS;
			i++, reg += (i*8)) {
		val = afi_readl(reg);
		val |= AFI_PEX_CTRL_REFCLK_EN |
			(1 << AFI_PEX0_CTRL_0_PEX0_CLKREQ_EN);
		afi_writel(val, reg);
		val &= ~AFI_PEX_CTRL_RST;
		afi_writel(val, reg);
		mdelay(2);
		val = afi_readl(reg) | AFI_PEX_CTRL_RST;
		afi_writel(val, reg);
	}
	afi_writel(0, AFI_PEXBIAS_CTRL_0);

	/* Enable dual controller and both ports */
	val = afi_readl(AFI_PCIE_CONFIG);
	val &= ~(AFI_PCIE_CONFIG_PCIEC0_DISABLE_DEVICE |
		 AFI_PCIE_CONFIG_PCIEC1_DISABLE_DEVICE |
		 AFI_PCIE_CONFIG_PCIEC2_DISABLE_DEVICE |
		 AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	val |= AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_DUAL;
#else
	val |= AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_411;
#endif
	afi_writel(val, AFI_PCIE_CONFIG);

	val = afi_readl(AFI_FUSE) & ~AFI_FUSE_PCIE_T0_GEN2_DIS;
	afi_writel(val, AFI_FUSE);

	/* Initialze internal PHY, enable up to 16 PCIE lanes */
	pads_writel(0x0, PADS_CTL_SEL);

	/* override IDDQ to 1 on all 4 lanes */
	val = pads_readl(PADS_CTL) | PADS_CTL_IDDQ_1L;
	pads_writel(val, PADS_CTL);

	/*
	 * set up PHY PLL inputs select PLLE output as refclock,
	 * set TX ref sel to div10 (not div5)
	 */
	val = pads_readl(PADS_PLL_CTL);
	val &= ~(PADS_PLL_CTL_REFCLK_MASK | PADS_PLL_CTL_TXCLKREF_MASK);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	val |= (PADS_PLL_CTL_REFCLK_INTERNAL_CML | PADS_PLL_CTL_TXCLKREF_DIV10);
#else
	val |= (PADS_PLL_CTL_REFCLK_INTERNAL_CML |
			PADS_PLL_CTL_TXCLKREF_BUF_EN);
#endif
	pads_writel(val, PADS_PLL_CTL);

	/* take PLL out of reset  */
	val = pads_readl(PADS_PLL_CTL) | PADS_PLL_CTL_RST_B4SM;
	pads_writel(val, PADS_PLL_CTL);

	/*
	 * Hack, set the clock voltage to the DEFAULT provided by hw folks.
	 * This doesn't exist in the documentation
	 */
	pads_writel(0xfa5cfa5c, 0xc8);
	pads_writel(0x0000FA5C, NV_PCIE2_PADS_REFCLK_CFG1);

	/* Wait for the PLL to lock */
	do {
		val = pads_readl(PADS_PLL_CTL);
		mdelay(1);
		timeout--;
	} while (!(val & PADS_PLL_CTL_LOCKDET) && timeout);

	if (!timeout) {
		pr_err("%s: TIMEOUT: unable to get PLLE lock.\n", __func__);
		return -EBUSY;
	}

	/* turn off IDDQ override */
	val = pads_readl(PADS_CTL) & ~PADS_CTL_IDDQ_1L;
	pads_writel(val, PADS_CTL);

	/* enable TX/RX data */
	val = pads_readl(PADS_CTL);
	val |= (PADS_CTL_TX_DATA_EN_1L | PADS_CTL_RX_DATA_EN_1L);
	pads_writel(val, PADS_CTL);

	/* Take the PCIe interface module out of reset */
	tegra_periph_reset_deassert(tegra_pcie.pcie_xclk);

	/* Finally enable PCIe */
	val = afi_readl(AFI_CONFIGURATION) | AFI_CONFIGURATION_EN_FPCI;
	afi_writel(val, AFI_CONFIGURATION);

	val = (AFI_INTR_EN_INI_SLVERR | AFI_INTR_EN_INI_DECERR |
	       AFI_INTR_EN_TGT_SLVERR | AFI_INTR_EN_TGT_DECERR |
	       AFI_INTR_EN_TGT_WRERR | AFI_INTR_EN_DFPCI_DECERR |
	       AFI_INTR_EN_PRSNT_SENSE);
	afi_writel(val, AFI_AFI_INTR_ENABLE);
	afi_writel(0xffffffff, AFI_SM_INTR_ENABLE);

	/* FIXME: No MSI for now, only INT */
	afi_writel(AFI_INTR_MASK_INT_MASK, AFI_INTR_MASK);

	/* Disable all execptions */
	afi_writel(0, AFI_FPCI_ERROR_MASKS);

	return 0;
}

static int tegra_pci_enable_regulators(void)
{

	if (tegra_pcie.power_rails_enabled)
		return 0;

	if (tegra_pcie.regulator_hvdd)
		regulator_enable(tegra_pcie.regulator_hvdd);
	if (tegra_pcie.regulator_pexio)
		regulator_enable(tegra_pcie.regulator_pexio);
	if (tegra_pcie.regulator_avdd_plle)
		regulator_enable(tegra_pcie.regulator_avdd_plle);

	tegra_pcie.power_rails_enabled = 1;

	return 0;
}

static int tegra_pci_get_regulators(void)
{
	int err;
	if (tegra_pcie.regulator_hvdd == NULL) {
		printk(KERN_INFO "PCIE.C: %s : regulator hvdd_pex\n",
					__func__);
		tegra_pcie.regulator_hvdd =
			regulator_get(NULL, "hvdd_pex");
		if (IS_ERR_OR_NULL(tegra_pcie.regulator_hvdd)) {
			pr_err("%s: unable to get hvdd_pex regulator\n",
					__func__);
			tegra_pcie.regulator_hvdd = 0;
		}
	}

	if (tegra_pcie.regulator_pexio == NULL) {
		printk(KERN_INFO "PCIE.C: %s : regulator pexio\n", __func__);
		tegra_pcie.regulator_pexio =
			regulator_get(NULL, "vdd_pexb");
		if (IS_ERR_OR_NULL(tegra_pcie.regulator_pexio)) {
			pr_err("%s: unable to get pexio regulator\n", __func__);
			tegra_pcie.regulator_pexio = 0;
		}
	}

	/*SATA and PCIE use same PLLE, In default configuration,
	* and we set default AVDD_PLLE with SATA.
	* So if use default board, you have to turn on (LDO2) AVDD_PLLE.
	 */
	if (tegra_pcie.regulator_avdd_plle == NULL) {
		printk(KERN_INFO "PCIE.C: %s : regulator avdd_plle\n",
				__func__);
		tegra_pcie.regulator_avdd_plle = regulator_get(NULL,
						"avdd_plle");
		if (IS_ERR_OR_NULL(tegra_pcie.regulator_avdd_plle)) {
			pr_err("%s: unable to get avdd_plle regulator\n",
				__func__);
			tegra_pcie.regulator_avdd_plle = 0;
		}
	}

	if (tegra_pcie.plat_data->gpio) {
		tegra_pcie.power_gpio = tegra_pcie.plat_data->gpio;
		err = gpio_request(tegra_pcie.power_gpio, "power_pcie");
		if (err < 0) {
			pr_err("%s: couldn't get power_pcie gpio\n", __func__);
			tegra_pcie.power_gpio = 0;
		}

		if (tegra_pcie.power_gpio) {
			err = gpio_direction_output(tegra_pcie.power_gpio, 1);
			if (err < 0) {
				pr_err("%s: couldn't set gpio direction for"
						"power_pcie\n",	__func__);
				tegra_pcie.power_gpio = 0;
			}
		}
	}

	return tegra_pci_enable_regulators();
}

static int tegra_pci_disable_regulators(void)
{
	int err = 0;
	if (tegra_pcie.power_rails_enabled == 0)
		goto err_exit;

	if (tegra_pcie.regulator_hvdd)
		err = regulator_disable(tegra_pcie.regulator_hvdd);
	if (err)
		goto err_exit;
	if (tegra_pcie.regulator_pexio)
		err = regulator_disable(tegra_pcie.regulator_pexio);
	if (err)
		goto err_exit;
	if (tegra_pcie.regulator_avdd_plle)
		err = regulator_disable(tegra_pcie.regulator_avdd_plle);
	tegra_pcie.power_rails_enabled = 0;
err_exit:
	return err;
}

#ifdef CONFIG_PM
static int tegra_pcie_power_on(void)
{
	int err = 0;
	if (tegra_pcie.pcie_power_enabled)
		return 0;
	err = tegra_pci_enable_regulators();
	if (err)
		goto err_exit;
	err = tegra_powergate_power_on(TEGRA_POWERGATE_PCIE);
	if (err)
		goto err_exit;
	if (tegra_pcie.pll_e)
		clk_enable(tegra_pcie.pll_e);

	tegra_pcie.pcie_power_enabled = 1;
err_exit:
	return err;
}
#endif

static int tegra_pcie_power_off(void)
{
	int err = 0;
	if (tegra_pcie.pcie_power_enabled == 0)
		return 0;
	if (tegra_pcie.pll_e)
		clk_disable(tegra_pcie.pll_e);
	if (tegra_pcie.clk_afi)
		clk_disable(tegra_pcie.clk_afi);
	if (tegra_pcie.clk_pcie)
		clk_disable(tegra_pcie.clk_pcie);

	err = tegra_pci_disable_regulators();
	tegra_powergate_power_off(TEGRA_POWERGATE_PCIE);
	tegra_pcie.pcie_power_enabled = 0;
	return err;
}

static int tegra_pcie_clocks_get(struct platform_device *pdev)
{
	tegra_pcie.pcie_xclk = clk_get(&pdev->dev, "pciex");
	if (IS_ERR_OR_NULL(tegra_pcie.pcie_xclk)) {
		pr_err("%s: unable to get PCIE Xclock\n", __func__);
		goto error_exit;
	}
	tegra_pcie.clk_afi = clk_get(&pdev->dev, "afi");
	if (IS_ERR_OR_NULL(tegra_pcie.clk_afi)) {
		pr_err("%s: unable to get AFI clock\n", __func__);
		goto error_exit;
	}
	tegra_pcie.clk_pcie = clk_get(&pdev->dev, "pcie");
	if (IS_ERR_OR_NULL(tegra_pcie.clk_pcie)) {
		pr_err("%s: unable to get PCIE clock\n", __func__);
		goto error_exit;
	}
	tegra_pcie.pll_e = clk_get_sys(NULL, "pll_e");
	if (IS_ERR_OR_NULL(tegra_pcie.pll_e)) {
		pr_err("%s: unable to get PLLE\n", __func__);
		goto error_exit;
	}

	tegra_periph_reset_assert(tegra_pcie.clk_afi);
	tegra_periph_reset_assert(tegra_pcie.clk_pcie);

	clk_enable(tegra_pcie.clk_afi);
	clk_enable(tegra_pcie.clk_pcie);

	tegra_periph_reset_deassert(tegra_pcie.clk_afi);
	tegra_periph_reset_deassert(tegra_pcie.clk_pcie);

	return 0;
error_exit:
	if (tegra_pcie.pcie_xclk)
		clk_put(tegra_pcie.pcie_xclk);
	if (tegra_pcie.clk_afi)
		clk_put(tegra_pcie.clk_afi);
	if (tegra_pcie.clk_pcie)
		clk_put(tegra_pcie.clk_pcie);
	if (tegra_pcie.pll_e)
		clk_put(tegra_pcie.pll_e);
	return -EINVAL;
}

static void tegra_pcie_clocks_put(void)
{
	clk_put(tegra_pcie.pll_e);
	clk_put(tegra_pcie.pcie_xclk);
	clk_put(tegra_pcie.clk_pcie);
	clk_put(tegra_pcie.clk_afi);
}

static int __init tegra_pcie_get_resources(struct platform_device *pdev)
{
	struct resource *res_mmio = 0;
	int err;
	tegra_pcie.power_rails_enabled = 0;
	tegra_powergate_power_on(TEGRA_POWERGATE_PCIE);
	err = tegra_pci_get_regulators();
	if (err) {
		pr_err("PCIE: failed to enable power rails %d\n", err);
		goto err_pwr_on_rail;
	}

	err = tegra_pcie_clocks_get(pdev);
	if (err) {
		pr_err("PCIE: failed to get clocks: %d\n", err);
		return err;
	}

	tegra_periph_reset_assert(tegra_pcie.pcie_xclk);
	err = clk_enable(tegra_pcie.pll_e);
	if (err) {
		pr_err("PCIE: failed to enable PLLE: %d\n", err);
		goto err_pwr_on;
	}

	tegra_pcie.regs = ioremap_nocache(TEGRA_PCIE_BASE, PCIE_IOMAP_SZ);
	if (tegra_pcie.regs == NULL) {
		pr_err("PCIE: Failed to map PCI/AFI registers\n");
		err = -ENOMEM;
		goto err_map_reg;
	}
	res_mmio = &tegra_pcie.res_mmio;

	err = request_resource(&iomem_resource, res_mmio);
	if (err) {
		pr_err("PCIE: Failed to request resources: %d\n", err);
		goto err_req_io;
	}

	tegra_pcie_io_base = ioremap_nocache(res_mmio->start,
					     resource_size(res_mmio));
	if (tegra_pcie_io_base == NULL) {
		pr_err("PCIE: Failed to map IO\n");
		err = -ENOMEM;
		goto err_map_io;
	}

	err = request_irq(INT_PCIE_INTR, tegra_pcie_isr,
			  IRQF_SHARED, "PCIE", &tegra_pcie);
	if (err) {
		pr_err("PCIE: Failed to register IRQ: %d\n", err);
		goto err_irq;
	}
	set_irq_flags(INT_PCIE_INTR, IRQF_VALID);

	return 0;

err_irq:
	iounmap(tegra_pcie_io_base);
err_map_io:
	release_resource(&tegra_pcie.res_mmio);
err_req_io:
	iounmap(tegra_pcie.regs);
err_map_reg:
	tegra_pcie_power_off();
err_pwr_on:
	tegra_pcie_clocks_put();
err_pwr_on_rail:
	tegra_pci_disable_regulators();
	return err;
}

/*
 * FIXME: If there are no PCIe cards attached, then calling this function
 * can result in the increase of the bootup time as there are big timeout
 * loops.
 */
#define TEGRA_PCIE_LINKUP_TIMEOUT	200	/* up to 1.2 seconds */

static bool tegra_pcie_check_link(struct tegra_pcie_port *pp, int idx,
				  u32 reset_reg)
{
	u32 reg;
	int retries = 3;
	int timeout;

	do {

		timeout = TEGRA_PCIE_LINKUP_TIMEOUT;
		while (timeout) {
			reg = readl(pp->base + RP_VEND_XP);

			if (reg & RP_VEND_XP_DL_UP)
				break;

			mdelay(1);
			timeout--;
		}

		if (!timeout)  {
			pr_err("PCIE: port %d: link down, retrying\n", idx);
			goto retry;
		}

		timeout = TEGRA_PCIE_LINKUP_TIMEOUT;
		while (timeout) {
			reg = readl(pp->base + RP_LINK_CONTROL_STATUS);

			if (reg & 0x20000000)
				return true;

			mdelay(1);
			timeout--;
		}

retry:
#if 0
		/* FIXME: reseting link is causing hang on T20 on
		 * other port detection.
		 */

		/* Pulse the PEX reset */
		reg = afi_readl(reset_reg) & ~AFI_PEX_CTRL_RST;
		afi_writel(reg, reset_reg);
		mdelay(2);
		reg = afi_readl(reset_reg) | AFI_PEX_CTRL_RST;
		afi_writel(reg, reset_reg);
#endif
		retries--;
	} while (retries);

	return false;
}

static void __init tegra_pcie_add_port(int index, u32 offset, u32 reset_reg)
{
	struct tegra_pcie_port *pp;

	pp = tegra_pcie.port + tegra_pcie.num_ports;

	pp->index = -1;
	pp->base = tegra_pcie.regs + offset;
	pp->link_up = tegra_pcie_check_link(pp, index, reset_reg);

	if (!pp->link_up) {
		pp->base = NULL;
		printk(KERN_INFO "PCIE: port %d: link down, ignoring\n", index);
		return;
	}

	/* magic register to fix eye value at high temperatures */
	if (!offset)
		writel(0x05100A0C, (NV_PCIE2_RP_CTL_3 + pp->base));
	else
		writel(0x05120A0C, (NV_PCIE2_RP_CTL_3 + pp->base));

	tegra_pcie.num_ports++;
	pp->index = index;
	pp->root_bus_nr = -1;
	memset(pp->res, 0, sizeof(pp->res));
}

static int tegra_pcie_init(struct platform_device *pdev)
{
	int err = 0;
	int port;
	int rp_offset = 0;
	int ctrl_offset = AFI_PEX0_CTRL;

	err = tegra_pcie_get_resources(pdev);
	if (err)
		return err;

	err = tegra_pcie_enable_controller();
	if (err)
		return err;

	/* setup the AFI address translations */
	tegra_pcie_setup_translations();
	for (port = 0; port < MAX_PCIE_SUPPORTED_PORTS; port++) {
		ctrl_offset += (port * 8);
		rp_offset = (rp_offset + 0x1000) * port;
		if (tegra_pcie.plat_data->port_status[port])
			tegra_pcie_add_port(port, rp_offset, ctrl_offset);
	}

	tegra_pcie.pcie_power_enabled = 1;
	if (tegra_pcie.num_ports)
		pci_common_init(&tegra_pcie_hw);
	else
		err = tegra_pcie_power_off();

	return err;
}

static int tegra_pci_probe(struct platform_device *pdev)
{
	tegra_pcie.plat_data = pdev->dev.platform_data;
	dev_dbg(&pdev->dev, "PCIE.C: %s : _port_status[0] %d\n",
		__func__, tegra_pcie.plat_data->port_status[0]);
	dev_dbg(&pdev->dev, "PCIE.C: %s : _port_status[1] %d\n",
		__func__, tegra_pcie.plat_data->port_status[1]);
	dev_dbg(&pdev->dev, "PCIE.C: %s : _port_status[2] %d\n",
		__func__, tegra_pcie.plat_data->port_status[2]);

	return tegra_pcie_init(pdev);
}

#ifdef CONFIG_PM
static int tegra_pci_suspend(struct platform_device *pdev, pm_message_t state)
{
	return tegra_pcie_power_off();
}

static int tegra_pci_resume(struct platform_device *pdev)
{
	return tegra_pcie_power_on();
}
#endif

static int tegra_pci_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver tegra_pci_driver = {
	.probe   = tegra_pci_probe,
	.remove  = tegra_pci_remove,
#ifdef CONFIG_PM
	.suspend = tegra_pci_suspend,
	.resume  = tegra_pci_resume,
#endif
	.driver  = {
		.name  = "tegra-pcie",
		.owner = THIS_MODULE,
	},
};

static int __init tegra_pci_init_driver(void)
{
	return platform_driver_register(&tegra_pci_driver);
}

static void __exit tegra_pci_exit_driver(void)
{
	platform_driver_unregister(&tegra_pci_driver);
}

module_init(tegra_pci_init_driver);
module_exit(tegra_pci_exit_driver);

static struct irq_chip tegra_irq_chip_msi_pcie = {
	.name = "PCIe-MSI",
	.mask = mask_msi_irq,
	.unmask = unmask_msi_irq,
	.enable = unmask_msi_irq,
	.disable = mask_msi_irq,
};

/* 1:1 matching of these to the MSI vectors, 1 per bit */
/* and each mapping matches one of the available interrupts */
/*   irq should equal INT_PCI_MSI_BASE + index */
struct msi_map_entry {
	bool used;
	u8 index;
	int irq;
};

/* hardware supports 256 max*/
#if (INT_PCI_MSI_NR > 256)
#error "INT_PCI_MSI_NR too big"
#endif

#define MSI_MAP_SIZE  (INT_PCI_MSI_NR)
static struct msi_map_entry msi_map[MSI_MAP_SIZE];

static void msi_map_init(void)
{
	int i;

	for (i = 0; i < MSI_MAP_SIZE; i++) {
		msi_map[i].used = false;
		msi_map[i].index = i;
		msi_map[i].irq = 0;
	}
}

/* returns an index into the map*/
static struct msi_map_entry *msi_map_get(void)
{
	struct msi_map_entry *retval = NULL;
	int i;

	for (i = 0; i < MSI_MAP_SIZE; i++) {
		if (!msi_map[i].used) {
			retval = msi_map + i;
			retval->irq = INT_PCI_MSI_BASE + i;
			retval->used = true;
			break;
		}
	}

	return retval;
}

void msi_map_release(struct msi_map_entry *entry)
{
	if (entry) {
		entry->used = false;
		entry->irq = 0;
	}
}

static irqreturn_t pci_tegra_msi_isr(int irq, void *arg)
{
	int i;
	int offset;
	int index;
	u32 reg;

	for (i = 0; i < 8; i++) {
		reg = afi_readl(AFI_MSI_VEC0_0 + i * 4);
		while (reg != 0x00000000) {
			offset = find_first_bit((unsigned long int *)&reg, 32);
			index = i * 32 + offset;
			if (index < MSI_MAP_SIZE) {
				if (msi_map[index].used)
					generic_handle_irq(msi_map[index].irq);
				else
					printk(KERN_INFO "unexpected MSI(1)\n");
			} else {
				/* that's weird who triggered this?*/
				/* just clear it*/
				printk(KERN_INFO "unexpected MSI (2)\n");
			}
			/* clear the interrupt */
			afi_writel(1ul << index, AFI_MSI_VEC0_0 + i * 4);
			/* see if there's any more pending in this vector */
			reg = afi_readl(AFI_MSI_VEC0_0 + i * 4);
		}
	}

	return IRQ_HANDLED;
}

static bool pci_tegra_enable_msi(void)
{
	bool retval = false;
	static bool already_done;
	u32 reg;
	u32 msi_base = 0;
	u32 msi_aligned = 0;

	/* enables MSI interrupts.  */
	/* this only happens once. */
	if (already_done) {
		retval = true;
		goto exit;
	}

	msi_map_init();

	if (request_irq(INT_PCIE_MSI, pci_tegra_msi_isr,
		IRQF_SHARED, "PCIe-MSI",
		pci_tegra_msi_isr)) {
			pr_err("%s: Cannot register IRQ %u\n",
				__func__, INT_PCIE_MSI);
			goto exit;
	}

	/* setup AFI/FPCI range */
	/* FIXME do this better! should be based on PAGE_SIZE */
	msi_base = __get_free_pages(GFP_KERNEL, 3);
	msi_aligned = ((msi_base + ((1<<12) - 1)) & ~((1<<12) - 1));
	msi_aligned = virt_to_bus((void *)msi_aligned);

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	afi_writel(msi_aligned, AFI_MSI_FPCI_BAR_ST_0);
#else
	/* different from T20!*/
	afi_writel(msi_aligned>>8, AFI_MSI_FPCI_BAR_ST_0);
#endif
	afi_writel(msi_aligned, AFI_MSI_AXI_BAR_ST_0);
	/* this register is in 4K increments */
	afi_writel(1, AFI_MSI_BAR_SZ_0);

	/* enable all MSI vectors */
	afi_writel(0xffffffff, AFI_MSI_EN_VEC0_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC1_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC2_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC3_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC4_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC5_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC6_0);
	afi_writel(0xffffffff, AFI_MSI_EN_VEC7_0);

	/* and unmask the MSI interrupt */
	reg = 0;
	reg |= ((1 << AFI_INTR_MASK_0_INT_MASK) |
			(1 << AFI_INTR_MASK_0_MSI_MASK));
	afi_writel(reg, AFI_INTR_MASK_0);

	set_irq_flags(INT_PCIE_MSI, IRQF_VALID);

	already_done = true;
	retval = true;
exit:
	if (!retval) {
		if (msi_base)
			free_pages(msi_base, 3);
	}
	return retval;
}


/* called by arch_setup_msi_irqs in drivers/pci/msi.c */
int arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	int retval = -EINVAL;
	struct msi_msg msg;
	struct msi_map_entry *map_entry = NULL;

	if (!pci_tegra_enable_msi())
		goto exit;

	map_entry = msi_map_get();
	if (map_entry == NULL)
		goto exit;


	dynamic_irq_init(map_entry->irq);
	set_irq_chip_and_handler(map_entry->irq,
				&tegra_irq_chip_msi_pcie,
				handle_simple_irq);

	set_irq_msi(map_entry->irq, desc);

	msg.address_lo = afi_readl(AFI_MSI_AXI_BAR_ST_0);
	/* 32 bit address only */
	msg.address_hi = 0;
	msg.data = map_entry->index;

	write_msi_msg(map_entry->irq, &msg);

	retval = 0;
exit:
	if (retval != 0) {
		if (map_entry)
			msi_map_release(map_entry);
	}

	return retval;
}

void arch_teardown_msi_irq(unsigned int irq)
{
	int i;
	for (i = 0; i < MSI_MAP_SIZE; i++) {
		if ((msi_map[i].used) && (msi_map[i].irq == irq)) {
			dynamic_irq_cleanup(msi_map[i].irq);
			msi_map_release(msi_map + i);
			break;
		}
	}
}
