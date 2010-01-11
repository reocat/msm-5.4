/*
 * toss/linux/toss.c
 *
 * Terminal Operating Systerm Scheduler
 *
 * History:
 *    2009/08/14 - [Charles Chiou] created file
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/map.h>

#include <mach/toss/toss.h>
#include "init.h"

#define HOTBOOT_MEM_ADDR	(ambarella_phys_to_virt(0xc007fff0))
#define HOTBOOT_MAGIC0		(0x14cd78a0)
#define HOTBOOT_MAGIC1		(0x319837fb)

struct toss_s *toss = NULL;
EXPORT_SYMBOL(toss);

/*
 * User-space script path.
 */
static char toss_script_path[256] = "/etc/toss/toss.event.sh";

/*
 * Incoming script invocation argv.
 */
static char *toss_incoming_argv[] =
{
	toss_script_path,
	"incoming",
	NULL,
};

/*
 * Outgoing script invocation argv.
 */
static char *toss_outgoing_argv[] =
{
	toss_script_path,
	"outgoing",
	NULL,
};

/*
 * Minimal script envp to get things going.
 */
static char *toss_script_envp[] = {
	"HOME=/",
	"TERM=linux",
	"PATH=/sbin:/bin:/usr/bin:/usr/sbin:",
	NULL,
};

/*
 * Switch to another OS.
 */
int toss_switch(unsigned int personality)
{
	int rval = 0;
	unsigned int idx_this, idx_that;
	struct toss_personality_s *os_this, *os_that;
	unsigned long flags;

	if (toss == NULL) {
		pr_err("toss: inactive!\n");
		rval = -EINVAL;
		goto done;
	}

	if (personality >= MAX_TOSS_PERSONALITY) {
		pr_err("toss: invalid personality requested (%d)!\n",
		       personality);
		rval = -EINVAL;
		goto done;
	}

	idx_this = toss->active;
	idx_that = personality;
	os_this = &toss->personalities[idx_this];
	os_that = &toss->personalities[idx_that];

	if (personality == toss->active) {
		pr_err("toss: attempting to switch to self (%d)!\n",
		       personality);
		rval = -EINVAL;
		goto done;
	}

	if (os_that->init_pc == 0x0) {
		pr_err("toss: personality (%d) is not ready!\n",
		       personality);
		rval = -EINVAL;
		goto done;
	}

	pr_notice("toss: handing off from linux\n");

	/* Invoke user-space script to handle incoming OS switch */
	call_usermodehelper(toss_script_path,
			    toss_outgoing_argv,
			    toss_script_envp,
			    UMH_WAIT_PROC);

	rval = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_PRE_TOSS, NULL));
	if (rval) {
		pr_err("%s: AMBA_EVENT_PRE_TOSS failed(%d)\n",
			__func__, rval);
	}

	local_irq_save(flags);

	rval = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_PRE_TOSS, NULL));
	if (rval) {
		pr_err("%s: AMBA_EVENT_PRE_TOSS failed(%d)\n",
			__func__, rval);
	}

	/* Update the context of TOSS and jump to the other OS */
	toss->oldctx = toss->active;
	toss->active = personality;
	os_that->activated++;
	toss->vtext_toss_handoff((unsigned int) &os_this->state,
				 (unsigned int) &os_that->state);

	rval = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_POST_TOSS, NULL));
	if (rval) {
		pr_err("%s: AMBA_EVENT_POST_TOSS failed(%d)\n",
			__func__, rval);
	}

	local_irq_restore(flags);

	rval = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_POST_TOSS, NULL));
	if (rval) {
		pr_err("%s: AMBA_EVENT_POST_TOSS failed(%d)\n",
			__func__, rval);
	}

	/* Invoke user-space script to handle outgoing OS switch */
	call_usermodehelper(toss_script_path,
			    toss_incoming_argv,
			    toss_script_envp,
			    UMH_WAIT_PROC);

	pr_notice("toss: linux resumes control\n");

done:

	return rval;
}
EXPORT_SYMBOL(toss_switch);

/*
 * Hotboot to the boot loader.
 */
void hotboot(unsigned int pattern)
{

	pr_notice("hotboot to 0x%.8x ...\n\n", pattern);

	local_irq_disable();

	/* Write pattern to memory */
	amba_writel(HOTBOOT_MEM_ADDR + 0x0, HOTBOOT_MAGIC0);
	amba_writel(HOTBOOT_MEM_ADDR + 0x4, pattern);
	amba_writel(HOTBOOT_MEM_ADDR + 0x8, 0x0);
	amba_writel(HOTBOOT_MEM_ADDR + 0xC, HOTBOOT_MAGIC1);

	/* Reset chip */
	rct_reset_chip();

	local_irq_enable();
}

