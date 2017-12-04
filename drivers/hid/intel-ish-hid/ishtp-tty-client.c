/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

#include "ishtp-dev.h"
#include "client.h"

/*
 * ISH TX/RX ring buffer pool size. ISH transport reserves the space for these
 * during connect request.
 */
#define TTY_CL_RX_RING_SIZE	32
#define TTY_CL_TX_RING_SIZE	16

/* Commands to send to ISH UART driver */
enum ish_uart_command{
	UART_GET_CONFIG = 1,
	UART_SET_CONFIG,
	UART_SEND_DATA,
	UART_RECV_DATA,
	UART_ABORT_WRITE,
	UART_ABORT_READ,
};

#define CMD_MASK	GENMASK(6,0)
#define IS_RESPONSE	BIT(7)

/**
 * struct ishtp_tty_msg - Command header for ISH UART commands.
 * @command:		One of the ish_uart_command. Bit 7 is the response.
 * @status:		Command response status. Non 0, is error condition.
 * @size:		Size of the command excluding header.
 *
 * This structure is used as header for every command/data sent to ISH
 * UART.
 */
struct ishtp_tty_msg {
	u8 command;
	u8 status;
	u16 size;
};

/**
 * struct ish_uart_config - UART configuration data
 * @baud:		Baud rate in bits per second.
 * @bits_length:	Number of bits.
 * @stop_bits:		Number of stop bits.
 * @flow_control:	Flow control ON/OFF.
 *
 * This structure is used for every UART GET/SET configuration commands.
 */
struct ish_uart_config {
	u32 baud;
	u8 bits_length:4;
	u8 stop_bits:2;
	u8 unused_0:1;
	u8 unused_1:1;
	u8 flow_control:1;
	u8 reserved:7;
};

/**
 * struct ishtp_cl_info - ISH transport client instance data
 * @cl_device:		ISH transport client device information.
 * @ishtp_cl:		ISH transport firmware client information.
 * @get_report_done:	Set once a command is sent and response is received.
 * @wait_send_report_done: Set when send data response is received.
 * @last_cmd_status:	Last command status.
 * @ishtp_response_wait: Wait Q to wait for command response.
 * @ishtp_send_wait:	Wait Q to wait for send response.
 * @max_msg_size:	Max message size of a single command.
 * @max_ring_buffer_bytes: Max buffer size allocated by ISH transort layer.
 * @msg_buffer:		Pre allocated message buffer to copy and send.
 *
 * Encapsulate all information related to an ISH client. This information is
 * necessary to connect, send and receive data from ISH UART via ISH transport.
 */
struct ishtp_cl_info {
	struct ishtp_cl_device *cl_device;
	struct ishtp_cl *ishtp_cl;
	bool get_report_done;
	bool wait_send_report_done;
	int last_cmd_status;
	wait_queue_head_t ishtp_response_wait;
	wait_queue_head_t ishtp_send_wait;
	int max_msg_size;
	int max_ring_buffer_bytes;
	u8 *msg_buffer;
};

/**
 * struct ishtp_cl_tty - TTY client instance data
 * @cl_info: ishtp client information related to this tty client.
 * @port:	Stores the port information.
 * @baud:	Stores the current buad rate.
 * @bits_length: Stores the current number of bits.
 *
 * Encapsulate all information related to a tty client.
 */
struct ishtp_cl_tty {
	struct ishtp_cl_info *cl_info;
	struct tty_port port;
	u32 baud;
	u8 bits_length;
};

#define cl_dev_info(cl_info) &cl_info->ishtp_cl->device->dev

/*
 * Each ISH Transport client has a unique GUID. This defines GUID of tty
 * client.
 */
static const guid_t tty_ishtp_guid = GUID_INIT(0x6f2647c7, 0x3e16, 0x4d79,
					       0xb4, 0xff, 0x02, 0x89, 0x28,
					       0xee, 0xeb, 0xca);
/**
 * ish_get_free_buffer_size() - Get Free buffers in the ring
 * @tty:	TTY instance of this client.
 *
 * Each request is queued in the ISH transort ring buffers to send via IPC.
 * This function gets available free space in these ring buffers.
 *
 * Return: Size of current free space in the ring buffer in bytes.
 */
