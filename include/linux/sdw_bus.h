/*
 *  sdw_bus.h - Definition for SoundWire bus interface.
 *
 * This header file refers to the MIPI SoundWire 1.0. The comments try to
 * follow the same conventions with a capital letter for all standard
 * definitions such as Master, Slave, Data Port, etc. When possible, the
 * constant numeric values are kept the same as in the MIPI specifications
 *
 *  Copyright (C) 2016 Intel Corp
 *  Author:  Hardik Shah  <hardik.t.shah@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#ifndef _LINUX_SDW_BUS_H
#define _LINUX_SDW_BUS_H

#include <linux/device.h>	/* for struct device */
#include <linux/mod_devicetable.h> /* For Name size */
#include <linux/rtmutex.h> /* For rt mutex */


#define SOUNDWIRE_MAX_DEVICES 11

#define SDW_NUM_DEV_ID_REGISTERS 6

/* Port flow mode, used to indicate what port flow mode
 * slave supports
 */
#define SDW_PORT_FLOW_MODE_ISOCHRONOUS		0x1
#define SDW_PORT_FLOW_MODE_TX_CONTROLLED	0x2
#define SDW_PORT_FLOW_MODE_RX_CONTROLLED	0x4
#define SDW_PORT_FLOW_MODE_ASYNCHRONOUS		0x8

/* Bit-mask used to indicate Port capability, OR both bits if
 * Port is bidirectional capable
 */
#define SDW_PORT_SOURCE				0x1
#define SDW_PORT_SINK				0x2

/* Mask to specify what type of sample packaging mode
 * is supported by port
 */
#define	SDW_PORT_BLK_PKG_MODE_BLK_PER_PORT_MASK	0x1
#define	SDW_PORT_BLK_PKG_MODE_BLK_PER_CH_MASK 0x2

/* Mask to specify data encoding supported by port */
#define SDW_PORT_ENCODING_TYPE_TWOS_CMPLMNT	0x1
#define SDW_PORT_ENCODING_TYPE_SIGN_MAGNITUDE	0x2
#define SDW_PORT_ENCODING_TYPE_IEEE_32_FLOAT	0x4

/* enum sdw_driver_type: There are different driver callbacks for slave and
 *			master. This is to differentiate between slave driver
 *			and master driver. Bus driver binds master driver to
 *			master device and slave driver to slave device using
 *			this field. Driver populates this field based on whether
 *			its handling slave or master device.
 */
enum sdw_driver_type {
	SDW_DRIVER_TYPE_MASTER = 0,
	SDW_DRIVER_TYPE_SLAVE = 1,
};

/**
 * enum sdw_block_pkg_mode: Block packing mode for the port.
 * @SDW_PORT_BLK_PKG_MODE_BLK_PER_PORT: Block packing per port
 * @SDW_PORT_BLK_PKG_MODE_BLK_PER_CH: Block packing per channel.
 */
enum sdw_block_pkg_mode {
	SDW_PORT_BLK_PKG_MODE_BLK_PER_PORT = 0,
	SDW_PORT_BLK_PKG_MODE_BLK_PER_CH = 1,
};


/**
 * enum sdw_command_response: Data Port type
 * @SDW_COMMAND_OK: Command is Ok.
 * @SDW_COMMAND_IGNORED: Command is ignored.
 * @SDW_COMMAND_FAILED: Command failed.
 */
enum sdw_command_response {
	SDW_COMMAND_OK = 0,
	SDW_COMMAND_IGNORED = 1,
	SDW_COMMAND_FAILED = 2,
};
/**
 * enum sdw_dpn_type: Data Port type
 * @SDW_FULL_DP: Full Data Port supported.
 * @SDW_SIMPLIFIED_DP: Simplified Data Port. Following registers are not
 *			implemented by simplified data port
 *			DPN_SampleCtrl2, DPN_OffsetCtrl2, DPN_HCtrl and
 *			DPN_BlockCtrl3
 */
enum sdw_dpn_type {
	SDW_FULL_DP = 0,
	SDW_SIMPLIFIED_DP = 1,
};

/**
 * enum sdw_dpn_grouping: Maximum block group count supported.
 * @SDW_BLOCKGROUPCOUNT_1: Maximum Group count 1 supported.
 * @SDW_BLOCKGROUPCOUNT_2: Maximum Group count 2 supported.
 * @SDW_BLOCKGROUPCOUNT_3: Maximum Group count 3 supported.
 * @SDW_BLOCKGROUPCOUNT_4: Maximum Group count 4 supported.
 */
enum sdw_dpn_grouping {
	SDW_BLOCKGROUPCOUNT_1 = 0,
	SDW_BLOCKGROUPCOUNT_2 = 1,
	SDW_BLOCKGROUPCOUNT_3 = 2,
	SDW_BLOCKGROUPCOUNT_4 = 3,
};

/**
 * enum sdw_prep_ch_behavior: Specifies the dependencies between
 *				Channel Prepare sequence and bus
 *				clock configuration.This property is not
 *				required for ports implementing a
 *				Simplified ChannelPrepare State Machine (SCPSM)
 * @SDW_CH_PREP_ANY_TIME: Channel Prepare can happen at any bus clock rate
 * @SDW_CH_PREP_AFTER_BUS_CLK_CHANGE: : Channel Prepare sequence needs to
 *				happen after bus clock is changed to a
 *				frequency supported by this mode or
 *				compatible modes described by the next field.
 *				This may be required, e.g. when the Slave
 *				internal audio clocks are derived from the
 *				bus clock.
 */
enum sdw_prep_ch_behavior {
	SDW_CH_PREP_ANY_TIME = 0,
	SDW_CH_PREP_AFTER_BUS_CLK_CHANGE = 1,
};

/**
 * enum sdw_slave_status: Slave status reported in PING frames
 * @SDW_SLAVE_STAT_NOT_PRESENT: Slave is not present.
 * @SDW_SLAVE_STAT_ATTACHED_OK: Slave is Attached to the bus.
 * @SDW_SLAVE_STAT_ALERT: Some alert condition on the Slave.
 * @SDW_SLAVE_STAT_RESERVED: Reserved.
 */
enum sdw_slave_status {
	SDW_SLAVE_STAT_NOT_PRESENT = 0,
	SDW_SLAVE_STAT_ATTACHED_OK = 1,
	SDW_SLAVE_STAT_ALERT = 2,
	SDW_SLAVE_STAT_RESERVED = 3,
};

enum sdw_stream_type {
	SDW_STREAM_PCM = 0,
	SDW_STREAM_PDM = 1,
};


enum sdw_rt_state {
	SDW_RT_INITIALIZED = 0,
	SDW_RT_CONFIGURED = 1,
};

/**
 * enum sdw_ch_prepare_mode: Channel prepare mode.
 * @SDW_SIMPLIFIED_CP_SM: Simplified channel prepare.
 * @SDW_CP_SM: Normal channel prepare.
 */
