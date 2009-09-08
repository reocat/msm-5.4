/*
 * arch/arm/mach-ambarella/include/mach/crypto.h
 *
 * History:
 *	2009/2/27 - [Jack Huang] created file
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

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <mach/hardware.h> instead"
#endif

#ifndef __ASM_ARCH_CRYPTO_H__
#define __ASM_ARCH_CRYPTO_H__

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/


/****************************************************/
/* Controller registers definitions                 */
/****************************************************/
#define CRYPT_D_HI_OFFSET			0x00
#define CRYPT_D_LO_OFFSET			0x04
#define CRYPT_D_INPUT_HI_OFFSET			0x08
#define CRYPT_D_INPUT_LO_OFFSET			0x0c
#define CRYPT_D_OPCODE_OFFSET			0x10
#define CRYPT_D_OUTPUT_READY_OFFSET		0x14
#define CRYPT_D_OUTPUT_HI_OFFSET		0x18
#define CRYPT_D_OUTPUT_LO_OFFSET		0x1c
#define CRYPT_D_INT_EN_OFFSET			0x20
#define CRYPT_A_128_96_OFFSET			0x24
#define CRYPT_A_128_64_OFFSET			0x28
#define CRYPT_A_128_32_OFFSET			0x2c
#define CRYPT_A_128_0_OFFSET			0x30
#define CRYPT_A_192_160_OFFSET			0x34
#define CRYPT_A_192_128_OFFSET			0x38
#define CRYPT_A_192_96_OFFSET			0x3c
#define CRYPT_A_192_64_OFFSET			0x40
#define CRYPT_A_192_32_OFFSET			0x44
#define CRYPT_A_192_0_OFFSET			0x48
#define CRYPT_A_256_224_OFFSET			0x4c
#define CRYPT_A_256_192_OFFSET			0x50
#define CRYPT_A_256_160_OFFSET			0x54
#define CRYPT_A_256_128_OFFSET			0x58
#define CRYPT_A_256_96_OFFSET			0x5c
#define CRYPT_A_256_64_OFFSET			0x60
#define CRYPT_A_256_32_OFFSET			0x64
#define CRYPT_A_256_0_OFFSET			0x68
#define CRYPT_A_INPUT_96_OFFSET			0x6c
#define CRYPT_A_INPUT_64_OFFSET			0x70
#define CRYPT_A_INPUT_32_OFFSET			0x74
#define CRYPT_A_INPUT_0_OFFSET			0x78
#define CRYPT_A_OPCODE_OFFSET			0x7c
#define CRYPT_A_OUTPUT_READY_OFFSET		0x80
#define CRYPT_A_OUTPUT_96_OFFSET		0x84
#define CRYPT_A_OUTPUT_64_OFFSET		0x88
#define CRYPT_A_OUTPUT_32_OFFSET		0x8c
#define CRYPT_A_OUTPUT_0_OFFSET			0x90
#define CRYPT_A_INT_EN_OFFSET			0x94

#define CRYPT_D_HI_REG				CRYPT_UNIT_REG(0x00)
#define CRYPT_D_LO_REG				CRYPT_UNIT_REG(0x04)
#define CRYPT_D_INPUT_HI_REG			CRYPT_UNIT_REG(0x08)
#define CRYPT_D_INPUT_LO_REG			CRYPT_UNIT_REG(0x0c)
#define CRYPT_D_OPCODE_REG			CRYPT_UNIT_REG(0x10)
#define CRYPT_D_OUTPUT_READY_REG		CRYPT_UNIT_REG(0x14)
#define CRYPT_D_OUTPUT_HI_REG			CRYPT_UNIT_REG(0x18)
#define CRYPT_D_OUTPUT_LO_REG			CRYPT_UNIT_REG(0x1c)
#define CRYPT_D_INT_EN_REG			CRYPT_UNIT_REG(0x20)
#define CRYPT_A_128_96_REG			CRYPT_UNIT_REG(0x24)
#define CRYPT_A_128_64_REG			CRYPT_UNIT_REG(0x28)
#define CRYPT_A_128_32_REG			CRYPT_UNIT_REG(0x2c)
#define CRYPT_A_128_0_REG			CRYPT_UNIT_REG(0x30)
#define CRYPT_A_192_160_REG			CRYPT_UNIT_REG(0x34)
#define CRYPT_A_192_128_REG			CRYPT_UNIT_REG(0x38)
#define CRYPT_A_192_96_REG			CRYPT_UNIT_REG(0x3c)
#define CRYPT_A_192_64_REG			CRYPT_UNIT_REG(0x40)
#define CRYPT_A_192_32_REG			CRYPT_UNIT_REG(0x44)
#define CRYPT_A_192_0_REG			CRYPT_UNIT_REG(0x48)
#define CRYPT_A_256_224_REG			CRYPT_UNIT_REG(0x4c)
#define CRYPT_A_256_192_REG			CRYPT_UNIT_REG(0x50)
#define CRYPT_A_256_160_REG			CRYPT_UNIT_REG(0x54)
#define CRYPT_A_256_128_REG			CRYPT_UNIT_REG(0x58)
#define CRYPT_A_256_96_REG			CRYPT_UNIT_REG(0x5c)
#define CRYPT_A_256_64_REG			CRYPT_UNIT_REG(0x60)
#define CRYPT_A_256_32_REG			CRYPT_UNIT_REG(0x64)
#define CRYPT_A_256_0_REG			CRYPT_UNIT_REG(0x68)
#define CRYPT_A_INPUT_96_REG			CRYPT_UNIT_REG(0x6c)
#define CRYPT_A_INPUT_64_REG			CRYPT_UNIT_REG(0x70)
#define CRYPT_A_INPUT_32_REG			CRYPT_UNIT_REG(0x74)
#define CRYPT_A_INPUT_0_REG			CRYPT_UNIT_REG(0x78)
#define CRYPT_A_OPCODE_REG			CRYPT_UNIT_REG(0x7c)
#define CRYPT_A_OUTPUT_READY_REG			CRYPT_UNIT_REG(0x80)
#define CRYPT_A_OUTPUT_96_REG			CRYPT_UNIT_REG(0x84)
#define CRYPT_A_OUTPUT_64_REG			CRYPT_UNIT_REG(0x88)
#define CRYPT_A_OUTPUT_32_REG			CRYPT_UNIT_REG(0x8c)
#define CRYPT_A_OUTPUT_0_REG			CRYPT_UNIT_REG(0x90)
#define CRYPT_A_INT_EN_REG			CRYPT_UNIT_REG(0x94)

#endif

