/* Keytable for NVIDIA Remote Controller
 *
 * Copyright (c) 2014, NVIDIA Coporation. All rights reserved.
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
	{ 0x10009, KEY_0 },
	{ 0x10000, KEY_1 },
	{ 0x10001, KEY_2 },
	{ 0x10002, KEY_3 },
	{ 0x10003, KEY_4 },
	{ 0x10004, KEY_5 },
	{ 0x10005, KEY_6 },
	{ 0x10006, KEY_7 },
	{ 0x10007, KEY_8 },
	{ 0x10008, KEY_9 },
	{ 0x10012, KEY_VOLUMEUP },
	{ 0x10013, KEY_VOLUMEDOWN },
	{ 0x10010, KEY_CHANNELUP },
	{ 0x10011, KEY_CHANNELDOWN },
	{ 0x10074, KEY_UP },
	{ 0x10075, KEY_DOWN },
	{ 0x10034, KEY_LEFT },
	{ 0x10033, KEY_RIGHT },
	{ 0x10060, KEY_HOME },
};

static struct rc_map_list nvidia_map = {
	.map = {
			.scan = foster_table,
			.size = ARRAY_SIZE(foster_table),
			.rc_type = RC_BIT_SONY12,
			.name = RC_MAP_NVIDIA,
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
MODULE_AUTHOR("Jun Yan <juyan@nvidia.com>");
