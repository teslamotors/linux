/* This header provides constants for binding ti,ina3221x */

#ifndef _DT_BINDINGS_IIO_METER_INA3221X_H
#define _DT_BINDINGS_IIO_METER_INA3221X_H

/* Channel numbers */
#define INA3221_CHANNEL0	0
#define INA3221_CHANNEL1	1
#define INA3221_CHANNEL2	2

/* Monitor type */
#define INA3221_VOLTAGE		0
#define INA3221_CURRENT		1
#define INA3221_POWER		3

/* Measurement technique normal/trigger */
#define INA3221_NORMAL		0
#define INA3221_TRIGGER		1

/* To get the iio spec index */
#define INA3221_CHAN_INDEX(chan, type, add)	\
	(chan * 5 + INA3221_##type + INA3221_##add)

#endif
