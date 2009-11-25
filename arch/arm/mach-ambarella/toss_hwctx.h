/*
 * toss/toss_hwctx.h
 *
 * Terminal Operating Systerm Scheduler
 *
 * History:
 *    2009/11/10 - [Charles Chiou] created file
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
 */

#ifndef __TOSS_HWCTX_H__
#define __TOSS_HWCTX_H__

/*
 * HAL
 */
__ARMCC_PACK__ struct toss_hwctx_hal_s
{
	void *ahb_base;
	void *apb_base;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * VIC
 */
__ARMCC_PACK__ struct toss_hwctx_vic_s
{
	unsigned int vic_int_sel_reg;
	unsigned int vic_inten_reg;
	unsigned int vic_soften_reg;
	unsigned int vic_proten_reg;
	unsigned int vic_sense_reg;
	unsigned int vic_bothedge_reg;
	unsigned int vic_event_reg;

	unsigned int vic2_int_sel_reg;
	unsigned int vic2_inten_reg;
	unsigned int vic2_soften_reg;
	unsigned int vic2_proten_reg;
	unsigned int vic2_sense_reg;
	unsigned int vic2_bothedge_reg;
	unsigned int vic2_event_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * GPIO
 */
__ARMCC_PACK__ struct toss_hwctx_gpio_s
{
	unsigned int gpio0_data_reg;
	unsigned int gpio0_dir_reg;
	unsigned int gpio0_is_reg;
	unsigned int gpio0_ibe_reg;
	unsigned int gpio0_iev_reg;
	unsigned int gpio0_ie_reg;
	unsigned int gpio0_afsel_reg;
	unsigned int gpio0_mask_reg;
	unsigned int gpio0_enable_reg;

	unsigned int gpio1_data_reg;
	unsigned int gpio1_dir_reg;
	unsigned int gpio1_is_reg;
	unsigned int gpio1_ibe_reg;
	unsigned int gpio1_iev_reg;
	unsigned int gpio1_ie_reg;
	unsigned int gpio1_afsel_reg;
	unsigned int gpio1_mask_reg;
	unsigned int gpio1_enable_reg;

	unsigned int gpio2_data_reg;
	unsigned int gpio2_dir_reg;
	unsigned int gpio2_is_reg;
	unsigned int gpio2_ibe_reg;
	unsigned int gpio2_iev_reg;
	unsigned int gpio2_ie_reg;
	unsigned int gpio2_afsel_reg;
	unsigned int gpio2_mask_reg;
	unsigned int gpio2_enable_reg;

	unsigned int gpio3_data_reg;
	unsigned int gpio3_dir_reg;
	unsigned int gpio3_is_reg;
	unsigned int gpio3_ibe_reg;
	unsigned int gpio3_iev_reg;
	unsigned int gpio3_ie_reg;
	unsigned int gpio3_afsel_reg;
	unsigned int gpio3_mask_reg;
	unsigned int gpio3_enable_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * Timer
 */
__ARMCC_PACK__ struct toss_hwctx_timer_s
{
	unsigned int timer1_status_reg;
	unsigned int timer1_reload_reg;
	unsigned int timer1_match1_reg;
	unsigned int timer1_match2_reg;
	unsigned int timer2_status_reg;
	unsigned int timer2_reload_reg;
	unsigned int timer2_match1_reg;
	unsigned int timer2_match2_reg;
	unsigned int timer3_status_reg;
	unsigned int timer3_reload_reg;
	unsigned int timer3_match1_reg;
	unsigned int timer3_match2_reg;
	unsigned int timer_ctr_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * UART
 */
__ARMCC_PACK__ struct toss_hwctx_uart_s
{
	unsigned int uart0_ie_reg;
	unsigned int uart0_dll_reg;
	unsigned int uart0_dlh_reg;
	unsigned int uart0_fc_reg;
	unsigned int uart0_lc_reg;
	unsigned int uart0_mc_reg;
	unsigned int uart0_sc_reg;	/* byte */

	unsigned int uart1_ie_reg;
	unsigned int uart1_dll_reg;
	unsigned int uart1_dlh_reg;
	unsigned int uart1_fc_reg;
	unsigned int uart1_lc_reg;
	unsigned int uart1_mc_reg;
	unsigned int uart1_sc_reg;	/* byte */

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * I2C
 */
__ARMCC_PACK__ struct toss_hwctx_i2c_s
{
	unsigned int idc_enr_reg;
	unsigned int idc_ctrl_reg;
	unsigned int idc_psll_reg;
	unsigned int idc_pslh_reg;
	unsigned int idc_fmctrl_reg;

	unsigned int idc2_enr_reg;
	unsigned int idc2_ctrl_reg;
	unsigned int idc2_psll_reg;
	unsigned int idc2_pslh_reg;
	unsigned int idc2_fmctrl_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * SPI
 */
__ARMCC_PACK__ struct toss_hwctx_spi_s
{
	unsigned int spi_ctrlr0_reg;
	unsigned int spi_ctrlr1_reg;
	unsigned int spi_ssienr_reg;
	unsigned int spi_mwcr_reg;
	unsigned int spi_ser_reg;
	unsigned int spi_baudr_reg;
	unsigned int spi_txftlr_reg;
	unsigned int spi_rxftlr_reg;
	unsigned int spi_txflr_reg;
	unsigned int spi_rxflr_reg;
	unsigned int spi_sr_reg;
	unsigned int spi_imr_reg;

	unsigned int spi2_ctrlr0_reg;
	unsigned int spi2_ctrlr1_reg;
	unsigned int spi2_ssienr_reg;
	unsigned int spi2_mwcr_reg;
	unsigned int spi2_ser_reg;
	unsigned int spi2_baudr_reg;
	unsigned int spi2_txftlr_reg;
	unsigned int spi2_rxftlr_reg;
	unsigned int spi2_txflr_reg;
	unsigned int spi2_rxflr_reg;
	unsigned int spi2_sr_reg;
	unsigned int spi2_imr_reg;

	unsigned int spi_slave_ctrlr0_reg;
	unsigned int spi_slave_ctrlr1_reg;
	unsigned int spi_slave_ssienr_reg;
	unsigned int spi_slave_mwcr_reg;
	unsigned int spi_slave_ser_reg;
	unsigned int spi_slave_baudr_reg;
	unsigned int spi_slave_txftlr_reg;
	unsigned int spi_slave_rxftlr_reg;
	unsigned int spi_slave_txflr_reg;
	unsigned int spi_slave_rxflr_reg;
	unsigned int spi_slave_sr_reg;
	unsigned int spi_slave_imr_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * IR
 */
__ARMCC_PACK__ struct toss_hwctx_ir_s
{
	unsigned int ir_control_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * ADC
 */
__ARMCC_PACK__ struct toss_hwctx_adc_s
{
	unsigned int adc_control_reg;
	unsigned int adc_enable_reg;
	unsigned int adc_chan0_intr_reg;
	unsigned int adc_chan1_intr_reg;
	unsigned int adc_chan2_intr_reg;
	unsigned int adc_chan3_intr_reg;
	unsigned int adc_chan4_intr_reg;
	unsigned int adc_chan5_intr_reg;
	unsigned int adc_chan6_intr_reg;
	unsigned int adc_chan7_intr_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * PWM
 */
__ARMCC_PACK__ struct toss_hwctx_pwm_s
{
	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * Watch Dog
 */
__ARMCC_PACK__ struct toss_hwctx_wdog_s
{
	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * DMA
 */
__ARMCC_PACK__ struct toss_hwctx_dma_s
{
	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * I2S
 */
__ARMCC_PACK__ struct toss_hwctx_i2s_s
{
	unsigned int i2s_mode_reg;
	unsigned int i2s_rx_ctrl_reg;
	unsigned int i2s_tx_ctrl_reg;
	unsigned int i2s_wlen_reg;
	unsigned int i2s_wpos_reg;
	unsigned int i2s_slot_reg;
	unsigned int i2s_tx_fifo_lth_reg;
	unsigned int i2s_rx_fifo_gth_reg;
	unsigned int i2s_clock_reg;
	unsigned int i2s_init_reg;
	unsigned int i2s_tx_int_enable_reg;
	unsigned int i2s_rx_int_enable_reg;
	unsigned int i2s_rx_echo_reg;
	unsigned int i2s_24bitmux_mode_reg;
	unsigned int i2s_gateoff_reg;
	unsigned int i2s_channel_select_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * FIOS
 */
__ARMCC_PACK__ struct toss_hwctx_fios_s
{
	unsigned int fio_ctr_reg;
	unsigned int fio_sta_reg;
	unsigned int fio_dmactr_reg;
	unsigned int fio_dmaadr_reg;
	unsigned int fio_dmasta_reg;

	unsigned int flash_ctr_reg;
	unsigned int flash_tim0_reg;
	unsigned int flash_tim1_reg;
	unsigned int flash_tim2_reg;
	unsigned int flash_tim3_reg;
	unsigned int flash_tim4_reg;
	unsigned int flash_tim5_reg;

	unsigned int xd_ctr_reg;
	unsigned int xd_tim0_reg;
	unsigned int xd_tim1_reg;
	unsigned int xd_tim2_reg;
	unsigned int xd_tim3_reg;
	unsigned int xd_tim4_reg;
	unsigned int xd_tim5_reg;

	unsigned int cf_ctr_reg;
	unsigned int cf_rd_tim0_reg;
	unsigned int cf_rd_tim1_reg;
	unsigned int cf_rd_tim2_reg;
	unsigned int cf_rd_tim3_reg;
	unsigned int cf_rd_tim4_reg;
	unsigned int cf_rd_tim5_reg;
	unsigned int cf_wr_tim0_reg;
	unsigned int cf_wr_tim1_reg;
	unsigned int cf_wr_tim2_reg;
	unsigned int cf_wr_tim3_reg;
	unsigned int cf_wr_tim4_reg;
	unsigned int cf_wr_tim5_reg;
	unsigned int cf_cm_tim0_reg;
	unsigned int cf_cm_tim1_reg;
	unsigned int cf_cm_tim2_reg;
	unsigned int cf_cm_tim3_reg;
	unsigned int cf_cm_tim4_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * SD
 */
__ARMCC_PACK__ struct toss_hwctx_sd_s
{
	unsigned int sd_dma_addr_reg;
	unsigned int sd_blk_sz_reg;	/* half-word */
	unsigned int sd_blk_cnt_reg;	/* half-word */
	unsigned int sd_arg_reg;
	unsigned int sd_xfr_reg;		/* half-word */
	unsigned int sd_host_reg;	/* byte */
	unsigned int sd_pwr_reg;		/* byte */
	unsigned int sd_gap_reg;		/* byte */
	unsigned int sd_wak_reg;		/* byte */
	unsigned int sd_clk_reg;		/* half-word */
	unsigned int sd_tmo_reg;		/* byte */
	unsigned int sd_nisen_reg; 	/* half-word */
	unsigned int sd_eisen_reg;	/* half-word */
	unsigned int sd_nixen_reg;	/* half-word */
	unsigned int sd_eixen_reg;	/* half-word */

	unsigned int sd2_dma_addr_reg;
	unsigned int sd2_blk_sz_reg;	/* half-word */
	unsigned int sd2_blk_cnt_reg;	/* half-word */
	unsigned int sd2_arg_reg;
	unsigned int sd2_xfr_reg;	/* half-word */
	unsigned int sd2_host_reg;	/* byte */
	unsigned int sd2_pwr_reg;	/* byte */
	unsigned int sd2_gap_reg;	/* byte */
	unsigned int sd2_wak_reg;	/* byte */
	unsigned int sd2_clk_reg;	/* half-word */
	unsigned int sd2_tmo_reg;	/* byte */
	unsigned int sd2_nisen_reg;	/* half-word */
	unsigned int sd2_eisen_reg;	/* half-word */
	unsigned int sd2_nixen_reg;	/* half-word */
	unsigned int sd2_eixen_reg;	/* half-word */

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * USBDC
 */
__ARMCC_PACK__ struct toss_hwctx_usbdc_s
{
	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * GDMA
 */
__ARMCC_PACK__ struct toss_hwctx_gdma_s
{
	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * Crypto
 */
__ARMCC_PACK__ struct toss_hwctx_crypto_s
{
	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * VIN
 */
__ARMCC_PACK__ struct toss_hwctx_vin_s
{
	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * VOUT
 */
__ARMCC_PACK__ struct toss_hwctx_vout_s
{
	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * Hardware context of each personality.
 */
__ARMCC_PACK__ struct toss_hwctx_s
{
	struct toss_hwctx_hal_s		hal;
	struct toss_hwctx_vic_s		vic;
	struct toss_hwctx_gpio_s	gpio;
	struct toss_hwctx_timer_s	timer;
	struct toss_hwctx_uart_s	uart;
	struct toss_hwctx_i2c_s		i2c;
	struct toss_hwctx_spi_s		spi;
	struct toss_hwctx_ir_s		ir;
	struct toss_hwctx_adc_s		adc;
	struct toss_hwctx_pwm_s		pwm;
	struct toss_hwctx_wdog_s	wdog;
	struct toss_hwctx_dma_s		dma;
	struct toss_hwctx_i2s_s		i2s;
	struct toss_hwctx_fios_s	fios;
	struct toss_hwctx_sd_s		sd;
	struct toss_hwctx_usbdc_s	usbdc;
	struct toss_hwctx_gdma_s	gdma;
	struct toss_hwctx_crypto_s	crypto;
	struct toss_hwctx_vin_s		vin;
	struct toss_hwctx_vout_s	vout;
} __ATTRIB_PACK__;

#endif
