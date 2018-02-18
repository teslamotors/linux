/*
 * Copyright (C) 2010,2011 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *	Erik Gilling <ccross@android.com>
 *
 * Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved.
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


#include <linux/resource.h>
#include <linux/dma-mapping.h>
#include <linux/fsl_devices.h>
#include <linux/serial_8250.h>
#include <linux/mipi-bif-tegra.h>
#include <linux/nvhost.h>
#include <linux/clk.h>
#include <linux/tegra-soc.h>
#include <mach/irqs.h>
#include <linux/tegra_smmu.h>
#include <linux/dma-contiguous.h>

#ifdef CONFIG_PLATFORM_ENABLE_IOMMU
#include <asm/dma-iommu.h>
#endif

#include "gpio-names.h"
#include "iomap.h"
#include "devices.h"
#include "board.h"

#define TEGRA_DMA_REQ_SEL_I2S_1			2
#define TEGRA_DMA_REQ_SEL_SPD_I			3
#define TEGRA_DMA_REQ_SEL_I2S2_1		7
#define TEGRA_DMA_REQ_SEL_SPI			11
#define TEGRA_DMA_REQ_SEL_DTV			TEGRA_DMA_REQ_SEL_SPI

static struct resource emc_resource[] = {
	[0] = {
		.start	= TEGRA_EMC_BASE,
		.end	= TEGRA_EMC_BASE + TEGRA_EMC_SIZE-1,
		.flags	= IORESOURCE_MEM,
	}
};

struct platform_device tegra_emc_device = {
	.name		= "tegra-emc",
	.id		= -1,
	.resource	= emc_resource,
	.num_resources	= ARRAY_SIZE(emc_resource),
};

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
static struct resource resources_nor[] = {
	[0] = {
		.start = INT_SNOR,
		.end = INT_SNOR,
		.flags = IORESOURCE_IRQ,
	},
	[1] = {
		/* Map SNOR Controller */
		.start = TEGRA_SNOR_BASE,
		.end = TEGRA_SNOR_BASE + TEGRA_SNOR_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		/* Map the size of flash */
		.start = TEGRA_NOR_FLASH_BASE,
		.end = TEGRA_NOR_FLASH_BASE + TEGRA_NOR_FLASH_SIZE - 1,
		.flags = IORESOURCE_MEM,
	}
};

struct platform_device tegra_nor_device = {
	.name = "tegra-nor",
	.id = -1,
	.num_resources = ARRAY_SIZE(resources_nor),
	.resource = resources_nor,
	.dev = {
		.coherent_dma_mask = 0xffffffff,
	},
};
#endif

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
static struct resource dtv_resource[] = {
	[0] = {
		.start  = TEGRA_DTV_BASE,
		.end    = TEGRA_DTV_BASE + TEGRA_DTV_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_DTV,
		.end	= TEGRA_DMA_REQ_SEL_DTV,
		.flags	= IORESOURCE_DMA
	},
};

struct platform_device tegra_dtv_device = {
	.name           = "tegra_dtv",
	.id             = -1,
	.resource       = dtv_resource,
	.num_resources  = ARRAY_SIZE(dtv_resource),
	.dev = {
		.init_name = "dtv",
		.coherent_dma_mask = 0xffffffff,
	},
};
#endif

