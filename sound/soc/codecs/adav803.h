/*
 * adav803.h  --  ADAV803 Soc Audio driver
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * History:
 *	2009/03/05 - [Cao Rongrong] Created file
 * 
 * Copyright (C) 2004-2009, Ambarella, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef _ADAV803_H
#define _ADAV803_H

/* ADAV803 register space */
#define ADAV803_NUMREGS	126

#define ADAV803_SRC_CLK_Ctrl_Reg_Adr	0x00
#define ADAV803_SPDIF_In_Reg_Adr	0x03
#define ADAV803_Playback_Ctrl_Reg_Adr	0x04
#define ADAV803_AuxIn_Reg_Adr		0x05
#define ADAV803_Record_Ctrl_Reg_Adr	0x06
#define ADAV803_AuxOut_Reg_Adr		0x07
#define ADAV803_GD_Mute_Reg_Adr		0x08
#define ADAV803_SPDIF_Rx_Ctrl1_Reg_Adr	0x09
#define ADAV803_SPDIF_Rx_Ctrl2_Reg_Adr	0x0a
#define ADAV803_Rx_Buffer_Config_Adr	0x0b
#define ADAV803_SPDIF_Tx_Ctrl_Reg_Adr	0x0c
#define ADAV803_Autobuffer_Adr	0x11
#define ADAV803_SPDIF_Rx_Error_Adr	0x18
#define ADAV803_SPDIF_Tx_Mute_Reg_Adr	0x1e
#define ADAV803_NonAudio_Preamble_Reg_Adr	0x1f
#define ADAV803_DataPath1_Reg_Adr	0x62
#define ADAV803_DataPath2_Reg_Adr	0x63
#define ADAV803_DAC_Ctrl1_Reg_Adr	0x64
#define ADAV803_DAC_Ctrl2_Reg_Adr	0x65
#define ADAV803_DAC_Ctrl3_Reg_Adr	0x66
#define ADAV803_DAC_Ctrl4_Reg_Adr	0x67
#define ADAV803_DAC_Left_Volume_Adr	0x68
#define ADAV803_DAC_Right_Volume_Adr	0x69
#define ADAV803_DAC_LeftPeakVolume_Adr	0x6a
#define ADAV803_DAC_RightPeakVolume_Adr	0x6b
#define ADAV803_ADC_Left_PGA_Adr	0x6c
#define ADAV803_ADC_Right_PGA_Adr	0x6d
#define ADAV803_ADC_Ctrl1_Reg_Adr	0x6e
#define ADAV803_ADC_Ctrl2_Reg_Adr	0x6f
#define ADAV803_ADC_Left_Volume_Adr	0x70
#define ADAV803_ADC_Right_Volume_Adr	0x71
#define ADAV803_ADC_LeftPeakVolume_Adr	0x72
#define ADAV803_ADC_RightPeakVolume_Adr	0x73
#define ADAV803_PLL_Ctrl1_Reg_Adr	0x74
#define ADAV803_PLL_Ctrl2_Reg_Adr	0x75
#define ADAV803_Internal_CLK_Ctrl1_Adr	0x76
#define ADAV803_Internal_CLK_Ctrl2_Adr	0x77
#define ADAV803_PLL_Source_Adr		0x78
#define ADAV803_PLL_Output_Enable_Adr	0x7A
#define ADAV803_ALC_Ctrl1_Reg_Adr	0x7B
#define ADAV803_ALC_Ctrl2_Reg_Adr	0x7C
#define ADAV803_ALC_Ctrl3_Reg_ADr	0x7D


#define ADAV803_u03_TX_Normal		0
#define ADAV803_u03_TX_Loopback_RX	1

#define ADAV803_u04_Slave		0
#define ADAV803_u04_PLL_CLK	(1 << 3)
#define ADAV803_u04_Internal_CLK1	(2 << 3)
#define ADAV803_u04_Internal_CLK2	(3 << 3)

#define ADAV803_u04_LeftJustified		0
#define ADAV803_u04_I2S			1
#define ADAV803_u04_24bit_RightJustified	4
#define ADAV803_u04_20bit_RightJustified	5
#define ADAV803_u04_18bit_RightJustified	6
#define ADAV803_u04_16bit_RightJustified	7

