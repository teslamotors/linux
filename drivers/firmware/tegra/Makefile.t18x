# Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

ccflags-y			+= -Idrivers/firmware/tegra
ccflags-y			+= -I../t18x/include
subdir-ccflags-y		+= -Werror

obj-y				+= mail_t186.o
obj-y				+= mail_native_ivc.o
obj-$(CONFIG_TEGRA_HV_MANAGER)	+= bpmp_virt.o
obj-$(CONFIG_TEGRA_HV_MANAGER)	+= mail_virt_ivc.o
