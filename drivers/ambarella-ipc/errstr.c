/*
 * drivers/ambarella-ipc/errstr.c
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

#include <linux/aipc/aipc.h>

const char *ipc_strerror(enum clnt_stat stat)
{
	static const char *errstr[] = {
		"success",
		"invalid client handle",
		"server program not registered",
		"server procedure not registered",
		"failed in sending",
		"failed in receiving",
		"call timeout",
		"version mismatch",
		"server program unavailable",
		"server procedure unavailable",
		"server handler unable to process argument",
		"server ran out of memory",
		"unknown",
		"processsing",
		"command queue is full",
		"ipc service is not initialized",
		"ipc service is canceled",
	};
	static int nerrstr = sizeof(errstr) / sizeof(const char *);

	if (stat < nerrstr)
		return errstr[stat];

	return "bad code";
}
EXPORT_SYMBOL(ipc_strerror);