static int ish_get_free_buffer_size(struct tty_struct *tty)
{
	struct ishtp_cl_tty *cl_tty = tty->driver_data;

	return ishtp_cl_get_tx_free_rings(cl_tty->cl_info->ishtp_cl) *
		(cl_tty->cl_info->max_msg_size - sizeof(struct ishtp_tty_msg));
}

/**
 * ishtp_wait_for_response() - Wait for response for a command
 * @cl_info:	client instance info.
 *
 * If the sender is waiting for a command response, then call to this
 * function will suspend caller till the timeout or the response from the
 * firmware.
 *
 * Return: 0 for success and -ETIMEDOUT for timeout.
 */
static int ishtp_wait_for_response(struct ishtp_cl_info *cl_info)
{
	if (cl_info->get_report_done)
		return 0;

	/*
	 * ISH firmware max delay for one time sending failure is 1Hz,
	 * and firmware will retry 2 times, so 3Hz is used for timeout.
	 */
	wait_event_interruptible_timeout(cl_info->ishtp_response_wait,
					 cl_info->get_report_done, 3 * HZ);

	if (!cl_info->get_report_done) {
		dev_err(cl_dev_info(cl_info), "Timeout: ISHTP response\n");
		return -ETIMEDOUT;
	}

	return 0;
}

/**
 * ish_tty_install() - tty operation callback for install
 * @driver:	tty driver registerd to handle.
 * @tty:	tty structure passed from the core.
 *
 * During install callback create a new tty client instance for the registered
 * ISH transport client device and initialize port.
 *
 * Return: 0 for success and negative value for error.
 */
static int ish_tty_install(struct tty_driver *driver, struct tty_struct *tty)
{
	struct ishtp_cl_tty *cl_tty;
	struct ishtp_cl_device *cl_device;
	static const struct tty_port_operations ish_port_ops;

	cl_tty = kzalloc(sizeof(*cl_tty), GFP_KERNEL);
	if (!cl_tty)
		return -ENOMEM;

	cl_device = container_of(tty->dev->parent, struct ishtp_cl_device, dev);

	cl_tty->cl_info = ishtp_get_drvdata(cl_device);
	cl_tty->cl_info->ishtp_cl->client_data = cl_tty;

	tty_port_init(&cl_tty->port);
	cl_tty->port.ops = &ish_port_ops;

	tty->driver_data = cl_tty;

	return tty_port_install(&cl_tty->port, driver, tty);
}

/**
 * ish_tty_open() - tty operation callback for open
 * @tty:	tty structure passed from the core.
 * @filp:	file pointer passed from the tty core,
 *
 * Calls tty_port_open once called from tty core.
 *
 * Return: Return value of tty_port_open().
 */
static int ish_tty_open(struct tty_struct *tty, struct file *filp)
{
	struct ishtp_cl_tty *cl_tty = tty->driver_data;

	return tty_port_open(&cl_tty->port, tty, filp);
}

/**
 * ish_tty_close() - tty operation callback for close
 * @tty:	tty structure passed from the core.
 * @filp:	file pointer passed from the tty core,
 *
 * Calls tty_port_close once called from tty core and frees
 * tty instance data.
 *
 * Return: None.
 */
static void ish_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct ishtp_cl_tty *cl_tty = tty->driver_data;

	tty_port_close(&cl_tty->port, tty, filp);
	kfree(cl_tty);
}

/**
 * ish_tty_write() - tty operation callback for write
 * @tty:	tty structure passed from the core.
 * @buf:	Buffer to send
 * @count:	Length of buffer.
 *
 * This request simply queues the data upto available buffer space in
 * the transport TX ring buffers.
 * This function shouldn't call any function which sleeps as the write
 * callback can be called in the interrupt context from the core.
 *
 * Return: Return the size of data copied to ring buffers or error code.
 */
