/* i2c_drv_global.c
 *
 * Raydium TouchScreen driver.
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.

 * Copyright (c) 2021  Raydium tech Ltd.
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
 * BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Google Inc. or Linaro Ltd. nor the names of
 *    its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written
 *    permission.
 * * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <Config.h>
#include "ic_drv_global.h"
#include "ic_drv_interface.h"
#ifdef __KERNEL__
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#endif

#include "drv_interface.h"



#if !SELFTEST
#include "usb.h"
#include "Descriptors.h"
#endif
/*****************************************************************************
**
**                  Declared Global Variable
**
*****************************************************************************/

/********************* Global *********************/


unsigned short g_u16_dev_id;

unsigned char g_u8_drv_interface;

volatile unsigned char g_u8_gpio_irq_trigger;

unsigned char g_u8_raw_data_buffer[MAX_IMAGE_BUFFER_SIZE * 2];
unsigned short g_u16_raw_data_tmp[MAX_IMAGE_BUFFER_SIZE];

/* IC Test */
short g_i16_raw_data_1_short_buf[MAX_SENSING_PIN_NUM];
short g_i16_raw_data_2_open_buf[MAX_SENSING_PIN_NUM];
unsigned short g_u16_raw_data3_cc_buf[MAX_SENSING_PIN_NUM];
/*CC*/
unsigned short g_u16_uc_buf[MAX_SENSING_PIN_NUM];
unsigned short g_u16_raw_data3_golden_cc_buf[MAX_SENSING_PIN_NUM];
unsigned short g_u16_uc_golden_cc_buf[MAX_SENSING_PIN_NUM];
/* each node test result (bit[0]:open ng, bit[1]:short ng, bit[2]:uniformity ng..etc)*/
unsigned int g_u32_test_result[MAX_SENSING_PIN_NUM]; 
unsigned char g_u8_test_result[MAX_SENSING_PIN_NUM];


unsigned int g_u32_wearable_test_result;
unsigned char g_u8_wearable_pin_map[MAX_IMAGE_BUFFER_SIZE];


unsigned char g_u8_data_buf[DATA_BUFFER_SIZE];
unsigned char g_u8_mipi_read_buf[56];

unsigned char g_u8_channel_x;
unsigned char g_u8_channel_y;

unsigned char g_u8_is_normal_fw;

volatile unsigned short g_u16_test_items_host_cmd;
unsigned short g_u16_test_items_tool_cmd;
unsigned char g_u8_ic_power_on_ng;
char g_i8_test_baseline_msg[30];
volatile unsigned short g_u16_panel_jig_set_test_items;
bool b_is_auo_jig_testing;

#if ENABLE_TEST_TIME_MEASURMENT || ENABLE_TEST_TIME_MEASURMENT_CC
unsigned int g_u32_spend_time;
#endif

unsigned int g_u32_fw_cc_version;
unsigned char g_u8_print_debug_msg;
unsigned char g_u8_display_interface;
unsigned char g_u8_PAGE_ADDR;

int g_u32_dongle_flash_ini_addr; /*0xF000*/
int  g_u32_ini_threshold_addr;/*DONGLE_FLASH_INI_ADDR + 16*/
int  g_u32_ini_para_addr;/*INI_THRESHOLD_ADDR + 36*/
int  g_u32_ini_raw_data_3_cc_addr;/*INI_RAW_DATA2_BL_ADDR+72*/
int  g_u32_ini_uc_cc_addr;/*INI_RAW_DATA_3_CC_ADDR + 72*/

int g_u32_initial_code_start_addr;

void ic_drv_init(void)
{
	g_u16_test_items_host_cmd = 0xFFFF;
	g_u16_test_items_tool_cmd = 0;
	g_u16_dev_id = DEVICE_ID_3X;
	g_u8_print_debug_msg = 0;
	g_u8_display_interface = DISPLAY_TOUCH_2_DRIVER_MODE;
	b_is_auo_jig_testing = false;
#if !SELFTEST
	g_u8_ic_power_on_ng = FALSE;
#endif
}

/*****************************************************************************
**                            End Of File
******************************************************************************/
