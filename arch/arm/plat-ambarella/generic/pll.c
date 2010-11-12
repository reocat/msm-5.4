/*
 * arch/arm/plat-ambarella/generic/pll.c
 *
 * Author: Cao Rongrong, <rrcao@ambarella.com>
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
#include <linux/bootmem.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>

#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mach/hardware.h>
#include <mach/init.h>
#include <hal/hal.h>

#define MAX_CMD_LENGTH				(32)

#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
static struct proc_dir_entry *mode_file = NULL;
static struct proc_dir_entry *performance_file = NULL;
#else
static struct proc_dir_entry *freq_file = NULL;
#endif

static inline unsigned long ambarella_cpufreq_scale(unsigned long old,
	u_int div, u_int mult)
{
#if BITS_PER_LONG == 32

	u64 result = ((u64) old) * ((u64) mult);
	do_div(result, div);
	return (unsigned long) result;

#elif BITS_PER_LONG == 64

	unsigned long result = old * ((u64) mult);
	result /= div;
	return result;

#endif
};

static unsigned long l_p_j_ref;
static unsigned int  l_p_j_ref_freq;

static void ambarella_adjust_jiffies(unsigned long val,
	unsigned int oldfreq, unsigned int newfreq)
{
	if (!l_p_j_ref_freq) {
		l_p_j_ref = loops_per_jiffy;
		l_p_j_ref_freq = oldfreq;
	}
	if ((val == AMBA_EVENT_PRE_CPUFREQ  && oldfreq < newfreq) ||
	    (val == AMBA_EVENT_POST_CPUFREQ && oldfreq > newfreq)) {
		loops_per_jiffy = ambarella_cpufreq_scale(l_p_j_ref,
			l_p_j_ref_freq, newfreq);
	}
}

#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)

struct ambarella_pll_info {
	amb_operating_mode_t operating_mode;
};

struct ambarella_pll_mode_info {
	char *name;
	unsigned int mode;
};

struct ambarella_pll_performance_info {
	char *name;
	unsigned int performance;
};

#if (CHIP_REV == A5S)
#define AMB_OPERATING_MODE_END		(AMB_OPERATING_MODE_IP_CAM + 1)
static struct ambarella_pll_mode_info mode_list[] = {
	{"preview", AMB_OPERATING_MODE_PREVIEW},
	{"still_capture", AMB_OPERATING_MODE_STILL_CAPTURE},
	{"capture", AMB_OPERATING_MODE_CAPTURE},
	{"playback", AMB_OPERATING_MODE_PLAYBACK},
	{"display_and_arm", AMB_OPERATING_MODE_DISPLAY_AND_ARM},
	{"standby", AMB_OPERATING_MODE_STANDBY},
	{"lcd_bypass", AMB_OPERATING_MODE_LCD_BYPASS},
	{"still_preview", AMB_OPERATING_MODE_STILL_PREVIEW},
	{"lowpower", AMB_OPERATING_MODE_LOW_POWER},
	{"raw", AMB_OPERATING_MODE_RAW},
	{"ipcam", AMB_OPERATING_MODE_IP_CAM},
};

static struct ambarella_pll_performance_info performance_list[] = {
	{"480P30", AMB_PERFORMANCE_480P30},
	{"720P30", AMB_PERFORMANCE_720P30},
	{"720P60", AMB_PERFORMANCE_720P60},
	{"1080I60_LowPower", AMB_PERFORMANCE_1080I60_LP},
	{"1080I60", AMB_PERFORMANCE_1080I60},
	{"1080P30", AMB_PERFORMANCE_1080P30},
	{"1080P60", AMB_PERFORMANCE_1080P60},
};
#elif (CHIP_REV == A7)
#define AMB_OPERATING_MODE_END		(AMB_OPERATING_MODE_LOW_POWER + 1)
static struct ambarella_pll_mode_info mode_list[] = {
	{"preview", AMB_OPERATING_MODE_PREVIEW},
	{"still_capture", AMB_OPERATING_MODE_STILL_CAPTURE},
	{"capture", AMB_OPERATING_MODE_CAPTURE},
	{"playback", AMB_OPERATING_MODE_PLAYBACK},
	{"display_and_arm", AMB_OPERATING_MODE_DISPLAY_AND_ARM},
	{"standby", AMB_OPERATING_MODE_STANDBY},
	{"lcd_bypass", AMB_OPERATING_MODE_LCD_BYPASS},
	{"still_preview", AMB_OPERATING_MODE_STILL_PREVIEW},
	{"lowpower", AMB_OPERATING_MODE_LOW_POWER},
};

static struct ambarella_pll_performance_info performance_list[] = {
	{"480P30", AMB_PERFORMANCE_480P30},
	{"720P30", AMB_PERFORMANCE_720P30},
	{"720P60", AMB_PERFORMANCE_720P60},
	{"1080I60", AMB_PERFORMANCE_1080I60},
	{"1080P30", AMB_PERFORMANCE_1080P30},
	{"1080P60", AMB_PERFORMANCE_1080P60},
};
#elif (CHIP_REV == I1)
#define AMB_OPERATING_MODE_END		(AMB_OPERATING_MODE_AUDIO_CAPTURE + 1)
static struct ambarella_pll_mode_info mode_list[] = {
	{"preview", AMB_OPERATING_MODE_PREVIEW},
	{"still_capture", AMB_OPERATING_MODE_STILL_CAPTURE},
	{"capture", AMB_OPERATING_MODE_CAPTURE},
	{"playback", AMB_OPERATING_MODE_PLAYBACK},
	{"display_and_arm", AMB_OPERATING_MODE_DISPLAY_AND_ARM},
	{"standby", AMB_OPERATING_MODE_STANDBY},
	{"lcd_bypass", AMB_OPERATING_MODE_LCD_BYPASS},
	{"still_preview", AMB_OPERATING_MODE_STILL_PREVIEW},
	{"lowpower", AMB_OPERATING_MODE_LOW_POWER},
	{"auido_playback", AMB_OPERATING_MODE_AUDIO_PLAYBACK},
	{"auido_capture", AMB_OPERATING_MODE_AUDIO_CAPTURE},
};

static struct ambarella_pll_performance_info performance_list[] = {
	{"480P30", AMB_PERFORMANCE_480P30},
	{"720P30", AMB_PERFORMANCE_720P30},
	{"720P60", AMB_PERFORMANCE_720P60},
	{"1080I60", AMB_PERFORMANCE_1080I60},
	{"1080P30", AMB_PERFORMANCE_1080P30},
	{"1080P60", AMB_PERFORMANCE_1080P60},
};
#endif

static struct ambarella_pll_info pll_info;

static int ambarella_pll_proc_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	amb_hal_success_t			result;
	amb_operating_mode_t			operating_mode;
	int					retlen = 0;
	int					i;

	result = amb_get_operating_mode(HAL_BASE_VP, &operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		retlen = -EPERM;
		goto ambarella_pll_proc_read_exit;
	}

	if (off != 0) {
		retlen = 0;
		goto ambarella_pll_proc_read_exit;
	}

	retlen += scnprintf((page + retlen), (count - retlen),
			"\nPossible Mode:\n");
	for (i = 0; i < ARRAY_SIZE(mode_list); i++) {
		retlen += scnprintf((page + retlen), (count - retlen),
				"\t%s\n", mode_list[i].name);
	}

	retlen += scnprintf((page + retlen), (count - retlen),
			"\nPossible Performance:\n");
	for (i = 0; i < ARRAY_SIZE(performance_list); i++) {
		retlen += scnprintf((page + retlen), (count - retlen),
				"\t%s\n", performance_list[i].name);
	}

	retlen += scnprintf((page + retlen), (count - retlen),
			"\nPLL Information:\n"
			"\tPerformance:\t%s\n"
			"\tMode:\t\t%s\n"
			"\tUSB:\t\t%s\n"
			"\tHDMI:\t\t%s\n"
			"\tDualStream:\t%s\n"
			"\tARM:\t\t%d Hz\n"
			"\tDram:\t\t%d Hz\n"
			"\tiDSP:\t\t%d Hz\n"
			"\tCore:\t\t%d Hz\n"
#if (CHIP_REV == I1)
			"\tCortex:\t\t%d Hz\n"
			"\tAXI:\t\t%d Hz\n"
			"\tDDD:\t\t%d Hz\n"
#endif
			"\tAHB:\t\t%d Hz\n"
			"\tAPB:\t\t%d Hz\n"
			"\tVOUT:\t\t%d Hz\n"
			"\tVOUT2:\t\t%d Hz\n"
			"\tVIN:\t\t%d Hz\n\n",
			performance_list[operating_mode.performance].name,
			mode_list[operating_mode.mode].name,
			operating_mode.usb_state ? "On" : "Off",
			operating_mode.hdmi_state ? "On" : "Off",
			operating_mode.dual_stream_state ? "On" : "Off",
			get_arm_bus_freq_hz(),
			get_dram_freq_hz(),
			get_idsp_freq_hz(),
			get_core_bus_freq_hz(),
#if (CHIP_REV == I1)
			amb_get_cortex_clock_frequency(HAL_BASE_VP),
			amb_get_axi_clock_frequency(HAL_BASE_VP),
			amb_get_3d_clock_frequency(HAL_BASE_VP),
#endif
			get_ahb_bus_freq_hz(),
			get_apb_bus_freq_hz(),
			get_vout_freq_hz(),
			get_vout2_freq_hz(),
			get_so_freq_hz());

	*eof = 1;

ambarella_pll_proc_read_exit:
	return retlen;
}

int ambarella_set_operating_mode(amb_operating_mode_t *popmode)
{
	int					errorCode = 0;
	amb_hal_success_t			result = AMB_HAL_SUCCESS;
	unsigned int				oldfreq, newfreq;
	unsigned long				flags;

	errorCode = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_PRE_CPUFREQ, NULL));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_PRE_CPUFREQ failed(%d)\n",
			__func__, errorCode);
	}

	oldfreq = get_arm_bus_freq_hz();

	local_irq_save(flags);

	errorCode = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_PRE_CPUFREQ, NULL));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_PRE_CPUFREQ failed(%d)\n",
			__func__, errorCode);
	}

	ambarella_timer_suspend(1);
	result = amb_set_operating_mode(HAL_BASE_VP, popmode);
	ambarella_timer_resume(1);

	errorCode = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_POST_CPUFREQ, NULL));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_POST_CPUFREQ failed(%d)\n",
			__func__, errorCode);
	}

	if (result != AMB_HAL_SUCCESS) {
		pr_err("%s: amb_set_operating_mode failed(%d)\n",
			__func__, result);
		errorCode = -EPERM;
	}

	local_irq_restore(flags);

	newfreq = get_arm_bus_freq_hz();
	ambarella_adjust_jiffies(AMBA_EVENT_POST_CPUFREQ, oldfreq, newfreq);

	errorCode = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_POST_CPUFREQ, NULL));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_POST_CPUFREQ failed(%d)\n",
			__func__, errorCode);
	}

	result = amb_get_operating_mode(HAL_BASE_VP, popmode);
	if (result != AMB_HAL_SUCCESS) {
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		errorCode = -EPERM;
	}

	return errorCode;
}

static int ambarella_mode_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	int					errorCode = 0;
	char					str[MAX_CMD_LENGTH];
	int					mode, i;
	amb_hal_success_t			result;

	i = (count < MAX_CMD_LENGTH) ? count : MAX_CMD_LENGTH;
	if (copy_from_user(str, buffer, i)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto pll_mode_proc_write_exit;
	}
	str[MAX_CMD_LENGTH - 1] = 0;

	mode = AMB_OPERATING_MODE_END;
	for (i = 0; i < ARRAY_SIZE(mode_list); i++) {
		if (strstr(str, mode_list[i].name) != NULL) {
			mode = mode_list[i].mode;
		}
	}

	if (mode >= AMB_OPERATING_MODE_END) {
		pr_err("%s: invalid mode (%s)!\n", __func__, str);
		errorCode = -EINVAL;
		goto pll_mode_proc_write_exit;
	}

	result = amb_get_operating_mode(HAL_BASE_VP, &pll_info.operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		errorCode = -EPERM;
		goto pll_mode_proc_write_exit;
	}

	pll_info.operating_mode.mode = mode;
	errorCode = ambarella_set_operating_mode(&pll_info.operating_mode);
	if (!errorCode)
		errorCode = count;

pll_mode_proc_write_exit:
	return errorCode;
}

static int ambarella_performance_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	int					errorCode = 0;
	char					str[MAX_CMD_LENGTH];
	int					performance, i;
	amb_hal_success_t			result;

	i = (count < MAX_CMD_LENGTH) ? count : MAX_CMD_LENGTH;
	if (copy_from_user(str, buffer, i)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto pll_performance_proc_write_exit;
	}
	str[MAX_CMD_LENGTH - 1] = 0;

	performance = AMB_PERFORMANCE_2160P60 + 1;
	for (i = 0; i < ARRAY_SIZE(performance_list); i++) {
		if (strstr(str, performance_list[i].name) != NULL) {
			performance = performance_list[i].performance;
		}
	}

	if (performance > AMB_PERFORMANCE_2160P60){
		pr_err("%s: invalid performance (%s)!\n", __func__, str);
		errorCode = -EINVAL;
		goto pll_performance_proc_write_exit;
	}

	result = amb_get_operating_mode(HAL_BASE_VP, &pll_info.operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		errorCode = -EPERM;
		goto pll_performance_proc_write_exit;
	}

	pll_info.operating_mode.performance = performance;
	errorCode = ambarella_set_operating_mode(&pll_info.operating_mode);
	if (!errorCode)
		errorCode = count;

pll_performance_proc_write_exit:
	return errorCode;
}

static int ambarella_init_pll_a5s(void)
{
	amb_hal_success_t			result;
	int					errorCode = 0;

	/* initial pll_info */
	result = amb_get_operating_mode(HAL_BASE_VP, &pll_info.operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: get operating mode failed(%d)\n",__func__, result);
		errorCode = -EPERM;
		goto pll_a5s_exit;
	}

	mode_file = create_proc_entry("mode", S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
	if (mode_file == NULL) {
		errorCode = -ENOMEM;
		pr_err("%s: create proc file (mode) fail!\n", __func__);
		goto pll_a5s_exit;
	} else {
		mode_file->read_proc = ambarella_pll_proc_read;
		mode_file->write_proc = ambarella_mode_proc_write;
	}

	performance_file = create_proc_entry("performance", S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
	if (performance_file == NULL) {
		errorCode = -ENOMEM;
		pr_err("%s: create proc file (performance) fail!\n", __func__);
		goto pll_a5s_exit;
	} else {
		performance_file->read_proc = ambarella_pll_proc_read;
		performance_file->write_proc = ambarella_performance_proc_write;
	}

pll_a5s_exit:
	return errorCode;
}

#else

struct ambarella_pll_info {
	unsigned int armfreq;
};

static struct ambarella_pll_info pll_info;

static int ambarella_freq_proc_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	int					retlen = 0;

	if (off != 0)
		return 0;

	retlen = scnprintf(page, count,
			"\nPLL Information:\n"
			"\tCore:\t%d Hz\n"
			"\tDram:\t%d Hz\n"
			"\tiDSP:\t%d Hz\n"
			"\tAHB:\t%d Hz\n"
			"\tAPB:\t%d Hz\n\n",
			get_core_bus_freq_hz(),
			get_dram_freq_hz(),
			get_idsp_freq_hz(),
			get_ahb_bus_freq_hz(),
			get_apb_bus_freq_hz());

	*eof = 1;

	return retlen;
}