enum sdw_ch_prepare_mode {
	SDW_SIMPLIFIED_CP_SM = 0,
	SDW_CP_SM = 1,
};

/**
 * enums dfw_clk_stop_prepare: Clock Stop prepare mode.
 * @SDW_CLOCK_STOP_MODE_0: Clock Stop mode 0
 * @SDW_CLOCK_STOP_MODE_1: Clock Stop mode 1
 */
enum sdw_clk_stop_mode {
	SDW_CLOCK_STOP_MODE_0 = 0,
	SDW_CLOCK_STOP_MODE_1 = 1,
};

/**
 *  enum sdw_data_direction: Data direction w.r.t Port. For e.g for playback
 *				between the Master and Slave, where Slave
 *				is codec, data direction for the Master
 *				port will be OUT, since its transmitting
 *				the data, while for the Slave (codec) it
 *				will be IN, since its receiving the data.
 *  @SDW_DATA_DIR_IN: Data is input to Port.
 *  @SDW_DATA_DIR_OUT: Data is output from Port.
 */
enum sdw_data_direction {
	SDW_DATA_DIR_IN = 0,
	SDW_DATA_DIR_OUT = 1,
};

/* Forward declaration of the data structures */
struct sdw_master;
struct sdw_slave;
struct sdw_msg;
struct sdw_bra_block;
struct sdw_mstr_driver;

/**
 * struct port_audio_mode_properties: Audio properties for the Port
 *
 * @max_frequency: Maximum frequency Port can support for the clock.
 *		The use of max_ and min_ frequency requires num_freq_config
 *		to be zero
 * @min_frequency: Minimum frequency Port can support for the clock.
 * @num_freq_configs: Array size for the frequencies supported by Port.
 * @freq_supported: Array of frequencies supported by the Port.
 * @glitchless_transitions_mask: Glitch transition mask from one mode to
 *				other mode. Each bit refers to a mode
 *				number.
 */

struct port_audio_mode_properties {
	unsigned int max_frequency;
	unsigned int min_frequency;
	unsigned int num_freq_configs;
	unsigned int *freq_supported;
	unsigned int max_sampling_frequency;
	unsigned int min_sampling_frequency;
	unsigned int num_sampling_freq_configs;
	unsigned int *sampling_freq_config;
	enum sdw_prep_ch_behavior ch_prepare_behavior;
	unsigned int glitchless_transitions_mask;
};

/**
 * struct sdw_slv_addr: Structure representing the device_id and
 *			and SoundWire logical Slave address.
 * @dev_id: 6-byte device id of the Slave
 * @slv_number: Logical SoundWire Slave number, in the range [1..11]
 * @assigned: Logical address is assigned to some Slave or not
 * @status: What is the current state of the slave.
 *
 */
struct sdw_slv_addr {
	struct sdw_slave *slave;
	u8 dev_id[SDW_NUM_DEV_ID_REGISTERS];
	u8 slv_number;
	bool assigned;
	enum sdw_slave_status status;
};

/**
 * struct sdw_slv_dpn_capabilities: Capabilities of the Data Port, other than
 *				Data Port 0 for SoundWire Slave
 * @port_direction: Direction of the Port. Sink or Source or bidirectional.
 *			Set appropriate bit mak based on port capabilities.
 * @port_number: Port number.
 * @max_word_length: Maximum length of the sample word.
 * @min_word_length: Minimum length of sample word.
 * @num_word_length: Length of supported word length buffer.
 *		The use of max_ and min_ word length requires
 *		num_word_length to be zero
 * @word_length_buffer: Array of the supported word length.
 * @dpn_type: Type of Data Port. Simplified or Normal data port.
 * @dpn_grouping: Max Block group count supported for this Port.
 * @prepare_ch: Channel prepare scheme. Simplified channel prepare or Normal
 *		channel prepare.
 * @imp_def_intr_mask: Implementation defined interrupt mask.
 * @min_ch_num: Minimum number of channels supported.
 * @max_ch_num: Maximum number of channels supported.
 * @num_ch_supported: Buffer length for the channels supported.
 *			The use of max_ and min_ ch_num requires
 *			num_ch_supported to be zero
 * @ch_supported: Array of the channel supported.
 * @port_flow_mode_mask: Transport flow modes supported by Port.
 * @block_packing_mode_mask: Block packing mode mask.
 * @port_encoding_type_mask: Port Data encoding type mask.
 * @num_audio_modes: Number of audio modes supported by device.
 * @mode_properties: Port audio mode properties buffer of size num_audio_modes
 */

struct sdw_slv_dpn_capabilities {
	unsigned int port_direction;
	unsigned int port_number;
	unsigned int max_word_length;
	unsigned int min_word_length;
	unsigned int num_word_length;
	unsigned int *word_length_buffer;
	enum sdw_dpn_type dpn_type;
	enum sdw_dpn_grouping dpn_grouping;
	enum sdw_ch_prepare_mode prepare_ch;
	unsigned int imp_def_intr_mask;
	unsigned int min_ch_num;
	unsigned int max_ch_num;
	unsigned int num_ch_supported;
	unsigned int *ch_supported;
	unsigned int port_flow_mode_mask;
	unsigned int block_packing_mode_mask;
	unsigned int port_encoding_type_mask;
	unsigned int num_audio_modes;
	struct port_audio_mode_properties *mode_properties;
};

/**
 *  struct sdw_slv_bra_capabilities: BRA Capabilities of the Slave.
 *  @max_bus_frequency: Maximum bus frequency of this mode, in Hz
 *  @min_bus_frequency: Minimum bus frequency of this mode, in Hz
 *		When using min-max properties, all values in the defined
 *		range are allowed. Use the config list in the next field
 *		if only discrete values are supported.
 *  @num_bus_config_frequency:  Number of discrete bus frequency configurations
 *  @bus_config_frequencies: Array of bus frequency configs.
 *  @max_data_per_frame: Maximum Data payload, in bytes per frame.
 *		Excludes header, CRC, footer. Maximum value is 470
 *  @min_us_between_transactions: Amount of delay, in microseconds,
 *		required to be inserted between BRA transactions.
 *		Use if Slave needs idle time between BRA transactions.
 *  @max_bandwidth: Maximum bandwidth (in bytes/s) that can be written/read
 *		(header, CRCs, footer excluded)
 *  @mode_block_alignment: Size of basic block in bytes. The Data payload
 *		size needs to be a multiple of this basic block and
 *		padding/repeating of the same value is required for
 *		transactions smaller than this basic block.
 */

struct sdw_slv_bra_capabilities {
	unsigned int max_bus_frequency;
	unsigned int min_bus_frequency;
	unsigned int num_bus_config_frequency;
	unsigned int *bus_config_frequencies;
	unsigned int max_data_per_frame;
	unsigned int min_us_between_transactions;
	unsigned int max_bandwidth;
	unsigned int mode_block_alignment;
};

