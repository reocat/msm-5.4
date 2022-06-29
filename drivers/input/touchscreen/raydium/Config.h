/* config.h
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
 
#ifndef __CONFIG_H
#define __CONFIG_H


/*****************************************************************************
**                            GLOBAL MARCO DEFINITION
******************************************************************************/
#define MINIMUM(x, y) (((x) < (y)) ? (x) : (y)) //  Compares two parameters, return minimum.
#define MAXIMUM(x, y) (((x) > (y)) ? (x) : (y)) //  Compares two parameters, return maximum.
#define ABS(x) ((x) >= 0 ? (x) : -(x))          // return absolute value

/*****************************************************************************
**                            GLOBAL FUNCTIONAL DEFINITION
******************************************************************************/

//Bootloader
#define BOOTLOADER                          1  //1: with bootloader, 0: without bootloader
#define SELFTEST                       	     1  //1: For System Selftest, 0:For Dongle Open/Short Tool
#if SELFTEST
#define SELFTEST_3X	1
#define	SELFTEST_2X	0
#else
#define SELFTEST_3X	0
#define	SELFTEST_2X	0
#endif
#define ENABLE_TEST_TIME_MEASURMENT		1
#define ENABLE_TEST_TIME_MEASURMENT_CC		0
#define ENABLE_WDT 				0
#define ENABLE_TEST_RSU_DATA_SHOW		0
#define ENABLE_AUO_VERIFY_LOG			0
#define ENABLE_CONTROL_OPENSHORT_WDT		0
#define ENABLE_TEST_GPIO_MEASURMENT		0
#endif /* end __CONFIG_H */
/*****************************************************************************
**                            End Of File
******************************************************************************/
