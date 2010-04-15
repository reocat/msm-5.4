#
# arch/arm/mach-ambarella/Kconfig
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

obj-y	+= rct.o fio.o irq.o timer.o
obj-y	+= gpio.o dma.o adc.o pwm.o pm.o pll.o
obj-y	+= init.o config.o fb.o sync_proc.o
obj-y	+= toss_linux.o
CFLAGS_toss_linux.o = \
	-I$(src) \
	-DTOSS_H_CHKSUM=$(word 1,$(shell cksum arch/arm/mach-ambarella/include/mach/toss/toss.h))UL \
	-DTOSS_HWCTX_H_CHKSUM=$(word 1,$(shell cksum arch/arm/mach-ambarella/include/mach/toss/toss_hwctx.h))UL \
	-DTOSS_DEVCTX_H_CHKSUM=$(word 1,$(shell cksum arch/arm/mach-ambarella/include/mach/toss/toss_devctx.h))UL
