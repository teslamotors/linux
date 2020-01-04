/*
 * arch/arm/mach-tegra/include/mach/tegra2_i2s_tdm.h
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

#ifndef __ARCH_ARM_MACH_TEGRA_I2S_TDM_H
#define __ARCH_ARM_MACH_TEGRA_I2S_TDM_H

#include <linux/kernel.h>
#include <linux/types.h>

/*
 * I2S_I2S_TDM_CTRL_0
 */
#define I2S_I2S_TDM_CTRL_TDM_EN                 (1<<31)
#define I2S_I2S_TDM_CTRL_TX_MSB_LSB_MASK        (1<<25)
#define I2S_I2S_TDM_CTRL_TX_MSB_LSB_SHIFT       (25)
#define I2S_I2S_TDM_CTRL_RX_MSB_LSB_MASK        (1<<24)
#define I2S_I2S_TDM_CTRL_RX_MSB_LSB_SHIFT       (24)
#define I2S_I2S_TDM_CTRL_TDM_EDGE_CTRL_MASK     (1<<22)
#define I2S_I2S_TDM_CTRL_TDM_EDGE_CTRL_SHIFT    (22)
#define I2S_I2S_TDM_CTRL_TOTAL_SLOTS_MASK       (0x7<<18)
#define I2S_I2S_TDM_CTRL_TOTAL_SLOTS_SHIFT      (18)
#define I2S_I2S_TDM_CTRL_TDM_BIT_SIZE_MASK      (0x1f<<12)
#define I2S_I2S_TDM_CTRL_TDM_BIT_SIZE_SHIFT     (12)
#define I2S_I2S_TDM_CTRL_RX_DATA_OFFSET_MASK    (0x3<<8)
#define I2S_I2S_TDM_CTRL_RX_DATA_OFFSET_SHIFT   (8)
#define I2S_I2S_TDM_CTRL_TX_DATA_OFFSET_MASK    (0x3<<6)
#define I2S_I2S_TDM_CTRL_TX_DATA_OFFSET_SHIFT   (6)
#define I2S_I2S_TDM_CTRL_FSYNC_WIDTH_MASK       (0x3f)
#define I2S_I2S_TDM_CTRL_FSYNC_WIDTH_SHIFT      (0)

/*
 * I2S_I2S_TDM_TX_RX_CTRL_0
 */
#define I2S_I2S_TDM_TX_RX_CTRL_TDM_TX_EN                  (1<<31)
#define I2S_I2S_TDM_TX_RX_CTRL_TDM_RX_EN                  (1<<29)
#define I2S_I2S_TDM_TX_RX_CTRL_TDM_RX_SLOT_ENABLES_MASK   (0xFF<<8)
#define I2S_I2S_TDM_TX_RX_CTRL_TDM_RX_SLOT_ENABLES_SHIFT  (8)
#define I2S_I2S_TDM_TX_RX_CTRL_TDM_TX_SLOT_ENABLES_MASK   (0xFF<<0)
#define I2S_I2S_TDM_TX_RX_CTRL_TDM_TX_SLOT_ENABLES_SHIFT  (0)
#define I2S_I2S_TDM_TX_FIFO_BUSY 			  (1<<30)
#define I2S_I2S_TDM_RX_FIFO_BUSY			  (1<<28)
/*
 * API
 */

int i2s_tdm_dump_registers(int ifc);
int i2s_tdm_enable(int ifc);
int i2s_tdm_set_tx_msb_first(int ifc, int msb_first);	/*msb_first(0), lsb_first(1) */
int i2s_tdm_set_rx_msb_first(int ifc, int msb_first);	/*msb_first(0), lsb_first(1) */
int i2s_tdm_set_tdm_edge_ctrl_highz(int ifc, int highz);
int i2s_tdm_set_total_slots(int ifc, int num_slots);
int i2s_tdm_set_bit_size(int ifc, int bit_size);
int i2s_tdm_set_rx_data_offset(int ifc, int data_offset);
int i2s_tdm_set_tx_data_offset(int ifc, int data_offset);
int i2s_tdm_set_fsync_width(int ifc, int fsync_width);
int i2s_tdm_set_tdm_tx_en(int ifc);
int i2s_tdm_set_tdm_rx_en(int ifc);
int i2s_tdm_set_tdm_tx_disable(int ifc);
int i2s_tdm_set_tdm_rx_disable(int ifc);
int i2s_tdm_set_rx_slot_enables(int ifc, int slot_mask);
int i2s_tdm_set_tx_slot_enables(int ifc, int slot_mask);
int i2s_tdm_fifo_ready (int ifc , int fifo);
int i2s_tdm_get_status(int ifc);

#endif /* __ARCH_ARM_MACH_TEGRA_I2S_TDM_H */
