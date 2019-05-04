/*
 * Written by Dean Matsen
 * Copyright (C) 2013-2018 Honeywell
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

#ifndef F1_MMC_KEY_H_
#define F1_MMC_KEY_H_

int get_f1_mmc_key ( unsigned keynum,
                     unsigned keylen_bytes,
                     unsigned char *key );

#endif /* F1_MMC_KEY_H_ */
