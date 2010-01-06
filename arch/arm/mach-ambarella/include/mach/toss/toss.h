/*
 * system/src/toss/toss.h
 *
 * Terminal Operating Systerm Scheduler
 *
 * History:
 *    2009/08/14 - [Charles Chiou] created file
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
 */

#ifndef __TOSS_H__
#define __TOSS_H__

#define MAX_TOSS_PERSONALITY 		2UL

#if defined(__GNUC__)
/* GNU Compiler Setting */
#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__  __attribute__ ((packed))
#endif
#ifndef __ARMCC_PACK__
#define __ARMCC_PACK__
#endif
#elif defined(__arm)
/* RVCT or ADS Compiler setting */
#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__
#endif
#ifndef __ARMCC_PACK__
#define __ARMCC_PACK__  __packed
#endif
#else
/* Unknown compiler */
#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__
#endif
#ifndef __ARMCC_PACK__
#define __ARMCC_PACK__
#endif
#endif

/*
 * ARM state of the OS.
 */
__ARMCC_PACK__ struct toss_arm_state_s
{
	unsigned int r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12;
	unsigned int sp, lr, pc;
	unsigned int cpsr;
	unsigned int usr_sp, usr_lr;
	unsigned int svr_sp, svr_lr, svr_spsr;
	unsigned int abt_sp, abt_lr, abt_spsr;
	unsigned int irq_sp, irq_lr, irq_spsr;
	unsigned int und_sp, und_lr, und_spsr;
	unsigned int fiq_sp, fiq_lr, fiq_spsr;
	unsigned int fiq_r8, fiq_r9, fiq_r10, fiq_r11, fiq_r12;
	unsigned int cp15_control;
	unsigned int cp15_ttbr;
	unsigned int cp15_dacl;
	unsigned int rsv[22];	/* For padding */
} __ATTRIB_PACK__;

/*
 * State to describe each OS personality.
 */
__ARMCC_PACK__ struct toss_personality_s
{
	char name[32];			/* Name of OS */
	unsigned int init_pc;		/* OS initialization PC */
	unsigned int activated;		/* Activation count */
	unsigned int bootparam_atag_data;	/* ATAG data address */
	unsigned int bootparam_membase;		/* Memory base */
	unsigned int bootparam_memsize;		/* Memory size */
	unsigned int rsv[54];		/* For padding */
	struct toss_arm_state_s state;	/* ARM state */
} __ATTRIB_PACK__;

#if defined(__PRKERNEL_AMB__)
#include <toss_hwctx.h>
#include <toss_devctx.h>
#else
#include <mach/toss/toss_hwctx.h>
#include <mach/toss/toss_devctx.h>
#endif

/*
 * Data structure versioning checksum;
 */
__ARMCC_PACK__ struct toss_header_chksum_s
{

	unsigned int toss_h;
	unsigned int toss_hwctx_h;
	unsigned int toss_devctx_h;
	unsigned int rsv;
} __ATTRIB_PACK__;

/*
 * Global TOSS data structure.
 */
__ARMCC_PACK__ struct toss_s
{
#define VTEXT_PAGES	2
	unsigned char vtext[4096 * VTEXT_PAGES];

	unsigned int active;		/* The active personality */
	unsigned int oldctx;		/* The old personality */
	void (*vtext_toss_handoff)(unsigned int, unsigned int);
	unsigned char pad0[244];

	/* Checksum versioning */
	struct toss_header_chksum_s header_chksum[MAX_TOSS_PERSONALITY];
	unsigned char pad2[256 - (sizeof(struct toss_header_chksum_s) *
				 MAX_TOSS_PERSONALITY)];

	/* The personalities describe the ARM state of the OS */
	struct toss_personality_s personalities[MAX_TOSS_PERSONALITY];

	/* The hwctx is a container for hardware (chip) context */
	struct toss_hwctx_s hwctx[MAX_TOSS_PERSONALITY];

	/* The devctx is a container for device (off-chip/logical) context */
	struct toss_devctx_s devctx[MAX_TOSS_PERSONALITY];
} __ATTRIB_PACK__;

#endif