static int ish_tty_write(struct tty_struct *tty, const unsigned char *buf,
			 int count)
{
	struct ishtp_cl_tty *cl_tty = tty->driver_data;
	struct ishtp_cl_info *cl_info = cl_tty->cl_info;
	struct ishtp_tty_msg *ishtp_msg;
	unsigned char *msg_buf;
	u32 max_payload_sz;
	int c, sent = 0, ret;

	ishtp_msg = (struct ishtp_tty_msg *)cl_info->msg_buffer;
	ishtp_msg->command = UART_SEND_DATA;
	msg_buf = (unsigned char *)&ishtp_msg[1];

	max_payload_sz = cl_info->max_msg_size - sizeof(struct ishtp_tty_msg);

	while (count > 0) {
		if (ish_get_free_buffer_size(tty) < count) {
			break; /* return partial sent char count or 0 */
		}

		c = count;
		if (c > max_payload_sz)
			c = max_payload_sz;

		ishtp_msg->size = c;
		memcpy(msg_buf, buf, c);

		ret = ishtp_cl_send(cl_info->ishtp_cl, cl_info->msg_buffer,
				    sizeof(struct ishtp_tty_msg) + c);
		if (ret)
			return ret;

		buf += c;
		count -= c;
		sent += c;
	}

	return sent;
}

/* List of supported baud rates */
static u32 supported_rates[] = {
	115200,
	921600,
	2000000,
};

/**
 * ish_tty_match_baudrate() - Match the baud rate requested
 * @baud:	Requestd baud rate.
 *
 * Look for a match in the supported baud rates. If not return the lower
 * supported baud rate.

 * Return: Matched baud rate.
 */
static u32 ish_tty_match_baudrate(unsigned int baud)
{
	int count = ARRAY_SIZE(supported_rates);
	int i;

	for (i = 0; i < count; ++i) {
		if (supported_rates[i] == baud)
			return baud;
		else if ((i != count - 1) && (baud < supported_rates[i + 1]))
			return supported_rates[i];
	}

	return supported_rates[count -1];
}

/**
 * ish_tty_set_termios() -Set terminal parameters
 * @tty:	tty structure passed from the core.
 * @old_termios: Previous termios settings
 *
 * Sets terminal parameters. In case the parameters are not supported then,
 * this updates the tty termios with the acceptable parameters.
 *
 * Return: None.
 */
static void ish_tty_set_termios(struct tty_struct *tty,
					struct ktermios *old_termios)
{
	struct ishtp_cl_tty *cl_tty = tty->driver_data;
	struct ishtp_cl_info *cl_info = cl_tty->cl_info;

	struct ishtp_tty_msg *ishtp_msg;
	struct ktermios *termios;
	struct ish_uart_config *cfg;
	int ret;

	if (old_termios && !tty_termios_hw_change(&tty->termios, old_termios))
		return;

	termios = &tty->termios;
	ishtp_msg = (struct ishtp_tty_msg *)cl_info->msg_buffer;

	ishtp_msg->command = UART_SET_CONFIG;
	cfg = (struct ish_uart_config *)&ishtp_msg[1];

	switch (C_CSIZE(tty)) {
	case CS5:
		cfg->bits_length = 5;
		break;
	case CS6:
		cfg->bits_length = 6;
		break;
	case CS7:
		cfg->bits_length = 7;
		break;
	default:
	case CS8:
		cfg->bits_length = 8;
		break;
	}

	if (C_CRTSCTS(tty))
		cfg->flow_control = true;
	else
		cfg->flow_control = false;

	cfg->baud = tty_termios_baud_rate(termios);
	cfg->baud = ish_tty_match_baudrate(cfg->baud);

	ishtp_msg->size = sizeof(struct ish_uart_config);

	cl_info->get_report_done = false;
	/* ishtp message send out */
	ret = ishtp_cl_send(cl_info->ishtp_cl, cl_info->msg_buffer,
			    sizeof(struct ishtp_tty_msg) + ishtp_msg->size);
	if (ret)
		return;

	/* wait for message send completely */
	if (ishtp_wait_for_response(cl_info)) {
		return;
	}

	/* Update termios with the supported properties */
	termios->c_cflag &= ~CMSPAR;
	tty_termios_encode_baud_rate(termios, cfg->baud, cfg->baud);
}

