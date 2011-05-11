# AMBARELLA Platform Support
snd-soc-ambarella-objs := ambarella_pcm.o
snd-soc-ambarella-i2s-objs := ambarella_i2s.o

obj-$(CONFIG_SND_AMBARELLA_SOC)		+= snd-soc-ambarella.o
obj-$(CONFIG_SND_AMBARELLA_SOC_I2S)	+= snd-soc-ambarella-i2s.o

# AMBARELLA Machine Support
snd-soc-ipcam-objs := ipcam.o
snd-soc-a5sevk-objs := a5sevk.o
snd-soc-i1evk-objs := i1evk.o
snd-soc-dummy-objs := dummy.o
snd-soc-a5spa2-objs := a5spa2.o

obj-$(CONFIG_IPCAM_BOARD)		+= snd-soc-ipcam.o
obj-$(CONFIG_A5SEVK_BOARD)		+= snd-soc-a5sevk.o
obj-$(CONFIG_I1EVK_BOARD)		+= snd-soc-i1evk.o
obj-$(CONFIG_AMBARELLA_DUMMY_BOARD)	+= snd-soc-dummy.o
obj-$(CONFIG_A5S_PA2_BOARD)		+= snd-soc-a5spa2.o