/**
 *  struct sdw_async_xfer_data: Data to be provided by bus driver to
 *				to master controller, in case bus driver
 *				driver doesnt want to call synchronous
 *				xfer message API. This is used by bus
 *				driver during aggregation, where it calls
 *				the bank switch of multiple master
 *				if the stream is split between two master.
 *				In this case bus driver will wait outside
 *				master controller context for bank switch
 *				to happen.
 *  @result:			Result of the asynchronous transfer.
 *  @xfer_complete		Bus driver will wait on this. Master controller
 *				needs to ack on this for transfer complete.
 *  @msg			Message to be transferred.
 */

struct sdw_async_xfer_data {
	int result;
	struct completion xfer_complete;
	struct sdw_msg *msg;
};

/**
 * struct sdw_slv_dp0_capabilities: Capabilities of the Data Port 0 of Slave.
 *
 * @max_word_length: Maximum word length supported by the Data Port.
 * @min_word_length: Minimum word length supported by the Data Port.
 * @num_word_length: Array size of the buffer containing the supported
 *			word lengths.
 *			The use of max_ and min_ word length requires
 *			num_word_length to be zero
 * @word_length_buffer: Array containing supported word length.
 * @bra_use_flow_control: Flow control is required or not for bra block
 *			transfer.
 * @bra_initiator_supported: Can Slave be BRA initiator.
 * @ch_prepare_mode: Type of channel prepare scheme. Simplified or Normal
 *			channel prepare.
 * @impl_def_response_supported;: If True (nonzero), implementation-defined
 *			response is supported. This information may be used
 *			by a device driver to request that a generic bus
 *			driver forwards the response to the client device
 *			driver.
 * @imp_def_intr_mask: Implementation defined interrupt mask for DP0 Port.
 * @impl_def_bpt_supported: If True (nonzero), implementation-defined
 *			Payload Type is supported. This information is used
 *			to bypass the BRA protocol and may only be of
 *			interest when a device driver is aware of the
 *			Capabilities of the Master controller and Slave
 *			devices.
 * @slave_bra_cap: BRA capabilities of the Slave.
 */

struct sdw_slv_dp0_capabilities {
	unsigned int max_word_length;
	unsigned int min_word_length;
	unsigned int num_word_length;
	unsigned int *word_length_buffer;
	unsigned int bra_use_flow_control;
	bool bra_initiator_supported;
	enum sdw_ch_prepare_mode ch_prepare_mode;
	bool impl_def_response_supported;
	unsigned int imp_def_intr_mask;
	bool impl_def_bpt_supported;
	struct sdw_slv_bra_capabilities slave_bra_cap;
};

/** struct sdw_slv_capabilities: Capabilities of the SoundWire Slave. This
 *				is public structure for slave drivers to
 *				updated its capability to bus driver.
 *
 * @wake_up_unavailable: Slave is capable of waking up the Master.
 * @test_mode_supported: Slave supports test modes.
 * @clock_stop1_mode_supported: Clock stop 1 mode supported by this Slave.
 * @simplified_clock_stop_prepare: Simplified clock stop prepare
 *				supported.
 * @highphy_capable: Slave is highphy_capable or not?
 * @paging_supported: Paging registers supported for Slave?
 * @bank_delay_support: Bank switching delay for Slave
 * @port_15_read_behavior: Slave behavior when the Master attempts a Read to
 *			the Port15 alias
 *			0: Command_Ignored
 *			1: Command_OK, Data is OR of all registers
 * @sdw_dp0_supported: DP0 is supported by Slave.
 * @sdw_dp0_cap: Data Port 0 Capabilities of the Slave.
 * @num_of_sdw_ports: Number of SoundWire Data ports present. The representation
 *			assumes contiguous Port numbers starting at 1.
 * @sdw_dpn_cap: Capabilities of the SoundWire Slave ports.
 */

struct sdw_slv_capabilities {
	bool wake_up_unavailable;
	bool test_mode_supported;
	bool clock_stop1_mode_supported;
	bool simplified_clock_stop_prepare;
	bool highphy_capable;
	bool paging_supported;
	bool bank_delay_support;
	unsigned int port_15_read_behavior;
	bool sdw_dp0_supported;
	struct sdw_slv_dp0_capabilities *sdw_dp0_cap;
	int num_of_sdw_ports;
	struct sdw_slv_dpn_capabilities *sdw_dpn_cap;
};


/**
 * struct sdw_slave: Represents SoundWire Slave device
 *				(similar to 'i2c_client' on I2C)
 *		This is not public structure. Maintained by
 *		bus driver internally.
 * @dev: Driver model representation of the device
 * @slave_cap_updated: Did slave device driver updated slave capabilties
 *			to bus.
 * @name: Name of the driver to use with the device.
 * @dev_id: 6-byte unique device identification.
 * @driver: Slave's driver, pointer to access routine.
 * @mstr: SoundWire Master, managing the bus on which this Slave is
 * @slv_number: Logical address of the Slave, assigned by bus driver
 * @node: Node to add the Slave to the list of Slave devices managed
 *		by same Master.
 * @port_ready: Port ready completion flag for each Port of the Slave;
 * @sdw_slv_cap: Slave Capabilities.
 */
struct sdw_slave {
	struct device		dev;
	bool			slave_cap_updated;
	char			name[SOUNDWIRE_NAME_SIZE];
	u8			dev_id[6];
	struct sdw_slv_addr	*slv_addr;
	struct sdw_slave_driver	*driver;
	struct sdw_master	*mstr;
	u8			slv_number;
	struct list_head	node;
	struct completion	*port_ready;
	struct sdw_slv_capabilities sdw_slv_cap;
};
#define to_sdw_slave(d) container_of(d, struct sdw_slave, dev)

/**
 *  struct sdw_bus_params: Bus params for the Slave to be ready for next
 *  bus changes.
 *  @num_rows: Number of rows in new frame to be effective.
 *  @num_cols: Number of columns in new frame to be effective.
 *  @bus_clk_freq: Clock frequency for the bus.
 *  @bank: Register bank, which Slave driver should program for
 *			implementation define Slave registers. This is the
 *			inverted value of the current bank.
 */

struct sdw_bus_params {
	int num_rows;
	int num_cols;
	int bus_clk_freq;
	int bank;
};

