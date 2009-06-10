/*
 * arch/arm/mach-ambarella/rct.c
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2008/01/08 - [Anthony Ginger] Port to 2.6.28
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
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

#include <linux/device.h>
#include <linux/delay.h>

#include <asm/io.h>

#include <mach/hardware.h>

#undef readb
#undef readw
#undef readl
#undef writeb
#undef writew
#undef writel
#define readb(v)		amba_readb(v)
#define readw(v)		amba_readw(v)
#define readl(v)		amba_readl(v)
#define writeb(v,d)		amba_writeb(v,d)
#define writew(v,d)		amba_writew(v,d)
#define writel(v,d)		amba_writel(v,d)
#define dly_tsk(x)		mdelay(10 * (x))

#if (CHIP_REV == A1)
#include "rct/a1.c"
#endif
#if (CHIP_REV == A2)
#include "rct/a2.c"
#endif
#if (CHIP_REV == A2M) || (CHIP_REV == A2S)
#include "rct/a2s.c"
#endif
#if (CHIP_REV == A3)
#include "rct/a3.c"
#endif
#if (CHIP_REV == A5)
#include "rct/a5.c"
#endif
#if (CHIP_REV == A6)
#include "rct/a6.c"
#endif
#include "rct/audio.c"

EXPORT_SYMBOL(rct_pll_init);
EXPORT_SYMBOL(get_apb_bus_freq_hz);
EXPORT_SYMBOL(get_ahb_bus_freq_hz);
EXPORT_SYMBOL(get_core_bus_freq_hz);
EXPORT_SYMBOL(get_dram_freq_hz);
EXPORT_SYMBOL(get_idsp_freq_hz);
EXPORT_SYMBOL(get_adc_freq_hz);
EXPORT_SYMBOL(get_uart_freq_hz);
EXPORT_SYMBOL(get_ssi_freq_hz);
EXPORT_SYMBOL(get_motor_freq_hz);
EXPORT_SYMBOL(get_pwm_freq_hz);
EXPORT_SYMBOL(get_ir_freq_hz);
EXPORT_SYMBOL(get_host_freq_hz);
EXPORT_SYMBOL(get_sd_freq_hz);
EXPORT_SYMBOL(get_sd_scaler);
EXPORT_SYMBOL(get_so_freq_hz);
EXPORT_SYMBOL(get_vout_freq_hz);
EXPORT_SYMBOL(get_vout2_freq_hz);
EXPORT_SYMBOL(get_stepping_info);
EXPORT_SYMBOL(get_hdmi_phy_freq_hz);
EXPORT_SYMBOL(get_hdmi_cec_freq_hz);
EXPORT_SYMBOL(get_vin_freq_hz);
EXPORT_SYMBOL(get_ssi_clk_src);
EXPORT_SYMBOL(get_vout_clk_rescale_value);
EXPORT_SYMBOL(get_vout2_clk_rescale_value);
EXPORT_SYMBOL(rct_boot_from);
EXPORT_SYMBOL(rct_is_cf_trueide);
//EXPORT_SYMBOL(rct_is_eth_enabled);
EXPORT_SYMBOL(rct_power_down);
EXPORT_SYMBOL(rct_reset_chip);
EXPORT_SYMBOL(rct_reset_fio);
EXPORT_SYMBOL(rct_reset_fio_only);
EXPORT_SYMBOL(rct_reset_cf);
EXPORT_SYMBOL(rct_reset_xd);
EXPORT_SYMBOL(rct_reset_flash);
EXPORT_SYMBOL(rct_reset_dma);
EXPORT_SYMBOL(rct_set_uart_pll);
EXPORT_SYMBOL(rct_set_sd_pll);
EXPORT_SYMBOL(rct_enable_sd_pll);
EXPORT_SYMBOL(rct_disable_sd_pll);
EXPORT_SYMBOL(rct_change_sd_pll);
EXPORT_SYMBOL(rct_set_ir_pll);
EXPORT_SYMBOL(rct_set_motor_freq_hz);
EXPORT_SYMBOL(rct_set_pwm_freq_hz);
EXPORT_SYMBOL(rct_set_ssi_pll);
#if (SPI_INSTANCES == 2)
EXPORT_SYMBOL(rct_set_ssi2_pll);
EXPORT_SYMBOL(get_ssi2_freq_hz);
#endif
//EXPORT_SYMBOL(rct_set_host_pll);
EXPORT_SYMBOL(rct_set_so_freq_hz);
EXPORT_SYMBOL(rct_set_vout_freq_hz);
EXPORT_SYMBOL(rct_set_hdmi_phy_freq_hz);
#ifdef ENABLE_MS_AHB_HOST
EXPORT_SYMBOL(rct_set_ms_pll);
EXPORT_SYMBOL(get_ms_freq_hz);
EXPORT_SYMBOL(rct_enable_ms);
EXPORT_SYMBOL(rct_disable_ms);
EXPORT_SYMBOL(rct_set_dven_clk);
#endif
EXPORT_SYMBOL(rct_set_vin_freq_hz);
EXPORT_SYMBOL(rct_cal_vin_freq_hz_slvs);
EXPORT_SYMBOL(rct_set_adc_clk);
EXPORT_SYMBOL(rct_set_ssi_clk_src);
EXPORT_SYMBOL(rct_set_so_pclk_freq_hz);
EXPORT_SYMBOL(rct_set_so_clk_src);
EXPORT_SYMBOL(rct_rescale_so_pclk_freq_hz);
EXPORT_SYMBOL(rct_set_vout_clk_src);
EXPORT_SYMBOL(rct_rescale_vout_clk_freq_hz);
EXPORT_SYMBOL(rct_set_vout2_freq_hz);
EXPORT_SYMBOL(rct_set_vout2_clk_src);
EXPORT_SYMBOL(rct_rescale_vout2_clk_freq_hz);
EXPORT_SYMBOL(rct_set_hdmi_clk_src);
//EXPORT_SYMBOL(rct_set_gclk_vo_hdmi);
EXPORT_SYMBOL(rct_set_vin_lvds_pad);
#if (RCT_AUDIO_PLL_CONF_MODE > 0)
EXPORT_SYMBOL(rct_set_audio_pll_reset);
EXPORT_SYMBOL(rct_set_audio_pll_fs);
#else
EXPORT_SYMBOL(rct_set_audio_pll_fs);
#endif
EXPORT_SYMBOL(rct_audio_pll_alan_zhu_magic_init);
EXPORT_SYMBOL(rct_set_pll_frac_mode);
EXPORT_SYMBOL(rct_set_aud_ctrl2_reg);
EXPORT_SYMBOL(get_audio_freq_hz);
EXPORT_SYMBOL(set_audio_sfreq);
EXPORT_SYMBOL(get_audio_sfreq);
EXPORT_SYMBOL(rct_alan_zhu_magic_loop);
EXPORT_SYMBOL(rct_set_usb_ana_on);
EXPORT_SYMBOL(rct_suspend_usb);
EXPORT_SYMBOL(rct_set_usb_clk);
EXPORT_SYMBOL(rct_set_usb_ext_clk);
EXPORT_SYMBOL(rct_set_usb_int_clk);
EXPORT_SYMBOL(rct_set_usb_debounce);
EXPORT_SYMBOL(rct_turn_off_usb_pll);
EXPORT_SYMBOL(rct_ena_usb_int_clk);
EXPORT_SYMBOL(read_usb_reg_setting);
EXPORT_SYMBOL(read_pll_so_reg);
EXPORT_SYMBOL(read_cg_so_reg);
EXPORT_SYMBOL(write_pll_so_reg);
EXPORT_SYMBOL(write_cg_so_reg);