#if defined(CONFIG_PROC_FS)

static int toss_read_proc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	unsigned int l = 0, i;
	*start = page + off;

	if (toss == NULL)
		return 0;

	l += sprintf(*start + l, "active: %d\n", toss->active);

	for (i = 0; i < MAX_TOSS_PERSONALITY; i++) {
		struct toss_personality_s *p = &toss->personalities[i];
		l += sprintf(*start + l, "---------------------------------------\n");
		l += sprintf(*start + l, "name: %s\n", p->name);
		l += sprintf(*start + l, "init_pc: 0x%.8x\n", p->init_pc);
		l += sprintf(*start + l, "activated: %d\n", p->activated);
		l += sprintf(*start + l, "r0:  0x%.8x r1:  0x%.8x r2:  0x%.8x r3:  0x%.8x\n",
			      p->state.r0, p->state.r1, p->state.r2, p->state.r3);
		l += sprintf(*start + l, "r4:  0x%.8x r5:  0x%.8x r6:  0x%.8x r7:  0x%.8x\n",
			      p->state.r4, p->state.r5, p->state.r6, p->state.r7);
		l += sprintf(*start + l, "r8:  0x%.8x r9:  0x%.8x r10: 0x%.8x r11: 0x%.8x\n",
			      p->state.r8, p->state.r9,
			     p->state.r10, p->state.r11);
		l += sprintf(*start + l, "r12: 0x%.8x\n", p->state.r12);
		l += sprintf(*start + l, "sp:  0x%.8x lr:  0x%.8x pc:  0x%.8x\n",
			       p->state.sp, p->state.lr, p->state.pc);
		l += sprintf(*start + l, "cpsr:    0x%.8x\n", p->state.cpsr);

		l += sprintf(*start + l, "usr_sp:  0x%.8x usr_lr:  0x%.8x\n",
			      p->state.usr_sp, p->state.usr_lr);
		l += sprintf(*start + l, "svr_sp:  0x%.8x svr_lr:  0x%.8x svr_spsr: 0x%.8x\n",
			      p->state.svr_sp, p->state.svr_lr, p->state.svr_spsr);
		l += sprintf(*start + l, "abt_sp:  0x%.8x abt_lr:  0x%.8x abt_spsr: 0x%.8x\n",
			      p->state.abt_sp, p->state.abt_lr, p->state.abt_spsr);
		l += sprintf(*start + l, "irq_sp:  0x%.8x irq_lr:  0x%.8x irq_spsr: 0x%.8x\n",
			      p->state.irq_sp, p->state.irq_lr, p->state.irq_spsr);
		l += sprintf(*start + l, "und_sp:  0x%.8x und_lr:  0x%.8x und_spsr: 0x%.8x\n",
			      p->state.und_sp, p->state.und_lr, p->state.und_spsr);
		l += sprintf(*start + l, "fiq_sp:  0x%.8x fiq_lr:  0x%.8x fiq_spsr: 0x%.8x\n",
			      p->state.fiq_sp, p->state.fiq_lr, p->state.fiq_spsr);
		l += sprintf(*start + l, "fiq_r8:  0x%.8x fiq_r9:  0x%.8x fiq_r10:  0x%.8x\n",
			      p->state.fiq_r8, p->state.fiq_r9, p->state.fiq_r10);
		l += sprintf(*start + l, "fiq_r11: 0x%.8x fiq_r12: 0x%.8x\n",
			      p->state.fiq_r11, p->state.fiq_r12);
		l += sprintf(*start + l, "cp15_control: 0x%.8x\n", p->state.cp15_control);
		l += sprintf(*start + l, "cp15_ttbr:    0x%.8x\n", p->state.cp15_ttbr);
		l += sprintf(*start + l, "cp15_dacl:    0x%.8x\n", p->state.cp15_dacl);
	}

	if (off + l > count) {
		*eof = 1;
		l = 0;
	}

	return l;
}

static int toss_write_proc(struct file *file, const char __user *buffer,
			   unsigned long count, void *data)
{
	int rval = 0;
	char buf[128];
	unsigned int personality;

	if (count > sizeof(data))
		count = sizeof(data);

	if (copy_from_user(buf, buffer, count)) {
		rval = -EFAULT;
		goto done;
	}
	buf[count - 1] = '\0';

	if (sscanf(buf, "%x", &personality) != 1) {
		rval = -EFAULT;
	}

	rval = toss_switch(personality);

done:

	return rval < 0 ? rval : count;
}

