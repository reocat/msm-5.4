/*
 * arch/arm/plat-ambarella/include/plat/crypto.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
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

#ifndef __PLAT_AMBARELLA_CRYPTO_H
#define __PLAT_AMBARELLA_CRYPTO_H

/* ==========================================================================*/
#define AMBARELLA_CRYPTO_ALIGNMENT	(16)
#define AMBARELLA_CRA_PRIORITY		(300)
#define AMBARELLA_COMPOSITE_PRIORITY	(400)

#define AMBA_HW_ENCRYPT_CMD		(0)
#define AMBA_HW_DECRYPT_CMD		(1)

/* ==========================================================================*/
#ifndef __ASSEMBLER__

#if (CHIP_REV == I1)
#define CRYPTO_DATA_64BIT (1)
#define CRYPTO_MODE_SWITCH (0)
#elif (CHIP_REV == A7)
#define CRYPTO_DATA_64BIT (1)
#define CRYPTO_MODE_SWITCH (1)
#else
#define CRYPTO_DATA_64BIT (0)
#define CRYPTO_MODE_SWITCH (0)
#endif

struct ambarella_platform_crypto_info{
	u32					mode_switch;   /*a7 support mode switch between  "Binary Compatibility Mode" and "Non-Binary Compatibility Mode" */
									   /* TODO: And now, haven't support a7 mode switch,the a7 is default mode ( non-binary compatible mode, do not need opcode)*/
									   /* For iONE,this flag is 0, and default mode is non-binary compatible mode, do not need opcode*/
									   /* For a5s, this flag is 0, and default mode is binary compatibility mode,  need opcode*/
									   /* For a7,  this flag is 1, and default mode is non-binary compatible mode, do not need opcode*/
									   /* TODO: And now, haven't support a7 mode switch,*/
									   /* 1: support switch*/
									   /* 0: do not support switch */
	u32					binary_mode;   /* 1: binary compatibility mode need opcode*/
									   /* 0: non-binary compatible mode do not need opcode*/
	u32					data_swap;     /*swap from little to big endian 0:do not need swap  1:need swap */
	u32					reg_64;        /*64 bit register,need memcpy to write or read*/
									   /* 0: 32-bit ,you can use writel*/
									   /* 1: 64-bit ,you should use memcpy to write or read 64-bit at once */
	u32					md5_sha1;      /* 0:do not support md5 or sha1*/
									   /* 1:support md5 and sha1*/
	u32					reserved;
};

/* ==========================================================================*/
extern struct platform_device			ambarella_crypto;

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

