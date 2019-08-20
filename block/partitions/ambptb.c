/*
 *  fs/partitions/ambptb.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
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

#include "check.h"
#include "ambptb.h"
#include <plat/ptb.h>
#include <linux/of.h>

//#define ambptb_debug

#ifdef ambptb_debug
#define ambptb_prt printk
#else
#define ambptb_prt(format, arg...) do {} while (0)
#endif

int ambptb_emmc_partition(struct parsed_partitions *state)
{
	struct device_node *np = NULL, *pp;
	struct device *dev;
	int sector_size, index = 1, rval = -1;

	dev = disk_to_dev(state->bdev->bd_disk);

	while (dev) {
		if (of_device_is_compatible(dev->of_node, "ambarella,sdmmc")) {
			np = of_get_child_by_name(dev->of_node, "partitions");
			break;
		}

		dev = dev->parent;
	}

	if (np == NULL)
		return 0;

	sector_size = bdev_logical_block_size(state->bdev);

	for_each_child_of_node(np,  pp) {
		char str_tmp[1 + BDEVNAME_SIZE + 10 + 1];
		const char *partition_name;
		const __be32 *reg;
		int len, a_cells, s_cells;

		reg = of_get_property(pp, "reg", &len);
		if (!reg)
			goto exit;

		a_cells = of_n_addr_cells(pp);
		s_cells = of_n_size_cells(pp);
		if (len / 4 != a_cells + s_cells)
			goto exit;

		put_partition(state, index++,
			of_read_number(reg, a_cells) / sector_size,
			of_read_number(reg + a_cells, s_cells) / sector_size);

		partition_name = of_get_property(pp, "label", &len);
		if (!partition_name)
			goto exit;

		snprintf(str_tmp, sizeof(str_tmp), "(%s)", partition_name);
		strlcat(state->pp_buf, str_tmp, PAGE_SIZE);

		printk(KERN_INFO " %s: 0x%016llx, 0x%016llx\n", str_tmp,
			of_read_number(reg, a_cells),
			of_read_number(reg + a_cells, s_cells));
	}

	strlcat(state->pp_buf, "\n", PAGE_SIZE);
	rval = 1;

exit:
	if (np)
		of_node_put(np);
	return rval;
}

int ambptb_partition(struct parsed_partitions *state)
{
	int i, val, slot = 1;
	unsigned char *data;
	Sector sect;
	u32 sect_size, sect_offset, sect_address, ptb_address;
	flpart_meta_t *ptb_meta;
	char ptb_tmp[1 + BDEVNAME_SIZE + 1];
	int result = 0;
	struct device_node * np;

	ambptb_prt("amb partition: %s\n", bdevname(state->bdev, ptb_tmp));

	sect_size = bdev_logical_block_size(state->bdev);
	sect_offset = (sizeof(ptb_header_t) + sizeof(flpart_table_t)) % sect_size;

	np = of_find_node_with_property(NULL, "amb,ptb_address");
	if(!np)
		return ambptb_emmc_partition(state);

	val = of_property_read_u32(np, "amb,ptb_address", &ptb_address);
	if(val < 0) {
		result = -1;
		goto ambptb_partition_exit;
	}

	sect_address = (ptb_address * sect_size + sizeof(ptb_header_t) + sizeof(flpart_table_t)) / sect_size;
	data = read_part_sector(state, sect_address, &sect);
	if (!data) {
		result = -1;
		goto ambptb_partition_exit;
	}

	ptb_meta = (flpart_meta_t *)(data + sect_offset);
	ambptb_prt("%s: magic[0x%08X]\n", __func__, ptb_meta->magic);
	if (ptb_meta->magic == PTB_META_MAGIC3) {
		for (i = 0; i < PART_MAX; i++) {
			if (slot >= state->limit)
				break;

			if (((ptb_meta->part_info[i].dev & PART_DEV_EMMC) ==
				PART_DEV_EMMC) &&
				(ptb_meta->part_info[i].nblk)) {
				state->parts[slot].from =
					ptb_meta->part_info[i].sblk;
				state->parts[slot].size =
					ptb_meta->part_info[i].nblk;
				snprintf(ptb_tmp, sizeof(ptb_tmp), " %s",
					ptb_meta->part_info[i].name);
				strlcat(state->pp_buf, ptb_tmp, PAGE_SIZE);
				ambptb_prt("%s: %s [p%d]\n", __func__,
					ptb_meta->part_info[i].name, slot);
				slot++;
			}
		}
		strlcat(state->pp_buf, "\n", PAGE_SIZE);
		result = 1;
	} else if ((ptb_meta->magic == PTB_META_MAGIC) ||
		(ptb_meta->magic == PTB_META_MAGIC2)) {
		for (i = 0; i < PART_MAX; i++) {
			if (slot >= state->limit)
				break;
			if ((ptb_meta->part_info[i].dev == BOOT_DEV_SM) &&
				(ptb_meta->part_info[i].nblk)) {
				state->parts[slot].from =
					ptb_meta->part_info[i].sblk;
				state->parts[slot].size =
					ptb_meta->part_info[i].nblk;
				snprintf(ptb_tmp, sizeof(ptb_tmp), " %s",
					ptb_meta->part_info[i].name);
				strlcat(state->pp_buf, ptb_tmp, PAGE_SIZE);
				ambptb_prt("%s: %s [p%d]\n", __func__,
					ptb_meta->part_info[i].name, slot);
				slot++;
			}
		}
		strlcat(state->pp_buf, "\n", PAGE_SIZE);
		result = 1;
	}
	put_dev_sector(sect);

ambptb_partition_exit:
	return result;
}

