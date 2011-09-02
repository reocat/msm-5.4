/*
 * drivers/ambarella-ipc/ipc_log.c
 *
 * Ambarella IPC Log
 *
 * History:
 *    2011/05/18 - [Henry Lin] created file
 *
 * Copyright (C) 2011-2011, Ambarella, Inc.
 *
 * All rights reserved. No Part of this file may be reproduced, stored
 * in a retrieval system, or transmitted, in any form, or by any means,
 * electronic, mechanical, photocopying, recording, or otherwise,
 * without the prior consent of Ambarella, Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/irq.h>

#include <linux/aipc/ipc_log.h>
#include <linux/aipc/ipc_slock.h>

#include <mach/boss.h>

/*
 * Definitions
 */
#define IPC_LOG_MSG_BUF_SIZE		256

/*
 * Structures
 */
typedef struct ipc_log_s {
	int	output;
	u8	*buf;
	u8	msg[IPC_LOG_MSG_BUF_SIZE];
} ipc_log_t;

/*
 * Globals
 */
static ipc_log_t G_ipc_log;

/*
 * Declaration
 */
extern struct boss_s *boss;

/*
 * Forward Declaration
 */

/*
 * Init log system.
 */
int ipc_log_init(u32 addr, u32 size)
{
	G_ipc_log.buf = (u8 *) addr;
	G_ipc_log.output = IPC_LOG_LEVEL_OUTPUT;

	return 0;
}

/*
 * Print to log buffer.
 */
void ipc_log_print(int level, const char *fmt, ...)
{
#if IPC_LOG_ENABLE
	va_list args;
	int len, left;
	u8 *msg;

	va_start (args, fmt);

	if (level == IPC_LOG_LEVEL_ERROR) {
		vprintk(fmt, args);
		printk("\n");
	}

	if (level > boss->ipc_log_level) {
		return;
	}

	ipc_spin_lock(boss->ipc_log_lock, IPC_SLOCK_POS_LOG_PRINT);

	msg = G_ipc_log.msg;
	len = vsnprintf((char *) msg, IPC_LOG_MSG_BUF_SIZE, fmt, args);
	len++;
	boss->ipc_log_total += len;

	left = boss->ipc_log_size - boss->ipc_log_ptr - 1;
	if (len > left) {
		memcpy(&G_ipc_log.buf[boss->ipc_log_ptr], msg, left);
		G_ipc_log.buf[boss->ipc_log_size - 1] = 0;
		boss->ipc_log_ptr = 0;
		msg += left;
		len -= left;
	}
	memcpy(&G_ipc_log.buf[boss->ipc_log_ptr], msg, len);
	boss->ipc_log_ptr = (boss->ipc_log_ptr + len) & IPC_LOG_MEM_SIZE_MASK;

	ipc_spin_unlock(boss->ipc_log_lock, IPC_SLOCK_POS_LOG_PRINT);

	if (G_ipc_log.output) {
		if (level != IPC_LOG_LEVEL_ERROR) {
			printk((char *) G_ipc_log.msg);
			printk("\n");
		}
	}
	
	va_end (args);
#endif
}

/*
 * Set log level.
 */
void ipc_log_set_level(int level)
{
	ipc_spin_lock(boss->ipc_log_lock, IPC_SLOCK_POS_LOG_PRINT);
	boss->ipc_log_level = level;
	ipc_spin_unlock(boss->ipc_log_lock, IPC_SLOCK_POS_LOG_PRINT);
}

/*
 * Enable/Disable log output.
 */
void ipc_log_set_output(int output)
{
	G_ipc_log.output = output;
}

