/**
 * Microchip switch common sysfs header
 *
 * Copyright (c) 2016-2019 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef __KSZ_SW_SYSFS_H
#define __KSZ_SW_SYSFS_H

void ksz_sw_sysfs_exit(struct ksz_device *sw, struct ksz_sw_sysfs *info,
	struct device *dev);
int ksz_sw_sysfs_init(struct ksz_device *sw, struct ksz_sw_sysfs *info,
	struct device *dev);

#endif