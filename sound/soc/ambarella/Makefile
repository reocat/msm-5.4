# AMBARELLA Platform Support
snd-soc-ambarella-objs := ambarella_pcm.o
snd-soc-ambarella-i2s-objs := ambarella_i2s.o

obj-$(CONFIG_SND_AMBARELLA_SOC)		+= snd-soc-ambarella.o
obj-$(CONFIG_SND_AMBARELLA_SOC_I2S)	+= snd-soc-ambarella-i2s.o

# AMBARELLA Machine Support
snd-soc-ipcam-objs := ipcam.o
snd-soc-a2sbub-objs := a2sbub.o
snd-soc-a2sipcam-objs := a2sipcam.o
snd-soc-a5ssdk-objs := a5ssdk.o
snd-soc-dummy-objs := dummy.o

obj-$(CONFIG_IPCAM_BOARD)		+= snd-soc-ipcam.o
obj-$(CONFIG_A2SBUB_BOARD)		+= snd-soc-a2sbub.o
obj-$(CONFIG_A2SIPCAM_BOARD)		+= snd-soc-a2sipcam.o
obj-$(CONFIG_A5SSDK_BOARD)		+= snd-soc-a5ssdk.o
obj-$(CONFIG_AMBARELLA_DUMMY_BOARD)	+= snd-soc-dummy.o

