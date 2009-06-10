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

obj-y += rct.o fio.o config.o irq.o timer.o gpio.o dma.o adc.o pwm.o fb.o pm.o

obj-$(CONFIG_CPU_FREQ_AMBARELLA)		+= cpu-ambarella.o

# Create your own init.c if necessary
obj-$(CONFIG_AMBARELLA_CHIP_A2)			+= init.o
obj-$(CONFIG_AMBARELLA_CHIP_A2S)		+= init.o
obj-$(CONFIG_AMBARELLA_CHIP_A2M)		+= init.o
obj-$(CONFIG_AMBARELLA_CHIP_A3)			+= init.o