static int hotboot_read_proc(char *page, char **start, off_t off,
			     int count, int *eof, void *data)
{
	return 0;
}

static int hotboot_write_proc(struct file *file, const char __user *buffer,
			      unsigned long count, void *data)
{
	int rval = 0;
	char buf[128];
	unsigned int pattern;

	if (count > sizeof(data))
		count = sizeof(data);

	if (copy_from_user(buf, buffer, count)) {
		rval = -EFAULT;
		goto done;
	}
	buf[count - 1] = '\0';

	if (sscanf(buf, "%x", &pattern) != 1) {
		rval = -EFAULT;
		goto done;
	}

	hotboot(pattern);

done:

	return rval < 0 ? rval : count;
}

#endif  /* CONFIG_PROC_FS */

/*
 * Entry function.
 */
int __init ambarella_init_toss(void)
{
	int rval = 0;
	struct toss_header_chksum_s *root_chksum, *my_chksum;
	struct toss_personality_s *personality;
#if defined(CONFIG_PROC_FS)
	struct proc_dir_entry *pde_toss;
	struct proc_dir_entry *pde_hb;
#endif
	struct ambarella_nand_timing nand_timing;

	if (toss == NULL) {
		pr_info("toss: inactive!\n");
		goto ambarella_init_toss_exit;
	}

	root_chksum = &toss->header_chksum[0];

	/* Set the checksums */
	my_chksum = &toss->header_chksum[toss->active];
	my_chksum->toss_h = TOSS_H_CHKSUM;
	my_chksum->toss_hwctx_h = TOSS_HWCTX_H_CHKSUM;
	my_chksum->toss_devctx_h = TOSS_DEVCTX_H_CHKSUM;

	if (my_chksum->toss_h != root_chksum->toss_h) {
		pr_err("toss: toss_h checksum mismatch (0x%.8x != 0x%.8x)!\n",
		       my_chksum->toss_h, root_chksum->toss_h);
		pr_err("toss: driver NOT installed!\n");
		rval = -EINVAL;
		goto ambarella_init_toss_exit;
	}
	if (my_chksum->toss_hwctx_h != root_chksum->toss_hwctx_h)
		pr_warning("toss: toss_hwctx.h out of date (0x%.8x != 0x%.8x)!\n",
		       my_chksum->toss_hwctx_h, root_chksum->toss_hwctx_h);
	if (my_chksum->toss_devctx_h != root_chksum->toss_devctx_h)
		pr_warning("toss: toss_devctx.h out of date (0x%.8x != 0x%.8x)!\n",
		       my_chksum->toss_devctx_h, root_chksum->toss_devctx_h);

	/* Setup my own personality */
	personality = &toss->personalities[toss->active];
	snprintf(personality->name, sizeof(personality->name), "Linux");

#if defined(CONFIG_PROC_FS)
	pde_toss = create_proc_entry("toss", S_IRUGO | S_IWUSR,
				     get_ambarella_proc_dir());
	if (pde_toss == NULL)
		return -ENOMEM;

	pde_toss->read_proc = toss_read_proc;
	pde_toss->write_proc = toss_write_proc;
	pde_toss->owner = THIS_MODULE;

	pde_hb = create_proc_entry("hotboot", S_IRUGO | S_IWUSR,
				   get_ambarella_proc_dir());
	if (pde_hb == NULL)
		return -ENOMEM;

	pde_hb->read_proc = hotboot_read_proc;
	pde_hb->write_proc = hotboot_write_proc;
	pde_hb->owner = THIS_MODULE;
#endif

	nand_timing.control = toss->hwctx[toss->oldctx].fios.flash_ctr_reg;
	nand_timing.timing0 = toss->hwctx[toss->oldctx].fios.flash_tim0_reg;
	nand_timing.timing1 = toss->hwctx[toss->oldctx].fios.flash_tim1_reg;
	nand_timing.timing2 = toss->hwctx[toss->oldctx].fios.flash_tim2_reg;
	nand_timing.timing3 = toss->hwctx[toss->oldctx].fios.flash_tim3_reg;
	nand_timing.timing4 = toss->hwctx[toss->oldctx].fios.flash_tim4_reg;
	nand_timing.timing5 = toss->hwctx[toss->oldctx].fios.flash_tim5_reg;
	ambarella_init_nand_hotboot(&nand_timing);

ambarella_init_toss_exit:
	return rval;
}
