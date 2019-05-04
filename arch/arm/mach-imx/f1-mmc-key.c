/*
 * Written by Dean Matsen
 * Copyright (C) 2016 Honeywell
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/f1-mmc-key.h>
#include <linux/imx-caam.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>

#define MAX_MMC_KEYS            1
#define MMC_KEY_SIZE            (256/8) /* bytes */
#define XFER_KEY_SIZE           (256/8) /* bytes */
#define ENCTEST_DATA_SIZE       16      /* One AES block */

#define CPU_BLOB_SIZE   (MMC_KEY_SIZE + BLOB_OVERHEAD)

struct f1_mmc_key_dev
  {
  struct device dev;

  spinlock_t lock;

  unsigned keynum;

  int sysfs_added;

      /* Used for computing things from each other.
       */
  int have_raw_key;
  int have_cpu_blob;
  int have_xfer_key;
  int have_xfer_blob;

  unsigned char raw_key[MMC_KEY_SIZE];

      /* Red blob version of raw key encrypted using the present
       * CPU encryption
       */
  unsigned char cpu_blob[CPU_BLOB_SIZE];

  unsigned char xfer_key[XFER_KEY_SIZE];
  unsigned char xfer_blob[MMC_KEY_SIZE];
  };

#define dev_to_key_dev(p) container_of((p),struct f1_mmc_key_dev,dev)

#define CLEAR_ARRAY(a) memset ( (a), 0, sizeof(a) )
#define COPY_ARRAY_LEFT(dest,src) memcpy ( (dest), (src), sizeof(dest) )
#define COPY_ARRAY_RIGHT(src,dest) memcpy ( (dest), (src), sizeof(dest) )

static struct f1_mmc_key_dev dev_tab[MAX_MMC_KEYS];
static int g_dev_added[MAX_MMC_KEYS];

//static DEFINE_SPINLOCK ( f1_mmc_key_lock );

/* Some random numbers that act on the CPU red blob
 */
static unsigned char g_modifier[] =
{
  0xAE, 0x8D, 0x9F, 0x28, 0xB8, 0xEE, 0x32, 0x10,
  0x1E, 0xC6, 0xEC, 0x2D, 0xAB, 0xE9, 0xA8, 0x30
};

/*--------------------------------------------------------------------------*/
/** Do AES(ECB) encryption.
 *
 * NOTE: This should work for data sizes up to the page size, but beyond
 * that, I think maybe there needs to be multiple scatter lists or
 * something.  I didn't have time to research this, given the poor
 * documentation available.  This only needs to encrypt and decrypt
 * small things in this module anyway.
 */
/*--------------------------------------------------------------------------*/

static int aes_ecb_encrypt ( void *key, unsigned keylen,
                             void *indata,
                             unsigned long datalen,
                             void *outdata )
{
  struct crypto_blkcipher *tfm = 0;
  int tfm_open = 0;
  struct scatterlist sg_in;
  struct scatterlist sg_out;
  struct blkcipher_desc desc;
  int rval;

  sg_init_one ( &sg_in, (u8 *)indata, datalen );
  sg_init_one ( &sg_out, (u8 *)outdata, datalen );

  tfm = crypto_alloc_blkcipher ( "ecb(aes)", 0, 0 );
  if ( IS_ERR ( tfm ) )
    {
    printk ( KERN_ERR "No AES encryption available in kernel!\n" );
    rval = -ENODEV;
    goto errexit;
    }
  tfm_open = 1;

  rval = crypto_blkcipher_setkey ( tfm, key, keylen );
  if ( rval )
    {
    printk ( KERN_ERR "crypto_blkcipher_setkey returned %d\n", rval );
    goto errexit;
    }

  memset ( &desc, 0, sizeof(desc) );
  desc.tfm = tfm;

  rval = crypto_blkcipher_encrypt ( &desc, &sg_out, &sg_in, datalen );
  if ( rval )
    {
    printk ( KERN_ERR "crypto_blkcipher_encrypt returned %d\n", rval );
    goto errexit;
    }

  rval = 0;

errexit :

  if ( tfm_open )
    {
    crypto_free_blkcipher ( tfm );
    }

  return rval;
}

/*--------------------------------------------------------------------------*/
/** Do AES(ECB) decryption
 *
 * NOTE: This should work for data sizes up to the page size, but beyond
 * that, I think maybe there needs to be multiple scatter lists or
 * something.  I didn't have time to research this, given the poor
 * documentation available.  This only needs to encrypt and decrypt
 * small things in this module anyway.
 */
/*--------------------------------------------------------------------------*/

