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

ifeq ($(CONFIG_PLAT_AMBARELLA_GENERIC), y)
obj-y						+= misc/ambsyncproc.o
obj-y						+= misc/ambasyncproc.o
obj-y						+= misc/ambevent.o
obj-y						+= misc/atag.o
obj-y						+= misc/board.o
obj-y						+= misc/event.o
obj-y						+= misc/ambcache.o
obj-$(CONFIG_AMBARELLA_SUPPORT_BAPI)		+= misc/bapi.o
obj-$(CONFIG_PLAT_AMBARELLA_SUPPORT_GDMA)	+= misc/gdma.o
obj-$(CONFIG_PLAT_AMBARELLA_ADD_REGISTER_LOCK)	+= misc/ambreglock.o
obj-$(CONFIG_AMBARELLA_TIMER_HIGHRES)		+= misc/highres_timer.o
endif

ifeq ($(CONFIG_PLAT_AMBARELLA_GENERIC), y)
obj-y						+= generic/adc.o
obj-y						+= generic/audio.o
obj-y						+= generic/clk.o
obj-y						+= generic/crypto.o
obj-y						+= generic/dma.o
obj-y						+= generic/eth.o
obj-y						+= generic/fio.o
obj-y						+= generic/gpio.o
obj-y						+= generic/idc.o
obj-y						+= generic/ir.o
obj-y						+= generic/irq.o
obj-y						+= generic/pm.o
obj-y						+= generic/pwm.o
obj-y						+= generic/rct.o
obj-y						+= generic/rtc.o
obj-y						+= generic/sd.o
obj-y						+= generic/spi.o
obj-y						+= generic/timer.o
obj-y						+= generic/uart.o
obj-y						+= generic/udc.o
obj-y						+= generic/wdt.o
ifeq ($(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL), y)
obj-y						+= generic/pll_hal.o
else
obj-y						+= generic/pll_rct.o
endif
ifeq ($(CONFIG_PLAT_AMBARELLA_SUPPORT_USB), y)
obj-y						+= generic/uhc.o
endif
ifeq ($(CONFIG_PLAT_AMBARELLA_SUPPORT_SATA), y)
obj-y						+= generic/ahci.o
endif
endif

ifeq ($(CONFIG_PLAT_AMBARELLA_GENERIC), y)
obj-y						+= video/ambfb.o
endif

ifeq ($(CONFIG_PLAT_AMBARELLA_CORTEX), y)
obj-$(CONFIG_SMP)				+= cortex/smp.o
obj-$(CONFIG_SMP)				+= cortex/headsmp.o
obj-$(CONFIG_HOTPLUG_CPU)			+= cortex/hotplug.o
obj-$(CONFIG_LOCAL_TIMERS)			+= cortex/localtimer.o
endif

