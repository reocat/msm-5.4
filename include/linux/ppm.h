/*
 * include/linux/ppm.h
 *
 * Copyright 2014 Ambarella Inc.
 *
 */

#ifndef _PPM_H
#define _PPM_H


struct PPM_MEM_INFO_s {
	unsigned long virt;
	unsigned long phys;
	unsigned long size;
};

#define __PPM_MEM		0x99

#define PPM_GET_MEMIO_INFO		_IOR(__PPM_MEM, 1, struct PPM_MEM_INFO_s)

#endif	/* _PPM_H */