static int aes_ecb_decrypt ( void *key, unsigned keylen,
                             void *indata,
                             unsigned long datalen,
                             void *outdata )
{
  struct crypto_blkcipher *tfm = 0;
  int tfm_open = 0;
  struct scatterlist sg_in;
  struct scatterlist sg_out;
  struct blkcipher_desc desc;
  int rval;

  sg_init_one ( &sg_in, (u8 *)indata, datalen );
  sg_init_one ( &sg_out, (u8 *)outdata, datalen );

  tfm = crypto_alloc_blkcipher ( "ecb(aes)", 0, 0 );
  if ( IS_ERR ( tfm ) )
    {
    printk ( KERN_ERR "No AES encryption available in kernel!\n" );
    rval = -ENODEV;
    goto errexit;
    }
  tfm_open = 1;

  rval = crypto_blkcipher_setkey ( tfm, key, keylen );
  if ( rval )
    {
    printk ( KERN_ERR "crypto_blkcipher_setkey returned %d\n", rval );
    goto errexit;
    }

  memset ( &desc, 0, sizeof(desc) );
  desc.tfm = tfm;

  rval = crypto_blkcipher_decrypt ( &desc, &sg_out, &sg_in, datalen );
  if ( rval )
    {
    printk ( KERN_ERR "crypto_blkcipher_decrypt returned %d\n", rval );
    goto errexit;
    }

  rval = 0;

errexit :

  if ( tfm_open )
    {
    crypto_free_blkcipher ( tfm );
    }

  return rval;
}


/*--------------------------------------------------------------------------*/
/** Update dependent fields
 */
/*--------------------------------------------------------------------------*/

static void update_info ( struct f1_mmc_key_dev *pdev )
{
  int status;
  unsigned long flags;
  unsigned char cpu_blob[CPU_BLOB_SIZE];
  unsigned char raw_key[MMC_KEY_SIZE];
  unsigned char xfer_key[XFER_KEY_SIZE];
  unsigned char xfer_blob[MMC_KEY_SIZE];
  int have_cpu_blob;
  int have_raw_key;
  int have_xfer_key;
  int have_xfer_blob;

      /* Copy the existing data into local variables
       */
  spin_lock_irqsave ( &pdev->lock, flags );

  have_cpu_blob = pdev->have_cpu_blob;
  have_raw_key = pdev->have_raw_key;
  have_xfer_key = pdev->have_xfer_key;
  have_xfer_blob = pdev->have_xfer_blob;

  COPY_ARRAY_LEFT ( cpu_blob, pdev->cpu_blob );
  COPY_ARRAY_LEFT ( raw_key, pdev->raw_key );
  COPY_ARRAY_LEFT ( xfer_key, pdev->xfer_key );
  COPY_ARRAY_LEFT ( xfer_blob, pdev->xfer_blob );

  spin_unlock_irqrestore ( &pdev->lock, flags );

      /* First thing is to generate the raw key, if we don't already
       * have it
       */
  if ( ! have_raw_key &&
       have_cpu_blob )
    {
        /* NOTE: The CPU can detect if the red blob decapsulated
         * properly, so this call will fail before we get a chance to
         * try the key on the MMC card.
         */
    status = imx_caam_red_blob_to_key ( KEY_COVER_ECB,
                                        cpu_blob,
                                        sizeof(cpu_blob),
                                        g_modifier,
                                        raw_key );
    if ( ! status )
      {
      have_raw_key = 1;
      }
    }

  if ( ! have_raw_key &&
       have_xfer_key &&
       have_xfer_blob )
    {
        /* Decrypt xfer blob using xfer key.  The result is
         * the raw key
         */
    status = aes_ecb_decrypt ( xfer_key, sizeof(xfer_key),
                               xfer_blob, sizeof(xfer_blob),
                               raw_key );
    if ( ! status )
      {
          /* We have a key, but it could be wrong.  Unlike the red blob
           * decapsulation, we don't know until we try accessing the
           * media with it.
           */
      have_raw_key = 1;
      }
    }

      /* Now that we've generated the raw key, we can produce the
       * blobs
       */
  if ( have_raw_key &&
       ! have_cpu_blob )
    {

	    printk("update_info() calling imx_caam_key_to_red_blob");

    status = imx_caam_key_to_red_blob ( KEY_COVER_ECB,
                                        (unsigned char *)raw_key,
                                        sizeof(raw_key),
                                        g_modifier,
                                        cpu_blob );

    printk("update_info() status %d", status);

    if ( ! status )
      {
      have_cpu_blob = 1;
      }
    }

  if ( have_raw_key &&
       have_xfer_key &&
       ! have_xfer_blob )
    {
    status = aes_ecb_encrypt ( xfer_key, sizeof(xfer_key),
                               raw_key, sizeof(raw_key),
                               xfer_blob );
    if ( ! status )
      {
      have_xfer_blob = 1;
      }
    }

      /* Update data in device object
       */
  spin_lock_irqsave ( &pdev->lock, flags );

  if ( have_raw_key )
    {
    COPY_ARRAY_RIGHT ( raw_key, pdev->raw_key );
    pdev->have_raw_key = 1;
    }

  if ( have_cpu_blob )
    {
    COPY_ARRAY_RIGHT ( cpu_blob, pdev->cpu_blob );
    pdev->have_cpu_blob = 1;
    }

  if ( have_xfer_blob )
    {
    COPY_ARRAY_RIGHT ( xfer_blob, pdev->xfer_blob );
    pdev->have_xfer_blob = 1;
    }

  spin_unlock_irqrestore ( &pdev->lock, flags );

  CLEAR_ARRAY (  raw_key );
  CLEAR_ARRAY (  cpu_blob );
  CLEAR_ARRAY (  xfer_key );
  CLEAR_ARRAY (  xfer_blob );
}