/**
 * struct sdw_slave_driver: Manage SoundWire generic/Slave device driver
 * @driver_type: To distinguish between master and slave driver. Set and
 *		used by bus driver.
 * @probe: Binds this driver to a SoundWire Slave.
 * @remove: Unbinds this driver from the SoundWire Slave.
 * @shutdown: Standard shutdown callback used during powerdown/halt.
 * @suspend: Standard suspend callback used during system suspend
 * @resume: Standard resume callback used during system resume
 * @driver: Generic driver structure, according to driver model.
 * @handle_impl_def_interrupts: Slave driver callback, for status of the
 *			Slave other than "REPORT_PRESENT". There may be
 *			jack detect, pll locked kind of status update
 *			interrupt required by Slave, which Slave need to
 *			handle in impl_defined way, using implementation
 *			defined interrupts. This is callback function to
 *			Slave to handle implementation defined interrupts.
 * @handle_bus_changes: Slave callback function to let Slave configure
 *			implementation defined registers prior to any bus
 *			configuration changes. Bus configuration changes
 *			will be signaled by a bank switch initiated by the bus
 *			driver once all Slaves drivers have performed their
 *			imp-def configuration sequence (if any).
 *			If this callback is not implemented the bus driver
 *			will assume the Slave can tolerate bus configurations
 *			changes at any time.
 *
 * @handle_pre_port_prepare: Slave driver callback to allow Slave Port to be
 *				prepared by configuring impl defined register
 *				as part of Port prepare state machine.
 *				This fn is called before DPn_Prepare ctrl is
 *				written. Before this function is
 *				called Port state is un-prepared (CP_Stopped).
 *				This is optional based on any impl
 *				defined register needs to be set by Slave
 *				driver before Port is prepared.
 * @handle_post_port_prepare: Slave driver callback to allow Slave Port to be
 *				prepared by configuring impl defined register
 *				as part of Port prepare state machine.
 *				This is called after DPn_Prepare
 *				ctrl is written, and DPn_status reports as
 *				Port prepared(CP_Ready). This is optional
 *				based on any impl defined register needs to
 *				be set by Slave driver once Port is ready.
 * @handle_pre_port_unprepare: Slave driver callback to allow Slave Port to be
 *				un-prepared by configuring impl defined register
 *				as part of Port un-prepare state machine.
 *				This is called before DPn_Prepare ctrl is
 *				written. Before this function is called
 *				Port state is ready (CP_Ready).
 *				This is optional based on any impl
 *				defined register needs to be set by Slave
 *				driver before Port is un-prepared.
 * @handle_post_port_unprepare: Slave driver callback to allow Slave Port to be
 *				un-prepared by configuring impl defined register
 *				as part of Port prepare state machine.
 *				This is called after DPn_Prepare
 *				ctrl is written, and DPn_status reports as
 *				Port un-prepared (CP_Stopped).
 *				This is optional based on any impl defined
 *				register needs to be set by Slave driver once
 *				Port is un-prepared.
 *
 * @id_table: List of SoundWire Slaves supported by this driver
 */
struct sdw_slave_driver {
	enum sdw_driver_type driver_type;
	int (*probe)(struct sdw_slave *swdev, const struct sdw_slave_id *);
	int (*remove)(struct sdw_slave *swdev);
	void (*shutdown)(struct sdw_slave *swdev);
	int (*suspend)(struct sdw_slave *swdev,
		pm_message_t pmesg);
	int (*resume)(struct sdw_slave *swdev);
	struct device_driver driver;
	int (*handle_impl_def_interrupts)(struct sdw_slave *swdev,
		unsigned int intr_status_mask);
	int (*handle_bus_changes)(struct sdw_slave *swdev,
			struct sdw_bus_params *params);
	int (*handle_pre_port_prepare)(struct sdw_slave *swdev,
			int port, int ch_mask, int bank);
	int (*handle_post_port_prepare)(struct sdw_slave *swdev,
			int port, int ch_mask, int bank);
	int (*handle_pre_port_unprepare)(struct sdw_slave *swdev,
			int port, int ch_mask, int bank);
	int (*handle_post_port_unprepare)(struct sdw_slave *swdev,
			int port, int ch_mask, int bank);
	const struct sdw_slave_id *id_table;
};
#define to_sdw_slave_driver(d) container_of(d, struct sdw_slave_driver, driver)

/**
 * struct sdw_mstr_dpn_capabilities: Capabilities of the Data Port, other than
 *				Data Port 0 for SoundWire Master
 * @port_direction: Direction of the Port.
 * @port_number: Port number.
 * @max_word_length: Maximum length of the sample word.
 * @min_word_length: Minimum length of sample word.
 * @num_word_length: Length of supported word length buffer. This should be
 *			0 in order to use min and max.
 * @word_length_buffer: Array of the supported word length.
 * @dpn_type: Type of Data Port.
 * @dpn_grouping: Max Block count grouping supported for this Port. if
 *		slave supports only 1 block group count, than DPN_BlockCtrl2
 *		wont be programmed.
 * @min_ch_num: Minimum number of channels supported.
 * @max_ch_num: Maximum number of channels supported.
 * @num_ch_supported: Buffer length for the channels supported.This should be
 *			0 in order to use min and max.
 * @ch_supported: Array of the channel supported.
 * @port_mode_mask: Transport modes supported by Port.
 * @block_packing_mode_mask: Block packing mode mask.
 */

struct sdw_mstr_dpn_capabilities {
	unsigned int port_direction;
	unsigned int port_number;
	unsigned int max_word_length;
	unsigned int min_word_length;
	unsigned int num_word_length;
	unsigned int *word_length_buffer;
	enum sdw_dpn_type dpn_type;
	enum sdw_dpn_grouping dpn_grouping;
	unsigned int min_ch_num;
	unsigned int max_ch_num;
	unsigned int num_ch_supported;
	unsigned int *ch_supported;
	unsigned int port_mode_mask;
	unsigned int block_packing_mode_mask;
};

/**
 * struct sdw_mstr_dp0_capabilities: Capabilities of the Data Port 0 of Slave.
 *
 * @max_word_length: Maximum word length supported by the Data Port.
 * @min_word_length: Minimum word length supported by the Data Port.
 * @num_word_length: Array size of the buffer containing the supported
 *			word lengths.
 * @word_length_buffer: Array containing supported word length.
 * @bra_max_data_per_frame: Maximum Data size per BRA.
 */
struct sdw_mstr_dp0_capabilities {
	unsigned int max_word_length;
	unsigned int min_word_length;
	unsigned int num_word_length;
	unsigned int *word_length_buffer;
	unsigned int bra_max_data_per_frame;
};

/**
 * struct sdw_master_capabilities: Capabilities of the Master.
 *	This is filled by the software registering Master.
 * @base_clk_freq: Highest base frequency at which Master can be driven
 *	This is in Hz.
 * @monitor_handover_supported: Does Master support monitor handover.
 * @highphy_capable: Is Master Highphy capable?
 * @sdw_dp0_supported: Data port0 supported?
 * @sdw_dp0_cap: Capabilities of the dataport 0 of the Master.
 * @num_data_ports: Array size for the number of Data ports present in
 *			Master.
 * @sdw_dpn_cap: Array containing information about SoundWire Master
 *		Data Port Capabilities
 *
 */
struct sdw_master_capabilities {
	unsigned int			base_clk_freq;
	bool				monitor_handover_supported;
	bool				highphy_capable;
	bool				sdw_dp0_supported;
	struct sdw_mstr_dp0_capabilities sdw_dp0_cap;
	unsigned int			num_data_ports;
	struct sdw_mstr_dpn_capabilities *sdw_dpn_cap;

};

