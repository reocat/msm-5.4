/*
 * include/misc/amba_dspmem.h
 *
 * Copyright 2014 Ambarella Inc.
 *
 */

#ifndef _MISC_AMBA_DSPMEM_H
#define _MISC_AMBA_DSPMEM_H

#include <linux/limits.h>
#include <linux/ioctl.h>
#include <linux/compat.h>

#define AMBA_DSPMEM_NAME_DEF		"dev/amba_dspmem"

#define __AMBA_DSPMEM		0x99

#define AMBA_DSPMEM_GET_PHY_BASE	_IOR(__AMBA_DSPMEM, 1, unsigned int)
#define AMBA_DSPMEM_GET_SIZE		_IOR(__AMBA_DSPMEM, 2, unsigned int)

#endif	/* _MISC_AMBA_DSPMEM_H */