#define ADAV803_u04_RightJustified		4
#define ADAV803_u04_24bit			0
#define ADAV803_u04_20bit			1
#define ADAV803_u04_18bit			2
#define ADAV803_u04_16bit			3

#define ADAV803_u05_LeftJustified		0
#define ADAV803_u05_I2S			1
#define ADAV803_u05_24bit_RightJustified	4
#define ADAV803_u05_20bit_RightJustified	5
#define ADAV803_u05_18bit_RightJustified	6
#define ADAV803_u05_16bit_RightJustified	7

#define ADAV803_u05_RightJustified		4
#define ADAV803_u05_24bit			0
#define ADAV803_u05_20bit			1
#define ADAV803_u05_18bit			2
#define ADAV803_u05_16bit			3

#define ADAV803_u05_Slave		0
#define ADAV803_u05_PLL_CLK		(1 << 3)
#define ADAV803_u05_Internal_CLK1	(2 << 3)
#define ADAV803_u05_Internal_CLK2	(3 << 3)

#define ADAV803_u06_LeftJustified	0
#define ADAV803_u06_I2S		1
#define ADAV803_u06_Reserved		2
#define ADAV803_u06_RightJustified	3

#define ADAV803_u06_24Bit	0
#define ADAV803_u06_20Bit	(1 << 2)
#define ADAV803_u06_18Bit	(2 << 2)
#define ADAV803_u06_16Bit	(3 << 2)

#define ADAV803_u06_Slave		0
#define ADAV803_u06_PLL_CLK		(1 << 4)
#define ADAV803_u06_Internal_CLK1	(2 << 4)
#define ADAV803_u06_Internal_CLK2	(3 << 4)

#define ADAV803_u07_LeftJustified	0
#define ADAV803_u07_I2S		1
#define ADAV803_u07_Reserved		2
#define ADAV803_u07_RightJustified	3

#define ADAV803_u07_24Bit	0
#define ADAV803_u07_20Bit	(1 << 2)
#define ADAV803_u07_18Bit	(2 << 2)
#define ADAV803_u07_16Bit	(3 << 2)

#define ADAV803_u07_Slave		0
#define ADAV803_u07_PLL_CLK		(1 << 4)
#define ADAV803_u07_Internal_CLK1	(2 << 4)
#define ADAV803_u07_Internal_CLK2	(3 << 4)

#define ADAV803_MuteSRC_NoMute		0
#define ADAV803_MuteSRC_SoftMute	(1 << 7)

#define ADAV803_u09_Rx_NOCLK_PLL	0
#define ADAV803_u09_Rx_NOCLK_ICLK1	(1 << 7)

#define ADAV803_u09_RxCLK_128fs	0
#define ADAV803_u09_RxCLK_256fs	(1 << 5)
#define ADAV803_u09_RxCLK_512fs	(2 << 5)
#define ADAV803_u09_RxCLK_reserved	(3 << 5)

#define ADAV803_u09_Rx_AutoDeemp_Off	0
#define ADAV803_u09_Rx_AutoDeemp_On	(1 << 4)

#define ADAV803_u09_Rx_ERROR_NoAction		0
#define ADAV803_u09_Rx_ERROR_LastValid	(1 << 2)
#define ADAV803_u09_Rx_ERROR_Zero		(2 << 2)
#define ADAV803_u09_Rx_ERROR_Reserved 	(3 << 2)

#define ADAV803_u09_Rx_LoseLock_NoAction	0
#define ADAV803_u09_Rx_LoseLock_LastValid	1
#define ADAV803_u09_Rx_LoseLock_Zero		2
#define ADAV803_u09_Rx_LoseLock_SoftMute	3

#define ADAV803_u0a_RxMute_Off		0
#define ADAV803_u0a_RxMute_On	(1 << 7)

#define ADAV803_u0a_Rx_LRCLK_FromPLL		0
#define ADAV803_u0a_Rx_LRCLK_FromSerial	(1 << 6)

#define ADAV803_u0a_Rx_Serial_Playback	0
#define ADAV803_u0a_Rx_Serial_AuxIn	(1 << 4)
#define ADAV803_u0a_Rx_Serial_Record	(2 << 4)
#define ADAV803_u0a_Rx_Serial_AuxOut	(3 << 4)

#define ADAV803_u0c_Tx_Valid_Audio 0
#define ADAV803_u0c_Tx_NonValid_Audio (1 << 6)