/**
 * struct sdw_master: Master device controller on SoundWire bus.
 *				(similar to 'Master' on I2C)
 * @owner: Owner of this module. Generally THIS module.
 * @dev: Slave interface for this driver;
 * @nr: Bus number of SoundWire Master bus. Also referred to as link number.
 * @slv_list: List of SoundWire Slaves registered to the bus.
 * @name: Name of the Master driver.
 * @sdw_addr: Array containing Slave SoundWire bus Slave address information.
 * @bus_lock: Global lock for bus functions.
 * @num_slv: Number of SoundWire Slaves assigned logical address.
 * @wq: Workqueue instance for Slave detection.
 * @mstr_capabilities: Capabilities of the SoundWire Master controller.
 * @driver: Driver handling the Master.
 * @slv_released: Flag to indicate Slave release completion. Internally used
 *		by bus driver.
 * @timeout: Timeout before getting response from Slave.
 * @retries: How many times to retry before giving up on Slave response.
 * @ssp_tag_synchronized: Do bus driver needs to set SSP tag based on
 *			sample interval of of all streams.
 * @link_sync_mask: Bit mask representing all the other controller links
 *		with which this link is synchronized.
 *
 */
struct sdw_master {
	struct module		*owner;
	struct device		dev;
	unsigned int		nr;
	struct list_head	slv_list;
	char			name[SOUNDWIRE_NAME_SIZE];
	struct sdw_slv_addr	sdw_addr[SOUNDWIRE_MAX_DEVICES + 1];
	struct rt_mutex		bus_lock;
	u8			num_slv;
	struct workqueue_struct *wq;
	struct sdw_master_capabilities mstr_capabilities;
	struct sdw_mstr_driver	*driver;
	struct completion slv_released;
	struct list_head	mstr_rt_list;
	int timeout;
	int retries;
	bool ssp_tag_synchronized;
	int link_sync_mask;
};
#define to_sdw_master(d) container_of(d, struct sdw_master, dev)


/** struct sdw_port_params: This is used to program the
 *			Data Port based on Data Port
 *			stream params. These parameters cannot be changed
 *			dynamically
 *
 * @num : Port number for which params are there.
 * @word_length: Word length of the Port
 * @port_flow_mode: Port Data flow mode.
 * @port_data_mode: Test mode or normal mode.
 */
struct sdw_port_params {
	int num;
	int word_length;
	int port_flow_mode;
	int port_data_mode;
};

/** struct sdw_transport_params: This is used to program the
 *			Data Port based on Data Port
 *			transport params. These parameters may be changed
 *			dynamically based on Frame Shape changes and bandwidth
 *			allocation
 *
 * @num : Port number for which params are there.
 * @blockgroupcontrol_valid: Does Port implement block group control?
 * @blockgroupcontrol: Block group control value.
 * @sample_interval: Sample interval.
 * @offset1: Blockoffset of the payload Data.
 * @offset2: Blockoffset of the payload Data.
 * @hstart: Horizontal start of the payload Data.
 * @hstop: Horizontal stop of the payload Data.
 * @blockpackingmode: Block per channel or block per Port.
 * @lanecontrol: Data lane Port uses for Data transfer.
 */
struct sdw_transport_params {
	int num;
	bool blockgroupcontrol_valid;
	int blockgroupcontrol; /* DPN_BlockCtrl2 */
	int sample_interval;   /* DPN_SampleCtrl1 and DPN_SampleCtrl2 */
	int offset1;		/* DPN_OffsetCtrl1 */
	int offset2;		/* DPN_OffsetCtrl2 */
	int hstart;		/*  DPN_HCtrl  */
	int hstop;		/*  DPN_HCtrl  */
	int blockpackingmode;	/* DPN_BlockCtrl3 */
	int lanecontrol;	/* DPN_LaneCtrl */
};

/** struct sdw_prepare_ch: Prepare/Un-prepare the Data Port channel.
 *
 * @num : Port number for which params are there.
 * @ch_mask: prepare/un-prepare channels specified by ch_mask
 * @prepare: Prepare/Un-prepare channel
 */
struct sdw_prepare_ch {
	int num;
	int ch_mask;
	bool prepare;
};

/** struct sdw_activate_ch: Activate/Deactivate Data Port channel.
 *
 * @num : Port number for which params are there.
 * @ch_mask: Active channel mask for this port.
 * @activate: Activate/Deactivate channel
 */
struct sdw_activate_ch {
	int num;
	int ch_mask;
	bool activate;
};

/**
 * struct sdw_master_port_ops: Callback functions from bus driver
 *				to Master driver to set Master
 *				Data ports. Since Master registers
 *				are not standard, commands are passed
 *				to Master from bus and Master
 *				converts commands to register settings
 *				based on Master register map.
 * @dpn_set_port_params: Set the Port parameters for the Master Port.
 * @dpn_set_port_transport_params: Set transport parameters for the
 *				Master Port.
 * @dpn_port_prepare_ch: Prepare/Un-prepare the Master channels of the Port
 * @dpn_port_prepare_ch_pre: Called before calling dpn_port_prepare_ch, if
 *				Master driver needs to do update
 *				register settings before ch_prepare
 * @dpn_port_prepare_ch_post: Called after calling dpn_port_prepare_ch, if
 *				Master driver needs to do some
 *				register settings after ch_prepare
 * @dpn_port_activate_ch: Activate the channels of particular Master Port
 * @dpn_port_activate_ch_pre: Called before calling dpn_port_activate_ch, if
 *				Master driver needs to some register
 *				setting before activating channel.
 * @dpn_port_activate_ch_post : Called after calling dpn_port_activate_ch, if
 *				Master driver needs to some register
 *				setting after activating channel.
 */
struct sdw_master_port_ops {
	int (*dpn_set_port_params)(struct sdw_master *mstr,
			struct sdw_port_params *port_params, int bank);
	int (*dpn_set_port_transport_params)(struct sdw_master *mstr,
			struct sdw_transport_params *transport_params,
								int bank);
	int (*dpn_port_prepare_ch)(struct sdw_master *mstr,
			struct sdw_prepare_ch *prepare_ch);
	int (*dpn_port_prepare_ch_pre)(struct sdw_master *mstr,
			struct sdw_prepare_ch *prepare_ch);
	int (*dpn_port_prepare_ch_post)(struct sdw_master *mstr,
			struct sdw_prepare_ch *prepare_ch);
	int (*dpn_port_activate_ch)(struct sdw_master *mstr,
			struct sdw_activate_ch *activate_ch, int bank);
	int (*dpn_port_activate_ch_pre)(struct sdw_master *mstr,
			struct sdw_activate_ch *activate_ch, int bank);
	int (*dpn_port_activate_ch_post)(struct sdw_master *mstr,
			struct sdw_activate_ch *activate_ch, int bank);
};

