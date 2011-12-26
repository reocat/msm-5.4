/*
 * ambarella_crypto.c
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
#include <crypto/des.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/platform_device.h>

#include <mach/hardware.h>
#include <plat/crypto.h>

static DECLARE_COMPLETION(g_aes_irq_wait);
static DECLARE_COMPLETION(g_des_irq_wait);

static int config_polling_mode = 0;
module_param(config_polling_mode, int, S_IRUGO);

static const char *ambdev_name =
	"Ambarella Media Processor Cryptography Engine";

struct des_ctx {
	u32 expkey[DES_EXPKEY_WORDS];
};

struct ambarella_platform_crypto_info *platform_info;

//merge temp buffer
#define MAX_MERGE_SIZE 8
__attribute__ ((aligned (4))) u32 merg[MAX_MERGE_SIZE]={};

struct ambarella_crypto_dev_info {
	unsigned char __iomem 		*regbase;

	struct platform_device		*pdev;
	struct resource				*mem;
	unsigned int				aes_irq;
	unsigned int				des_irq;

	struct ambarella_platform_crypto_info *platform_info;
};
struct aes_fun_t{
	void (*opcode)(u32);
	void (*wdata)(u32*,u32*,int);
	void (*rdata)(u32*,u32*,int);
	void (*reg_enc)(u32 **offset);
	void (*reg_dec)(u32 **offset);
}aes_fun;
void aes_reg_enc_dec_32(u32 **offset)
{
#if (CRYPTO_DATA_64BIT == 0)
	*offset = (u32*)CRYPT_A_INPUT_96_REG;
#endif
}
void aes_reg_enc_64(u32 **offset)
{
#if (CRYPTO_DATA_64BIT == 1)
	*offset = (u32*)CRYPT_A_INPUT_ENC_96_REG;
#endif
}

void aes_reg_dec_64(u32 **offset)
{
#if (CRYPTO_DATA_64BIT == 1)
	*offset = (u32*)CRYPT_A_INPUT_DEC_96_REG;
#endif
}

void swap_write(u32 *offset,u32 *src,int size)
{
	int point=0;
	int len=size;

	for(size=(size>>2)-1;size >= 0;size--,point++){
		*(merg+point) = ntohl(*(src+size));
	}
	//the iONE registers need to accessed by 64-bit boundaries,so this place must use number as the size of memcpy.
	if (len == 8){
		memcpy(offset,merg,8);
	}else if (len == 16){
		memcpy(offset,merg,16);
	}else if (len == 24){
		memcpy(offset,merg,24);
	}else if (len == 32){
		memcpy(offset,merg,32);
	}
}

void swap_read(u32 *offset,u32 *src,int size)
{
	int point=0,len=size;

	//the iONE registers need to accessed by 64-bit boundaries,so this place must use number as the size of memcpy.
	if (len == 8){
		memcpy(merg,src,8);
	}else if (len == 16){
		memcpy(merg,src,16);
	}else if (len == 24){
		memcpy(merg,src,24);
	}else if (len == 32){
		memcpy(merg,src,32);
	}
	for(size=(size>>2)-1;size >= 0;size--,point++){
		*(offset+point) = ntohl(*(merg+size));
	}
}

void aes_opcode(u32 flag)
{
#if (CRYPTO_DATA_64BIT == 0)
	amba_writel(CRYPT_A_OPCODE_REG, flag);
#endif

}

void null_fun(u32 flag) {};

static void aes_encrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	struct crypto_aes_ctx *ctx=crypto_tfm_ctx(tfm);
	const __le32 *src = (const __le32 *)in;
	__le32 *dst = (__le32 *)out;
	u32 ready;
	u32 *offset=NULL;
	switch (ctx->key_length){
	case 16:
		offset = (u32*)CRYPT_A_128_96_REG;
		aes_fun.wdata(offset,ctx->key_enc,16);
		break;
	case 24:
		offset = (u32*)CRYPT_A_192_160_REG;
		aes_fun.wdata(offset,ctx->key_enc,24);
		break;
	case 32:
		offset = (u32*)CRYPT_A_256_224_REG;
		aes_fun.wdata(offset,ctx->key_enc,32);
		break;
	}
	//enc or dec option mode
	aes_fun.opcode(AMBA_HW_ENCRYPT_CMD);

	//get the input offset
	aes_fun.reg_enc(&offset);

	//input the src
	aes_fun.wdata(offset,(u32*)src,16);

	if(likely(config_polling_mode == 0)) {
		wait_for_completion_interruptible(&g_aes_irq_wait);
	}else{
		do{
			ready = amba_readl(CRYPT_A_OUTPUT_READY_REG);
		}while(ready != 1);
	}

	//get the output
	offset = (u32*)CRYPT_A_OUTPUT_96_REG;
	aes_fun.rdata(dst,offset,16);
}


static void aes_decrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	struct crypto_aes_ctx *ctx = crypto_tfm_ctx(tfm);
	const __le32 *src = (const __le32 *)in;
	__le32 *dst = (__le32 *)out;
	u32 ready;
	u32 *offset=NULL;
	switch (ctx->key_length) {
	case 16:
		offset = (u32*)CRYPT_A_128_96_REG;
		aes_fun.wdata(offset,ctx->key_enc,16);
	        break;
	case 24:
		offset = (u32*)CRYPT_A_192_160_REG;
		aes_fun.wdata(offset,ctx->key_enc,24);
	        break;
	case 32:
		offset = (u32*)CRYPT_A_256_224_REG;
		aes_fun.wdata(offset,ctx->key_enc,32);
	        break;
	}
	//enc or dec option mode
	aes_fun.opcode(AMBA_HW_DECRYPT_CMD);

	//get the input offset
	aes_fun.reg_dec(&offset);

	//input the src
	aes_fun.wdata(offset,(u32*)src,16);

	if(likely(config_polling_mode == 0)) {
		wait_for_completion_interruptible(&g_aes_irq_wait);
	}else{
		do{
			ready = amba_readl(CRYPT_A_OUTPUT_READY_REG);
		}while(ready != 1);
	}

	//get the output
	offset = (u32*)CRYPT_A_OUTPUT_96_REG;
	aes_fun.rdata(dst,offset,16);
}

static struct crypto_alg aes_alg = {
	.cra_name		=	"aes",
	.cra_driver_name	=	"aes-ambarella",
	.cra_priority		=	AMBARELLA_CRA_PRIORITY,
	.cra_flags		=	CRYPTO_ALG_TYPE_CIPHER,
	.cra_blocksize		=	AES_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct crypto_aes_ctx),
	.cra_alignmask		=	AMBARELLA_CRYPTO_ALIGNMENT - 1,
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

struct des_fun_t{
	void (*opcode)(u32);
	void (*wdata)(u32*,u32*,int);
	void (*rdata)(u32*,u32*,int);
	void (*reg_enc)(u32 **offset);
	void (*reg_dec)(u32 **offset);
}des_fun;

void des_reg_enc_dec_32(u32 **offset)
{
#if (CRYPTO_DATA_64BIT == 0)
	*offset = (u32*)CRYPT_D_INPUT_HI_REG;
#endif
}

void des_reg_enc_64(u32 **offset)
{
#if (CRYPTO_DATA_64BIT == 1)
	*offset = (u32*)CRYPT_D_INPUT_ENC_HI_REG;
#endif
}

void des_reg_dec_64(u32 **offset)
{
#if (CRYPTO_DATA_64BIT == 1)
	*offset = (u32*)CRYPT_D_INPUT_DEC_HI_REG;
#endif
}

void des_opcode(u32 flag)
{
#if (CRYPTO_DATA_64BIT == 0)
	amba_writel(CRYPT_D_OPCODE_REG, flag);
#endif
}

static int des_setkey(struct crypto_tfm *tfm, const u8 *key,
		      unsigned int keylen)
{
	struct des_ctx *dctx = crypto_tfm_ctx(tfm);
	u32 *flags = &tfm->crt_flags;
	u32 tmp[DES_EXPKEY_WORDS];
	int ret;

	/* Expand to tmp */
	ret = des_ekey(tmp, key);

	if (unlikely(ret == 0) && (*flags & CRYPTO_TFM_REQ_WEAK_KEY)) {
		*flags |= CRYPTO_TFM_RES_WEAK_KEY;
		return -EINVAL;
	}

	/* Copy to output */
	memcpy(dctx->expkey, key, keylen);

	return 0;
}

