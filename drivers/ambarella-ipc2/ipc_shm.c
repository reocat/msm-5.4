/*
 * drivers/ambarella-ipc/ipc_shm.c
 *
 * Authors:
 *	Henry Lin <hllin@ambarella.com>
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
 * Copyright (C) 2009-2011, Ambarella Inc.
 */

#include <linux/aipc/ipc_shm.h>

typedef struct ipc_shm_area_s {
	int	offset;
	int	num;
} ipc_shm_area_t;

typedef struct ipc_shm_obj_s {
	unsigned int	*shms;
	int	num;
	ipc_shm_area_t	areas[IPC_SHM_AREA_NUM];
} ipc_shm_obj_t;

static ipc_shm_obj_t shm_obj;

void ipc_shm_init (unsigned int addr, unsigned int size)
{
	int i, offset = 0;
	ipc_shm_area_t *area;

	memset ((void *) addr, 0, size);

	shm_obj.shms = (unsigned int *) addr;
	shm_obj.num = size / sizeof (unsigned int);

	area = &shm_obj.areas[IPC_SHM_AREA_GLOBAL];
	area->num = IPC_SHM_GLOBAL_NUM;
	area = &shm_obj.areas[IPC_SHM_AREA_ARM];
	area->num = IPC_SHM_ARM_NUM;
	area = &shm_obj.areas[IPC_SHM_AREA_CORTEX0];
	area->num = IPC_SHM_CORTEX_NUM;
	area = &shm_obj.areas[IPC_SHM_AREA_CORTEX1];
	area->num = IPC_SHM_CORTEX_NUM;

	for (i = 0; i < IPC_SHM_AREA_NUM; i++) {
		area = &shm_obj.areas[i];
		area->offset = offset;
		offset += area->num;
	}

}

unsigned int *ipc_shm_get(int aid, int i)
{
	return &shm_obj.shms[shm_obj.areas[aid].offset + i];
}

void ipc_shm_write(int aid, int i, unsigned int val)
{
	shm_obj.shms[shm_obj.areas[aid].offset + i] = val;
}

unsigned int ipc_shm_read(int aid, int i)
{
	return shm_obj.shms[shm_obj.areas[aid].offset + i];
}

void ipc_shm_inc(int aid, int i)
{
	shm_obj.shms[shm_obj.areas[aid].offset + i]++;
}