/*--------------------------------------------------------------------------*/
/** Handlers for raw MMC key
 */
/*--------------------------------------------------------------------------*/

static ssize_t store_raw_key ( struct device *linuxdev,
                               struct device_attribute *attr,
                               const char *buf,
                               size_t count )
{
  struct f1_mmc_key_dev *pdev = dev_to_key_dev ( linuxdev );
  unsigned long flags;

  if ( count != MMC_KEY_SIZE )
    {
    printk ( KERN_DEBUG "Key data is wrong size: %d\n", count);
    return count;
    }

  spin_lock_irqsave ( &pdev->lock, flags );

      /* Copy data and revoke blobs
       */
  memcpy ( pdev->raw_key, buf, sizeof(pdev->raw_key) );
  pdev->have_raw_key = 1;
  pdev->have_cpu_blob = 0;
  pdev->have_xfer_blob = 0;

  spin_unlock_irqrestore ( &pdev->lock, flags );

      /* Update computed data
       */
  update_info ( pdev );

  return count;
}

/*--------------------------------------------------------------------------*/
/** Get/Put Sd key in CPU-encrypted (red data) blob form
 */
/*--------------------------------------------------------------------------*/

static ssize_t show_cpu_blob ( struct device *linuxdev,
                               struct device_attribute *attr,
                               char *buf )
{
  struct f1_mmc_key_dev *pdev = dev_to_key_dev ( linuxdev );
  unsigned long flags;
  ssize_t rval = 0;

  spin_lock_irqsave ( &pdev->lock, flags );

  if ( pdev->have_cpu_blob )
    {

    rval = sizeof(pdev->cpu_blob);
    memcpy ( buf, pdev->cpu_blob, sizeof(pdev->cpu_blob) );

    }

  spin_unlock_irqrestore ( &pdev->lock, flags );

  return rval;
}

static ssize_t store_cpu_blob ( struct device *linuxdev,
                                struct device_attribute *attr,
                                const char *buf,
                                size_t count )
{
  struct f1_mmc_key_dev *pdev = dev_to_key_dev ( linuxdev );
  unsigned long flags;

  if ( count != CPU_BLOB_SIZE )
    {
    printk ( KERN_DEBUG "Key data is wrong size\n" );
    return count;
    }

  spin_lock_irqsave ( &pdev->lock, flags );

      /* Copy data and revoke blobs
       */
  memcpy ( pdev->cpu_blob, buf, sizeof(pdev->cpu_blob) );
  pdev->have_raw_key = 0;
  pdev->have_cpu_blob = 1;
  pdev->have_xfer_blob = 0;

  spin_unlock_irqrestore ( &pdev->lock, flags );

      /* Update computed data
       */
  update_info ( pdev );

  return count;
}

/*--------------------------------------------------------------------------*/
/** Handlers for MMC transfer key
 *
 * Note that the MMC transfer key is generated from a password.  When
 * the transfer key is set, it clears all other data.
 */
/*--------------------------------------------------------------------------*/