static struct resource sdhci_resource1[] = {
	[0] = {
		.start	= INT_SDMMC1,
		.end	= INT_SDMMC1,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC1_BASE,
		.end	= TEGRA_SDMMC1_BASE + TEGRA_SDMMC1_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
static struct resource sdhci_resource2[] = {
	[0] = {
		.start	= INT_SDMMC2,
		.end	= INT_SDMMC2,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC2_BASE,
		.end	= TEGRA_SDMMC2_BASE + TEGRA_SDMMC2_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};
#endif

static struct resource sdhci_resource3[] = {
	[0] = {
		.start	= INT_SDMMC3,
		.end	= INT_SDMMC3,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC3_BASE,
		.end	= TEGRA_SDMMC3_BASE + TEGRA_SDMMC3_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource4[] = {
	[0] = {
		.start	= INT_SDMMC4,
		.end	= INT_SDMMC4,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC4_BASE,
		.end	= TEGRA_SDMMC4_BASE + TEGRA_SDMMC4_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

/* board files should fill in platform_data register the devices themselvs.
 * See board-harmony.c for an example
 */
struct platform_device tegra_sdhci_device1 = {
	.name		= "sdhci-tegra",
	.id		= 0,
	.resource	= sdhci_resource1,
	.num_resources	= ARRAY_SIZE(sdhci_resource1),
};

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
struct platform_device tegra_sdhci_device2 = {
	.name		= "sdhci-tegra",
	.id		= 1,
	.resource	= sdhci_resource2,
	.num_resources	= ARRAY_SIZE(sdhci_resource2),
};
#endif

struct platform_device tegra_sdhci_device3 = {
	.name		= "sdhci-tegra",
	.id		= 2,
	.resource	= sdhci_resource3,
	.num_resources	= ARRAY_SIZE(sdhci_resource3),
};

struct platform_device tegra_sdhci_device4 = {
	.name		= "sdhci-tegra",
	.id		= 3,
	.resource	= sdhci_resource4,
	.num_resources	= ARRAY_SIZE(sdhci_resource4),
};

static struct resource tegra_usb1_resources[] = {
	[0] = {
		.start	= TEGRA_USB_BASE,
		.end	= TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB,
		.end	= INT_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_usb2_resources[] = {
	[0] = {
		.start	= TEGRA_USB2_BASE,
		.end	= TEGRA_USB2_BASE + TEGRA_USB2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB2,
		.end	= INT_USB2,
		.flags	= IORESOURCE_IRQ,
	},
};

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
static struct resource tegra_usb3_resources[] = {
	[0] = {
		.start	= TEGRA_USB3_BASE,
		.end	= TEGRA_USB3_BASE + TEGRA_USB3_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB3,
		.end	= INT_USB3,
		.flags	= IORESOURCE_IRQ,
	},
};
#endif

#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
static struct resource tegra_xusb_resources[] = {
	[0] = DEFINE_RES_MEM_NAMED(TEGRA_XUSB_HOST_BASE, TEGRA_XUSB_HOST_SIZE,
			"host"),
	[1] = DEFINE_RES_MEM_NAMED(TEGRA_XUSB_FPCI_BASE, TEGRA_XUSB_FPCI_SIZE,
			"fpci"),
	[2] = DEFINE_RES_MEM_NAMED(TEGRA_XUSB_IPFS_BASE, TEGRA_XUSB_IPFS_SIZE,
			"ipfs"),
	[3] = DEFINE_RES_MEM_NAMED(TEGRA_XUSB_PADCTL_BASE,
			TEGRA_XUSB_PADCTL_SIZE, "padctl"),
	[4] = DEFINE_RES_IRQ_NAMED(INT_XUSB_HOST_INT, "host"),
	[5] = DEFINE_RES_IRQ_NAMED(INT_XUSB_HOST_SMI, "host-smi"),
	[6] = DEFINE_RES_IRQ_NAMED(INT_XUSB_PADCTL, "padctl"),
	[7] = DEFINE_RES_IRQ_NAMED(INT_USB3, "usb3"),
	[8] = DEFINE_RES_IRQ_NAMED(INT_USB2, "usb2"),
};

static u64 tegra_xusb_dmamask = DMA_BIT_MASK(64);

struct platform_device tegra_xhci_device = {
	.name = "tegra-xhci",
	.id = -1,
	.dev = {
		.dma_mask = &tegra_xusb_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_xusb_resources,
	.num_resources = ARRAY_SIZE(tegra_xusb_resources),
};
#endif

static u64 tegra_ehci_dmamask = DMA_BIT_MASK(64);

struct platform_device tegra_ehci1_device = {
	.name	= "tegra-ehci",
	.id	= 0,
	.dev	= {
		.dma_mask	= &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_usb1_resources,
	.num_resources = ARRAY_SIZE(tegra_usb1_resources),
};

struct platform_device tegra_ehci2_device = {
	.name	= "tegra-ehci",
	.id	= 1,
	.dev	= {
		.dma_mask	= &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_usb2_resources,
	.num_resources = ARRAY_SIZE(tegra_usb2_resources),
};

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
struct platform_device tegra_ehci3_device = {
	.name	= "tegra-ehci",
	.id	= 2,
	.dev	= {
		.dma_mask	= &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_usb3_resources,
	.num_resources = ARRAY_SIZE(tegra_usb3_resources),
};
#endif

static struct resource tegra_pmu_resources[] = {
	[0] = {
		.start	= INT_CPU0_PMU_INTR,
		.end	= INT_CPU0_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= INT_CPU1_PMU_INTR,
		.end	= INT_CPU1_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	[2] = {
		.start	= INT_CPU2_PMU_INTR,
		.end	= INT_CPU2_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= INT_CPU3_PMU_INTR,
		.end	= INT_CPU3_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct platform_device tegra_pmu_device = {
	.name		= "arm-pmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_pmu_resources),
	.resource	= tegra_pmu_resources,
};

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource i2s_resource1[] = {
	[0] = {
		.start	= INT_I2S1,
		.end	= INT_I2S1,
		.flags	= IORESOURCE_IRQ
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_I2S_1,
		.end	= TEGRA_DMA_REQ_SEL_I2S_1,
		.flags	= IORESOURCE_DMA
	},
	[2] = {
		.start	= TEGRA_I2S1_BASE,
		.end	= TEGRA_I2S1_BASE + TEGRA_I2S1_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device1 = {
	.name		= "tegra20-i2s",
	.id		= 0,
	.resource	= i2s_resource1,
	.num_resources	= ARRAY_SIZE(i2s_resource1),
};

static struct resource i2s_resource2[] = {
	[0] = {
		.start	= INT_I2S2,
		.end	= INT_I2S2,
		.flags	= IORESOURCE_IRQ
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_I2S2_1,
		.end	= TEGRA_DMA_REQ_SEL_I2S2_1,
		.flags	= IORESOURCE_DMA
	},
	[2] = {
		.start	= TEGRA_I2S2_BASE,
		.end	= TEGRA_I2S2_BASE + TEGRA_I2S2_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device2 = {
	.name		= "tegra20-i2s",
	.id		= 1,
	.resource	= i2s_resource2,
	.num_resources	= ARRAY_SIZE(i2s_resource2),
};
#elif !defined(CONFIG_ARCH_TEGRA_APE)
static struct resource i2s_resource0[] = {
	[0] = {
		.start	= TEGRA_I2S0_BASE,
		.end	= TEGRA_I2S0_BASE + TEGRA_I2S0_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device0 = {
	.name		= "tegra30-i2s",
	.id		= 0,
	.resource	= i2s_resource0,
	.num_resources	= ARRAY_SIZE(i2s_resource0),
};

static struct resource i2s_resource1[] = {
	[0] = {
		.start	= TEGRA_I2S1_BASE,
		.end	= TEGRA_I2S1_BASE + TEGRA_I2S1_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device1 = {
	.name		= "tegra30-i2s",
	.id		= 1,
	.resource	= i2s_resource1,
	.num_resources	= ARRAY_SIZE(i2s_resource1),
};

static struct resource i2s_resource2[] = {
	[0] = {
		.start	= TEGRA_I2S2_BASE,
		.end	= TEGRA_I2S2_BASE + TEGRA_I2S2_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device2 = {
	.name		= "tegra30-i2s",
	.id		= 2,
	.resource	= i2s_resource2,
	.num_resources	= ARRAY_SIZE(i2s_resource2),
};

static struct resource i2s_resource3[] = {
	[0] = {
		.start	= TEGRA_I2S3_BASE,
		.end	= TEGRA_I2S3_BASE + TEGRA_I2S3_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device3 = {
	.name		= "tegra30-i2s",
	.id		= 3,
	.resource	= i2s_resource3,
	.num_resources	= ARRAY_SIZE(i2s_resource3),
};

static struct resource i2s_resource4[] = {
	[0] = {
		.start	= TEGRA_I2S4_BASE,
		.end	= TEGRA_I2S4_BASE + TEGRA_I2S4_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device4 = {
	.name		= "tegra30-i2s",
	.id		= 4,
	.resource	= i2s_resource4,
	.num_resources	= ARRAY_SIZE(i2s_resource4),
};
#endif

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource spdif_resource[] = {
	[0] = {
		.start	= INT_SPDIF,
		.end	= INT_SPDIF,
		.flags	= IORESOURCE_IRQ
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_SPD_I,
		.end	= TEGRA_DMA_REQ_SEL_SPD_I,
		.flags	= IORESOURCE_DMA
	},
	[2] = {
		.start	= TEGRA_SPDIF_BASE,
		.end	= TEGRA_SPDIF_BASE + TEGRA_SPDIF_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_spdif_device = {
	.name		= "tegra20-spdif",
	.id		= -1,
	.resource	= spdif_resource,
	.num_resources	= ARRAY_SIZE(spdif_resource),
};
#elif !defined(CONFIG_ARCH_TEGRA_APE)
static struct resource spdif_resource[] = {
	[0] = {
		.start	= TEGRA_SPDIF_BASE,
		.end	= TEGRA_SPDIF_BASE + TEGRA_SPDIF_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_spdif_device = {
	.name		= "tegra30-spdif",
	.id		= -1,
	.resource	= spdif_resource,
	.num_resources	= ARRAY_SIZE(spdif_resource),
};
#endif

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_APE)
static struct resource ahub_resource[] = {
	[0] = {
		.start	= TEGRA_APBIF0_BASE,
		.end	= TEGRA_APBIF3_BASE + TEGRA_APBIF3_SIZE - 1,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= TEGRA_AHUB_BASE,
		.end	= TEGRA_AHUB_BASE + TEGRA_AHUB_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_ahub_device = {
	.name	= "tegra30-ahub",
	.id	= -1,
	.resource	= ahub_resource,
	.num_resources	= ARRAY_SIZE(ahub_resource),
};

static struct resource dam_resource0[] = {
	[0] = {
		.start = TEGRA_DAM0_BASE,
		.end   = TEGRA_DAM0_BASE + TEGRA_DAM0_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dam_device0 = {
	.name = "tegra30-dam",
	.id = 0,
	.resource      = dam_resource0,
	.num_resources = ARRAY_SIZE(dam_resource0),
};

static struct resource dam_resource1[] = {
	[0] = {
		.start = TEGRA_DAM1_BASE,
		.end   = TEGRA_DAM1_BASE + TEGRA_DAM1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dam_device1 = {
	.name = "tegra30-dam",
	.id = 1,
	.resource      = dam_resource1,
	.num_resources = ARRAY_SIZE(dam_resource1),
};

static struct resource dam_resource2[] = {
	[0] = {
		.start = TEGRA_DAM2_BASE,
		.end   = TEGRA_DAM2_BASE + TEGRA_DAM2_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dam_device2 = {
	.name = "tegra30-dam",
	.id = 2,
	.resource      = dam_resource2,
	.num_resources = ARRAY_SIZE(dam_resource2),
};
#endif

#if defined(CONFIG_ARCH_TEGRA_APE)
static struct resource tegra_axbar_resource[] = {
	[0] = {
		.start = TEGRA_AXBAR_BASE,
		.end   = TEGRA_AXBAR_BASE + TEGRA_AXBAR_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_axbar_device = {
	.name = "tegra210-axbar",
	.id = -1,
	.resource      = tegra_axbar_resource,
	.num_resources = ARRAY_SIZE(tegra_axbar_resource),
};

static struct resource tegra_ope_resource0[] = {
	[0] = {
		.start = TEGRA_OPE1_BASE,
		.end   = TEGRA_OPE1_BASE + TEGRA_OPE1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_ope_device0 = {
	.name = "tegra210-ope",
	.id = 0,
	.resource      = tegra_ope_resource0,
	.num_resources = ARRAY_SIZE(tegra_ope_resource0),
};

static struct resource tegra_ope_resource1[] = {
	[0] = {
		.start = TEGRA_OPE2_BASE,
		.end   = TEGRA_OPE2_BASE + TEGRA_OPE2_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_ope_device1 = {
	.name = "tegra210-ope",
	.id = 1,
	.resource      = tegra_ope_resource1,
	.num_resources = ARRAY_SIZE(tegra_ope_resource1),
};

static struct resource tegra_peq_resource0[] = {
	[0] = {
		.start = TEGRA_PEQ1_BASE,
		.end   = TEGRA_PEQ1_BASE + TEGRA_PEQ1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_peq_device0 = {
	.name = "tegra210-peq",
	.id = 0,
	.resource      = tegra_peq_resource0,
	.num_resources = ARRAY_SIZE(tegra_peq_resource0),
};

static struct resource tegra_peq_resource1[] = {
	[0] = {
		.start = TEGRA_PEQ2_BASE,
		.end   = TEGRA_PEQ2_BASE + TEGRA_PEQ2_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_peq_device1 = {
	.name = "tegra210-peq",
	.id = 1,
	.resource      = tegra_peq_resource1,
	.num_resources = ARRAY_SIZE(tegra_peq_resource1),
};

static struct resource tegra_mbdrc_resource0[] = {
	[0] = {
		.start = TEGRA_MBDRC1_BASE,
		.end   = TEGRA_MBDRC1_BASE + TEGRA_MBDRC1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_mbdrc_device0 = {
	.name = "tegra210-mbdrc",
	.id = 0,
	.resource      = tegra_mbdrc_resource0,
	.num_resources = ARRAY_SIZE(tegra_mbdrc_resource0),
};

static struct resource tegra_mbdrc_resource1[] = {
	[0] = {
		.start = TEGRA_MBDRC2_BASE,
		.end   = TEGRA_MBDRC2_BASE + TEGRA_MBDRC2_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_mbdrc_device1 = {
	.name = "tegra210-mbdrc",
	.id = 1,
	.resource      = tegra_mbdrc_resource1,
	.num_resources = ARRAY_SIZE(tegra_mbdrc_resource1),
};

static struct resource tegra_amixer_resource[] = {
	[0] = {
		.start = TEGRA_MIXER_BASE,
		.end = TEGRA_MIXER_BASE + TEGRA_MIXER_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_amixer_device = {
	.name = "tegra210-amixer",
	.id = 0,
	.resource = tegra_amixer_resource,
	.num_resources = ARRAY_SIZE(tegra_amixer_resource),
};

static struct resource tegra_mvc_resource0[] = {
	[0] = {
		.start = TEGRA_MVC1_BASE,
		.end = TEGRA_MVC1_BASE + TEGRA_MVC1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_mvc_device0 = {
	.name = "tegra210-mvc",
	.id = 0,
	.resource = tegra_mvc_resource0,
	.num_resources = ARRAY_SIZE(tegra_mvc_resource0),
};

static struct resource tegra_mvc_resource1[] = {
	[0] = {
		.start = TEGRA_MVC2_BASE,
		.end = TEGRA_MVC2_BASE + TEGRA_MVC2_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_mvc_device1 = {
	.name = "tegra210-mvc",
	.id = 1,
	.resource = tegra_mvc_resource1,
	.num_resources = ARRAY_SIZE(tegra_mvc_resource1),
};

static struct resource tegra_sfc_resource0[] = {
	[0] = {
		.start = TEGRA_SFC1_BASE,
		.end   = TEGRA_SFC1_BASE + TEGRA_SFC1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};
struct platform_device tegra_sfc_device0 = {
	.name = "tegra210-sfc",
	.id = 0,
	.resource      = tegra_sfc_resource0,
	.num_resources = ARRAY_SIZE(tegra_sfc_resource0),
};
static struct resource tegra_sfc_resource1[] = {
	[0] = {
		.start = TEGRA_SFC2_BASE,
		.end   = TEGRA_SFC2_BASE + TEGRA_SFC2_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};
struct platform_device tegra_sfc_device1 = {
	.name = "tegra210-sfc",
	.id = 1,
	.resource      = tegra_sfc_resource1,
	.num_resources = ARRAY_SIZE(tegra_sfc_resource1),
};
static struct resource tegra_sfc_resource2[] = {
	[0] = {
		.start = TEGRA_SFC3_BASE,
		.end   = TEGRA_SFC3_BASE + TEGRA_SFC3_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};
struct platform_device tegra_sfc_device2 = {
	.name = "tegra210-sfc",
	.id = 2,
	.resource      = tegra_sfc_resource2,
	.num_resources = ARRAY_SIZE(tegra_sfc_resource2),
};
static struct resource tegra_sfc_resource3[] = {
	[0] = {
		.start = TEGRA_SFC4_BASE,
		.end   = TEGRA_SFC4_BASE + TEGRA_SFC4_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};
struct platform_device tegra_sfc_device3 = {
	.name = "tegra210-sfc",
	.id = 3,
	.resource      = tegra_sfc_resource3,
	.num_resources = ARRAY_SIZE(tegra_sfc_resource3),
};

static struct resource tegra_i2s_resource0[] = {
	[0] = {
		.start = TEGRA_I2S1_BASE,
		.end   = TEGRA_I2S1_BASE + TEGRA_I2S1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device0 = {
	.name = "tegra210-i2s",
	.id = 0,
	.resource      = tegra_i2s_resource0,
	.num_resources = ARRAY_SIZE(tegra_i2s_resource0),
};

static struct resource tegra_i2s_resource1[] = {
	[0] = {
		.start = TEGRA_I2S2_BASE,
		.end   = TEGRA_I2S2_BASE + TEGRA_I2S2_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device1 = {
	.name = "tegra210-i2s",
	.id = 1,
	.resource      = tegra_i2s_resource1,
	.num_resources = ARRAY_SIZE(tegra_i2s_resource1),
};

static struct resource tegra_i2s_resource2[] = {
	[0] = {
		.start = TEGRA_I2S3_BASE,
		.end   = TEGRA_I2S3_BASE + TEGRA_I2S3_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device2 = {
	.name = "tegra210-i2s",
	.id = 2,
	.resource      = tegra_i2s_resource2,
	.num_resources = ARRAY_SIZE(tegra_i2s_resource2),
};

static struct resource tegra_i2s_resource3[] = {
	[0] = {
		.start = TEGRA_I2S4_BASE,
		.end   = TEGRA_I2S4_BASE + TEGRA_I2S4_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device3 = {
	.name = "tegra210-i2s",
	.id = 3,
	.resource      = tegra_i2s_resource3,
	.num_resources = ARRAY_SIZE(tegra_i2s_resource3),
};

static struct resource tegra_i2s_resource4[] = {
	[0] = {
		.start = TEGRA_I2S5_BASE,
		.end   = TEGRA_I2S5_BASE + TEGRA_I2S5_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device4 = {
	.name = "tegra210-i2s",
	.id = 4,
	.resource      = tegra_i2s_resource4,
	.num_resources = ARRAY_SIZE(tegra_i2s_resource4),
};

static struct resource tegra_dmic_resource0[] = {
	[0] = {
		.start = TEGRA_DMIC1_BASE,
		.end   = TEGRA_DMIC1_BASE + TEGRA_DMIC1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dmic_device0 = {
	.name = "tegra210-dmic",
	.id = 0,
	.resource      = tegra_dmic_resource0,
	.num_resources = ARRAY_SIZE(tegra_dmic_resource0),
};

static struct resource tegra_dmic_resource1[] = {
	[0] = {
		.start = TEGRA_DMIC2_BASE,
		.end   = TEGRA_DMIC2_BASE + TEGRA_DMIC2_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dmic_device1 = {
	.name = "tegra210-dmic",
	.id = 1,
	.resource      = tegra_dmic_resource1,
	.num_resources = ARRAY_SIZE(tegra_dmic_resource1),
};

static struct resource tegra_dmic_resource2[] = {
	[0] = {
		.start = TEGRA_DMIC3_BASE,
		.end   = TEGRA_DMIC3_BASE + TEGRA_DMIC3_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dmic_device2 = {
	.name = "tegra210-dmic",
	.id = 2,
	.resource      = tegra_dmic_resource2,
	.num_resources = ARRAY_SIZE(tegra_dmic_resource2),
};

static struct resource tegra_spkprot_resource0[] = {
	[0] = {
		.start = TEGRA_SPKPROT1_BASE,
		.end   = TEGRA_SPKPROT1_BASE + TEGRA_SPKPROT1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};
struct platform_device tegra_spkprot_device0 = {
	.name = "tegra210-spkprot",
	.id = 0,
	.resource      = tegra_spkprot_resource0,
	.num_resources = ARRAY_SIZE(tegra_spkprot_resource0),
};

static struct resource tegra_adma_resource[] = {
	[0] = {
		.start = TEGRA_ADMA_BASE,
		.end   = TEGRA_ADMA_BASE + TEGRA_ADMA_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

static u64 tegra_adma_dmamask = DMA_BIT_MASK(64);
struct platform_device tegra_adma_device = {
	.name     = "tegra210-adma",
	.id       = -1,
	.resource = tegra_adma_resource,
	.num_resources = ARRAY_SIZE(tegra_adma_resource),
	.dev = {
		.dma_mask = &tegra_adma_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64)
	}
};

static struct resource tegra_admaif_resource[] = {
	[0] = {
		.start = TEGRA_ADMAIF_BASE,
		.end   = TEGRA_ADMAIF_BASE + TEGRA_ADMAIF_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_admaif_device = {
	.name = "tegra210-admaif",
	.id   = -1,
	.resource      = tegra_admaif_resource,
	.num_resources = ARRAY_SIZE(tegra_admaif_resource),
};
#endif

struct platform_device spdif_dit_device = {
	.name = "spdif-dit",
	.id = 0,
};

struct platform_device bluetooth_dit_device = {
	.name = "spdif-dit",
	.id = 1,
};

struct platform_device baseband_dit_device = {
	.name = "spdif-dit",
	.id = 2,
};

struct platform_device tegra_pcm_device = {
	.name = "tegra-pcm-audio",
	.id = -1,
};

struct platform_device tegra_offload_device = {
	.name = "tegra-offload",
	.id = -1,
};

struct platform_device tegra30_avp_audio_device = {
	.name = "tegra30-avp-audio",
	.id = -1,
};

static struct resource w1_resources[] = {
	[0] = {
		.start = INT_OWR,
		.end   = INT_OWR,
		.flags = IORESOURCE_IRQ
	},
	[1] = {
		.start = TEGRA_OWR_BASE,
		.end = TEGRA_OWR_BASE + TEGRA_OWR_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_w1_device = {
	.name          = "tegra_w1",
	.id            = -1,
	.resource      = w1_resources,
	.num_resources = ARRAY_SIZE(w1_resources),
};

static struct resource tegra_udc_resources[] = {
	[0] = {
		.start	= TEGRA_USB_BASE,
		.end	= TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB,
		.end	= INT_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 tegra_udc_dmamask = DMA_BIT_MASK(64);

struct platform_device tegra_udc_device = {
	.name	= "tegra-udc",
	.id	= 0,
	.dev	= {
		.dma_mask	= &tegra_udc_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_udc_resources,
	.num_resources = ARRAY_SIZE(tegra_udc_resources),
};

static struct resource tegra_otg_resources[] = {
	[0] = {
		.start	= TEGRA_USB_BASE,
		.end	= TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB,
		.end	= INT_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device tegra_otg_device = {
	.name		= "tegra-otg",
	.id		= -1,
	.resource	= tegra_otg_resources,
	.num_resources	= ARRAY_SIZE(tegra_otg_resources),
};

#ifdef CONFIG_SATA_AHCI_TEGRA
static u64 tegra_sata_dma_mask = DMA_BIT_MASK(64);

static struct resource tegra_sata_resources[] = {
	[0] = {
		.start = TEGRA_SATA_BAR5_BASE,
		.end = TEGRA_SATA_BAR5_BASE + TEGRA_SATA_BAR5_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = TEGRA_SATA_CONFIG_BASE,
		.end = TEGRA_SATA_CONFIG_BASE + TEGRA_SATA_CONFIG_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = INT_SATA_CTL,
		.end = INT_SATA_CTL,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_sata_device = {
	.name 	= "tegra-sata",
	.id 	= 0,
	.dev 	= {
		.coherent_dma_mask = DMA_BIT_MASK(64),
		.dma_mask = &tegra_sata_dma_mask,
	},
	.resource = tegra_sata_resources,
	.num_resources = ARRAY_SIZE(tegra_sata_resources),
};
#endif

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource das_resource[] = {
	[0] = {
		.start	= TEGRA_APB_MISC_DAS_BASE,
		.end	= TEGRA_APB_MISC_DAS_BASE + TEGRA_APB_MISC_DAS_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_das_device = {
	.name		= "tegra20-das",
	.id		= -1,
	.resource	= das_resource,
	.num_resources	= ARRAY_SIZE(das_resource),
};
#endif

#if defined(CONFIG_TEGRA_IOVMM_GART) || defined(CONFIG_TEGRA_IOMMU_GART)
static struct resource tegra_gart_resources[] = {
	[0] = {
		.name	= "mc",
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_MC_BASE,
		.end	= TEGRA_MC_BASE + TEGRA_MC_SIZE - 1,
	},
	[1] = {
		.name	= "gart",
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_GART_BASE,
		.end	= TEGRA_GART_BASE + TEGRA_GART_SIZE - 1,
	}
};

struct platform_device tegra_gart_device = {
	.name		= "tegra_gart",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_gart_resources),
	.resource	= tegra_gart_resources
};
#endif

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
#define CLK_RESET_RST_SOURCE	0x0
static struct resource tegra_wdt_resources[] = {
	[0] = {
		.start	= TEGRA_CLK_RESET_BASE + CLK_RESET_RST_SOURCE,
		.end	= TEGRA_CLK_RESET_BASE + CLK_RESET_RST_SOURCE + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= TEGRA_TMR1_BASE,
		.end	= TEGRA_TMR1_BASE + TEGRA_TMR1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= INT_TMR1,
		.end	= INT_TMR1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device tegra_wdt_device = {
	.name		= "tegra_wdt",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_wdt_resources),
	.resource	= tegra_wdt_resources,
};
#else
static struct resource tegra_wdt0_resources[] = {
	[0] = {
		.start	= TEGRA_WDT0_BASE,
		.end	= TEGRA_WDT0_BASE + TEGRA_WDT0_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= TEGRA_TMR7_BASE,
		.end	= TEGRA_TMR7_BASE + TEGRA_TMR7_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= INT_WDT_CPU,
		.end	= INT_WDT_CPU,
		.flags	= IORESOURCE_IRQ,
	},
#ifdef CONFIG_TEGRA_FIQ_DEBUGGER
	[3] = {
		.start	= TEGRA_QUATERNARY_ICTLR_BASE,
		.end	= TEGRA_QUATERNARY_ICTLR_BASE + \
				TEGRA_QUATERNARY_ICTLR_SIZE -1,
		.flags	= IORESOURCE_MEM,
	},
#endif
};

struct platform_device tegra_wdt0_device = {
	.name		= "tegra_wdt",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(tegra_wdt0_resources),
	.resource	= tegra_wdt0_resources,
};

#endif

static struct resource tegra_rtc_resources[] = {
	[0] = {
		.start = TEGRA_RTC_BASE,
		.end = TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_RTC,
		.end = INT_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_rtc_device = {
	.name = "tegra_rtc",
	.id   = -1,
	.resource = tegra_rtc_resources,
	.num_resources = ARRAY_SIZE(tegra_rtc_resources),
};


static struct resource tegra_nvavp_resources[] = {
	[0] = {
		.start  = INT_SHR_SEM_INBOX_IBF,
		.end    = INT_SHR_SEM_INBOX_IBF,
		.flags  = IORESOURCE_IRQ,
		.name   = "mbox_from_nvavp_pending",
	},
};

struct platform_device nvavp_device = {
	.name           = "nvavp",
	.id             = -1,
	.resource       = tegra_nvavp_resources,
	.num_resources  = ARRAY_SIZE(tegra_nvavp_resources),
	.dev  = {
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
static struct resource tegra_kbc_resources[] = {
	[0] = {
		.start = TEGRA_KBC_BASE,
		.end   = TEGRA_KBC_BASE + TEGRA_KBC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_KBC,
		.end   = INT_KBC,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_kbc_device = {
	.name = "tegra-kbc",
	.id = -1,
	.resource = tegra_kbc_resources,
	.num_resources = ARRAY_SIZE(tegra_kbc_resources),
	.dev = {
		.platform_data = 0,
	},
};
#endif

#if defined(CONFIG_TEGRA_SKIN_THROTTLE)
struct platform_device tegra_skin_therm_est_device = {
	.name	= "therm_est",
	.id	= -1,
	.num_resources	= 0,
	.dev = {
		.platform_data = 0,
	},
};
#endif

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
static struct resource tegra_tsensor_resources[]= {
	{
		.start	= TEGRA_TSENSOR_BASE,
		.end	= TEGRA_TSENSOR_BASE + TEGRA_TSENSOR_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_TSENSOR,
		.end	= INT_TSENSOR,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start 	= TEGRA_PMC_BASE + 0x1B0,
		/* 2 pmc registers mapped */
		.end	= TEGRA_PMC_BASE + 0x1B0 + (2 * 4),
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device tegra_tsensor_device = {
	.name	= "tegra-tsensor",
	.id	= -1,
	.num_resources	= ARRAY_SIZE(tegra_tsensor_resources),
	.resource	= tegra_tsensor_resources,
	.dev = {
		.platform_data = 0,
	},
};
#endif

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
static u64 tegra_se_dma_mask = DMA_BIT_MASK(32);
static u64 tegra12_se_dma_mask = DMA_BIT_MASK(64);

static struct resource tegra_se_resources[] = {
	[0] = {
		.start = TEGRA_SE_BASE,
		.end = TEGRA_SE_BASE + TEGRA_SE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start	= TEGRA_PMC_BASE,
		.end	= TEGRA_PMC_BASE + SZ_256 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start = INT_SE,
		.end = INT_SE,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_se_device = {
	.name = "tegra-se",
	.id = -1,
	.dev = {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.dma_mask = &tegra_se_dma_mask,
	},
	.resource = tegra_se_resources,
	.num_resources = ARRAY_SIZE(tegra_se_resources),
};

struct platform_device tegra11_se_device = {
	.name = "tegra11-se",
	.id = -1,
	.dev = {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.dma_mask = &tegra_se_dma_mask,
	},
	.resource = tegra_se_resources,
	.num_resources = ARRAY_SIZE(tegra_se_resources),
};

struct platform_device tegra12_se_device = {
	.name = "tegra12-se",
	.id = -1,
	.dev = {
		.coherent_dma_mask = DMA_BIT_MASK(64),
		.dma_mask = &tegra12_se_dma_mask,
	},
	.resource = tegra_se_resources,
	.num_resources = ARRAY_SIZE(tegra_se_resources),
};
#endif

static struct resource tegra_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0,
		.end	= 0,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device tegra_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= tegra_disp1_resources,
	.num_resources	= ARRAY_SIZE(tegra_disp1_resources),
};

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
static struct resource tegra_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
		.start	= 0,
		.end	= 0,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device tegra_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= tegra_disp2_resources,
	.num_resources	= ARRAY_SIZE(tegra_disp2_resources),
	.dev = {
		.platform_data = 0,
	},
};
#endif

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct resource cl_dvfs_resource[] = {
#ifndef CONFIG_ARCH_TEGRA_13x_SOC
	[0] = {
		.start	= TEGRA_CL_DVFS_BASE,
		.end	= TEGRA_CL_DVFS_BASE + TEGRA_CL_DVFS_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
#else
	[0] = {
		.start	= TEGRA_CLK13_RESET_BASE + 0x84,
		.end	= TEGRA_CLK13_RESET_BASE + 0x84 + TEGRA_CL_DVFS_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= TEGRA_CL_DVFS_BASE,
		.end	= TEGRA_CL_DVFS_BASE + TEGRA_CL_DVFS_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
#endif
};

struct platform_device tegra_cl_dvfs_device = {
	.name		= "tegra_cl_dvfs",
	.id		= -1,
	.resource	= cl_dvfs_resource,
	.num_resources	= ARRAY_SIZE(cl_dvfs_resource),
};
#endif

struct platform_device tegra_fuse_device = {
	.name	= "tegra-fuse",
	.id	= -1,
};
