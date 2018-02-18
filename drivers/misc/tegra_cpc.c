/*
 * tegra_cpc.c - Access CPC storage blocks through i2c bus
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/completion.h>

/* misc device */
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <asm/unaligned.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/tegra_cpc.h>

#define CPC_I2C_DEADLINE_MS 1000
#define TEGRA_CPC_DEBUG 0

/*
	Helper macro section
*/

/* returns 0 on a successful copy. Returns -ENOMEM otherwise */
inline int tegra_cpc_serialize_field_n(u8 **dest, u8 *source, size_t len,
	size_t *rem_buf_size)
{
	if (unlikely(*rem_buf_size < len))
		return -ENOMEM;
	memcpy(*dest, source, len);
	(*rem_buf_size) -= len;
	(*dest) = &((*dest)[len]);
	return 0;
}

#define TEGRA_CPC_SERIALIZE_FIELD(dest, source, rem_buf_len) \
	tegra_cpc_serialize_field_n(dest, (u8 *) &source, sizeof(source), \
		rem_buf_len)

/* returns 0 on a successful copy. Returns -ENOMEM otherwise */
inline int tegra_cpc_deserialize_field_n(u8 *dest, u8 **source, size_t len,
	size_t *rem_buf_size)
{
	if (unlikely(*rem_buf_size < len))
		return -ENOMEM;
	memcpy(dest, *source, len);
	(*rem_buf_size) -= len;
	(*source) = &((*source)[len]);
	return 0;
}

#define TEGRA_CPC_DESERIALIZE_FIELD(dest, source, rem_buf_len) \
	tegra_cpc_deserialize_field_n((u8 *) &dest, source, sizeof(dest), \
		rem_buf_len)

#define TEGRA_CPC_COPY_TO_VAR(var, source) \
	memcpy(&var, source, sizeof(source))

inline int tegra_cpc_verify_fr_len(u8 length, struct i2c_client *i2c_client)
{
	if (unlikely((length == 0) ||
		(length > CPC_MAX_DATA_SIZE))) {
		dev_err(&i2c_client->dev,
			"The requested size is invalid: %u",
			(unsigned int) length);
		return -EINVAL;
	}
	return 0;
}

/*
	Local data structure section
*/
struct cpc_i2c_host {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	struct i2c_msg readback;

	int irq;
	struct mutex lock;
	struct completion complete;
};

/* There will be at most one uC instance per board */
static atomic_t cpc_in_use;
static struct cpc_i2c_host *cpc_host;

/*
   Serialization and deserialization logic

   serialize functions assume buffer is enough to hold the entire fr, and
   that the content of fr has been verified already

   deserialize function assumes that the buffer is enough to hold the fr
*/

/* read counter */
static int cpc_serialize_read_counter(struct tegra_cpc_frame *fr, u8 buffer[],
	size_t buf_size) {

	size_t rem_buf_size = buf_size;
	struct tegra_cpc_read_counter_data *fr_cmd = &fr->cmd_data.read_counter;

	if (TEGRA_CPC_SERIALIZE_FIELD(&buffer, fr->req, &rem_buf_size) ||
		TEGRA_CPC_SERIALIZE_FIELD(&buffer, fr_cmd->nonce,
			&rem_buf_size) ||
		TEGRA_CPC_SERIALIZE_FIELD(&buffer, fr_cmd->derivation_value,
			&rem_buf_size))
		return -ENOMEM;

	return buf_size - rem_buf_size;
}

static inline size_t cpc_deserialize_size_read_counter(
		struct tegra_cpc_frame *fr) {
	struct tegra_cpc_read_counter_data *fr_cmd = &fr->cmd_data.read_counter;
	return sizeof(fr_cmd->hmac) + sizeof(fr_cmd->write_counter);
}

static int cpc_deserialize_read_counter(struct tegra_cpc_frame *fr, u8 buffer[],
		size_t rem_buf_size) {
	struct tegra_cpc_read_counter_data *fr_cmd = &fr->cmd_data.read_counter;

	if (TEGRA_CPC_DESERIALIZE_FIELD(fr_cmd->hmac, &buffer, &rem_buf_size) ||
		TEGRA_CPC_DESERIALIZE_FIELD(fr_cmd->write_counter, &buffer,
			&rem_buf_size))
		return -ENOMEM;
	return 0;
}