/**
 * struct sdw_master_ops: Callback operations from bus driver to Master
 *				Master driver.Bus driver calls these
 *				functions to control the bus parameters
 *				in Master hardware specific way. Its
 *				like i2c_algorithm to access the bus
 *				in Master specific way.
 *
 *				Slave registers are standard.
 * @xfer_msg: Callback function to Master driver to read/write
 *		Slave registers.
 * @xfer_bulk: Callback function to Master driver for bulk transfer.
 * @monitor_handover: Allow monitor to be owner of command, if requested.
 * @set_ssp_interval: Set SSP interval.
 * @set_clock_freq: Set the clock frequency based on bandwidth requirement.
 *			Controller driver sets the frequency in hardware
 *			specific way.
 *
 */

struct sdw_master_ops {
	enum sdw_command_response (*xfer_msg)(struct sdw_master *mstr,
		struct sdw_msg *msg, bool program_scp_addr_page);
	enum sdw_command_response (*xfer_msg_async)(struct sdw_master *mstr,
		struct sdw_msg *msg, bool program_scp_addr_page,
		struct sdw_async_xfer_data *data);
	int (*xfer_bulk)(struct sdw_master *mstr,
		struct sdw_bra_block *block);
	int (*monitor_handover)(struct sdw_master *mstr,
		bool handover);
	int (*set_ssp_interval)(struct sdw_master *mstr,
			int ssp_interval, int bank);
	int (*set_clock_freq)(struct sdw_master *mstr,
			int cur_clk_freq, int bank);
	int (*set_frame_shape)(struct sdw_master *mstr,
			int col, int row, int bank);
};

/**
 * struct sdw_mstr_driver: Manage SoundWire Master/Master device driver
 * @driver_type: To distinguish between master and slave driver. Set and
 *		used by bus driver.
 * @probe: Binds this driver to a SoundWire Master.
 * @remove: Unbinds this driver from the SoundWire Master.
 * @shutdown: Standard shutdown callback used during powerdown/halt.
 * @suspend: Standard suspend callback used during system suspend
 * @resume: Standard resume callback used during system resume
 * @driver: SoundWire device drivers should initialize name and owner field of
 *      this structure.
 * @mstr_ops: Callback operations from bus driver to Master driver for
 *		programming and controlling bus parameters and to program
 *		Slave  registers.
 * @mstr_port_ops: Commands to setup the Master ports. Master register
 *		map is not defined by standard. So these ops represents the
 *		commands to setup Master ports.
 * @id_table: List of SoundWire devices supported by this driver.
 */
struct sdw_mstr_driver {
	enum sdw_driver_type driver_type;
	int (*probe)(struct sdw_master *sdwmstr, const struct sdw_master_id *);
	int (*remove)(struct sdw_master *sdwmstr);
	void (*shutdown)(struct sdw_master *sdwmstr);
	int (*suspend)(struct sdw_master *sdwmstr,
		pm_message_t pmesg);
	int (*resume)(struct sdw_master *sdwmstr);
	struct device_driver driver;
	struct sdw_master_ops *mstr_ops;
	struct sdw_master_port_ops *mstr_port_ops;
	const struct sdw_master_id *id_table;
};
#define to_sdw_mstr_driver(d) container_of(d, struct sdw_mstr_driver, driver)

/**
 * struct sdw_msg : Message to be sent on bus. This is similar to i2c_msg
 *			on I2C bus.
 *			Actually controller sends the message on bus
 *			in hardware specific way. This interface is from
 *			bus driver to Slaves.
 * @slave_addr: Slave address
 * @ssp_tag: send message at ssp_tag. It should be used when a command needs
 *		to be issued during the next SSP. For all normal reads/writes
 *		this should be zero. This will be used for broadcast write
 *		to SCP_FrameCtrl register by bus driver only. Normally
 *		slave driver should always set ssp_tag  to 0.
 * @addr_page1: SCP address page 1
 * @addr_page2: SCP address page 2
 * @flag: Message to be read or write.
 * @addr: Address of the register to be read;
 * @len: Length of the message to be read. Successive increment in the
 *	register address for every message.
 * @buf: Buf to be written or read from the register.
 */
struct sdw_msg {
	u8 slave_addr;
	bool ssp_tag;
	u8 addr_page1;
	u8 addr_page2;
#define SDW_MSG_FLAG_READ	0x0
#define SDW_MSG_FLAG_WRITE	0x1
	u8 flag;
	u16 addr;
	u16 len;
	u8 *buf;
};

/**
 * sdw_stream_config: Stream configuration of the device. This includes
 *			Master and Slave.
 * @frame_rate: Audio frame rate of the stream.
 * @channel_count: Channel count of the stream.
 * @bps: Number of bits per audio sample.
 * @direction: Direction of the Data. What is the data direction for the
 *		device calling stream_config. This is w.r.t device.
 * @type: Stream type PCM or PDM
 *
 */
struct sdw_stream_config {
	unsigned int frame_rate;
	unsigned int channel_count;
	unsigned int bps;
	enum sdw_data_direction direction;
	enum sdw_stream_type	type;
};

/**
 * sdw_port_cfg: SoundWire Port configuration, Configuration is done
 *		for all port of all the devices which are part of stream.
 *		All the ports of stream handles same pcm parameters accept
 *		channels. e.g Master may handle stereo channels using single
 *		port, but slave may handle Left and Right channel on one
 *		port each.
 * @port_num:	Port number to be configured
 * @ch_mask: Which channels needs to be activated for this Port.
 */
struct sdw_port_cfg {
	int port_num;
	unsigned int ch_mask;
};

/**
 * sdw_port_config: List of the ports handled by slave or master
 *			for particular stream. Both slave and master calls
 *			this with ports they trasmit/recevie onfor particular
 *			stream.
 * @num_ports: Number of ports to be configured.
 * @port_cfg : Port configuration for each Port.
 */
struct sdw_port_config {
	unsigned int num_ports;
	struct sdw_port_cfg *port_cfg;
};

/**
 * struct sdw_bra_block: Data block to be sent/received using SoundWire
 *			bulk transfer protocol
 * @slave_addr: Slave logical address from/to which transfer
 *			needs to take place.
 * @operation: Read operation or write operation.
 * @num_bytes: Number of Data bytes to be transferred.
 * @reg_offset: Register offset from where the first byte to read/write.
 * @values: Array containing value for write operation and to be filled
 *		for read operation.
 */
struct  sdw_bra_block {
	int slave_addr;
	int cmd;
	int num_bytes;
	int reg_offset;
	u8 *values;
};

/**
 *  Struct sdw_slave_status: Status of all the SoundWire Slave devices.
 *  @status: Array of status of SoundWire Slave devices. 0 is also
 *		a soundwire device during enumeration, so adding +1 for
 *		that. Actual number fo devices that can be supported are
 *		11 as defined by SOUNDWIRE_MAX_DEVICES
 */
struct sdw_status {
	enum sdw_slave_status status[SOUNDWIRE_MAX_DEVICES + 1];
};

/**
 * sdw_add_master_controller: Add SoundWire Master controller interface
 * @mstr: Controller to be registered as SoundWire Master interface.
 *	This is to be called for each Master interface.
 *	This is same as I2C, where each adapter register specifies one
 *	pair of clock and Data lines (link).
 */
