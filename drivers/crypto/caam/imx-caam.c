/*
 * imx-caam.c
 *
 * Written by Dean Matsen
 * Copyright (C) 2016 Honeywell
 * All Rights Reserved.
 *
 * This module is written with the hopes that someday, when
 * Freescale provides a real API for accessing CAAM functions,
 * this can become the interface to it.
 *
 * Angelo Dureghello <angelo.dureghello@timesys.com>
 * Porting to 4.14, with some reformatting for more readability,
 *   respect Linux codyingstyle
 * Fix for retrieving jobring device
 */

#include "compat.h"
#include "linux/imx-caam.h"
#include "regs.h"
#include "snvsregs.h"
#include "intern.h"
#include "jr.h"
#include "desc_constr.h"
#include "error.h"

static struct device *get_jrdev ( void );

#if 0
static void key_display(char *label, unsigned char *key, unsigned size )
{
	unsigned i;

	printk ( KERN_DEBUG "%s:\n", label );
	for (i = 0; i < size; i += 8)
	{
	printk ( KERN_DEBUG "[%04d] 0x%02x, 0x%02x, 0x%02x, 0x%02x, "
			    "0x%02x, 0x%02x, 0x%02x, 0x%02x,\n",
	     i, key[i], key[i + 1], key[i + 2], key[i + 3],
	     key[i + 4], key[i + 5], key[i + 6], key[i + 7]);
	}
}


static void dump_jobdesc ( u32 *jd, unsigned dsize )
{
	unsigned i;

	dsize /= 4;
	for ( i = 0; i < dsize; i++ )
	{
	printk ( KERN_DEBUG "  [%02u @ %08lX] %lX\n",
	     i, (unsigned long)(jd + i), (unsigned long)jd[i] );
	}
}
#endif

/*--------------------------------------------------------------------------*/
/** Get a job ring device (a device whose private data is struct
 * caam_drv_private_jr).
 *
 * In this implementation, we return job ring device 0 of the CAAM
 * control device.
 *
 * In future, this could be modified to do something else.
 */
/*--------------------------------------------------------------------------*/
static struct device *get_jrdev ()
{
	static struct device *dev;

	if (!dev)
		dev = caam_jr_alloc();

	if (!dev) {
		pr_err("cannot get a valid job ring");
		return 0;
	}

	return dev;
}

/*--------------------------------------------------------------------------*/
/* Submit CAAM job
 */
/*--------------------------------------------------------------------------*/
struct imx_caam_job_result {
	u32 err;
	struct completion completion;
};

static void imx_caam_job_done(struct device *dev,
			      u32 *desc, u32 err, void *context)
{
	struct imx_caam_job_result *res = context;

	res->err = err;
	complete(&res->completion);
}

static int imx_caam_do_job (struct device *jrdev, u32 *jobdesc)
{
	struct imx_caam_job_result res;
	int rval = 0;

	init_completion(&res.completion);

	rval = caam_jr_enqueue(jrdev, jobdesc, imx_caam_job_done, &res);

	if (!rval) {
		wait_for_completion_interruptible(&res.completion);
		rval = res.err;
	}

	return rval;
}

/*--------------------------------------------------------------------------*/
/** Convert plain key in general memory to red blob in general memory
 *
 * @param keyauth KEY_COVER_* ie: KEY_COVER_ECB (AES-ECB) or
 *     KEY_COVER_CCM (AES-CCM)
 *
 * @param key The plaintext key
 *
 * @param keylen The size of the key (not the blob) in bytes
 *
 * @param keymod Must be GENMEM_KEYMOD_LEN (16) bytes of key modifier
 *    (protects key from unauthorized use if this modifier is a secret)
 *
 * @param outbuf The resulting blob.  This must be big enough to contain
 *    (keylen + BLOB_OVERHEAD) bytes of data.
 */
/*--------------------------------------------------------------------------*/

