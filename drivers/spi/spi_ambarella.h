/*
 * linux/drivers/spi/spi_ambarella.h
 *
 * History:
 *	2008/03/03 - [Louis Sun]  created file
 *	2009/06/23 - [Zhenwu Xue] ported from 2.6.22.10
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

#ifndef  __SPI_AMBARELLA_H__
#define  __SPI_AMBARELLA_H__

/* SPI rw mode */
#define SPI_WRITE_READ		0
#define SPI_WRITE_ONLY		1
#define SPI_READ_ONLY		2

/* Tx FIFO empty interrupt mask */
#define SPI_TXEIS_MASK		0x00000001

/* SPI Parameters */
#define SPI_FIFO_LENGTH		16L
#define SPI_DUMMY_DATA		0xffff
#define MAX_QUERY_TIMES		10

/* Default SPI settings */
#define SPI_MODE		SPI_MODE_0
#define SPI_SCPOL		0
#define SPI_SCPH		0
#define SPI_FRF			0
#define SPI_CFS			0x0
#define SPI_DFS			0xf
#define SPI_BAUD_RATE		200000

#endif