#define ADAV803_u0c_Tx_Rx_Ratio11	0
#define ADAV803_u0c_Tx_Rx_Ratio12	(1 << 3)
#define ADAV803_u0c_Tx_Rx_Ratio14	(2 << 3)
#define ADAV803_u0c_Tx_Rx_Ratio21	(5 << 3)
#define ADAV803_u0c_Tx_Rx_Ratio41	(6 << 3)

#define ADAV803_u0c_Tx_InterCLK1	0
#define ADAV803_u0c_Tx_InterCLK2	(1 << 1)
#define ADAV803_u0c_Tx_RecoveredPLL	(2 << 1)
#define ADAV803_u0c_Tx_Reserved	(3 << 1)

#define ADAV803_u0c_Tx_Disable	0
#define ADAV803_u0c_Tx_Enable		1

#define ADAV803_u1e_Tx_unmute	0
#define ADAV803_u1e_Tx_mute	(1 << 5)

#define ADAV803_u1e_NoDeemp	0
#define ADAV803_u1e_32KDeemp	(1 << 1)
#define ADAV803_u1e_44KDeemp	(2 << 1)
#define ADAV803_u1e_48KDeemp	(3 << 1)


#define ADAV803_u62_SRC1_ADC	0
#define ADAV803_u62_SRC1_DIR	(1 << 6)
#define ADAV803_u62_SRC1_Playback	(2 << 6)
#define ADAV803_u62_SRC1_AuxIn	(3 << 6)
#define ADAV803_u62_SRC1_SRC	(4 << 6)
#define ADAV803_u62_REC2_ADC	0
#define ADAV803_u62_REC2_DIR	(1 << 3)
#define ADAV803_u62_REC2_Playback	(2 << 3)
#define ADAV803_u62_REC2_AuxIn	(3 << 3)
#define ADAV803_u62_REC2_SRC	(4 << 3)
#define ADAV803_u62_AUXO2_ADC		0
#define ADAV803_u62_AUXO2_DIR		1
#define ADAV803_u62_AUXO2_Playback	2
#define ADAV803_u62_AUXO2_AuxIn		3
#define ADAV803_u62_AUXO2_SRC		4


#define ADAV803_u63_DAC2_ADC	0
#define ADAV803_u63_DAC2_DIR	(1 << 3)
#define ADAV803_u63_DAC2_Playback	(2 << 3)
#define ADAV803_u63_DAC2_AuxIn	(3 << 3)
#define ADAV803_u63_DAC2_SRC	(4 << 3)
#define ADAV803_u63_DIT2_ADC	0
#define ADAV803_u63_DIT2_DIR	1
#define ADAV803_u63_DIT2_Playback	2
#define ADAV803_u63_DIT2_AuxIn	3
#define ADAV803_u63_DIT2_SRC	4


#define ADAV803_u64_DR_ALL_Normal	0
#define ADAV803_u64_DR_ALL	(1 << 7)
#define ADAV803_u64_DR_DIG_Normal	0
#define ADAV803_u64_DR_DIG	(1 << 6)
#define ADAV803_u64_Normal	0
#define ADAV803_u64_BothRight	(1 << 4)
#define ADAV803_u64_BothLeft	(2 << 4)
#define ADAV803_u64_SwapLR	(3 << 4)
#define ADAV803_u64_BothPos	0
#define ADAV803_u64_LeftNeg	(1 << 2)
#define ADAV803_u64_RightNeg	(2 << 2)
#define ADAV803_u64_BothNeg	(3 << 2)
#define ADAV803_u64_Right_Mute	0
#define ADAV803_u64_Right_UnMute	(1 << 1)
#define ADAV803_u64_Left_Mute	0
#define ADAV803_u64_Left_UnMute	1

#define ADAV803_u65_MCLK	0
#define ADAV803_u65_MCLK32	(1 << 4)
#define ADAV803_u65_MCLK12	(2 << 4)
#define ADAV803_u65_MCLK13	(3 << 4)
#define ADAV803_u65_8X			0
#define ADAV803_u65_4X			(1 << 2)
#define ADAV803_u65_2X			(2 << 2)
#define ADAV803_u65_DFS1_Reserved	(3 << 2)
#define ADAV803_u65_None_DEEM	0
#define ADAV803_u65_441K_DEEM	1
#define ADAV803_u65_32K_DEEM	2
#define ADAV803_u65_48K_DEEM	3