static ssize_t store_xfer_key ( struct device *linuxdev,
                                struct device_attribute *attr,
                                const char *buf,
                                size_t count )
{
  struct f1_mmc_key_dev *pdev = dev_to_key_dev ( linuxdev );
  unsigned long flags;

  if ( count != XFER_KEY_SIZE )
    {
    printk ( KERN_DEBUG "Key data is wrong size\n" );
    return count;
    }

  spin_lock_irqsave ( &pdev->lock, flags );

      /* Copy data and revoke blobs
       */
  memcpy ( pdev->xfer_key, buf, sizeof(pdev->xfer_key) );
  pdev->have_raw_key = 0;
  pdev->have_cpu_blob = 0;
  pdev->have_xfer_blob = 0;
  pdev->have_xfer_key = 1;

  spin_unlock_irqrestore ( &pdev->lock, flags );

      /* Update computed data
       */
  update_info ( pdev );

  return count;
}

/*--------------------------------------------------------------------------*/
/** Get/Put transfer key blob form
 */
/*--------------------------------------------------------------------------*/

static ssize_t show_xfer_blob ( struct device *linuxdev,
                                struct device_attribute *attr,
                                char *buf )
{
  struct f1_mmc_key_dev *pdev = dev_to_key_dev ( linuxdev );
  unsigned long flags;
  ssize_t rval = 0;

  spin_lock_irqsave ( &pdev->lock, flags );

  if ( pdev->have_xfer_blob )
    {
    rval = sizeof(pdev->xfer_blob);
    memcpy ( buf, pdev->xfer_blob, sizeof(pdev->xfer_blob) );
    }

  spin_unlock_irqrestore ( &pdev->lock, flags );

  return rval;
}

static ssize_t store_xfer_blob ( struct device *linuxdev,
                                 struct device_attribute *attr,
                                 const char *buf,
                                 size_t count )
{
  struct f1_mmc_key_dev *pdev = dev_to_key_dev ( linuxdev );
  unsigned long flags;

  if ( count != MMC_KEY_SIZE )
    {
    printk ( KERN_DEBUG "Key data is wrong size\n" );
    return count;
    }

  spin_lock_irqsave ( &pdev->lock, flags );

      /* Copy data and revoke blobs
       */
  memcpy ( pdev->xfer_blob, buf, sizeof(pdev->xfer_blob) );
  pdev->have_raw_key = 0;
  pdev->have_cpu_blob = 0;
  pdev->have_xfer_blob = 1;

  spin_unlock_irqrestore ( &pdev->lock, flags );

      /* Update computed data
       */
  update_info ( pdev );

  return count;
}

/*--------------------------------------------------------------------------*/
/** Check if we have an MMC key or not
 */
/*--------------------------------------------------------------------------*/

static ssize_t show_have_raw_key ( struct device *linuxdev,
                                   struct device_attribute *attr,
                                   char *buf )
{
  struct f1_mmc_key_dev *pdev = dev_to_key_dev ( linuxdev );
  unsigned long flags;
  int flag = 0;

  spin_lock_irqsave ( &pdev->lock, flags );
  flag = pdev->have_raw_key;
  spin_unlock_irqrestore ( &pdev->lock, flags );

  return sprintf ( buf, "%d\n", flag );
}

/*--------------------------------------------------------------------------*/
/** Get test data.  This is an encrypted version of a sequence of 16
 * 0x55 ("U") characters.
 */
/*--------------------------------------------------------------------------*/

static ssize_t show_enctest_ciphertext ( struct device *linuxdev,
                                struct device_attribute *attr,
                                char *buf )
{
  struct f1_mmc_key_dev *pdev = dev_to_key_dev ( linuxdev );
  unsigned long flags;
  ssize_t rval = 0;
  unsigned char raw_key[MMC_KEY_SIZE];
  unsigned char plaintext[ENCTEST_DATA_SIZE];
  int have_raw_key;

  spin_lock_irqsave ( &pdev->lock, flags );

  if ( have_raw_key = pdev->have_raw_key )
    COPY_ARRAY_LEFT ( raw_key, pdev->raw_key );

  spin_unlock_irqrestore ( &pdev->lock, flags );

  if ( have_raw_key )
    {
    memset ( plaintext, 0x55, sizeof(plaintext) );
    rval = aes_ecb_encrypt ( raw_key, sizeof(raw_key),
                             plaintext, sizeof(plaintext),
                             buf );
    if ( ! rval )
      rval = ENCTEST_DATA_SIZE;
    }

  CLEAR_ARRAY ( raw_key );

  return rval;
}

/*--------------------------------------------------------------------------*/
/* sysfs dispatching
 */
/*--------------------------------------------------------------------------*/

