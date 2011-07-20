#
# arch/arm/mach-ambarella/Makefile
#
# History:
#	2006/12/18 - [Charles Chiou] created file
#
# Copyright (C) 2004-2009, Ambarella, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#

obj-$(CONFIG_MACH_COCONUT)	+= init-coconut.o
obj-$(CONFIG_MACH_DURIAN)	+= init-durian.o
obj-$(CONFIG_MACH_ELEPHANT)	+= init-elephant.o

ifeq ($(CONFIG_PLAT_AMBARELLA_BOSS),y)
obj-$(CONFIG_PLAT_AMBARELLA_I1_BOSS)	+= init-boss.o
obj-$(CONFIG_PLAT_AMBARELLA_A5S_BOSS)	+= init-boss_a5s.o
else
obj-$(CONFIG_MACH_AMBARELLA)	+= init-generic.o
endif

obj-y				+= touch-ak4183.o
obj-y				+= touch-chacha_mt4d.o
obj-y				+= touch-tm1510.o
obj-y				+= touch-tm1726.o
obj-y				+= touch-nt11001.o
obj-y				+= rtc-is112022m.o
obj-y				+= vin.o
obj-y				+= vout.o
obj-y				+= codec-wm8994.o
obj-y				+= codec-wm8737.o
obj-y				+= wm831x-config.o