static void des_encrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	struct des_ctx *ctx = crypto_tfm_ctx(tfm);
	const __le32 *src = (const __le32 *)in;
	__le32 *dst = (__le32 *)out;
	u32 ready;
	u32 *offset=NULL;
	//set key
	des_fun.wdata((u32*)CRYPT_D_HI_REG,ctx->expkey,8);

	//enc or dec option mode
	des_fun.opcode(AMBA_HW_ENCRYPT_CMD);

	//get the input offset
	des_fun.reg_enc(&offset);

	//input the src
	aes_fun.wdata(offset,(u32*)src,8);

	if(likely(config_polling_mode == 0)) {
		wait_for_completion_interruptible(&g_des_irq_wait);
	}else{
		do{
			ready = amba_readl(CRYPT_D_OUTPUT_READY_REG);
		}while(ready != 1);
	}

	//get the output
	offset = (u32*)CRYPT_D_OUTPUT_HI_REG;
	aes_fun.rdata(dst,offset,8);
}

static void des_decrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	struct des_ctx *ctx = crypto_tfm_ctx(tfm);
	const __le32 *src = (const __le32 *)in;
	__le32 *dst = (__le32 *)out;
	u32 ready;
	u32 *offset=NULL;

	//set key
	des_fun.wdata((u32*)CRYPT_D_HI_REG,ctx->expkey,8);

	//enc or dec option mode
	des_fun.opcode(AMBA_HW_DECRYPT_CMD);

	//get the input offset
	des_fun.reg_dec(&offset);

	//input the src
	aes_fun.wdata(offset,(u32*)src,8);

	if(likely(config_polling_mode == 0)) {
		wait_for_completion_interruptible(&g_des_irq_wait);
	}else{
		do{
			ready = amba_readl(CRYPT_D_OUTPUT_READY_REG);
		}while(ready != 1);
	}

	//get the output
	offset = (u32*)CRYPT_D_OUTPUT_HI_REG;
	aes_fun.rdata(dst,offset,8);

}


