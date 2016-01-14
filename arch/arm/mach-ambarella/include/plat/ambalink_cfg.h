/**
 * History:
 *    2013/10/29 - [Joey Li] created file
 *
 * Copyright (C) 2012-2014, Ambarella, Inc.
 *
 * All rights reserved. No Part of this file may be reproduced, stored
 * in a retrieval system, or transmitted, in any form, or by any means,
 * electronic, mechanical, photocopying, recording, or otherwise,
 * without the prior consent of Ambarella, Inc.
 */
#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK

#ifdef CONFIG_AMBALINK_SHMADDR

/*
 * User define
 */
#define MAX_RPMSG_NUM_BUFS              (2048)
#define RPMSG_BUF_SIZE                  (2048)

/*
 * for RPMSG module
 */
#define RPMSG_TOTAL_BUF_SPACE           (MAX_RPMSG_NUM_BUFS * RPMSG_BUF_SIZE)
/* Alignment to 0x1000, the calculation details can be found in the document. */
#define VRING_SIZE                      ((((MAX_RPMSG_NUM_BUFS / 2) * 19 + (0x1000 - 1)) & ~(0x1000 - 1)) + \
                                        (((MAX_RPMSG_NUM_BUFS / 2) * 17 + (0x1000 - 1)) & ~(0x1000 - 1)))
#define RPC_PROFILE_SIZE                0x1000
#define RPC_RPMSG_PROFILE_SIZE          (RPC_PROFILE_SIZE + ((17 * MAX_RPMSG_NUM_BUFS + (0x1000 - 1)) & ~(0x1000 -1)))

#define VRING_C0_AND_C1_BUF             (CONFIG_AMBALINK_SHMADDR)
#define VRING_C0_TO_C1                  (VRING_C0_AND_C1_BUF + RPMSG_TOTAL_BUF_SPACE)
#define VRING_C1_TO_C0                  (VRING_C0_TO_C1 + VRING_SIZE)

/* FIXME: */
#if 1
#define AHB_BASE			(0xe0000000)
#define APB_BASE			(0xe8000000)

/* Secure and Scratchpad */
#define AHB_SCRATCHPAD_OFFSET		0x1B000
#define AHB_SCRATCHPAD_BASE		(AHB_BASE + AHB_SCRATCHPAD_OFFSET)
#define AHB_SCRATCHPAD_REG(x)		(AHB_SCRATCHPAD_BASE + (x))
#endif

#define AHB_SP_SWI_SET_OFFSET           0x10
#define AHB_SP_SWI_CLEAR_OFFSET         0x14

#define AXI_SOFT_IRQ1(x)                ((x) + 32)  /* 0 <= x <= 6 */
#define AXI_SOFT_IRQ2(x)                ((x) + 48)  /* 0 <= x <= 6 */

#define VRING_IRQ_C0_TO_C1_KICK         AXI_SOFT_IRQ1(0)
#define VRING_IRQ_C0_TO_C1_ACK          AXI_SOFT_IRQ1(1)
#define VRING_IRQ_C1_TO_C0_KICK         AXI_SOFT_IRQ1(2)
#define VRING_IRQ_C1_TO_C0_ACK          AXI_SOFT_IRQ1(3)
#define MUTEX_IRQ_REMOTE                AXI_SOFT_IRQ1(4)
#define MUTEX_IRQ_LOCAL                 AXI_SOFT_IRQ1(5)
#define AMBALINK_AMP_SUSPEND_KICK       AXI_SOFT_IRQ1(6)

/*
 * for spinlock module
 */
#define AIPC_SLOCK_ADDR                 (VRING_C1_TO_C0 + VRING_SIZE)
#define AIPC_SLOCK_SIZE                 (0x1000)

/*
 * for mutex module
 */
#define AIPC_MUTEX_ADDR                 (AIPC_SLOCK_ADDR + AIPC_SLOCK_SIZE)
#define AIPC_MUTEX_SIZE                 0x1000

/*
 * for RPMSG suspend backup area
 */
#define RPMSG_SUSPEND_BACKUP_SIZE       0x20000

/*
 * for RPC and RPMSG profiling
 */
#define RPC_PROFILE_ADDR                (AIPC_MUTEX_ADDR + AIPC_MUTEX_SIZE + RPMSG_SUSPEND_BACKUP_SIZE)
#define RPMSG_PROFILE_ADDR              (RPC_PROFILE_ADDR + RPC_PROFILE_SIZE)

/*
 * general settings
 */
#define AMBALINK_CORE_LOCAL             0x2
#define ERG_SIZE                        8

#define ambalink_virt_to_phys(x)        virt_to_phys(x)
#define ambalink_phys_to_virt(x)        phys_to_virt(x)
#define ambalink_page_to_phys(x)        (page_to_phys(x) -  DEFAULT_MEM_START)
#define ambalink_phys_to_page(x)        phys_to_page((x) +  DEFAULT_MEM_START)

/* address translation in loadable kernel module */
#define ambalink_lkm_virt_to_phys(x)	(__pfn_to_phys(vmalloc_to_pfn((void *) (x))) + ((u32)(x) & (~PAGE_MASK)) - DEFAULT_MEM_START)

#else
#error "CONFIG_AMBAKLINK_SHM_ADDR is not defined"
#endif

#else
#error "include ambalink_cfg.h while PLAT_AMBARELLA_AMBALINK is not defined"
#endif