static void ambarella_freq_set_pll(unsigned int new_freq_cpu)
{
	unsigned int				cur_freq_cpu;

#if ((CHIP_REV == A2) || (CHIP_REV == A3))
	do {
		cur_freq_cpu = amba_readl(PLL_CORE_CTRL_REG) & 0xfff00000;
		if (new_freq_cpu > pll_info.armfreq) {
			cur_freq_cpu += 0x01000000;
		} else {
			cur_freq_cpu -= 0x01000000;
		}
		cur_freq_cpu |= (amba_readl(PLL_CORE_CTRL_REG) & 0x000fffff);
		amba_writel(PLL_CORE_CTRL_REG, cur_freq_cpu);
		mdelay(20);
		cur_freq_cpu = get_core_bus_freq_hz();
	} while ((new_freq_cpu > pll_info.armfreq && cur_freq_cpu < new_freq_cpu) ||
		 (new_freq_cpu < pll_info.armfreq && cur_freq_cpu > new_freq_cpu));
#elif ((CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) || \
	(CHIP_REV == A5) || (CHIP_REV == A6))
	do {
		u32 reg, intprog, sdiv, sout, valwe;

		reg = amba_readl(PLL_CORE_CTRL_REG);
		intprog = PLL_CTRL_INTPROG(reg);
		sout = PLL_CTRL_SOUT(reg);
		sdiv = PLL_CTRL_SDIV(reg);
		valwe = PLL_CTRL_VALWE(reg);
		valwe &= 0xffe;
		if (new_freq_cpu > pll_info.armfreq)
			intprog++;
		else
			intprog--;

		reg = PLL_CTRL_VAL(intprog, sout, sdiv, valwe);
		amba_writel(PLL_CORE_CTRL_REG, reg);

		/* PLL write enable */
		reg |= 0x1;
		amba_writel(PLL_CORE_CTRL_REG, reg);

		/* FIXME: wait a while */
		mdelay(20);
		cur_freq_cpu = get_core_bus_freq_hz();
	} while ((new_freq_cpu > pll_info.armfreq && cur_freq_cpu < new_freq_cpu) ||
		(new_freq_cpu < pll_info.armfreq && cur_freq_cpu > new_freq_cpu));