/**
 * ish_tty_write_room() - write_room() callback
 * @tty:	tty structure passed from the core.
 *
 * Returns space availble in ISH transport TX ring buffers in bytes.
 *
 * Return: space available in bytes.
 */
static int ish_tty_write_room(struct tty_struct *tty)
{
	return ish_get_free_buffer_size(tty);
}

/**
 * ish_tty_chars_in_buffer() - chars_in_buffer() callback
 * @tty:	tty structure passed from the core.
 *
 * Returns number of characters in ISH transport ring buffers, which are
 * not yet sent via IPC to ISH.
 *
 * Return: space space used in bytes.
 */
static int ish_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct ishtp_cl_tty *cl_tty = tty->driver_data;
	struct ishtp_cl_info *cl_info = cl_tty->cl_info;

	return cl_info->max_ring_buffer_bytes - ish_get_free_buffer_size(tty);
}

/**
 * ish_tty_wait_until_sent() - wait_until_sent() callback
 * @tty:	tty structure passed from the core.
 *
 * Wait till all the bytes in the ISH transort ring buffers sent or timeout.
 * This callback is allowed to sleep.
 *
 * Return: None.
 */
static void ish_tty_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct ishtp_cl_tty *cl_tty = tty->driver_data;
	struct ishtp_cl_info *cl_info = cl_tty->cl_info;
	u64 begin, end, elapsed;

	elapsed = begin = get_jiffies_64();
	end = begin + timeout;
	while (ishtp_cl_get_tx_free_buffer_size(cl_info->ishtp_cl) !=
	       cl_info->max_ring_buffer_bytes) {

		cl_info->wait_send_report_done = true;
		if (timeout) {
			wait_event_interruptible_timeout(cl_info->ishtp_send_wait,
							 cl_info->wait_send_report_done,
							 end - elapsed);
			elapsed = get_jiffies_64() - begin;
			if (end <= (begin + elapsed))
				break;
		} else {
			wait_event_interruptible(cl_info->ishtp_send_wait,
						 cl_info->wait_send_report_done);
		}
		cl_info->wait_send_report_done = false;
	}
}

/**
 * process_recv() - Process receive data callback
 * @cl_info:	client instance for which data received.
 * @recv_buf:	Received buffer
 * @data_len;	Received size
 *
 * Process received data from ISH UART driver. This can be a new RX data
 * indication or response for sent commands.
 *
 * Return: None.
 */
