/*
 * drivers/ambarella-ipc/mem.c
 *
 * Ambarella IPC in Linux kernel-space.
 *
 * Authors:
 *	Charles Chiou <cchiou@ambarella.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * Copyright (C) 2009-2010, Ambarella Inc.
 */

#include <linux/slab.h>
#include <linux/aipc/aipc.h>

void *ambarella_ipc_malloc(unsigned int size)
{
	return kmalloc(size, GFP_KERNEL);
}
EXPORT_SYMBOL(ambarella_ipc_malloc);

void ambarella_ipc_free(void *ptr)
{
	if (ptr != NULL)
		kfree(ptr);
}
EXPORT_SYMBOL(ambarella_ipc_free);
