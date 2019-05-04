/*
 * Copyright 2009-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU Lesser General
 * Public License.  You may obtain a copy of the GNU Lesser General
 * Public License Version 2.1 or later at the following locations:
 *
 * http://www.opensource.org/licenses/lgpl-license.html
 * http://www.gnu.org/copyleft/lgpl.html
 */

#ifndef __ASM_ARCH_IMX_CAAM_H__
#define __ASM_ARCH_IMX_CAAM_H__
#ifdef __KERNEL__

/* Define treatment of red/black keys */
#define RED_KEY 0
#define BLACK_KEY 1

/* Define key encryption/covering options */
#define KEY_COVER_ECB 0	/* cover key in AES-ECB */
#define KEY_COVER_CCM 1 /* cover key with AES-CCM */

/* Define space required for BKEK + MAC tag storage in any blob */
#define BLOB_OVERHEAD (32 + 16)

#define SECMEM_KEYMOD_LEN 8
#define GENMEM_KEYMOD_LEN 16

/* from imx-caam.c */

int imx_caam_key_to_red_blob ( u8 keyauth,
                               u8 *key,
                               unsigned keylen,
                               u8 *keymod,
                               u8 *outbuf );
int imx_caam_red_blob_to_key ( u8 keyauth,
                               u8 *blob,
                               unsigned bloblen,
                               u8 *keymod,
                               u8 *outbuf );

#endif				/* _KERNEL */
#endif				/* __ASM_ARCH_IMX_CAAM_H__ */