static void process_recv(struct ishtp_cl_info *cl_info, void *recv_buf,
			 size_t data_len)
{
	struct ishtp_cl_device *cl_device = cl_info->cl_device;
	struct ishtp_cl_tty *cl_tty = cl_info->ishtp_cl->client_data;
	struct ishtp_tty_msg *ishtp_msg;
	struct ish_uart_config *cfg;
	unsigned char *payload;
	size_t payload_len, total_len, cur_pos;

	if (data_len < sizeof(struct ishtp_tty_msg)) {
		dev_err(&cl_device->dev,
			"Error, received %zu which is < min %lu", data_len,
			sizeof(struct ishtp_tty_msg));
		return;
	}

	payload = recv_buf + sizeof(struct ishtp_tty_msg);
	total_len = data_len;
	cur_pos = 0;

	do {
		ishtp_msg = (struct ishtp_tty_msg *)(recv_buf + cur_pos);
		payload_len = ishtp_msg->size;

		switch (ishtp_msg->command & CMD_MASK) {
		case UART_GET_CONFIG:
			cl_info->get_report_done = true;
			if (!(ishtp_msg->command & IS_RESPONSE) ||
			    (ishtp_msg->status)) {
				dev_err(&cl_device->dev,
					"Recv command with status error\n");
				wake_up_interruptible(&cl_info->ishtp_response_wait);
				break;
			}

			if (cl_tty) {
				cfg = (struct ish_uart_config *)payload;
				cl_tty->baud = cfg->baud;
				cl_tty->bits_length = cfg->bits_length;
				dev_dbg(&cl_device->dev,
					"Command: get config: %d:%d\n",
					cl_tty->baud, cl_tty->bits_length);
			}
			wake_up_interruptible(&cl_info->ishtp_response_wait);
			break;
		case UART_SET_CONFIG:
			cl_info->get_report_done = true;
			if (!(ishtp_msg->command & IS_RESPONSE) ||
			    (ishtp_msg->status)) {
				dev_err(&cl_device->dev,
					"Recv command with status error\n");
				wake_up_interruptible(&cl_info->ishtp_response_wait);
				break;
			}

			cl_info->last_cmd_status = 0;
			dev_dbg(&cl_device->dev,
				"Command: set config success\n");
			wake_up_interruptible(&cl_info->ishtp_response_wait);
			break;
		case UART_SEND_DATA:
			if (!(ishtp_msg->command & IS_RESPONSE) ||
			    (ishtp_msg->status)) {
				cl_info->last_cmd_status = -EIO;
				dev_err(&cl_device->dev,
					"Recv command with status error\n");
			} else {
				dev_dbg (&cl_device->dev,
					 "Command: send data done\n");
			}
			if (cl_info->wait_send_report_done)
				wake_up_interruptible(&cl_info->ishtp_send_wait);
			break;
		case UART_RECV_DATA:
			dev_dbg(&cl_device->dev,
				"Command: recv data: len=%ld\n",
				payload_len);
			print_hex_dump_bytes("", DUMP_PREFIX_NONE, payload,
					     payload_len);
			if (cl_tty) {
				tty_insert_flip_string(&cl_tty->port, payload,
						       payload_len);
				tty_flip_buffer_push(&cl_tty->port);
			}
			break;
		default:
			break;
		}

		cur_pos += payload_len + sizeof(struct ishtp_tty_msg);
		payload += payload_len + sizeof(struct ishtp_tty_msg);

	} while (cur_pos < total_len);
}

/**
 * tty_ishtp_cl_event_cb() - Receive data callback
 * @cl_device:	client device for which data received.
 *
 * Extract data from RX ring buffer and process received data.
 *
 * Return: None.
 */
static void tty_ishtp_cl_event_cb(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_info *cl_info = ishtp_get_drvdata(cl_device);
	struct ishtp_cl *cl = cl_info->ishtp_cl;
	struct ishtp_cl_rb *rb_in_proc;

	while ((rb_in_proc = ishtp_cl_rx_get_rb(cl)) &&
	       rb_in_proc->buffer.data) {
		process_recv(cl_info, rb_in_proc->buffer.data,
			     rb_in_proc->buf_idx);
		ishtp_cl_io_rb_recycle(rb_in_proc);
	}
}

/**
 * ishtp_cl_tty_connect() - Connect to ISH UART driver client
 * @cl_device:	client device for which data received.
 *
 * Issue a connect request using ISH transport service and register
 * a RX callback.
 *
 * Return: 0 for succcess or error code for failure.
 */
