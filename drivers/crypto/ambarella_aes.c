/*
 * ambarella_aes.c
 *
 * History:
 *	2009/09/07 - [Qiao Wang]
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
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include "ambarella_aes.h"

static void aes_encrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	struct crypto_aes_ctx *ctx = crypto_tfm_ctx(tfm);
	const __le32 *src = (const __le32 *)in;
	__le32 *dst = (__le32 *)out;
	u32 ready;

	switch (ctx->key_length) {
	case 16:
		amba_writew(CRYPT_A_128_0_REG,   ctx->key_enc[0]);
		amba_writew(CRYPT_A_128_32_REG,  ctx->key_enc[1]);
		amba_writew(CRYPT_A_128_64_REG,  ctx->key_enc[2]);
		amba_writew(CRYPT_A_128_96_REG,  ctx->key_enc[3]);
		break;
	case 24:
		amba_writew(CRYPT_A_192_0_REG,   ctx->key_enc[0]);
		amba_writew(CRYPT_A_192_32_REG,  ctx->key_enc[1]);
		amba_writew(CRYPT_A_192_64_REG,  ctx->key_enc[2]);
		amba_writew(CRYPT_A_192_96_REG,  ctx->key_enc[3]);
		amba_writew(CRYPT_A_192_128_REG, ctx->key_enc[4]);
		amba_writew(CRYPT_A_192_160_REG, ctx->key_enc[5]);
		break;
	case 32:
		amba_writew(CRYPT_A_256_0_REG,   ctx->key_enc[0]);
		amba_writew(CRYPT_A_256_32_REG,  ctx->key_enc[1]);
		amba_writew(CRYPT_A_256_64_REG,  ctx->key_enc[2]);
		amba_writew(CRYPT_A_256_96_REG,  ctx->key_enc[3]);
		amba_writew(CRYPT_A_256_128_REG, ctx->key_enc[4]);
		amba_writew(CRYPT_A_256_160_REG, ctx->key_enc[5]);
		amba_writew(CRYPT_A_256_192_REG, ctx->key_enc[6]);
		amba_writew(CRYPT_A_256_224_REG, ctx->key_enc[7]);
		break;
	}
	amba_writew(CRYPT_A_OPCODE_REG, AMBA_HW_ENCRYPT_CMD);

	amba_writew(CRYPT_A_INPUT_0_REG, src[0]);
	amba_writew(CRYPT_A_INPUT_32_REG, src[0]);
	amba_writew(CRYPT_A_INPUT_64_REG, src[0]);
	amba_writew(CRYPT_A_INPUT_96_REG, src[0]);

	do{
		ready = amba_readw(CRYPT_A_OUTPUT_READY_REG);
	}while(ready != 1);

	dst[0] = amba_readw(CRYPT_A_OUTPUT_0_REG);
	dst[1] = amba_readw(CRYPT_A_OUTPUT_32_REG);
	dst[2] = amba_readw(CRYPT_A_OUTPUT_64_REG);
	dst[3] = amba_readw(CRYPT_A_OUTPUT_96_REG);

}

static void aes_decrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	struct crypto_aes_ctx *ctx = crypto_tfm_ctx(tfm);
	const __le32 *src = (const __le32 *)in;
	__le32 *dst = (__le32 *)out;
	u32 ready;

	switch (ctx->key_length) {
	case 16:
		amba_writew(CRYPT_A_128_0_REG,   ctx->key_dec[0]);
		amba_writew(CRYPT_A_128_32_REG,  ctx->key_dec[1]);
		amba_writew(CRYPT_A_128_64_REG,  ctx->key_dec[2]);
		amba_writew(CRYPT_A_128_96_REG,  ctx->key_dec[3]);
		break;
	case 24:
		amba_writew(CRYPT_A_192_0_REG,   ctx->key_dec[0]);
		amba_writew(CRYPT_A_192_32_REG,  ctx->key_dec[1]);
		amba_writew(CRYPT_A_192_64_REG,  ctx->key_dec[2]);
		amba_writew(CRYPT_A_192_96_REG,  ctx->key_dec[3]);
		amba_writew(CRYPT_A_192_128_REG, ctx->key_dec[4]);
		amba_writew(CRYPT_A_192_160_REG, ctx->key_dec[5]);
		break;
	case 32:
		amba_writew(CRYPT_A_256_0_REG,   ctx->key_dec[0]);
		amba_writew(CRYPT_A_256_32_REG,  ctx->key_dec[1]);
		amba_writew(CRYPT_A_256_64_REG,  ctx->key_dec[2]);
		amba_writew(CRYPT_A_256_96_REG,  ctx->key_dec[3]);
		amba_writew(CRYPT_A_256_128_REG, ctx->key_dec[4]);
		amba_writew(CRYPT_A_256_160_REG, ctx->key_dec[5]);
		amba_writew(CRYPT_A_256_192_REG, ctx->key_dec[6]);
		amba_writew(CRYPT_A_256_224_REG, ctx->key_dec[7]);
		break;
	}
	amba_writew(CRYPT_A_OPCODE_REG, AMBA_HW_DECRYPT_CMD);

	amba_writew(CRYPT_A_INPUT_0_REG, src[0]);
	amba_writew(CRYPT_A_INPUT_32_REG, src[0]);
	amba_writew(CRYPT_A_INPUT_64_REG, src[0]);
	amba_writew(CRYPT_A_INPUT_96_REG, src[0]);

	do{
		ready = amba_readw(CRYPT_A_OUTPUT_READY_REG);
	}while(ready != 1);

	dst[0] = amba_readw(CRYPT_A_OUTPUT_0_REG);
	dst[1] = amba_readw(CRYPT_A_OUTPUT_32_REG);
	dst[2] = amba_readw(CRYPT_A_OUTPUT_64_REG);
	dst[3] = amba_readw(CRYPT_A_OUTPUT_96_REG);

}

static struct crypto_alg aes_alg = {
	.cra_name		=	"aes",
	.cra_driver_name	=	"aes-ambarella",
	.cra_priority		=	AMBARELLA_CRA_PRIORITY,
	.cra_flags		=	CRYPTO_ALG_TYPE_CIPHER,
	.cra_blocksize		=	AES_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct crypto_aes_ctx),
	.cra_alignmask		=	AMBARELLA_ALIGNMENT - 1,
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(aes_alg.cra_list),
	.cra_u			=	{
		.cipher = {
			.cia_min_keysize	=	AES_MIN_KEY_SIZE,
			.cia_max_keysize	=	AES_MAX_KEY_SIZE,
			.cia_setkey	   		= 	crypto_aes_set_key,
			.cia_encrypt	 	=	aes_encrypt,
			.cia_decrypt	  	=	aes_decrypt,
		}
	}
};

static int ecb_aes_encrypt(struct blkcipher_desc *desc,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	return 0;
}

static int ecb_aes_decrypt(struct blkcipher_desc *desc,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	int err = 0;
	return err;
}

static struct crypto_alg ecb_aes_alg = {
	.cra_name		=	"ecb(aes)",
	.cra_driver_name	=	"ecb-aes-ambarella",
	.cra_priority		=	AMBARELLA_COMPOSITE_PRIORITY,
	.cra_flags		=	CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		=	AES_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct crypto_aes_ctx),
	.cra_alignmask		=	AMBARELLA_ALIGNMENT - 1,
	.cra_type		=	&crypto_blkcipher_type,
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(ecb_aes_alg.cra_list),
	.cra_u			=	{
		.blkcipher = {
			.min_keysize		=	AES_MIN_KEY_SIZE,
			.max_keysize		=	AES_MAX_KEY_SIZE,
			.setkey	   		= 	crypto_aes_set_key,
			.encrypt		=	ecb_aes_encrypt,
			.decrypt		=	ecb_aes_decrypt,
		}
	}
};

static int cbc_aes_encrypt(struct blkcipher_desc *desc,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	int err = 0;

	return err;
}

static int cbc_aes_decrypt(struct blkcipher_desc *desc,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	int err = 0;

	return err;
}

static struct crypto_alg cbc_aes_alg = {
	.cra_name		=	"cbc(aes)",
	.cra_driver_name	=	"cbc-aes-ambarella",
	.cra_priority		=	AMBARELLA_COMPOSITE_PRIORITY,
	.cra_flags		=	CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		=	AES_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct crypto_aes_ctx),
	.cra_alignmask		=	AMBARELLA_ALIGNMENT - 1,
	.cra_type		=	&crypto_blkcipher_type,
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(cbc_aes_alg.cra_list),
	.cra_u			=	{
		.blkcipher = {
			.min_keysize		=	AES_MIN_KEY_SIZE,
			.max_keysize		=	AES_MAX_KEY_SIZE,
			.ivsize			=	AES_BLOCK_SIZE,
			.setkey	   		= 	crypto_aes_set_key,
			.encrypt		=	cbc_aes_encrypt,
			.decrypt		=	cbc_aes_decrypt,
		}
	}
};

static int __init ambarella_init(void)
{
	int ret;

	if ((ret = crypto_register_alg(&aes_alg)))
		goto aes_err;
	printk(KERN_NOTICE PFX "Using Ambarella hw engine for AES algorithm.\n");

out:
	return ret;
aes_err:
	printk(KERN_ERR PFX "Ambarella engine AES initialization failed.\n");
	goto out;
}

static void __exit ambarella_fini(void)
{
	crypto_unregister_alg(&aes_alg);
}

module_init(ambarella_init);
module_exit(ambarella_fini);

MODULE_DESCRIPTION("Ambarella HW AES algorithm support");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qiao Wang");

MODULE_ALIAS("aes-all");

