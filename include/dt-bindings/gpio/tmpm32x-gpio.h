/*
 * This header provides constants for binding nvidia,tegra*-gpio.
 *
 * The first cell in Tegra's GPIO specifier is the GPIO ID. The macros below
 * provide names for this.
 *
 * The second cell contains standard flag values specified in gpio.h.
 */

#ifndef _DT_BINDINGS_GPIO_TMPM32X_GPIO_H
#define _DT_BINDINGS_GPIO_TMPM32X_GPIO_H

#include <dt-bindings/gpio/gpio.h>

#define TMPM32X_GPIO_BANK_ID_A 0
#define TMPM32X_GPIO_BANK_ID_B 1
#define TMPM32X_GPIO_BANK_ID_C 2
#define TMPM32X_GPIO_BANK_ID_D 3
#define TMPM32X_GPIO_BANK_ID_E 4
#define TMPM32X_GPIO_BANK_ID_F 5
#define TMPM32X_GPIO_BANK_ID_G 6
#define TMPM32X_GPIO_BANK_ID_H 7
#define TMPM32X_GPIO_BANK_ID_I 8
#define TMPM32X_GPIO_BANK_ID_J 9
#define TMPM32X_GPIO_BANK_ID_K 10
#define TMPM32X_GPIO_BANK_ID_L 11
#define TMPM32X_GPIO_BANK_ID_M 12
#define TMPM32X_GPIO_BANK_ID_N 13

#define TMPM32X_GPIO(bank, offset) \
	((TMPM32X_GPIO_BANK_ID_##bank * 8) + offset)

#endif