/* write frame */
static int cpc_serialize_write_frame(struct tegra_cpc_frame *fr, u8 buffer[],
		size_t buf_size) {

	size_t rem_buf_size = buf_size;
	struct tegra_cpc_write_frame_data *fr_cmd = &fr->cmd_data.write_frame;
	size_t oneshot_copy_size = sizeof(fr_cmd->nonce) +
		sizeof(fr_cmd->write_counter) + fr_cmd->length;

	if (TEGRA_CPC_SERIALIZE_FIELD(&buffer, fr->req, &rem_buf_size) ||
		TEGRA_CPC_SERIALIZE_FIELD(&buffer, fr_cmd->hmac,
			&rem_buf_size) ||
		TEGRA_CPC_SERIALIZE_FIELD(&buffer, fr_cmd->length,
			&rem_buf_size))
		return -ENOMEM;

	if (rem_buf_size < oneshot_copy_size)
		return -ENOMEM;

	rem_buf_size -= oneshot_copy_size;

	memcpy(buffer, &fr_cmd->nonce, oneshot_copy_size);
	return buf_size - rem_buf_size;
}

/* read frame */
static int cpc_serialize_read_frame(struct tegra_cpc_frame *fr, u8 buffer[],
		size_t buf_size) {
	size_t rem_buf_size = buf_size;
	struct tegra_cpc_read_frame_data *fr_cmd = &fr->cmd_data.read_frame;

	if (TEGRA_CPC_SERIALIZE_FIELD(&buffer, fr->req, &rem_buf_size) ||
		TEGRA_CPC_SERIALIZE_FIELD(&buffer, fr_cmd->nonce,
			&rem_buf_size) ||
		TEGRA_CPC_SERIALIZE_FIELD(&buffer, fr_cmd->length,
			&rem_buf_size))
		return -ENOMEM;

	return buf_size - rem_buf_size;
}

static inline size_t cpc_deserialize_size_read_frame(
		struct tegra_cpc_frame *fr) {
	struct tegra_cpc_read_frame_data *fr_cmd = &fr->cmd_data.read_frame;
	return sizeof(fr_cmd->hmac) + fr_cmd->length;
}

static int cpc_deserialize_read_frame(struct tegra_cpc_frame *fr, u8 buffer[],
		size_t rem_buf_size) {
	struct tegra_cpc_read_frame_data *fr_cmd = &fr->cmd_data.read_frame;

	if (TEGRA_CPC_DESERIALIZE_FIELD(fr_cmd->hmac, &buffer, &rem_buf_size) ||
		tegra_cpc_deserialize_field_n(fr_cmd->data, &buffer,
			fr_cmd->length, &rem_buf_size))
		return -ENOMEM;
	return 0;
}

/* Get Status */
static int cpc_serialize_get_status(struct tegra_cpc_frame *fr, u8 buffer[],
		size_t buf_size) {
	size_t rem_buf_size = buf_size;

	/* get status can run without any data from the user space */
	u8 req = CPC_GET_RESULT;
	if (TEGRA_CPC_SERIALIZE_FIELD(&buffer, req, &rem_buf_size))
		return -ENOMEM;
	return buf_size - rem_buf_size;
}

static inline size_t cpc_deserialize_size_get_status(
		struct tegra_cpc_frame *fr) {
	return sizeof(fr->hmac) + sizeof(fr->result) +
		sizeof(fr->nonce);
}

static int cpc_deserialize_get_status(struct tegra_cpc_frame *fr, u8 buffer[],
		size_t rem_buf_size) {
	if (TEGRA_CPC_DESERIALIZE_FIELD(fr->hmac, &buffer, &rem_buf_size) ||
		TEGRA_CPC_DESERIALIZE_FIELD(fr->result, &buffer,
			&rem_buf_size) ||
		TEGRA_CPC_DESERIALIZE_FIELD(fr->nonce, &buffer, &rem_buf_size))
		return -ENOMEM;
	return 0;
}

/* Get Version */
static int cpc_serialize_get_version(struct tegra_cpc_frame *fr, u8 buffer[],
		size_t buf_size) {
	size_t rem_buf_size = buf_size;
	struct tegra_cpc_get_version_data *fr_cmd = &fr->cmd_data.get_version;
	u8 req = CPC_GET_VERSION;
	if (TEGRA_CPC_SERIALIZE_FIELD(&buffer, req, &rem_buf_size) ||
		TEGRA_CPC_SERIALIZE_FIELD(&buffer, fr_cmd->nonce,
		&rem_buf_size))
		return -ENOMEM;
	return buf_size - rem_buf_size;
}

static inline size_t cpc_deserialize_size_get_version(
		struct tegra_cpc_frame *fr) {
	struct tegra_cpc_get_version_data *fr_cmd = &fr->cmd_data.get_version;
	return sizeof(fr_cmd->major_ver) + sizeof(fr_cmd->minor_ver);
}

