#ifndef __TDF8532_H_
#define __TDF8532_H_

#define ACK_TIMEOUT 10000

#define CHNL_MAX 5

#define AMP_MOD 0x80
#define END -1

#define MSG_TYPE_STX 0x02
#define MSG_TYPE_NAK 0x15
#define MSG_TYPE_ACK 0x6

#define HEADER_SIZE 3
#define HEADER_TYPE 0
#define HEADER_PKTID 1
#define HEADER_LEN 2

/* Set commands */
#define SET_CLK_STATE 0x1A
#define CLK_DISCONNECT 0x00
#define CLK_CONNECT 0x01

#define SET_CHNL_ENABLE 0x26
#define SET_CHNL_DISABLE 0x27

#define SET_CHNL_MUTE 0x42
#define SET_CHNL_UNMUTE 0x43

/* Helpers */
#define CHNL_MASK(channels) (u8)((0x00FF << channels) >> 8)

#define tdf8532_amp_write(dev_data, ...)\
	__tdf8532_single_write(dev_data, 0, AMP_MOD, __VA_ARGS__, END)

struct tdf8532_priv {
	struct i2c_client *i2c;
	u8 channels;
	u8 pkt_id;
};

#endif