static int ishtp_cl_tty_connect(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl *cl;
	struct ishtp_fw_client *fw_client;
	struct ishtp_cl_info *cl_info;
	int ret;

	cl_info = kzalloc(sizeof(*cl_info), GFP_KERNEL);
	if (!cl_info)
		return -ENOMEM;

	cl_info->max_msg_size = cl_device->fw_client->props.max_msg_length;

	cl_info->msg_buffer = kzalloc(sizeof(struct ishtp_tty_msg) +
				      cl_info->max_msg_size, GFP_KERNEL);
	if (!cl_info->msg_buffer) {
		ret = -ENOMEM;
		goto out_cl_info_free;
	}

	cl = ishtp_cl_allocate(cl_device->ishtp_dev);
	if (!cl) {
		ret = -ENOMEM;
		goto out_msg_buff_free;
	}

	ret = ishtp_cl_link(cl, ISHTP_HOST_CLIENT_ID_ANY);
	if (ret)
		goto out_client_free;


	fw_client = ishtp_fw_cl_get_client(cl->dev, &tty_ishtp_guid);
	if (!fw_client) {
		ret = -ENOENT;
		goto out_client_free;
	}

	cl->fw_client_id = fw_client->client_id;
	cl->state = ISHTP_CL_CONNECTING;
	cl->rx_ring_size = TTY_CL_RX_RING_SIZE;
	cl->tx_ring_size = TTY_CL_TX_RING_SIZE;
	ret = ishtp_cl_connect(cl);
	if (ret) {
		dev_err(&cl_device->dev, "client connect failed\n");
		goto out_client_free;
	}

	cl_info->cl_device = cl_device;
	cl_info->ishtp_cl = cl;

	init_waitqueue_head(&cl_info->ishtp_response_wait);
	init_waitqueue_head(&cl_info->ishtp_send_wait);
	cl_info->max_ring_buffer_bytes = ishtp_cl_get_tx_free_buffer_size(cl);

	ishtp_set_drvdata(cl_device, cl_info);

	/* Register read callback */
	ishtp_register_event_cb(cl_device, tty_ishtp_cl_event_cb);

       ishtp_get_device(cl_device);

	return 0;

out_client_free:
	ishtp_cl_free(cl);
out_msg_buff_free:
	kfree(cl_info->msg_buffer);
out_cl_info_free:
	kfree(cl_info);

	return ret;
}

/**
 * ishtp_cl_tty_disconnect() - Disonnect to ISH UART driver client
 * @cl_device:	client device for which data received.
 *
 * Issue a disconnect request and cleanup queues.
 *
 * Return: None.
 */
static void ishtp_cl_tty_disconnect(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_info *cl_info = ishtp_get_drvdata(cl_device);
	struct ishtp_cl *cl = cl_info->ishtp_cl;

	cl->state = ISHTP_CL_DISCONNECTING;
	ishtp_cl_disconnect(cl);
	ishtp_put_device(cl_device);
	ishtp_cl_unlink(cl);
	ishtp_cl_flush_queues(cl);
	ishtp_cl_free(cl);
	kfree(cl_info->msg_buffer);
	kfree(cl_info);
}

/**
 * ish_tty_get_bootup_termios() - Get ISH UART default baud rate
 * @cl_info:	client instance to probe
 * @cl_tty:	A place holder to get the received parameters.
 *
 * Issue command to get UART configuration during driver init.
 * The output can be used to set the default baud rate during
 * tty driver register.
 *
 * Return: None.
 */
static void ish_tty_get_bootup_termios(struct ishtp_cl_info *cl_info,
				       struct ishtp_cl_tty *cl_tty)
{
	struct ishtp_tty_msg *ishtp_msg;

	ishtp_msg = (struct ishtp_tty_msg *)cl_info->msg_buffer;

	ishtp_msg->command = UART_GET_CONFIG;
	ishtp_msg->size = 0;

	cl_tty->baud = 0;

	cl_info->ishtp_cl->client_data = cl_tty;
	cl_info->get_report_done = false;
	ishtp_cl_send(cl_info->ishtp_cl, cl_info->msg_buffer,
		      sizeof(struct ishtp_tty_msg));
	ishtp_wait_for_response(cl_info);
	cl_info->ishtp_cl->client_data = NULL;
}

static struct tty_operations ish_tty_ops = {
	.install = ish_tty_install,
	.open = ish_tty_open,
	.close = ish_tty_close,
	.write = ish_tty_write,
	.set_termios	= ish_tty_set_termios,
	.write_room = ish_tty_write_room,
	.chars_in_buffer = ish_tty_chars_in_buffer,
	.wait_until_sent = ish_tty_wait_until_sent,
};

static struct tty_driver *ish_tty_driver;

/**
 * ishtp_cl_tty_init() - TTY driver init
 * @cl_device:	client device for which data received.
 *
 * Register a tty driver and device for ISH UART.
 *
 * Return: 0 for success of error code for failure.
 */