#define ADAV803_u66_ZFVOL_Enable	0
#define ADAV803_u66_ZFVOL_Disable	(1 << 2)
#define ADAV803_u66_ZFDATA_Enable	0
#define ADAV803_u66_ZFDATA_Disable	(1 << 1)
#define ADAV803_u66_ZFPOL_ActiveHigh	0
#define ADAV803_u66_ZFPOL_ActiveLow	1

#define ADAV803_u67_INTAS_ZEROL	0
#define ADAV803_u67_INTRPT	(1 << 6)
#define ADAV803_u67_ASZEROR	0
#define ADAV803_u67_ASZEROL	(1 << 4)
#define ADAV803_u67_ONE_ZERO	(2 << 4)
#define ADAV803_u67_BOTH_ZERO	(3 << 4)


#define ADAV803_ADC_Ctrl1_MCLK12	0
#define ADAV803_ADC_Ctrl1_MCLK14	(1 << 7)

#define ADAV803_ADC_Ctrl1_NoHPF		0
#define ADAV803_ADC_Ctrl1_withHPF	(1 << 6)

#define ADAV803_ADC_Ctrl1_PWR_Normel	0
#define ADAV803_ADC_Ctrl1_PWR_Down	(1 << 5)

#define ADAV803_ADC_Ctrl1_Analog_PWR_Normal	0
#define ADAV803_ADC_Ctrl1_Analog_PWR_Down	(1 << 4)


#define ADAV803_ADC_Ctrl1_Right_UnMute	0
#define ADAV803_ADC_Ctrl1_Right_Mute	(1 << 3)

#define ADAV803_ADC_Ctrl1_Left_UnMute	0
#define ADAV803_ADC_Ctrl1_Left_Mute	(1 << 2)

#define ADAV803_ADC_Ctrl1_PGA_PWR_Left_Normal 	0
#define ADAV803_ADC_Ctrl1_PGA_PWR_Left_Down	(1 << 1)

#define ADAV803_ADC_Ctrl1_PGA_PWR_Right_Normal 	0
#define ADAV803_ADC_Ctrl1_PGA_PWR_Right_Down	1


#define ADAV803_ADC_Ctrl2_BUF_PWR_Normal	0
#define ADAV803_ADC_Ctrl2_BUF_PWR_Down		(1 << 4)

#define ADAV803_ADC_Ctrl2_MCLKDIV1	0
#define ADAV803_ADC_Ctrl2_MCLKDIV2	1
#define ADAV803_ADC_Ctrl2_MCLKDIV3	2

#define ADAV803_PLL_Ctrl1_SYSCLK3_FromPLL	0
#define ADAV803_PLL_Ctrl1_Reserved1		(1 << 6)
#define ADAV803_PLL_Ctrl1_Reserved2		(2 << 6)
#define ADAV803_PLL_Ctrl1_SYSCLK3_FromDIRIN	(3 << 6)

#define ADAV803_PLL_Ctrl1_MCLKIDIV2_Diable	0
#define ADAV803_PLL_Ctrl1_MCLKIDIV2_Enable	(1 << 5)

#define ADAV803_PLL_Ctrl1_PLLDIV_Diable		0
#define ADAV803_PLL_Ctrl1_PLLDIV_Enable		(1 << 4)

#define ADAV803_PLL_Ctrl1_PLL2PD_PWR_Normal 	0
#define ADAV803_PLL_Ctrl1_PLL2PD_PWR_Down 	(1 << 3)

#define ADAV803_PLL_Ctrl1_PLL1PD_PWR_Normal	0
#define ADAV803_PLL_Ctrl1_PLL1PD_PWR_Down	(1 << 2)

#define ADAV803_PLL_Ctrl1_XTAL_PWR_Normal	0
#define ADAV803_PLL_Ctrl1_XTAL_PWR_Down		(1 << 1)

#define ADAV803_PLL_Ctrl1_SYSCLK3_512Fs		0
#define ADAV803_PLL_Ctrl1_SYSCLK3_256Fs		1

