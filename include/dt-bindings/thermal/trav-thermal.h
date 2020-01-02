/*
 * Copyright (C) 2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TRAV_THERMAL_SH__
#define __TRAV_THERMAL_SH__

/* Locations from SAM-1042 */

#define TMU_CPUCL0_LOCAL	0		/* T3 */
#define TMU_CPUCL0_TEM4_GPU	1		/* R4 */
#define TMU_CPUCL0_TEM_TRIP1_N	2		/* R6 */
#define TMU_CPUCL0_CLUSTER0_NE	3		/* R8 */
#define TMU_CPUCL0_CLUSTER0_SW	4		/* R9 */
#define TMU_CPUCL0_CLUSTER0_SE	5		/* R10 */
#define TMU_CPUCL0_CLUSTER0_NW	6		/* R7 */
#define TMU_CPUCL0_TEM_TRIP1_S	7		/* R16 */

#define TMU_CPUCL2_LOCAL	8		/* T4 */
#define TMU_CPUCL2_CLUSTER2_NE	9		/* R12 */
#define TMU_CPUCL2_CLUSTER2_SW	10		/* R13 */
#define TMU_CPUCL2_CLUSTER2_SE	11		/* R14 */
#define TMU_CPUCL2_CLUSTER2_NW	12		/* R11 */

#define TMU_GT_LOCAL		13		/* T2 */
#define TMU_GT_TEM2_GPU		14		/* R2 */
#define TMU_GT_TEM3_GPU		15		/* R3 */
#define TMU_GT_TEM_TRIP0_N	16		/* R5 */
#define TMU_GT_TEM_TRIP0_S	17		/* R15 */

#define TMU_TOP			18		/* T5 */

#define TMU_GPU_LOCAL		19		/* T1 */
#define TMU_GPU_TEM0_GPU	20		/* R0 */
#define TMU_GPU_TEM1_GPU	21		/* R1 */

/* Pseudo sensors for aggregate thermal zones */

/*
 * CPU sensors:
 *   TMU_CPUCL0_LOCAL
 *   TMU_CPUCL2_LOCAL
 *   All TMU_CPUCL*_CLUSTER* sensors
 */
#define TMU_AGG_CPU_THERMAL	22

/* Trip0: TMU_GT_TEM_TRIP0_N and TMU_GT_TEM_TRIP0_S sensors */
#define TMU_AGG_TRIP0_THERMAL	23

/* Trip1: TMU_CPUCL0_TEM_TRIP1_N and TMU_CPUCL0_TEM_TRIP1_S sensors */
#define TMU_AGG_TRIP1_THERMAL	24

/*
 * GPU sensors:
 *   TMU_GPU_LOCAL
 *   TMU_CPUCL0_TEM4_GPU
 *   TMU_GT_TEM2_GPU
 *   TMU_GT_TEM3_GPU
 *   TMU_GPU_TEM0_GPU
 *   TMU_GPU_TEM1_GPU
 */
#define TMU_AGG_GPU_THERMAL	25

/*
 * And the remaining sensors:
 *   TMU_TOP
 *   TMU_GT_LOCAL
 */
#define TMU_AGG_OTHER_THERMAL	26

#endif /* __TRAV_THERMAL_SH__ */