static int ishtp_cl_tty_init(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_info *cl_info = ishtp_get_drvdata(cl_device);
	static struct ktermios termios;
	struct ishtp_cl_tty cl_tty;
	struct device *dev;
	int ret;

	ret = ishtp_cl_tty_connect(cl_device);
	if (ret)
		return ret;

	termios.c_cflag = CS8 | HUPCL | CLOCAL;
	ish_tty_get_bootup_termios(cl_info, &cl_tty);
	if (cl_tty.baud)
		tty_termios_encode_baud_rate(&termios, cl_tty.baud,
					     cl_tty.baud);
	else
		termios.c_cflag |= B115200;

	ish_tty_driver = alloc_tty_driver(1);
	if (!ish_tty_driver) {
		ret = -ENOMEM;
		goto disconnect_tty_driver;
	}

	ish_tty_driver->owner = THIS_MODULE;
	ish_tty_driver->driver_name = "ish-serial";
	ish_tty_driver->name = "ttyISH";
	ish_tty_driver->minor_start = 0;
	ish_tty_driver->major = 0;
	ish_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	ish_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	ish_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;

	ish_tty_driver->init_termios = tty_std_termios;
	ish_tty_driver->init_termios.c_cflag = termios.c_cflag;
	ish_tty_driver->init_termios.c_lflag = ISIG | ICANON | IEXTEN;
	tty_set_operations(ish_tty_driver, &ish_tty_ops);

	ret = tty_register_driver(ish_tty_driver);
	if (ret) {
		goto free_tty_driver;
	}

	dev = tty_register_device(ish_tty_driver, 0, &cl_device->dev);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto unreg_tty_driver;
	}

	return 0;

unreg_tty_driver:
	tty_unregister_driver(ish_tty_driver);
free_tty_driver:
	put_tty_driver(ish_tty_driver);
disconnect_tty_driver:
	ishtp_cl_tty_disconnect(cl_device);

	return ret;
}

/**
 * ishtp_cl_tty_deinit() - TTY driver deinit
 * @cl_device:	client device for which data received.
 *
 * Unregister a tty driver and device for ISH UART.
 *
 * Return: None.
 */
static void ishtp_cl_tty_deinit(struct ishtp_cl_device *cl_device)
{
	tty_unregister_device(ish_tty_driver, 0);
	tty_unregister_driver(ish_tty_driver);
	put_tty_driver(ish_tty_driver);
	ishtp_cl_tty_disconnect(cl_device);
	ishtp_set_drvdata(cl_device, NULL);
}

/**
 * ishtp_cl_tty_probe() - ISH client driver probe
 * @cl_device:	client device for which data received.
 *
 * Once ISH core identifies a FW client this probe is called to
 * bind the handler by matching GUID.
 *
 * Return: 0 for success or error code.
 */
static int ishtp_cl_tty_probe(struct ishtp_cl_device *cl_device)
{
	int ret;

	if (!cl_device)
		return -ENODEV;

	if (uuid_le_cmp(tty_ishtp_guid,
			cl_device->fw_client->props.protocol_name) != 0)
		return	-ENODEV;

	ret = ishtp_cl_tty_init(cl_device);
	if (ret)
		return ret;

	return 0;
}

/**
 * ishtp_cl_tty_remove() - ISH client driver remove
 * @cl_device:	client device for which data received.
 *
 * Called during client drive removal to clean up.
 *
 * Return: 0 and no error code.
 */
static int ishtp_cl_tty_remove(struct ishtp_cl_device *cl_device)
{
	ishtp_cl_tty_deinit(cl_device);

	return 0;
}

static struct ishtp_cl_driver ishtp_cl_tty_driver = {
	.name = "ishtp-client",
	.probe = ishtp_cl_tty_probe,
	.remove = ishtp_cl_tty_remove,
};

static int __init ishtp_tty_client_init(void)
{
	return ishtp_cl_driver_register(&ishtp_cl_tty_driver);
}
module_init(ishtp_tty_client_init);

static void __exit ishtp_tty_client_exit(void)
{
	ishtp_cl_driver_unregister(&ishtp_cl_tty_driver);
}
module_exit(ishtp_tty_client_exit);

MODULE_DESCRIPTION("ISH ISHTP TTY client driver");
MODULE_AUTHOR("Even Xu <even.xu@intel.com>");
MODULE_AUTHOR("Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("ishtp:*");
