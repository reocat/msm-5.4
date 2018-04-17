/*
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.
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

#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK

#include <plat/chip.h>

enum {
		AMBARELLA_RPROC = 0,
		C1_AND_C2,
		AMBALINK_NUM,
};

struct aipc_shared_memory_info {
	u64	vring_host_and_clnt_buf;
	u64	vring_host_to_clnt;
	u64	vring_clnt_to_host;
};

struct ambalink_shared_memory_layout {
        u64     vring_c0_and_c1_buf;
        u64     vring_c0_to_c1;
        u64     vring_c1_to_c0;
        u64     rpmsg_suspend_backup_addr;
        u64     rpc_profile_addr;
        u64     rpmsg_profile_addr;
        u64     aipc_slock_addr;
        u64     aipc_mutex_addr;
};

extern struct ambalink_shared_memory_layout ambalink_shm_layout;
extern struct aipc_shared_memory_info aipc_shm_info[AMBALINK_NUM];
extern const char *rproc_names[AMBALINK_NUM];

void ambalink_init_mem(void);

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
#define MAX_RPC_RPMSG_PROFILE_SIZE      0x11000
/* Alignment to 0x1000, the calculation details can be found in the document. */
#define RPC_RPMSG_PROFILE_SIZE          (RPC_PROFILE_SIZE + ((17 * MAX_RPMSG_NUM_BUFS + (0x1000 - 1)) & ~(0x1000 -1)))

/* for RPMSG suspend backup area */
#define RPMSG_SUSPEND_BACKUP_SIZE       0x20000

/* for spinlock module */
#define AIPC_SLOCK_SIZE                 (0x1000)

/* for mutex module */
#define AIPC_MUTEX_SIZE                 0x1000

/* general settings */
#define AMBALINK_CORE_RTOS		0x1
#define AMBALINK_CORE_GUEST1		0x2
#define AMBALINK_CORE_GUEST2            0x4
#define ERG_SIZE                        16

/*
 * for software interrupt
 */
#define AHB_SP_SWI_SET_OFFSET           0x10
#define AHB_SP_SWI_CLEAR_OFFSET         0x14

#define AHB_SP_SWI_SET_OFFSET1          0x44
#define AHB_SP_SWI_CLEAR_OFFSET1        0x48

#define AXI_SPI_BASE                    32

#if (CHIP_REV == S5)
/*#define AXI_SOFT_IRQ(x)                ((x) + 0) */  /* 0 <= x <= 6 */
#define AXI_SOFT_IRQ(x)                ((x) + 0)  /* 0 <= x <= 6 */
#define AXI_SOFT_IRQ2(x)               ((x) + 16)  /* 0 <= x <= 6 */
#elif (CHIP_REV == S5L)
#define AXI_SOFT_IRQ(x)                ((x + AXI_SPI_BASE) + 7)  /* 0 <= x <= 13 */
#elif (CHIP_REV == CV1)
#define AXI_SOFT_IRQ(x)                ((x) + 91)  /* 0 <= x <= 13 */
#else
#error "unsupport chip!!"
#endif

#if (CHIP_REV == S5L) /*H22*/
#define VRING_IRQ_C0_TO_C1_KICK         AXI_SOFT_IRQ(0)
#define VRING_IRQ_C0_TO_C1_ACK          AXI_SOFT_IRQ(1)
#define VRING_IRQ_C1_TO_C0_KICK         AXI_SOFT_IRQ(2)
#define VRING_IRQ_C1_TO_C0_ACK          AXI_SOFT_IRQ(3)
#define MUTEX_IRQ_RTOS                	AXI_SOFT_IRQ(4)
#define MUTEX_IRQ_GUEST1		AXI_SOFT_IRQ(5)
#define MUTEX_IRQ_GUEST2		AXI_SOFT_IRQ(6)
#define VRING_IRQ_C1_TO_C2_KICK         AXI_SOFT_IRQ(7)
#define VRING_IRQ_C1_TO_C2_ACK          AXI_SOFT_IRQ(8)
#define VRING_IRQ_C2_TO_C1_KICK         AXI_SOFT_IRQ(9)
#define VRING_IRQ_C2_TO_C1_ACK          AXI_SOFT_IRQ(10)

#elif (CHIP_REV == S5) /*H2*/
#define VRING_IRQ_C0_TO_C1_KICK         AXI_SOFT_IRQ(0)
#define VRING_IRQ_C0_TO_C1_ACK          AXI_SOFT_IRQ(1)
#define VRING_IRQ_C1_TO_C0_KICK         AXI_SOFT_IRQ(2)
#define VRING_IRQ_C1_TO_C0_ACK          AXI_SOFT_IRQ(3)
#define MUTEX_IRQ_RTOS                	AXI_SOFT_IRQ(4)
#define MUTEX_IRQ_GUEST1		AXI_SOFT_IRQ(5)
#define MUTEX_IRQ_GUEST2		AXI_SOFT_IRQ(6)
#define VRING_IRQ_C1_TO_C2_KICK         AXI_SOFT_IRQ2(0)
#define VRING_IRQ_C1_TO_C2_ACK          AXI_SOFT_IRQ2(1)
#define VRING_IRQ_C2_TO_C1_KICK         AXI_SOFT_IRQ2(2)
#define VRING_IRQ_C2_TO_C1_ACK          AXI_SOFT_IRQ2(3)

#elif (CHIP_REV == CV1)
#define VRING_IRQ_C0_TO_C1_KICK         AXI_SOFT_IRQ(0)
#define VRING_IRQ_C0_TO_C1_ACK          AXI_SOFT_IRQ(1)
#define VRING_IRQ_C1_TO_C0_KICK         AXI_SOFT_IRQ(2)
#define VRING_IRQ_C1_TO_C0_ACK          AXI_SOFT_IRQ(3)
#define MUTEX_IRQ_RTOS                	AXI_SOFT_IRQ(4)
#define MUTEX_IRQ_GUEST1		AXI_SOFT_IRQ(5)
#define MUTEX_IRQ_GUEST2		AXI_SOFT_IRQ(6)
#define VRING_IRQ_C1_TO_C2_KICK         AXI_SOFT_IRQ(7)
#define VRING_IRQ_C1_TO_C2_ACK          AXI_SOFT_IRQ(8)
#define VRING_IRQ_C2_TO_C1_KICK         AXI_SOFT_IRQ(9)
#define VRING_IRQ_C2_TO_C1_ACK          AXI_SOFT_IRQ(10)

#else
#error "unsupported chip"
#endif

#define ambalink_page_to_phys(x)        page_to_phys(x)
#define ambalink_phys_to_page(x)        phys_to_page(x)

unsigned long ambalink_phys_to_virt(phys_addr_t paddr);
phys_addr_t ambalink_virt_to_phys(unsigned long vaddr);

/* address translation in loadable kernel module */
#define ambalink_lkm_virt_to_phys(x)	(__pfn_to_phys(vmalloc_to_pfn((void *) (x))) + ((u32)(x) & (~PAGE_MASK)) - DEFAULT_MEM_START)

#else
#error "include ambalink_cfg.h while ARCH_AMBARELLA_AMBALINK is not defined"
#endif