int imx_caam_key_to_red_blob (u8 keyauth,
                               u8 *key,
                               unsigned keylen,
                               u8 *keymod,
                               u8 *outbuf)
{
	int rval = 0;
	u8 __iomem *lkey = 0;
	dma_addr_t key_dma = 0;
	u8 __iomem *lkeymod = 0;
	dma_addr_t keymod_dma = 0;
	u8 __iomem *loutbuf = 0;
	dma_addr_t outbuf_dma = 0;
	u32 dsize, jstat;
	u32 __iomem *jobdesc = 0;
	unsigned keymod_len = GENMEM_KEYMOD_LEN;
	struct device *jrdev = 0;

	memset(outbuf, 0, keylen + BLOB_OVERHEAD);

	jrdev = get_jrdev();
	if (!jrdev) {
		rval = -ENODEV;
		goto errexit;
	}

	/* Build/map/flush the source data */
	lkey = kmalloc(keylen, GFP_KERNEL | GFP_DMA);
	if (!lkey) {
		rval = -ENOMEM;
		goto errexit;
	}


	memcpy(lkey, key, keylen);
	key_dma = dma_map_single(jrdev, lkey, keylen, DMA_TO_DEVICE);
	dma_sync_single_for_device(jrdev, key_dma, keylen, DMA_TO_DEVICE);

	/* Build/map/flush the key modifier */
	lkeymod = kmalloc(keymod_len, GFP_KERNEL | GFP_DMA);
	if (!lkeymod) {
		rval = -ENOMEM;
		goto errexit;
	}

	memcpy(lkeymod, keymod, keymod_len);
	keymod_dma = dma_map_single(jrdev, lkeymod, keymod_len, DMA_TO_DEVICE);
	dma_sync_single_for_device(jrdev,
				   keymod_dma, keymod_len, DMA_TO_DEVICE);

	/* Map output buffer */
	loutbuf = kmalloc(keylen + BLOB_OVERHEAD, GFP_KERNEL | GFP_DMA);
	if (!loutbuf) {
		rval = -ENOMEM;
		goto errexit;
	}

	outbuf_dma = dma_map_single(jrdev, loutbuf, keylen + BLOB_OVERHEAD,
					DMA_FROM_DEVICE);

	/* Build the encapsulation job descriptor */
	dsize = blob_encap_jobdesc(&jobdesc, keymod_dma, (u8 *)key_dma,
				    outbuf_dma, keylen, RED_KEY, SM_GENMEM,
				    keyauth);
	if (!dsize) {
		rval = -ENOMEM;
		goto errexit;
	}

	/* Do it */
	jstat = imx_caam_do_job(jrdev, jobdesc);

	dma_sync_single_for_cpu(jrdev, outbuf_dma, keylen + BLOB_OVERHEAD,
			            DMA_FROM_DEVICE);

	if (jstat) {
		printk(KERN_DEBUG "imx_caam_red_key_to_blob : jstat=%u\n",
			       (unsigned)jstat);
		rval = -EIO;
		goto errexit;
	}

	/* Success */
	memcpy ( outbuf, loutbuf, keylen + BLOB_OVERHEAD );
	rval = 0;

errexit:
	if (jobdesc)
		kfree(jobdesc);

	if (outbuf_dma)
		dma_unmap_single(jrdev, outbuf_dma, keylen + BLOB_OVERHEAD,
				       DMA_FROM_DEVICE);

	if (loutbuf)
		kfree(loutbuf);

	if (keymod_dma)
		dma_unmap_single(jrdev, keymod_dma, keymod_len, DMA_TO_DEVICE);

	if (lkeymod)
		kfree(lkeymod);

	if (key_dma)
		dma_unmap_single(jrdev, key_dma, keylen, DMA_TO_DEVICE);

	if (lkey)
		kfree (lkey);

	return rval;
}

EXPORT_SYMBOL(imx_caam_key_to_red_blob);

/*--------------------------------------------------------------------------*/
/** Convert red blob in general memory to plain key in general memory
 *
 * @param keyauth KEY_COVER_* ie: KEY_COVER_ECB (AES-ECB) or
 *     KEY_COVER_CCM (AES-CCM)
 *
 * @param blob the blob data
 *
 * @param bloblen The size of the blob.  This should be (key size +
 *     BLOB_OVERHEAD) bytes
 *
 * @param keymod Must be GENMEM_KEYMOD_LEN (16) bytes of key modifier
 *    (protects key from unauthorized use if this modifier is a secret)
 *
 * @param outbuf The resulting blob.  This must be big enough to contain
 *    (bloblen - BLOB_OVERHEAD) bytes of data
 */