int sdw_add_master_controller(struct sdw_master *mstr);

/**
 * sdw_del_master_controller: Master tear-down.
 * Master added with the "sdw_add_master_controller" API is teared down
 * using this API.
 * @mstr: Master to be teared down
 */
void sdw_del_master_controller(struct sdw_master *mstr);

/**
 * sdw_mstr_driver_register: SoundWire Master driver registration with SDW bus.
 *			This API will register the Master driver with the
 *			SoundWire bus. It is typically called from the
 *			driver's module-init function.
 * @drv: Master Driver to be associated with device.
 *
 */
int __sdw_mstr_driver_register(struct module *owner,
					struct sdw_mstr_driver *driver);
#define sdw_mstr_driver_register(drv) \
			__sdw_mstr_driver_register(THIS_MODULE, drv)

/**
 * sdw_mstr_driver_unregister: Undo effects of sdw_mstr_driver_register
 * @drv: SDW Master driver to be unregistered
 */
void sdw_mstr_driver_unregister(struct sdw_mstr_driver *drv);

/**
 * __sdw_slave_driver_register: SoundWire Slave driver registration with
 *				SDW bus. This API will register the Slave
 *				driver with the SoundWire bus. It is typically
 *				called from the driver's module-init function.
 * @drv: Driver to be associated with Slave.
 */
int __sdw_slave_driver_register(struct module *owner,
					struct sdw_slave_driver *drv);
#define sdw_slave_driver_register(drv) \
			__sdw_slave_driver_register(THIS_MODULE, drv)

/**
 * sdw_register_slave_capabilities: Register slave device capabilties to the
 *				bus driver. Since bus driver handles bunch
 *				of slave register programming it should
 *				be aware of slave device capabilties.
 *				Slave device is attached to bus based on
 *				enumeration. Once slave driver is attached
 *				to device and probe of slave driver is called
 *				on device and driver binding, slave driver
 *				should call this function to register its
 *				capabilties to bus. This should be the very
 *				first function to bus driver from slave driver
 *				once slave driver is registered and probed.
 * @slave: SoundWire Slave handle
 * @cap: Slave capabilities to be updated to bus driver.
 */
int sdw_register_slave_capabilities(struct sdw_slave *slave,
					struct sdw_slv_capabilities *cap);

/**
 * sdw_slave_driver_unregister: Undo effects of sdw_slave_driver_register
 * @drv: SDW Slave driver to be unregistered
 */
void sdw_slave_driver_unregister(struct sdw_slave_driver *drv);

/**
 * sdw_slave_transfer: Transfer SDW message on bus.
 * @mstr: Master which will transfer the message.
 * @msg: Array of messages to be transferred.
 * @num: Number of messages to be transferred, messages include read and write
 *		messages, but not the ping messages.
 */
int sdw_slave_transfer(struct sdw_master *mstr, struct sdw_msg *msg, int num);

/**
 * sdw_alloc_stream_tag: Allocate stream_tag for each audio stream
 *			between SoundWire Masters and Slaves.
 *			(Multiple Masters and Slave in case of
 *			aggregation) stream_tag is
 *			unique across system.  Stream tag represents the
 *			independent audio stream which can be controlled
 *			configure individually. Stream can be split between
 *			multiple Masters and Slaves. It can also be
 *			split between multiple ports of the Master and Slave.
 *			All the stream configuration for Masters and Slaves
 *			and ports of the Master and Slave for particular
 *			stream is done on stream_tag as handle. Normally
 *			stream is between CPU and Codec. CPU dai ops
 *			allocate the stream tag and programs same to the
 *			codec dai. If there are multiple codecs attached
 *			to each CPU DAI, like L and R digital speaker, both
 *			codecs should be programmed with same stream tag.
 *
 *
 * @uuid:		uuid  is used to make sure same stream tag gets
 *			allocated for same uuid. If stream tag is not
 *			allocated for uuid, it will get allocated and
 *			uuid-stream tag pair will be saved, for next
 *			allocation based on uuid. If this is NULL,
 *			new stream tag will be allocated each time this
 *			function is called.
 *
 *
 * @stream_tag: Stream tag returned by bus driver.
 */
int sdw_alloc_stream_tag(char *uuid, int *stream_tag);

/**
 * sdw_release_stream_tag: Free the already assigned stream tag.
 *
 * @stream_tag: Stream tag to be freed.
 */
void sdw_release_stream_tag(int stream_tag);

/**
 * sdw_config_stream: Configure the audio stream. Each stream between
 *			master and slave, or between slaves has unique
 *			stream tag.
 *			Master and Slave attaches to
 *			the stream using the unique stream_tag for each
 *			stream between Master(s) and Slave(s). Both
 *			Master and Slave driver call this function to
 *			attach to stream and let bus driver know the
 *			stream params. Master and Slave calls this function
 *			typically as part of hw_params ops of their respective
 *			DAIs to let bus driver know about stream parameters.
 *			Stream parameters between Tx and Rx direction
 *			should match. Function is reference counted so
 *			multiple Master and Slaves attached to particular
 *			stream can call to setup stream config.
 *			Master calls this function with Slave handle as
 *			NULL.
 * @mstr: Master handle,
 * @slave: SoundWire Slave handle, Null if stream configuration is called
 *             by Master driver.
 * @stream_config: Stream configuration for the SoundWire audio stream.
 * @stream_tag: Stream_tag representing the audio stream. All Masters and Slaves
 *             part of the same stream will have same stream tag. So bus drivers
 *             know  which all Masters and Slaves are part of stream.
 *
 */
int sdw_config_stream(struct sdw_master *mstr,
		struct sdw_slave *slave,
		struct sdw_stream_config *stream_config,
		unsigned int stream_tag);

/**
 * sdw_release_stream: De-associates Master(s) and Slave(s) from stream. Reverse
 *		effect of the sdw_config_stream
 *
 * @mstr: Master handle,
 * @slave: SoundWire Slave handle, Null if stream configuration is called
 *		by Master driver.
 * @stream_tag: Stream_tag representing the audio stream. All Masters and Slaves
 *		part of the same stream has same stream tag. So bus drivers
 *		know  which all Masters and Slaves are part of stream.
 *
 */
int sdw_release_stream(struct sdw_master *mstr,
		struct sdw_slave *slave,
		unsigned int stream_tag);

/**
 * sdw_config_port: Master(s) and Slave(s) are associated to stream.
 *			Each Master and Slave can handle stream using
 *			different SoundWire Port number(s).
 *			e.g Master may handle stereo stream using single
 *			Data Port, while Slave may handle each channel
 *			of Data stream using one Port each. Bus driver
 *			needs to know the stream to Port association
 *			for each Master(s) and Slave(s) assocated with the
 *			stream for configuring Master and Slave ports
 *			based on transport params calculate by bus for a
 *			stream. Both Master and Slave call this function to let
 *			bus driver know about stream to Port mapping.
 *			Master driver calls this with Slave handle
 *			as NULL.
 * @mstr: Master handle where the Slave is connected.
 * @slave: Slave handle.
 * @port_config: Port configuration for each Port of SoundWire Slave.
 * @stream_tag: Stream tag, where this Port is connected.
 *
 */
