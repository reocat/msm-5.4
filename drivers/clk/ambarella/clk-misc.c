/*
 * arch/arm/mach-ambarella/clk.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
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
#include <linux/list.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <asm/uaccess.h>
#include <plat/iav_helper.h>
#include <plat/rct.h>

struct ambarella_service pll_service;

static const char *gclk_names[] = {
	"pll_out_core", "pll_out_sd", "gclk_cortex", "gclk_axi",
	"smp_twd", "gclk_ddr", "gclk_core", "gclk_ahb", "gclk_apb",
	"gclk_idsp", "gclk_so", "gclk_vo2", "gclk_vo", "gclk_uart",
	"gclk_audio", "gclk_sdxc", "gclk_sdio", "gclk_sd", "gclk_ir",
	"gclk_adc", "gclk_ssi", "gclk_ssi2", "gclk_ssi3", "gclk_pwm",
};

static int ambarella_clock_proc_show(struct seq_file *m, void *v)
{
	struct clk *gclk;
	int i;

	seq_printf(m, "\nClock Information:\n");
	for (i = 0; i < ARRAY_SIZE(gclk_names); i++) {
		gclk = clk_get_sys(NULL, gclk_names[i]);
		if (IS_ERR(gclk))
			continue;

		seq_printf(m, "\t%s:\t%lu Hz\n",
			__clk_get_name(gclk), clk_get_rate(gclk));
	}

	return 0;
}

static ssize_t ambarella_clock_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	struct clk *gclk;
	char *buf, clk_name[32];
	int freq, rval = count;

	pr_warn("!!!DANGEROUS!!! You must know what you are doning!\n");

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		rval = -EFAULT;
		goto exit;
	}

	sscanf(buf, "%s %d", clk_name, &freq);

	gclk = clk_get_sys(NULL, clk_name);
	if (IS_ERR(gclk)) {
		pr_err("Invalid clk name\n");
		rval = -EINVAL;
		goto exit;
	}

	clk_set_rate(gclk, freq);

exit:
	kfree(buf);
	return rval;
}

static int ambarella_clock_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ambarella_clock_proc_show, PDE_DATA(inode));
}

static const struct file_operations proc_clock_fops = {
	.open = ambarella_clock_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = ambarella_clock_proc_write,
};

static int ambarella_pll_service(void *arg, void *result)
{
	struct ambsvc_pll *pll_svc = arg;
	struct clk *clk;
	int rval = 0;

	BUG_ON(!pll_svc || !pll_svc->name);

	clk = clk_get(NULL, pll_svc->name);
	if (IS_ERR(clk)) {
		pr_err("%s: ERR get %s\n", __func__, pll_svc->name);
		return -EINVAL;
	}

	switch (pll_svc->svc_id) {
	case AMBSVC_PLL_GET_RATE:
		pll_svc->rate = clk_get_rate(clk);
		break;
	case AMBSVC_PLL_SET_RATE:
		clk_set_rate(clk, pll_svc->rate);
		break;

	default:
		pr_err("%s: Invalid pll service (%d)\n", __func__, pll_svc->svc_id);
		rval = -EINVAL;
		break;
	}

	return rval;
}

static int __init ambarella_init_clk(void)
{
#if 0
	writel(0x0, CLK_SI_INPUT_MODE_REG);
#if (CHIP_REV == S2E)
	amba_rct_setbitsl(HDMI_CLOCK_CTRL_REG, 0x1);
#endif

#if (CHIP_REV == S2E)
	writel(UART_CLK_SRC_IDSP, UART_CLK_SRC_SEL_REG);
#endif
#endif

	proc_create_data("clock", S_IRUGO, get_ambarella_proc_dir(),
		&proc_clock_fops, NULL);

	/* register ambarella clk service for private operation */
	pll_service.service = AMBARELLA_SERVICE_PLL;
	pll_service.func = ambarella_pll_service;
	ambarella_register_service(&pll_service);

	return 0;
}

late_initcall(ambarella_init_clk);