#define ADAV803_PLL_Ctrl2_PLL2_FS48		0
#define ADAV803_PLL_Ctrl2_PLL2_FS_Reserved	(1 << 6)
#define ADAV803_PLL_Ctrl2_PLL2_FS32		(2 << 6)
#define ADAV803_PLL_Ctrl2_PLL2_FS441		(3 << 6)

#define ADAV803_PLL_Ctrl2_PLL2_OverSample_256Fs	0
#define ADAV803_PLL_Ctrl2_PLL2_OverSample_384Fs	(1 << 5)

#define ADAV803_PLL_Ctrl2_PLL2_Double_Fs_Disable	0
#define ADAV803_PLL_Ctrl2_PLL2_Double_Fs_Enable	(1 << 4)

#define ADAV803_PLL_Ctrl2_PLL1_FS48		0
#define ADAV803_PLL_Ctrl2_PLL1_FS_Reserved	(1 << 2)
#define ADAV803_PLL_Ctrl2_PLL1_FS32		(2 << 2)
#define ADAV803_PLL_Ctrl2_PLL1_FS441		(3 << 2)

#define ADAV803_PLL_Ctrl2_PLL1_OverSample_256Fs	0
#define ADAV803_PLL_Ctrl2_PLL1_OverSample_384Fs	(1 << 1)

#define ADAV803_PLL_Ctrl2_PLL1_Double_Fs_Disable	0
#define ADAV803_PLL_Ctrl2_PLL1_Double_Fs_Enable		1


#define ADAV803_u76_DACCLKSrc_XIN		0
#define ADAV803_u76_DACCLKSrc_MCLKI 		(1 << 5)
#define ADAV803_u76_DACCLKSrc_PLLINT1		(2 << 5)
#define ADAV803_u76_DACCLKSrc_PLLINT2 		(3 << 5)
#define ADAV803_u76_DACCLKSrc_DIRPLL_512Fs 	(4 << 5)
#define ADAV803_u76_DACCLKSrc_DIRPLL_256Fs	(5 << 5)
#define ADAV803_u76_DACCLKSrc_XIN_0		(6 << 5)
#define ADAV803_u76_DACCLKSrc_XIN_1		(7 << 5)
#define ADAV803_u76_ADCCLKSrc_XIN		0
#define ADAV803_u76_ADCCLKSrc_MCLKI 		(1 << 2)
#define ADAV803_u76_ADCCLKSrc_PLLINT1		(2 << 2)
#define ADAV803_u76_ADCCLKSrc_PLLINT2 		(3 << 2)
#define ADAV803_u76_ADCCLKSrc_DIRPLL_512Fs 	(4 << 2)
#define ADAV803_u76_ADCCLKSrc_DIRPLL_256Fs	(5 << 2)
#define ADAV803_u76_ADCCLKSrc_XIN_0		(6 << 2)
#define ADAV803_u76_ADCCLKSrc_XIN_1		(7 << 2)
#define ADAV803_u76_ICLK2Src_XIN	0
#define ADAV803_u76_ICLK2Src_MCLKI	1
#define ADAV803_u76_ICLK2Src_PLLINT1	2
#define ADAV803_u76_ICLK2Src_PLLINT2	3

#define ADAV803_u77_ICLK1Src_XIN	0
#define ADAV803_u77_ICLK1Src_MCLKI	(1 << 3)
#define ADAV803_u77_ICLK1Src_PLLINT1	(2 << 3)
#define ADAV803_u77_ICLK1Src_PLLINT2	(3 << 3)
#define ADAV803_u77_PLL2_FS2	0
#define ADAV803_u77_PLL2_FS22	(1 << 1)
#define ADAV803_u77_PLL2_FS3	(2 << 1)
#define ADAV803_u77_PLL2_FS32	(3 << 1)
#define ADAV803_u77_PLL1_FS	0
#define ADAV803_u77_PLL1_FS12 	1

#define ADAV803_u7a_DIRIN_PWR_Down	(1 << 5)

typedef struct audiocodec_clk_info_s {
	u8 mclk_idx;			/**refer to AudioCodec_MCLK*/
	u8 oversample_idx;	/**refer to AudioCodec_OverSample*/
} audiocodec_clk_info_t;

struct adav803_setup_data {
	int i2c_bus;
	unsigned short i2c_address;
};

extern struct snd_soc_dai adav803_dai;
extern struct snd_soc_codec_device soc_codec_dev_adav803;

#endif