int sdw_config_port(struct sdw_master *mstr,
			struct sdw_slave *slave,
			struct sdw_port_config *port_config,
			unsigned int stream_tag);

/**
 * sdw_prepare_and_enable: Prepare and enable all the ports of all the Master(s)
 *			and Slave(s) associated with this stream tag.
 *			Following will be done as part of prepare and
 *			enable by bus driver.
 *			1. Calculate new bandwidth required on bus
 *				because of addition be this new stream.
 *			2. Calculate new frameshape based on bandwidth
 *			3. Calculate the new clock frequency on which
 *				bus will be transistioned based on new
 *				bandwidth and frameshape.
 *			4. Calculate new transport params for the already
 *				active on the bus based on clock frequency,
 *				frameshape and bandwidth changes.
 *			5. Calculate transport params for this stream
 *			6. Program already active ports with new transport
 *				params and frame shape,
 *				change clock frequency of master.
 *			7. Prepare ports for this stream.
 *			8. Enable ports for this stream.
 * @stream_tag: Audio stream to be activated. Each stream has unique
 *		stream_tag. All the channels of all the ports of Slave(s)
 *		and Master(s) attached to this stream will be activated
 *		deactivated simultaneously at proper SSP or gsync.
 * @enable: Enable the ports as part of current call or not. If its
 *		false only till steps 1 to 7 will be executed as part of this
 *		function call. if its true, than steps 1 to 7 will be executed
 *		if not already done, else only step 8 will be executed.
 *
 */
int sdw_prepare_and_enable(int stream_tag, bool enable);

/**
 * sdw_disable_and_unprepare: Un-Prepare and disable all the ports of all the
 *			 Master(s) and Slave(s) associated with stream tag.
 *			Following will be done as part of Un-prepare and
 *			disable by bus driver.
 *			1. Disable all the ports for this stream.
 *			2. Un-Prepare ports for this stream.
 *			3. Calculate new bandwidth required on bus
 *				because of removal of this new stream.
 *			4. Calculate new frameshape based on bandwidth
 *			5. Calculate the new clock frequency on which
 *				bus will be transistioned based on new
 *				bandwidth and frameshape.
 *			6. Calculate new transport params for the already
 *				active on the bus based on clock frequency,
 *				frameshape and bandwidth changes.
 *			7.Program already active ports with new transport
 *				params and frame shape,
 *				change clock frequency of master.
 * @stream_tag: Audio stream to be disabled. Each stream has unique
 *		stream_tag. All the channels of all the ports of Slave(s)
 *		and Master(s) attached to this stream will be activated
 *		deactivated simultaneously at proper SSP or gsync.
 * @un_prepare: Un-prepare the ports as part of current call or not. If its
 *		false only step 1 will be executed as part of this
 *		function call. if its true, than step 1 will be executed
 *		if not already done, else only 2 to 7 will be executed.
 */
int sdw_disable_and_unprepare(int stream_tag, bool un_prepare);

/**
 * sdw_master_update_slv_status: Update the status of the Slave to the bus
 *			driver. Master calls this function based on the
 *			interrupt it gets once the Slave changes its
 *			state or from interrupts for the Master hardware
 *			that caches status information reported in PING frames
 * @mstr: Master handle for which status is reported.
 * @status: Array of status of each Slave.
 */
int sdw_master_update_slv_status(struct sdw_master *mstr,
					struct sdw_status *status);

/**
 * sdw_get_master: Return the Master handle from Master number.
 *			Increments the reference count of the module.
 *			Similar to i2c_get_adapter.
 *  nr: Master controller number.
 *  returns Master handle on success, else NULL
 */
struct sdw_master *sdw_get_master(int nr);

/**
 *  sdw_put_master: Reverses the effect of sdw_get_master
 *  mstr: Master controller handle.
 */
void sdw_put_master(struct sdw_master *mstr);


/**
 * module_sdw_slave_driver() - Helper macro for registering a sdw Slave driver
 * @__sdw_slave_driver: sdw_slave_driver struct
 *
 * Helper macro for sdw drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_sdw_slave_driver(__sdw_slave_driver) \
	module_driver(__sdw_slave_driver, sdw_slave_driver_register, \
			sdw_slave_driver_unregister)
/**
 * sdw_prepare_for_clock_change: Prepare all the Slaves for clock stop or
 *		clock start. Prepares Slaves based on what they support
 *		simplified clock stop or normal clock stop based on
 *		their capabilities registered to slave driver.
 * @mstr: Master handle for which clock state has to be changed.
 * @start: Prepare for starting or stopping the clock
 * @clk_stop_mode: Bus used which clock mode, if bus finds all the Slaves
 *		on the bus to be supported clock stop mode1 it prepares
 *		all the Slaves for mode1 else it will prepare all the
 *		Slaves for mode0.
 */
int sdw_prepare_for_clock_change(struct sdw_master *mstr, bool start,
			enum sdw_clk_stop_mode *clck_stop_mode);

/**
 * sdw_wait_for_slave_enumeration: Wait till all the slaves are enumerated.
 *			Typicall this function is called by master once
 *			it resumes its clock. This function waits in
 *			loop for about 2Secs before all slaves gets enumerated
 *			This function returns immediately if the clock
 *			stop mode0 was entered earlier, where slave need
 *			not re-enumerated.
 *
 * @mstr: Master handle
 * @slave: Slave handle
 */
int sdw_wait_for_slave_enumeration(struct sdw_master *mstr,
			struct sdw_slave *slave);

/**
 * sdw_stop_clock: Stop the clock. This function broadcasts the SCP_CTRL
 *			register with clock_stop_now bit set.
 * @mstr: Master handle for which clock has to be stopped.
 * @clk_stop_mode: Bus used which clock mode.
 */

int sdw_stop_clock(struct sdw_master *mstr, enum sdw_clk_stop_mode mode);

/* Return the adapter number for a specific adapter */
static inline int sdw_master_id(struct sdw_master *mstr)
{
	return mstr->nr;
}

static inline void *sdw_master_get_drvdata(const struct sdw_master *mstr)
{
	return dev_get_drvdata(&mstr->dev);
}

static inline void sdw_master_set_drvdata(struct sdw_master *mstr,
					void *data)
{
	dev_set_drvdata(&mstr->dev, data);
}

static inline void *sdw_slave_get_drvdata(const struct sdw_slave *slv)
{
	return dev_get_drvdata(&slv->dev);
}

static inline void sdw_slave_set_drvdata(struct sdw_slave *slv,
					void *data)
{
	dev_set_drvdata(&slv->dev, data);
}

#endif /*  _LINUX_SDW_BUS_H */
