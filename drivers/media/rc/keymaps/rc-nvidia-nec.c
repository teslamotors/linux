/* Keytable for NVIDIA Remote Controller
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <media/rc-map.h>
#include <linux/module.h>

static struct rc_map_table foster_table[] = {
	{ 0x807e12, KEY_VOLUMEUP },
	{ 0x807e15, KEY_VOLUMEDOWN },
	{ 0x807e0c, KEY_UP },
	{ 0x807e0e, KEY_DOWN },
	{ 0x807e0b, KEY_LEFT },
	{ 0x807e0d, KEY_RIGHT },
	{ 0x807e09, KEY_HOMEPAGE },
	{ 0x807e06, KEY_POWER },
	{ 0x807e03, KEY_SELECT },
	{ 0x807e02, KEY_BACK },
	{ 0x807e14, KEY_MUTE },
	{ 0x807e20, KEY_PLAYPAUSE },
	{ 0x807e11, KEY_PLAYCD },
	{ 0x807e08, KEY_PAUSECD },
	{ 0x807e07, KEY_STOP },
	{ 0x807e0f, KEY_FASTFORWARD },
	{ 0x807e0a, KEY_REWIND },
	{ 0x807e41, KEY_SLEEP },
	{ 0x807e45, KEY_WAKEUP },
};

static struct rc_map_list nvidia_map = {
	.map = {
			.scan = foster_table,
			.size = ARRAY_SIZE(foster_table),
			.rc_type = RC_BIT_NEC,
			.name = RC_MAP_NVIDIA_NEC,
	}
};

static int __init init_rc_map_nvidia(void)
{
	return rc_map_register(&nvidia_map);
}

static void __exit exit_rc_map_nvidia(void)
{
	rc_map_unregister(&nvidia_map);
}

module_init(init_rc_map_nvidia);
module_exit(exit_rc_map_nvidia);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Fu <danifu@nvidia.com>");