static DEVICE_ATTR ( raw_key, 0644, 0, store_raw_key );
static DEVICE_ATTR ( cpu_blob, 0644, show_cpu_blob, store_cpu_blob );
static DEVICE_ATTR ( have_raw_key, 0444, show_have_raw_key, 0 );
static DEVICE_ATTR ( xfer_key, 0644, 0, store_xfer_key );
static DEVICE_ATTR ( xfer_blob, 0644, show_xfer_blob, store_xfer_blob );
static DEVICE_ATTR ( enctest_ciphertext, 0644, show_enctest_ciphertext, 0 );

static const struct attribute *devattrlist[] =
  {
    &dev_attr_raw_key.attr,
    &dev_attr_cpu_blob.attr,
    &dev_attr_have_raw_key.attr,
    &dev_attr_xfer_key.attr,
    &dev_attr_xfer_blob.attr,
    &dev_attr_enctest_ciphertext.attr,
    NULL
  };

/*--------------------------------------------------------------------------*/
/* API used by dm-crypt to fetch the key
 */
/*--------------------------------------------------------------------------*/

int get_f1_mmc_key ( unsigned keynum,
                     unsigned keylen,
                     unsigned char *key )
{
  struct f1_mmc_key_dev *pdev = 0;
  unsigned long flags;
  int rval = 0;
  int in_lock = 0;

  if ( keynum >= MAX_MMC_KEYS )
    {
    rval = -EINVAL;
    goto errexit;
    }

  if ( keylen != MMC_KEY_SIZE )
    {
    rval = -EINVAL;
    goto errexit;
    }

  pdev = &dev_tab[keynum];

  spin_lock_irqsave ( &pdev->lock, flags );
  in_lock = 1;

  if ( ! pdev->have_raw_key )
    {
    rval = -ENOENT;
    goto errexit;
    }

  memcpy ( key, pdev->raw_key, keylen );

errexit :

  if ( in_lock )
    {
    spin_unlock_irqrestore ( &pdev->lock, flags );
    }

  return rval;
}

/*--------------------------------------------------------------------------*/
/* Module house keeping
 */
/*--------------------------------------------------------------------------*/

static void dev_cleanup ( struct f1_mmc_key_dev *pdev )
{
  if ( pdev->sysfs_added )
    {
    sysfs_remove_files ( &pdev->dev.kobj, devattrlist );
    pdev->sysfs_added = 0;
    }
}

static void f1_mmc_key_dev_release ( struct device *dev )
{
  // They are all static objects, so there's really nothing to do here
}

static void module_cleanup ( void )
{
  unsigned i;

  for ( i = 0; i < MAX_MMC_KEYS; i++ )
    {
    struct f1_mmc_key_dev *pdev = &dev_tab[i];

    if ( g_dev_added[i] )
      {
      dev_cleanup ( pdev );
      device_del ( &pdev->dev );
      g_dev_added[i] = 0;
      }
    }

}

static int init_dev ( struct f1_mmc_key_dev *pdev,
                      unsigned inst )
{
  char tempstr[16];
  int ret;

  memset ( pdev, 0, sizeof(*pdev) );

  spin_lock_init ( &pdev->lock );

  device_initialize ( &pdev->dev );

  pdev->keynum = inst;

  sprintf ( tempstr, "f1-mmc-key%u", inst );
  ret = dev_set_name( &pdev->dev, tempstr );
  if ( ret )
    goto errexit;

  pdev->dev.release = f1_mmc_key_dev_release;
  ret = device_add ( &pdev->dev );
  if ( ret )
    goto errexit;

  g_dev_added[inst] = 1;

  ret = sysfs_create_files ( &pdev->dev.kobj, devattrlist );
  if ( ret )
    goto errexit;

  pdev->sysfs_added = 1;

  printk ( KERN_DEBUG "MMC key device created\n" );

  return 0;

errexit :
  dev_cleanup ( pdev );
  return ret;
}

static int __init f1_mmc_key_init(void)
{
  int ret;
  unsigned i;

  printk ( KERN_INFO "F1 MMC Key Driver\n" );

  for ( i = 0; i < MAX_MMC_KEYS; i++ )
    {
    struct f1_mmc_key_dev *pdev = &dev_tab[i];

    if ( init_dev ( pdev, i ) )
      goto errexit;
    }

  return 0;

errexit :
  module_cleanup ();
  return ret;
}

static void __exit f1_mmc_key_exit(void)
{
  module_cleanup ();
}

module_init(f1_mmc_key_init);
module_exit(f1_mmc_key_exit);

MODULE_AUTHOR("Dean Matsen");
MODULE_DESCRIPTION("F1 I/O");
MODULE_LICENSE("GPL");





