/*
 * arch/arm/plat-ambarella/generic/init.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/memblock.h>
#include <linux/export.h>
#include <linux/clk.h>

#include <asm/cacheflush.h>
#include <asm/system_info.h>

#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/init.h>

#include <plat/bapi.h>
#include <plat/clk.h>

/* ==========================================================================*/
u64 ambarella_dmamask = DMA_BIT_MASK(32);
EXPORT_SYMBOL(ambarella_dmamask);

/* ==========================================================================*/
static struct proc_dir_entry *ambarella_proc_dir = NULL;

int __init ambarella_create_proc_dir(void)
{
	int ret_val = 0;

	ambarella_proc_dir = proc_mkdir("ambarella", NULL);
	if (!ambarella_proc_dir)
		ret_val = -ENOMEM;

	return ret_val;
}

struct proc_dir_entry *get_ambarella_proc_dir(void)
{
	return ambarella_proc_dir;
}
EXPORT_SYMBOL(get_ambarella_proc_dir);

/* ==========================================================================*/
void __init ambarella_memblock_reserve(void)
{
	struct ambarella_mem_rev_info		rev_info;
	int					i;
	u32					bstadd, bstsize;

	if (!get_ambarella_mem_rev_info(&rev_info)) {
		for (i = 0; i < rev_info.counter; i++) {
			memblock_reserve(rev_info.desc[i].physaddr,
				rev_info.desc[i].size);
		}
	}

	if (get_ambarella_bstmem_info(&bstadd, &bstsize) == AMB_BST_MAGIC) {
		pr_info("\t--:\t0x%08x[0x%08x]\tBST\n", bstadd, bstsize);
	}
}

/* ==========================================================================*/
int __init ambarella_init_machine(char *board_name, unsigned int ref_freq)
{
	int ret_val = 0;

	ambarella_board_generic.board_chip = AMBARELLA_BOARD_CHIP(system_rev);
	ambarella_board_generic.board_type = AMBARELLA_BOARD_TYPE(system_rev);
	ambarella_board_generic.board_rev = AMBARELLA_BOARD_REV(system_rev);

	pr_info("Ambarella %s:\n", board_name);
	pr_info("\tchip id:\t\t%d\n", ambarella_board_generic.board_chip);
	pr_info("\tboard type:\t\t%d\n", ambarella_board_generic.board_type);
	pr_info("\tboard revision:\t\t%d\n", ambarella_board_generic.board_rev);
	ambarella_board_generic.board_poc = amba_rct_readl(SYS_CONFIG_REG);
	pr_info("\tsystem configuration:\t0x%08x\n",
		ambarella_board_generic.board_poc);

#if defined(CONFIG_PLAT_AMBARELLA_CORTEX)
#if !defined(CONFIG_AMBARELLA_RAW_BOOT)
#if (CHIP_REV == I1 || CHIP_REV == S2)
	amba_rct_writel(SCALER_ARM_ASYNC_REG, 0xF);
#endif
#endif
#endif

	ret_val = ambarella_create_proc_dir();
	BUG_ON(ret_val != 0);

	ret_val = ambarella_clk_init(ref_freq);
	BUG_ON(ret_val != 0);

	ret_val = ambarella_init_gpio();
	BUG_ON(ret_val != 0);

	ret_val = ambarella_init_fio();
	BUG_ON(ret_val != 0);

	ret_val = ambarella_init_adc();
	BUG_ON(ret_val != 0);

#if defined(CONFIG_HAVE_PWM)
	ret_val = ambarella_init_pwm();
	BUG_ON(ret_val != 0);
#endif
	ret_val = ambarella_init_fb();
	BUG_ON(ret_val != 0);

	ret_val = ambarella_init_pm();
	BUG_ON(ret_val != 0);

	ret_val = ambarella_init_audio();
	BUG_ON(ret_val != 0);

	ret_val = ambarella_init_sd();
	BUG_ON(ret_val != 0);

#if (ETH_INSTANCES >= 1)
	ret_val = ambarella_init_eth0(ambarella_board_generic.eth0_mac);
	BUG_ON(ret_val != 0);
#endif
#if (ETH_INSTANCES >= 2)
	ret_val = ambarella_init_eth1(ambarella_board_generic.eth1_mac);
	BUG_ON(ret_val != 0);
#endif

	return ret_val;
}

void ambarella_restart_machine(char mode, const char *cmd)
{
#if defined(CONFIG_AMBARELLA_SUPPORT_BAPI)
	struct ambarella_bapi_reboot_info_s	reboot_info;

	reboot_info.magic = DEFAULT_BAPI_REBOOT_MAGIC;
	reboot_info.mode = AMBARELLA_BAPI_CMD_REBOOT_NORMAL;
	if (cmd) {
		if(strcmp(cmd, "recovery") == 0) {
			reboot_info.mode = AMBARELLA_BAPI_CMD_REBOOT_RECOVERY;
		} else if(strcmp(cmd, "fastboot") == 0) {
			reboot_info.mode = AMBARELLA_BAPI_CMD_REBOOT_FASTBOOT;
		}
	}
	ambarella_bapi_cmd(AMBARELLA_BAPI_CMD_SET_REBOOT_INFO, &reboot_info);
#endif

	local_irq_disable();
	local_fiq_disable();
	flush_cache_all();
	amba_rct_writel(SOFT_OR_DLL_RESET_REG, 0x2);
	amba_rct_writel(SOFT_OR_DLL_RESET_REG, 0x3);
}