/*--------------------------------------------------------------------------*/

int imx_caam_red_blob_to_key (u8 keyauth,
                               u8 *blob,
                               unsigned bloblen,
                               u8 *keymod,
                               u8 *outbuf)
{
	int rval = 0;
	u8 __iomem *lblob = 0;
	dma_addr_t blob_dma = 0;
	u8 __iomem *lkeymod = 0;
	dma_addr_t keymod_dma = 0;
	u8 __iomem *loutbuf = 0;
	dma_addr_t outbuf_dma = 0;
	u32 dsize, jstat;
	u32 __iomem *jobdesc = 0;
	unsigned keymod_len = GENMEM_KEYMOD_LEN;
	unsigned keylen = 0;
	struct device *jrdev = 0;

	if (bloblen < BLOB_OVERHEAD+1) {
		rval = -EINVAL;
		goto errexit;
	}
	keylen = bloblen - BLOB_OVERHEAD;

	memset(outbuf, 0, keylen);

	jrdev = get_jrdev();
	if (!jrdev) {
		rval = -ENODEV;
		goto errexit;
	}

	/* Build/map/flush the source data */
	lblob = kmalloc(bloblen, GFP_KERNEL | GFP_DMA);
	if (!lblob) {
		rval = -ENOMEM;
		goto errexit;
	}
	memcpy(lblob, blob, bloblen);
	blob_dma = dma_map_single(jrdev, lblob, bloblen, DMA_TO_DEVICE);
	dma_sync_single_for_device(jrdev, blob_dma, bloblen, DMA_TO_DEVICE);

  /* Build/map/flush the key modifier */
	lkeymod = kmalloc(keymod_len, GFP_KERNEL | GFP_DMA);
	if (!lkeymod) {
		rval = -ENOMEM;
		goto errexit;
	}
	memcpy(lkeymod, keymod, keymod_len);
	keymod_dma = dma_map_single(jrdev, lkeymod, keymod_len, DMA_TO_DEVICE);
	dma_sync_single_for_device(jrdev, keymod_dma, keymod_len, DMA_TO_DEVICE);

	/* Map output buffer */
	loutbuf = kmalloc(keylen, GFP_KERNEL | GFP_DMA);
	if (!loutbuf) {
		rval = -ENOMEM;
		goto errexit;
	}
	outbuf_dma = dma_map_single(jrdev, loutbuf, keylen, DMA_FROM_DEVICE);

	/* Build the encapsulation job descriptor */
	dsize = blob_decap_jobdesc(&jobdesc, keymod_dma, blob_dma,
				   (u8 *)outbuf_dma,
				   keylen, RED_KEY, SM_GENMEM, keyauth);
	if (!dsize) {
		rval = -ENOMEM;
		goto errexit;
	}

#if 0
	printk(KERN_DEBUG "Job descr:\n");
	dump_jobdesc(jobdesc, dsize);
#endif

	/* Do it */
	jstat = imx_caam_do_job(jrdev, jobdesc);
	//  jstat = sm_key_job(jrdev, jobdesc);

	dma_sync_single_for_cpu(jrdev, outbuf_dma, keylen, DMA_FROM_DEVICE);

#if 0
	key_display("decap key", loutbuf, keylen);
#endif

	if (jstat) {
		printk(KERN_DEBUG "imx_caam_red_blob_to_key : "
				"jstat=%u\n", (unsigned)jstat);
		rval = -EIO;
		goto errexit;
	}

	/* Success */
	memcpy(outbuf, loutbuf, keylen);
	rval = 0;

errexit:
	if (jobdesc)
		kfree(jobdesc);

	if (outbuf_dma)
		dma_unmap_single(jrdev, outbuf_dma, keylen, DMA_FROM_DEVICE);

	if (loutbuf)
		kfree(loutbuf);

	if (keymod_dma)
		dma_unmap_single(jrdev, keymod_dma, keymod_len, DMA_TO_DEVICE);

	if (lkeymod)
		kfree(lkeymod);

	if (blob_dma)
		dma_unmap_single(jrdev, blob_dma, bloblen, DMA_TO_DEVICE);

	if (lblob)
		kfree(lblob);

	return rval;
}

EXPORT_SYMBOL(imx_caam_red_blob_to_key);

