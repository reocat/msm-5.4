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
	u32 vic_int_sel_reg;
	u32 vic_inten_reg;
	u32 vic_soften_reg;
	u32 vic_proten_reg;
	u32 vic_sense_reg;
	u32 vic_bothedge_reg;
	u32 vic_event_reg;

	u32 vic2_int_sel_reg;
	u32 vic2_inten_reg;
	u32 vic2_soften_reg;
	u32 vic2_proten_reg;
	u32 vic2_sense_reg;
	u32 vic2_bothedge_reg;
	u32 vic2_event_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * GPIO
 */
__ARMCC_PACK__ struct toss_hwctx_gpio_s
{
	u32 gpio0_data_reg;
	u32 gpio0_dir_reg;
	u32 gpio0_is_reg;
	u32 gpio0_ibe_reg;
	u32 gpio0_iev_reg;
	u32 gpio0_ie_reg;
	u32 gpio0_afsel_reg;
	u32 gpio0_mask_reg;
	u32 gpio0_enable_reg;

	u32 gpio1_data_reg;
	u32 gpio1_dir_reg;
	u32 gpio1_is_reg;
	u32 gpio1_ibe_reg;
	u32 gpio1_iev_reg;
	u32 gpio1_ie_reg;
	u32 gpio1_afsel_reg;
	u32 gpio1_mask_reg;
	u32 gpio1_enable_reg;

	u32 gpio2_data_reg;
	u32 gpio2_dir_reg;
	u32 gpio2_is_reg;
	u32 gpio2_ibe_reg;
	u32 gpio2_iev_reg;
	u32 gpio2_ie_reg;
	u32 gpio2_afsel_reg;
	u32 gpio2_mask_reg;
	u32 gpio2_enable_reg;

	u32 gpio3_data_reg;
	u32 gpio3_dir_reg;
	u32 gpio3_is_reg;
	u32 gpio3_ibe_reg;
	u32 gpio3_iev_reg;
	u32 gpio3_ie_reg;
	u32 gpio3_afsel_reg;
	u32 gpio3_mask_reg;
	u32 gpio3_enable_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * Timer
 */
__ARMCC_PACK__ struct toss_hwctx_timer_s
{
	u32 timer1_status_reg;
	u32 timer1_reload_reg;
	u32 timer1_match1_reg;
	u32 timer1_match2_reg;
	u32 timer2_status_reg;
	u32 timer2_reload_reg;
	u32 timer2_match1_reg;
	u32 timer2_match2_reg;
	u32 timer3_status_reg;
	u32 timer3_reload_reg;
	u32 timer3_match1_reg;
	u32 timer3_match2_reg;
	u32 timer_ctr_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * UART
 */
__ARMCC_PACK__ struct toss_hwctx_uart_s
{
	u32 uart0_ie_reg;
	u32 uart0_dll_reg;
	u32 uart0_dlh_reg;
	u32 uart0_fc_reg;
	u32 uart0_lc_reg;
	u32 uart0_mc_reg;
	u32 uart0_sc_reg;	/* byte */

	u32 uart1_ie_reg;
	u32 uart1_dll_reg;
	u32 uart1_dlh_reg;
	u32 uart1_fc_reg;
	u32 uart1_lc_reg;
	u32 uart1_mc_reg;
	u32 uart1_sc_reg;	/* byte */

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * I2C
 */
__ARMCC_PACK__ struct toss_hwctx_i2c_s
{
	u32 idc_enr_reg;
	u32 idc_ctrl_reg;
	u32 idc_psll_reg;
	u32 idc_pslh_reg;
	u32 idc_fmctrl_reg;

	u32 idc2_enr_reg;
	u32 idc2_ctrl_reg;
	u32 idc2_psll_reg;
	u32 idc2_pslh_reg;
	u32 idc2_fmctrl_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * SPI
 */
__ARMCC_PACK__ struct toss_hwctx_spi_s
{
	u32 spi_ctrlr0_reg;
	u32 spi_ctrlr1_reg;
	u32 spi_ssienr_reg;
	u32 spi_mwcr_reg;
	u32 spi_ser_reg;
	u32 spi_baudr_reg;
	u32 spi_txftlr_reg;
	u32 spi_rxftlr_reg;
	u32 spi_txflr_reg;
	u32 spi_rxflr_reg;
	u32 spi_sr_reg;
	u32 spi_imr_reg;

	u32 spi2_ctrlr0_reg;
	u32 spi2_ctrlr1_reg;
	u32 spi2_ssienr_reg;
	u32 spi2_mwcr_reg;
	u32 spi2_ser_reg;
	u32 spi2_baudr_reg;
	u32 spi2_txftlr_reg;
	u32 spi2_rxftlr_reg;
	u32 spi2_txflr_reg;
	u32 spi2_rxflr_reg;
	u32 spi2_sr_reg;
	u32 spi2_imr_reg;

	u32 spi_slave_ctrlr0_reg;
	u32 spi_slave_ctrlr1_reg;
	u32 spi_slave_ssienr_reg;
	u32 spi_slave_mwcr_reg;
	u32 spi_slave_ser_reg;
	u32 spi_slave_baudr_reg;
	u32 spi_slave_txftlr_reg;
	u32 spi_slave_rxftlr_reg;
	u32 spi_slave_txflr_reg;
	u32 spi_slave_rxflr_reg;
	u32 spi_slave_sr_reg;
	u32 spi_slave_imr_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * IR
 */
__ARMCC_PACK__ struct toss_hwctx_ir_s
{
	u32 ir_control_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * ADC
 */
__ARMCC_PACK__ struct toss_hwctx_adc_s
{
	u32 adc_control_reg;
	u32 adc_enable_reg;
	u32 adc_chan0_intr_reg;
	u32 adc_chan1_intr_reg;
	u32 adc_chan2_intr_reg;
	u32 adc_chan3_intr_reg;
	u32 adc_chan4_intr_reg;
	u32 adc_chan5_intr_reg;
	u32 adc_chan6_intr_reg;
	u32 adc_chan7_intr_reg;

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
	u32 i2s_mode_reg;
	u32 i2s_rx_ctrl_reg;
	u32 i2s_tx_ctrl_reg;
	u32 i2s_wlen_reg;
	u32 i2s_wpos_reg;
	u32 i2s_slot_reg;
	u32 i2s_tx_fifo_lth_reg;
	u32 i2s_rx_fifo_gth_reg;
	u32 i2s_clock_reg;
	u32 i2s_init_reg;
	u32 i2s_tx_int_enable_reg;
	u32 i2s_rx_int_enable_reg;
	u32 i2s_rx_echo_reg;
	u32 i2s_24bitmux_mode_reg;
	u32 i2s_gateoff_reg;
	u32 i2s_channel_select_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * FIOS
 */
__ARMCC_PACK__ struct toss_hwctx_fios_s
{
	u32 fio_ctr_reg;
	u32 fio_sta_reg;
	u32 fio_dmactr_reg;
	u32 fio_dmaadr_reg;
	u32 fio_dmasta_reg;

	u32 flash_ctr_reg;
	u32 flash_tim0_reg;
	u32 flash_tim1_reg;
	u32 flash_tim2_reg;
	u32 flash_tim3_reg;
	u32 flash_tim4_reg;
	u32 flash_tim5_reg;

	u32 xd_ctr_reg;
	u32 xd_tim0_reg;
	u32 xd_tim1_reg;
	u32 xd_tim2_reg;
	u32 xd_tim3_reg;
	u32 xd_tim4_reg;
	u32 xd_tim5_reg;

	u32 cf_ctr_reg;
	u32 cf_rd_tim0_reg;
	u32 cf_rd_tim1_reg;
	u32 cf_rd_tim2_reg;
	u32 cf_rd_tim3_reg;
	u32 cf_rd_tim4_reg;
	u32 cf_rd_tim5_reg;
	u32 cf_wr_tim0_reg;
	u32 cf_wr_tim1_reg;
	u32 cf_wr_tim2_reg;
	u32 cf_wr_tim3_reg;
	u32 cf_wr_tim4_reg;
	u32 cf_wr_tim5_reg;
	u32 cf_cm_tim0_reg;
	u32 cf_cm_tim1_reg;
	u32 cf_cm_tim2_reg;
	u32 cf_cm_tim3_reg;
	u32 cf_cm_tim4_reg;

	unsigned int valid;
} __ATTRIB_PACK__;

/*
 * SD
 */
__ARMCC_PACK__ struct toss_hwctx_sd_s
{
	u32 sd_dma_addr_reg;
	u32 sd_blk_sz_reg;	/* half-word */
	u32 sd_blk_cnt_reg;	/* half-word */
	u32 sd_arg_reg;
	u32 sd_xfr_reg;		/* half-word */
	u32 sd_host_reg;	/* byte */
	u32 sd_pwr_reg;		/* byte */
	u32 sd_gap_reg;		/* byte */
	u32 sd_wak_reg;		/* byte */
	u32 sd_clk_reg;		/* half-word */
	u32 sd_tmo_reg;		/* byte */
	u32 sd_nisen_reg; 	/* half-word */
	u32 sd_eisen_reg;	/* half-word */
	u32 sd_nixen_reg;	/* half-word */
	u32 sd_eixen_reg;	/* half-word */

	u32 sd2_dma_addr_reg;
	u32 sd2_blk_sz_reg;	/* half-word */
	u32 sd2_blk_cnt_reg;	/* half-word */
	u32 sd2_arg_reg;
	u32 sd2_xfr_reg;	/* half-word */
	u32 sd2_host_reg;	/* byte */
	u32 sd2_pwr_reg;	/* byte */
	u32 sd2_gap_reg;	/* byte */
	u32 sd2_wak_reg;	/* byte */
	u32 sd2_clk_reg;	/* half-word */
	u32 sd2_tmo_reg;	/* byte */
	u32 sd2_nisen_reg;	/* half-word */
	u32 sd2_eisen_reg;	/* half-word */
	u32 sd2_nixen_reg;	/* half-word */
	u32 sd2_eixen_reg;	/* half-word */

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
