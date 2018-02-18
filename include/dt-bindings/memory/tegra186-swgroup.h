/*
 * This is the t18x specific component of the new SID dt-binding.
 */
#define TEGRA_SID_NVCSI		0x2	/* 2 */
#define TEGRA_SID_SE2		0xe	/* 14 */
#define TEGRA_SID_SE3		0xf	/* 15 */
#define TEGRA_SID_SCE		0x1f	/* 31 */

/* The GPC DMA clients. */
#define TEGRA_SID_GPCDMA_0	0x20	/* 32 */
#define TEGRA_SID_GPCDMA_1	0x21	/* 33 */
#define TEGRA_SID_GPCDMA_2	0x22	/* 34 */
#define TEGRA_SID_GPCDMA_3	0x23	/* 35 */
#define TEGRA_SID_GPCDMA_4	0x24	/* 36 */
#define TEGRA_SID_GPCDMA_5	0x25	/* 37 */
#define TEGRA_SID_GPCDMA_6	0x26	/* 38 */
#define TEGRA_SID_GPCDMA_7	0x27	/* 39 */

/* The APE DMA Clients. */
#define TEGRA_SID_APE_1		0x28	/* 40 */
#define TEGRA_SID_APE_2		0x29	/* 41 */

/* The Camera RTCPU. */
#define TEGRA_SID_RCE		0x2a	/* 42 */

/* The Camera RTCPU on Host1x address space. */
#define TEGRA_SID_RCE_1X	0x2b	/* 43 */

/* The Camera RTCPU running on APE */
#define TEGRA_SID_APE_CAM	0x2d	/* 45 */
#define TEGRA_SID_APE_CAM_1X	0x2e	/* 46 */

/* Host1x virtualization clients. */
#define TEGRA_SID_HOST1X_CTX0	0x38	/* 56 */
#define TEGRA_SID_HOST1X_CTX1	0x39	/* 57 */
#define TEGRA_SID_HOST1X_CTX2	0x3a	/* 58 */
#define TEGRA_SID_HOST1X_CTX3	0x3b	/* 59 */
#define TEGRA_SID_HOST1X_CTX4	0x3c	/* 60 */
#define TEGRA_SID_HOST1X_CTX5	0x3d	/* 61 */
#define TEGRA_SID_HOST1X_CTX6	0x3e	/* 62 */
#define TEGRA_SID_HOST1X_CTX7	0x3f	/* 63 */

/* Host1x command buffers */
#define TEGRA_SID_HC_VM0	0x40
#define TEGRA_SID_HC_VM1	0x41
#define TEGRA_SID_HC_VM2	0x42
#define TEGRA_SID_HC_VM3	0x43
#define TEGRA_SID_HC_VM4	0x44
#define TEGRA_SID_HC_VM5	0x45
#define TEGRA_SID_HC_VM6	0x46
#define TEGRA_SID_HC_VM7	0x47

/* SE data buffers */
#define TEGRA_SID_SE_VM0	0x48
#define TEGRA_SID_SE_VM1	0x49
#define TEGRA_SID_SE_VM2	0x4a
#define TEGRA_SID_SE_VM3	0x4b
#define TEGRA_SID_SE_VM4	0x4c
#define TEGRA_SID_SE_VM5	0x4d
#define TEGRA_SID_SE_VM6	0x4e
#define TEGRA_SID_SE_VM7	0x4f