static int cpc_deserialize_get_version(struct tegra_cpc_frame *fr, u8 buffer[],
		size_t rem_buf_size) {
	struct tegra_cpc_get_version_data *fr_cmd = &fr->cmd_data.get_version;
	if (TEGRA_CPC_DESERIALIZE_FIELD(fr_cmd->minor_ver, &buffer,
		&rem_buf_size) ||
		TEGRA_CPC_DESERIALIZE_FIELD(fr_cmd->major_ver, &buffer,
			&rem_buf_size))
		return -ENOMEM;
	return 0;
}

/*
   I2C related section
 */
static int cpc_send_i2c_msg(struct i2c_client *client, struct i2c_msg *msgs,
				unsigned int num_msg)
{
	int err;
	err = i2c_transfer(client->adapter, msgs, num_msg);
	if (unlikely(err != num_msg)) {
		if (err < 0)
			dev_err(&client->dev,
				"%s: i2c transfer failed. Error: %d\n",
				__func__, err);

		if (err >= 0)
			dev_err(&client->dev,
				"%s: i2c transfer failed. Transferred %d/%u\n",
				__func__, err, num_msg);
		return -EINVAL;
	}
	return 0;
}

/*
   Issues a given read command and perform deserialization of the read data

   Assumes the caller has performed all error checks on fr
 */
static int cpc_read_data(struct i2c_client *client, struct tegra_cpc_frame *fr,
	int (*cpc_serialization_strategy)
		(struct tegra_cpc_frame *fr, u8 buffer[], size_t buf_size),
	int expected_resp_len,
	int (*cpc_deserialization_strategy)
		(struct tegra_cpc_frame *fr, u8 buffer[], size_t buf_size))
{
	struct i2c_msg msg[2];
	int ret = 0;
	int packet_len;

	/*
	   The read command will not send any data. The uC may send more data
	   then length bytes, if HMAC is involved

	   The buffer for reading the data back has to be big enough to hold
	   the maximum value which can be expressed by the size of cpc_data_len
	*/
	u8 buffer_req[sizeof(struct tegra_cpc_frame)];
	u8 buffer_resp[sizeof(struct tegra_cpc_frame)];

	if (unlikely(!client->adapter))
		return -ENODEV;

	/*
	   First put header of CPC read command. Start with the write mode flag,
	   to communicate which command we are sending. Then read the response
	   on the bus
	 */

	packet_len = cpc_serialization_strategy(fr, buffer_req,
		sizeof(buffer_req));
	if (unlikely(packet_len <= 0)) {
		dev_err(&client->dev, "Serialization failure: %d\n",
			packet_len);
		return -EINVAL;
	}

	msg[0].addr = client->addr;
	msg[0].len = (__u16) packet_len;
	msg[0].flags = 0;
	msg[0].buf = buffer_req;
	/*
	   The second i2c packet is empty, as it is only a buffer space for
	   reading the requested data back
	*/
	msg[1].addr = client->addr;
	msg[1].len = expected_resp_len;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = buffer_resp;
	ret = cpc_send_i2c_msg(client, msg,
		sizeof(msg) / sizeof(struct i2c_msg));
	if (unlikely(ret))
		return ret;

	ret = cpc_deserialization_strategy(fr, buffer_resp, expected_resp_len);
	return ret;
}

/*
 * Executes a write command.
 */
static int cpc_write_data(struct i2c_client *client, struct tegra_cpc_frame *fr,
	int (*cpc_serialization_strategy)
		(struct tegra_cpc_frame *fr, u8 buffer[], size_t buf_size)) {
	struct i2c_msg msg[1];
	int packet_len;
	u8 buffer_req[sizeof(struct tegra_cpc_frame)];

	if (unlikely(!client->adapter))
		return -ENODEV;

	packet_len = cpc_serialization_strategy(fr, buffer_req,
		sizeof(buffer_req));
	if (unlikely(packet_len <= 0)) {
		dev_err(&client->dev, "Serialization failure: %d\n",
			packet_len);
		return -EINVAL;
	}
	msg[0].addr = client->addr;
	msg[0].len = (__u16) packet_len;
	msg[0].flags = 0;
	msg[0].buf = buffer_req;

	return cpc_send_i2c_msg(client, msg,
		sizeof(msg) / sizeof(struct i2c_msg));
}

