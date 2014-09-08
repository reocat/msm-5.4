/*
 * include/misc/amba_heapmem.h
 *
 * Copyright 2014 Ambarella Inc.
 *
 */

#ifndef _MISC_AMBA_HEAPMEM_H
#define _MISC_AMBA_HEAPMEM_H

#include <linux/limits.h>
#include <linux/ioctl.h>
#include <linux/compat.h>

#define AMBA_HEAPMEM_NAME_DEF		"dev/amba_heapmem"

#define __AMBA_HEAPMEM		0x99

#define AMBA_HEAPMEM_GET_PHY_BASE	_IOR(__AMBA_HEAPMEM, 1, unsigned int)
#define AMBA_HEAPMEM_GET_SIZE		_IOR(__AMBA_HEAPMEM, 2, unsigned int)

#endif	/* _MISC_AMBA_HEAPMEM_H */