#endif
}

static int ambarella_freq_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	char					str[MAX_CMD_LENGTH];
	int					errorCode = 0;
	unsigned int				i;
	unsigned long				flags;

	i = (count < MAX_CMD_LENGTH) ? count : MAX_CMD_LENGTH;
	if (copy_from_user(str, buffer, i)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto ambarella_pll_proc_write_exit;
	}
	str[MAX_CMD_LENGTH - 1] = 0;

	errorCode = sscanf(str, "%d", &i);
	if (errorCode != 1) {
		pr_err("%s: convert sting fail %d!\n", __func__, errorCode);
		errorCode = -EINVAL;
		goto ambarella_pll_proc_write_exit;
	}

	if (i > 243000000 || i < 135000000) {
		pr_err("%s:\n\tinvalid frequency (%d)\n",
			__func__, i);
		pr_info("\tfrequency should be 135000 ~ 243000 (in KHz)\n");
		errorCode = -EINVAL;
		goto ambarella_pll_proc_write_exit;
	}

	pr_debug("%s: %ld %d\n", __func__, count, i);
	errorCode = count;

	if(i == pll_info.armfreq)
		goto ambarella_pll_proc_write_exit;

	errorCode = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_PRE_CPUFREQ, NULL));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_PRE_CPUFREQ failed(%d)\n",
			__func__, errorCode);
	}

	ambarella_adjust_jiffies(AMBA_EVENT_PRE_CPUFREQ,
		pll_info.armfreq, i);

	local_irq_save(flags);

	errorCode = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_PRE_CPUFREQ, NULL));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_PRE_CPUFREQ failed(%d)\n",
			__func__, errorCode);
	}

	ambarella_timer_suspend(1);
	ambarella_freq_set_pll(i);
	ambarella_timer_resume(1);

	errorCode = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_POST_CPUFREQ, NULL));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_POST_CPUFREQ failed(%d)\n",
			__func__, errorCode);
	}

	local_irq_restore(flags);

	ambarella_adjust_jiffies(AMBA_EVENT_POST_CPUFREQ,
		pll_info.armfreq, i);

	errorCode = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_POST_CPUFREQ, NULL));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_POST_CPUFREQ failed(%d)\n",
			__func__, errorCode);
	}

	pll_info.armfreq = get_core_bus_freq_hz();

ambarella_pll_proc_write_exit:
	return errorCode;
}

static int ambarella_init_pll_general(void)
{
	int					errorCode = 0;

	/* initial pll_info */
	pll_info.armfreq = get_core_bus_freq_hz();

	freq_file = create_proc_entry("corepll", S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
	if (freq_file == NULL) {
		errorCode = -ENOMEM;
		pr_err("%s: create proc file (freq) fail!\n", __func__);
		goto pll_general_exit;
	} else {
		freq_file->read_proc = ambarella_freq_proc_read;
		freq_file->write_proc = ambarella_freq_proc_write;
	}

pll_general_exit:
	return errorCode;
}

#endif	/* End for #if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL) */

int __init ambarella_init_pll(void)
{
	int					errorCode = 0;

#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
	errorCode = ambarella_init_pll_a5s();
#else
	errorCode = ambarella_init_pll_general();
#endif

	return errorCode;
}

/* ==========================================================================*/
u32 ambarella_pll_suspend(u32 level)
{
	return 0;
}

u32 ambarella_pll_resume(u32 level)
{

#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
	amb_set_operating_mode(HAL_BASE_VP, &pll_info.operating_mode);
#else
	ambarella_freq_set_pll(pll_info.armfreq);
#endif
	return 0;
}