static struct crypto_alg des_alg = {
	.cra_name		=	"des",
	.cra_driver_name	=	"des-ambarella",
	.cra_priority		=	AMBARELLA_CRA_PRIORITY,
	.cra_flags		=	CRYPTO_ALG_TYPE_CIPHER,
	.cra_blocksize		=	DES_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct des_ctx),
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(des_alg.cra_list),
	.cra_u			=	{
		.cipher = {
			.cia_min_keysize	=	DES_KEY_SIZE,
			.cia_max_keysize	=	DES_KEY_SIZE,
			.cia_setkey		=	des_setkey,
			.cia_encrypt		=	des_encrypt,
			.cia_decrypt		=	des_decrypt
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
	.cra_alignmask		=	AMBARELLA_CRYPTO_ALIGNMENT - 1,
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
	.cra_alignmask		=	AMBARELLA_CRYPTO_ALIGNMENT - 1,
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

static irqreturn_t ambarella_aes_irq(int irqno, void *dev_id)
{
	complete(&g_aes_irq_wait);

	return IRQ_HANDLED;
}
static irqreturn_t ambarella_des_irq(int irqno, void *dev_id)
{
	complete(&g_des_irq_wait);

	return IRQ_HANDLED;
}

static int __devinit ambarella_crypto_probe(struct platform_device *pdev)
{
	int	errCode;
	int	aes_irq, des_irq;
	struct resource	*mem = 0;
	struct resource	*ioarea;
	struct ambarella_crypto_dev_info *pinfo = 0;

	platform_info = (struct ambarella_platform_crypto_info *)pdev->dev.platform_data;
	if (platform_info == NULL) {
		dev_err(&pdev->dev, "%s: Can't get platform_data!\n", __func__);
		errCode = - EPERM;
		goto crypto_errCode_na;
	}

	if(likely(config_polling_mode == 0)) {
		mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "registers");
		if (mem == NULL) {
			dev_err(&pdev->dev, "Get crypto mem resource failed!\n");
			errCode = -ENXIO;
			goto crypto_errCode_na;
		}

		aes_irq = platform_get_irq_byname(pdev,"aes-irq");
		if (aes_irq == -ENXIO) {
			dev_err(&pdev->dev, "Get crypto aes irq resource failed!\n");
			errCode = -ENXIO;
			goto crypto_errCode_na;
		}

		des_irq = platform_get_irq_byname(pdev,"des-irq");
		if (des_irq == -ENXIO) {
			dev_err(&pdev->dev, "Get crypto des irq resource failed!\n");
			errCode = -ENXIO;
			goto crypto_errCode_na;
		}

		ioarea = request_mem_region(mem->start, (mem->end - mem->start) + 1, pdev->name);
		if (ioarea == NULL) {
			dev_err(&pdev->dev, "Request crypto ioarea failed!\n");
			errCode = -EBUSY;
			goto crypto_errCode_na;
		}

		pinfo = kzalloc(sizeof(struct ambarella_crypto_dev_info), GFP_KERNEL);
		if (pinfo == NULL) {
			dev_err(&pdev->dev, "Out of memory!\n");
			errCode = -ENOMEM;
			goto crypto_errCode_ioarea;
		}

		pinfo->regbase = (unsigned char __iomem *)mem->start;
		pinfo->mem = mem;
		pinfo->pdev = pdev;
		pinfo->aes_irq = aes_irq;
		pinfo->des_irq = des_irq;
		pinfo->platform_info = platform_info;

		platform_set_drvdata(pdev, pinfo);

		amba_writel(CRYPT_A_INT_EN_REG, 0x0001);
		amba_writel(CRYPT_D_INT_EN_REG, 0x0001);

		errCode = request_irq(pinfo->aes_irq, ambarella_aes_irq,
			IRQF_TRIGGER_RISING, dev_name(&pdev->dev), pinfo);
		if (errCode) {
			dev_err(&pdev->dev,
				"%s: Request aes IRQ failed!\n", __func__);
			goto crypto_errCode_kzalloc;
		}

		errCode = request_irq(pinfo->des_irq, ambarella_des_irq,
			IRQF_TRIGGER_RISING, dev_name(&pdev->dev), pinfo);
		if (errCode) {
			dev_err(&pdev->dev,
				"%s: Request des IRQ failed!\n", __func__);
			goto crypto_errCode_free_aes_irq;
		}
	}
	//TODO: need to add a7 mode switch.now, it's default as compatibility mode
#if (CRYPTO_MODE_SWITCH == 1)
	amba_writel(CRYPT_BINARY_COMP_REG, 0x0000); // set a7 to compatibility mode  
#endif

	if (platform_info->binary_mode == 1){
		aes_fun.opcode = aes_opcode;
		des_fun.opcode = des_opcode;
	}else{
		aes_fun.opcode = null_fun;
		des_fun.opcode = null_fun;
	}

	if (platform_info->data_swap == 1){
		aes_fun.wdata = swap_write;
		aes_fun.rdata = swap_read;
		des_fun.wdata = swap_write;
		des_fun.rdata = swap_read;
	}else{
		aes_fun.wdata = (void*)memcpy;
		aes_fun.rdata = (void*)memcpy;
		des_fun.wdata = (void*)memcpy;
		des_fun.rdata = (void*)memcpy;
	}

	if (platform_info->reg_64 == 1){
		aes_fun.reg_enc = aes_reg_enc_64;
		aes_fun.reg_dec = aes_reg_dec_64;
		des_fun.reg_enc = des_reg_enc_64;
		des_fun.reg_dec = des_reg_dec_64;
	}else{
		aes_fun.reg_enc = aes_reg_enc_dec_32;
		aes_fun.reg_dec = aes_reg_enc_dec_32;
		des_fun.reg_enc = des_reg_enc_dec_32;
		des_fun.reg_dec = des_reg_enc_dec_32;
	}

	if ((errCode = crypto_register_alg(&aes_alg))) {
		dev_err(&pdev->dev, "reigster aes_alg  failed.\n");
		if(likely(config_polling_mode == 0)) {
			goto crypto_errCode_free_des_irq;
		} else {
			goto crypto_errCode_na;
		}
	}

	if ((errCode = crypto_register_alg(&des_alg))) {
		dev_err(&pdev->dev, "reigster des_alg failed.\n");

		crypto_unregister_alg(&aes_alg);

		if(likely(config_polling_mode == 0)) {
			goto crypto_errCode_free_des_irq;
		} else {
			goto crypto_errCode_na;
		}
	}

	if(likely(config_polling_mode == 0))
		dev_notice(&pdev->dev,"%s probed(interrupt mode).\n", ambdev_name);
	else
		dev_notice(&pdev->dev,"%s probed(polling mode).\n", ambdev_name);

	goto crypto_errCode_na;

crypto_errCode_free_des_irq:
	free_irq(pinfo->des_irq, pinfo);

crypto_errCode_free_aes_irq:
	free_irq(pinfo->aes_irq, pinfo);

crypto_errCode_kzalloc:
	platform_set_drvdata(pdev, NULL);
	kfree(pinfo);

crypto_errCode_ioarea:
	release_mem_region(mem->start, (mem->end - mem->start) + 1);

crypto_errCode_na:
	return errCode;
}

static int __exit ambarella_crypto_remove(struct platform_device *pdev)
{
	int errCode = 0;
	struct ambarella_crypto_dev_info *pinfo;

	crypto_unregister_alg(&aes_alg);
	crypto_unregister_alg(&des_alg);

	pinfo = platform_get_drvdata(pdev);

	if (pinfo && config_polling_mode == 0) {
		free_irq(pinfo->aes_irq, pinfo);
		free_irq(pinfo->des_irq, pinfo);
		platform_set_drvdata(pdev, NULL);
		release_mem_region(pinfo->mem->start,
			(pinfo->mem->end - pinfo->mem->start) + 1);
		kfree(pinfo);
	}
	dev_notice(&pdev->dev, "%s removed.\n", ambdev_name);

	return errCode;
}

static const char ambarella_crypto_name[] = "ambarella-crypto";

static struct platform_driver ambarella_crypto_driver = {
	.remove		= __exit_p(ambarella_crypto_remove),
#ifdef CONFIG_PM
	.suspend	= NULL,
	.resume		= NULL,
#endif
	.driver		= {
		.name	= ambarella_crypto_name,
		.owner	= THIS_MODULE,
	},
};

static int __init ambarella_crypto_init(void)
{
	return platform_driver_probe(&ambarella_crypto_driver,
		ambarella_crypto_probe);
}

static void __exit ambarella_crypto_exit(void)
{
	platform_driver_unregister(&ambarella_crypto_driver);
}

module_init(ambarella_crypto_init);
module_exit(ambarella_crypto_exit);

MODULE_DESCRIPTION("Ambarella Cryptography Engine");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qiao Wang");
MODULE_ALIAS("crypo-all");