/*
	Command from user space handling section
*/
/* Error checks against the content of data struct must be done here */
static int _cpc_do_data(struct cpc_i2c_host *cpc, struct tegra_cpc_frame *fr)
{
	int err;
	u32 req_nonce;
	u32 resp_nonce;

	mutex_lock(&cpc->lock);
	fr->result = CPC_RESULT_UNKNOWN_REQUEST;

	/* Each command's input error checked here */
	switch (fr->req) {
	case CPC_READ_M_COUNT:
		TEGRA_CPC_COPY_TO_VAR(req_nonce,
			fr->cmd_data.read_counter.nonce);
		err = cpc_read_data(cpc->i2c_client, fr,
			cpc_serialize_read_counter,
			cpc_deserialize_size_read_counter(fr),
			cpc_deserialize_read_counter);
		break;

	case CPC_READ_FRAME:
		if (tegra_cpc_verify_fr_len(fr->cmd_data.read_frame.length,
				cpc->i2c_client)) {
			fr->result = CPC_RESULT_DATA_LENGTH_MISMATCH;
			err = -EINVAL;
			goto fail;
		}
		TEGRA_CPC_COPY_TO_VAR(req_nonce, fr->cmd_data.read_frame.nonce);
		err = cpc_read_data(cpc->i2c_client, fr,
			cpc_serialize_read_frame,
			cpc_deserialize_size_read_frame(fr),
			cpc_deserialize_read_frame);
		break;

	case CPC_WRITE_FRAME:
		if (tegra_cpc_verify_fr_len(fr->cmd_data.write_frame.length,
				cpc->i2c_client)) {
			fr->result = CPC_RESULT_DATA_LENGTH_MISMATCH;
			err = -EINVAL;
			goto fail;
		}
		init_completion(&cpc->complete);
		TEGRA_CPC_COPY_TO_VAR(req_nonce,
			fr->cmd_data.write_frame.nonce);
		err = cpc_write_data(cpc->i2c_client, fr,
				cpc_serialize_write_frame);

		if (!err && !wait_for_completion_timeout(&cpc->complete,
				msecs_to_jiffies(CPC_I2C_DEADLINE_MS))) {
			dev_err(&cpc->i2c_client->dev,
				"%s timeout on write complete\n", __func__);
			fr->result = CPC_RESULT_WRITE_FAILURE;
			err = -ETIMEDOUT;
		}
		break;

	case CPC_GET_VERSION:
		TEGRA_CPC_COPY_TO_VAR(req_nonce,
			fr->cmd_data.get_version.nonce);
		err = cpc_read_data(cpc->i2c_client, fr,
			cpc_serialize_get_version,
			cpc_deserialize_size_get_version(fr),
			cpc_deserialize_get_version);
		break;

	default:
		err = -EINVAL;
		dev_err(&cpc->i2c_client->dev,
			"unsupported CPC command id %08x\n", fr->req);
	}

	if (err)
		goto fail;

	/* get status should run only when a command was ran successfully */
	err = cpc_read_data(cpc->i2c_client, fr,
		cpc_serialize_get_status,
		cpc_deserialize_size_get_status(fr),
		cpc_deserialize_get_status);

	if (unlikely(err)) {
		dev_err(&cpc->i2c_client->dev,
			"failed to run get status command: %d\n", err);
		fr->result = CPC_RESULT_UNEXPECTED_CONDITION;
		goto fail;
	}

	/*
	  The request was sent to uC, and uC responded.
	  Make sure the response is for the request sent.
	*/
	TEGRA_CPC_COPY_TO_VAR(resp_nonce, fr->nonce);
	if (unlikely(req_nonce != resp_nonce)) {
		dev_err(&cpc->i2c_client->dev,
			"Request nonce (%08x) mismatching response nonce (%08x)\n",
			req_nonce, resp_nonce);
		err = -EINVAL;
		fr->result = CPC_RESULT_UNEXPECTED_CONDITION;
	}

fail:
	mutex_unlock(&cpc->lock);
	return err;
}

static int cpc_open(struct inode *inode, struct file *filp)
{
	if (atomic_xchg(&cpc_in_use, 1))
		return -EBUSY;

	if (!cpc_host) {
		pr_info("CPC is not ready\n");
		return -EBADF;
	}

	filp->private_data = cpc_host;

	return nonseekable_open(inode, filp);
}

static int cpc_release(struct inode *inode, struct file *filp)
{
	WARN_ON(!atomic_xchg(&cpc_in_use, 0));
	return 0;
}

