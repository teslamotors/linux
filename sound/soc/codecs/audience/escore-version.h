/*
 * escore-version.h  --  Audience earSmart Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ESCORE_VERSION_H
#define _ESCORE_VERSION_H

#if defined(CONFIG_SND_SOC_ES704_ESCORE) || defined(CONFIG_SND_SOC_ES705_ESCORE)
#define ESCORE_VERSION "ESCORE_ES70X_1_4_0"
#elif defined(CONFIG_SND_SOC_ES755)
#define ESCORE_VERSION "ESCORE_ES75X_1_5_0"
#elif defined(CONFIG_SND_SOC_ES804_ESCORE)
#define ESCORE_VERSION "ESCORE_ES80X_1_4_0"
#elif defined(CONFIG_SND_SOC_ES854) || defined(CONFIG_SND_SOC_ES857)
#define ESCORE_VERSION "ESCORE_ES85X_1_4_0"
#endif

#endif /* _ESCORE_VERSION_H */
