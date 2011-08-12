/*
 * arch/arm/mach-boss/include/mach/boss.h
 *
 * Authors:
 *	Charles Chiou <cchiou@ambarella.com>
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

#ifndef __ASM_ARCH_BOSS_H
#define __ASM_ARCH_BOSS_H

#if !defined(__ASM__)
#include <linux/aipc/aipc.h>
#endif

#define BOSS_BOSS_MEM_SIZE		0x1000		/* 4KB */
#define BOSS_LINUX_VERSION		0x00000003	/* linux struct boss_s version */

#if (CHIP_REV == A5S)
#define BOSS_VIRT_H2G_INT_REQ_VEC	32	/* Virtual 'host-to-guest' irq */
#define BOSS_VIRT_H2G_INT_RLY_VEC	53	/* Virtual 'host-to-guest' irq */
#define BOSS_VIRT_G2H_INT_REQ_VEC	35	/* Virtual 'guest-to-host' irq */
#define BOSS_VIRT_G2H_INT_RLY_VEC	54	/* Virtual 'guest-to-host' irq */
#define BOSS_VIRT_H2G_MTX_VEC		51	/* Virtual 'host-to-guest' mutex irq */
#define BOSS_VIRT_G2H_MTX_VEC		52	/* Virtual 'guest-to-host' mutex irq */
#elif (CHIP_REV == I1)
#define BOSS_VIRT_H2G_INT_REQ_VEC	AXI_SOFT_IRQ(0)	/* Virtual 'host-to-guest' irq */
#define BOSS_VIRT_H2G_INT_RLY_VEC	AXI_SOFT_IRQ(2)	/* Virtual 'host-to-guest' irq */
#define BOSS_VIRT_G2H_INT_REQ_VEC	3	/* Virtual 'guest-to-host' irq */
#define BOSS_VIRT_G2H_INT_RLY_VEC	68	/* Virtual 'guest-to-host' irq */
#define BOSS_VIRT_H2G_MTX_VEC		AXI_SOFT_IRQ(1)	/* Virtual 'host-to-guest' irq */
#define BOSS_VIRT_G2H_MTX_VEC		64	/* Virtual 'guest-to-host' irq */
#else
#error "Boss is not supported on this chip!"
#endif

#define BOSS_VIRT_TIMER_INT_VEC		47	/* Virtual 'timer' to guest irq */
#define BOSS_VIRT_GIRQ_INT_VEC		50	/* Virtual 'guest IRQ' irq */

/* Keep these in sync with data structure below */
#define BOSS_ROOT_CTX_OFFSET		0
#define BOSS_GUEST_CTX_OFFSET		4
#define BOSS_MODE_OFFSET		8
#define BOSS_GILDLE_OFFSET		12
#define BOSS_ENVTIMER_OFFSET		16
#define BOSS_CVTIMER_OFFSET		20
#define BOSS_NVTIMER_OFFSET		24
#define BOSS_IRQNO_OFFSET		28
#define BOSS_VIC1MASK_OFFSET		32
#define BOSS_VIC2MASK_OFFSET		36
#define BOSS_VIC3MASK_OFFSET		40
#define BOSS_GPIO0MASK_OFFSET		44
#define BOSS_GPIO1MASK_OFFSET		48
#define BOSS_GPIO2MASK_OFFSET		52
#define BOSS_GPIO3MASK_OFFSET		56
#define BOSS_GPIO4MASK_OFFSET		60
#define BOSS_GPIO5MASK_OFFSET		64

#if !defined(__ASM__)

enum {
	BOSS_STATE_UNINIT = 0,
	BOSS_STATE_INIT,
	BOSS_STATE_BOOTING,
	BOSS_STATE_READY,
	BOSS_STATE_NUM
};

enum device_privilege
{
	PRIV_UNKNOWN=0,
	PRIV_LINUX_OS,
	PRIV_UITRON_OS
};

struct privilege
{
	unsigned int usb;
};

/*
 * CPU context.
 */
struct boss_context_s
{
	unsigned int r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12;
	unsigned int pc, cpsr;
	unsigned int usr_sp, usr_lr;
	unsigned int svr_sp, svr_lr, svr_spsr;
	unsigned int abt_sp, abt_lr, abt_spsr;
	unsigned int irq_sp, irq_lr, irq_spsr;
	unsigned int und_sp, und_lr, und_spsr;
	unsigned int fiq_sp, fiq_lr, fiq_spsr;
	unsigned int fiq_r8, fiq_r9, fiq_r10, fiq_r11, fiq_r12;
	unsigned int cp15_ctr;
	unsigned int cp15_ttb;
	unsigned int cp15_dac;
};

/*
 * The data structure for BOSS.
 */
struct boss_s
{
	struct boss_context_s *root_ctx;	/* Root context */
	struct boss_context_s *guest_ctx;	/* Guest context */
	unsigned int *mode;			/* BOSS mode */
	unsigned int *gidle;			/* Guest idle flag */

	/*
	 * The following 3 fields are used by BOSS to manage the
	 * vtimer interrupt to the guest OS.
	 */
	unsigned int *envtimer;		/* vtimer enabled? */
	unsigned int *cvtimer;		/* vtimer counter */
	unsigned int *nvtimer;		/* Number of vtimer delivered */
	unsigned int *irqno;		/* IRQ number computed by BOSS */

	/*
	 * The following 2 fields are zero-initialized by the root OS.
	 * The guest OS may modify the VIC controllers, but should set
	 * the bitmasks below to indicate to the root OS which lines that
	 * it subscribes to.
	 */
	unsigned int vic1mask;		/* VIC1 bitmask */
	unsigned int vic2mask;		/* VIC2 bitmask */
	unsigned int vic3mask;		/* VIC3 bitmask */
	unsigned int gpio0mask;		/* GPIO0 bitmask */
	unsigned int gpio1mask;		/* GPIO1 bitmask */
	unsigned int gpio2mask;		/* GPIO2 bitmask */
	unsigned int gpio3mask;		/* GPIO3 bitmask */
	unsigned int gpio4mask;		/* GPIO4 bitmask */
	unsigned int gpio5mask;		/* GPIO5 bitmask */

	/*
	 * The following 3 variables are initialized to 0 and expected to
	 * be filled in by the guest OS and maintained thereafter to trigger
	 * log buffer updates and dumps by the root OS.
	 */
	unsigned int log_buf_ptr;	/* Guest OS log buffer address */
	unsigned int log_buf_len_ptr;	/* Guest OS log buffer length */
	unsigned int log_buf_last_ptr;	/* Index to latest updated buf. */

	unsigned int state;		/* BOSS state */
	unsigned int lock;		/* BOSS lock */

	unsigned int smem_addr;	/* Shared memory address */
	unsigned int smem_size;	/* Shared memory length */

	unsigned int ipc_log_lock;
	unsigned int ipc_log_level;
	unsigned int ipc_log_ptr;
	unsigned int ipc_log_size;
	unsigned int ipc_log_total;

	/*
	 * device controll privilege, linux or uItron
	 */
	struct privilege device_priv;

	/*
	 * The following fields are used by the IPC drivers.
	 * The IPC binder on the root OS sets up the pointers and the sizes of
	 * the circular buffers so that the corresponding IPC binder on the
	 * remote OS can retrieve these when it boots up.
	 */
	struct ipc_buf_s ipc_buf;	/* IPC buffer */

};

extern struct boss_s *boss;

#endif  /* !__ASM__ */

#endif

