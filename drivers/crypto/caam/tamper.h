
/*
 * SNVS Tamper Detection
 * Copyright (C) 2012-2015 Freescale Semiconductor, Inc., All Rights Reserved
 */

#ifndef TAMPER_H
#define TAMPER_H

#include "snvsregs.h"
#include "secvio.h"

#define AT5_POLYSEED	0x12345678
#define SNVS_LPPGDR_INIT    0x41736166


typedef struct {
	u32 rx;
	u32 polarity;
}tamper_passive;

typedef struct {
	u32 lpsr;
	u32 lptdsr;
}tamper_status;

/* IOCTL commands */
#define TAMPER_IOCTL_PASSIVE_EN		_IOWR('T', 5, tamper_passive)
#define TAMPER_IOCTL_GET_STATUS		_IOR('T', 6, tamper_status)
#define TAMPER_IOCTL_CLEAR_STATUS	_IOWR('T', 7, tamper_status)
#define TAMPER_IOCTL_CLEAR_GLITCH_STATUS     _IOWR('T', 8, tamper_status)

#endif /* TAMPER_H */