static long cpc_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct cpc_i2c_host *cpc = file->private_data;
	struct tegra_cpc_frame *cpc_fr = NULL;
	long err = 0;

	if (_IOC_TYPE(cmd) != NVCPC_IOC_MAGIC) {
		err = -ENOTTY;
		goto fail;
	}

	if (_IOC_NR(cmd) > NVCPC_IOCTL_DO_IO) {
		err = -ENOTTY;
		goto fail;
	}

	switch (cmd) {
	case NVCPC_IOCTL_DO_IO:
		cpc_fr = kmalloc(sizeof(struct tegra_cpc_frame), GFP_KERNEL);
		if (!cpc_fr) {
			pr_err("%s: failed to allocate memory\n", __func__);
			err = -ENOMEM;
			goto fail;
		}

		if (copy_from_user(cpc_fr, (const void __user *)arg,
					sizeof(struct tegra_cpc_frame))) {
			err = -EFAULT;
			goto fail;
		}

		err = _cpc_do_data(cpc, cpc_fr);

		if (copy_to_user((void __user *)arg, cpc_fr,
					sizeof(struct tegra_cpc_frame))) {
			err = -EFAULT;
			goto fail;
		}
		break;
	default:
		pr_err("CPC: unsupported ioctl\n");
		err =  -EINVAL;
		goto fail;
	}

fail:
	kfree(cpc_fr);
	return err;
}

/*
	IRQ handling section
*/
static irqreturn_t cpc_irq(int irq, void *data)
{
	struct cpc_i2c_host *cpc = data;
	complete(&cpc->complete);
	return IRQ_HANDLED;
}

/*
	Driver setup section
*/
static const struct file_operations cpc_dev_fops = {
	.owner = THIS_MODULE,
	.open = cpc_open,
	.release = cpc_release,
	.unlocked_ioctl = cpc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cpc_ioctl,
#endif
};

static struct miscdevice cpc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tegra_cpc",
	.fops = &cpc_dev_fops,
};

static int cpc_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err = 0;

	/* Initialize stack variables and structures */
	cpc_host = kzalloc(sizeof(struct cpc_i2c_host), GFP_KERNEL);
	if (!cpc_host) {
		pr_err("unable to allocate memory\n");
		return -ENOMEM;
	}

	/* Setup I2C */
	cpc_host->i2c_client = client;
	i2c_set_clientdata(client, cpc_host);
	cpc_host->irq = client->irq;
	mutex_init(&cpc_host->lock);

	/* Setup IRQ */
	if (cpc_host->irq < 0) {
		dev_err(&client->dev,
			"Unable to enable IRQ\n");
		err = -EINVAL;
		goto pre_irq_fail;
	} else {
		/*
		   In case the IRQ fires before complete is initialized,
		   initialize complete now and over write later

		   Another option is to disable IRQ until the host is
		   ready to handle the IRQ, but that causes recurring burden
		   of re-enabling and re-disabling IRQ
		*/
		init_completion(&cpc_host->complete);
		err = request_threaded_irq(cpc_host->irq, cpc_irq, NULL,
					IRQF_TRIGGER_FALLING,
					dev_name(&client->dev), cpc_host);
		if (err) {
			dev_err(&client->dev, "%s: request irq failed %d\n",
				__func__, err);
			err = -EINVAL;
			goto pre_irq_fail;
		}
	}


	/* TODO: check for CP DRM controller err. Make sure it's ready
	 */

	/* setup misc device for userspace io */
	if (misc_register(&cpc_dev)) {
		pr_err("%s: misc device register failed\n", __func__);
		err = -ENOMEM;
		goto fail;
	}

	dev_info(&client->dev, "host driver for CPC\n");
	return 0;

fail:
	free_irq(cpc_host->irq, cpc_host);
pre_irq_fail:
	dev_set_drvdata(&client->dev, NULL);
	mutex_destroy(&cpc_host->lock);
	kfree(cpc_host);
	return err;
}

static int cpc_i2c_remove(struct i2c_client *client)
{
	struct cpc_i2c_host *cpc_host = dev_get_drvdata(&client->dev);

	mutex_destroy(&cpc_host->lock);
	free_irq(cpc_host->irq, cpc_host);
	kfree(cpc_host);
	cpc_host = NULL;
	return 0;
}

static struct of_device_id cpc_i2c_of_match_table[] = {
	{ .compatible = "nvidia,cpc", },
	{},
};

static const struct i2c_device_id cpc_i2c_id[] = {
	{ "cpc", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mmc_i2c_id);

static struct i2c_driver cpc_i2c_driver = {
	.driver = {
		.name = "cpc",
		.owner = THIS_MODULE,
		.of_match_table = cpc_i2c_of_match_table,
	},
	.probe = cpc_i2c_probe,
	.remove = cpc_i2c_remove,
	.id_table = cpc_i2c_id,
};

module_i2c_driver(cpc_i2c_driver);

MODULE_AUTHOR("Sang-Hun Lee");
MODULE_DESCRIPTION("Content protection misc driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:cpc_i2c");
