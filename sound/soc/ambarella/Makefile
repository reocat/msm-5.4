# AMBARELLA Platform Support
snd-soc-ambarella-objs := ambarella_pcm.o
snd-soc-ambarella-i2s-objs := ambarella_i2s.o

obj-$(CONFIG_SND_AMBARELLA_SOC)		+= snd-soc-ambarella.o
obj-$(CONFIG_SND_AMBARELLA_SOC_I2S)	+= snd-soc-ambarella-i2s.o

# AMBARELLA Machine Support
snd-soc-amba-board-objs := ambarella_board.o
snd-soc-dummy-objs := dummy.o

obj-$(CONFIG_AMBA_BOARD)		+= snd-soc-amba-board.o
obj-$(CONFIG_AMBARELLA_DUMMY_BOARD)	+= snd-soc-dummy.o
