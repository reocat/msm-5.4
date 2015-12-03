#
# Makefile for ambarella specific clk
#

obj-y						+= clk.o

ifeq ($(CONFIG_AMBARELLA_CALC_PLL),y)
obj-y						+= clk_calc.o
else
obj-y						+= clk_table.o
endif
