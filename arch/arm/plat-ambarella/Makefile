#
# arch/arm/plat-ambarella/Makefile
#
# History:
#	2010/05/26 - [Anthony Ginger] created file
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

obj-y	+= init.o

ifeq ($(CONFIG_PLAT_AMBARELLA_GENERIC),y)

obj-y	+= generic/adc.o generic/dma.o
obj-y	+= generic/fio.o generic/gpio.o
obj-y	+= generic/irq.o generic/pwm.o 
obj-y	+= generic/pll.o generic/pm.o
obj-y	+= generic/rct.o generic/timer.o

obj-y	+= generic/config.o generic/atag.o generic/event.o
obj-y	+= generic/board.o
obj-y	+= generic/ir.o
obj-y	+= generic/udc.o
obj-y	+= generic/eth.o
obj-y	+= generic/wdt.o
obj-y	+= generic/rtc.o
obj-y	+= generic/audio.o
obj-y	+= generic/spi.o
obj-y	+= generic/idc.o
obj-y	+= generic/sd.o
obj-y	+= generic/uart.o

obj-$(CONFIG_PLAT_AMBARELLA_SUPPORT_HW_CRYPTO)	+= generic/crypto.o
endif

obj-y	+= video/ambfb.o
obj-y	+= misc/ambsyncproc.o
obj-y	+= misc/ambasyncproc.o
obj-y	+= misc/ambevent.o
obj-$(CONFIG_PLAT_AMBARELLA_SUPPORT_GDMA)	+= misc/gdma.o
obj-y	+= misc/ambcache.o

