/*
 * arch/arm/plat-ambarella/generic/pll_hal.c
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
#include <linux/cpu.h>

#include <asm/uaccess.h>
#include <asm/localtimer.h>

#include <mach/hardware.h>

#include <plat/pll.h>
#include <plat/timer.h>

#include <hal/hal.h>

/* ==========================================================================*/
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

/* ==========================================================================*/
static struct proc_dir_entry *mode_file = NULL;
static struct proc_dir_entry *performance_file = NULL;
static struct ambarella_pll_info pll_info;

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
#define AMB_OPERATING_PERFORMANCE_END		(AMB_PERFORMANCE_2160P60)
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
	{"ipcam", AMB_OPERATING_MODE_IP_CAM},
};
#define AMB_OPERATING_PERFORMANCE_END		(AMB_PERFORMANCE_2160P60)
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
	{"lowpower_cortex", AMB_OPERATING_MODE_LOW_POWER_WITH_CORTEX},
	{"auido_playback", AMB_OPERATING_MODE_AUDIO_PLAYBACK},
	{"auido_capture", AMB_OPERATING_MODE_AUDIO_CAPTURE},
};
#define AMB_OPERATING_PERFORMANCE_END		(AMB_PERFORMANCE_2160P60)
static struct ambarella_pll_performance_info performance_list[] = {
	{"480P30", AMB_PERFORMANCE_480P30},
	{"720P30", AMB_PERFORMANCE_720P30},
	{"720P60", AMB_PERFORMANCE_720P60},
	{"1080I60", AMB_PERFORMANCE_1080I60},
	{"1080P30", AMB_PERFORMANCE_1080P30},
	{"1080P60", AMB_PERFORMANCE_1080P60},
};
#endif

/* ==========================================================================*/
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
	int					retval = 0;
	amb_hal_success_t			result = AMB_HAL_SUCCESS;
	unsigned int				oldfreq, newfreq;
	unsigned long				flags;

	retval = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_PRE_CPUFREQ, NULL));
	if (retval) {
		pr_err("%s: AMBA_EVENT_PRE_CPUFREQ failed(%d)\n",
			__func__, retval);
	}

	disable_nonboot_cpus();
	local_irq_save(flags);

	retval = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_PRE_CPUFREQ, NULL));
	if (retval) {
		pr_err("%s: AMBA_EVENT_PRE_CPUFREQ failed(%d)\n",
			__func__, retval);
	}

	oldfreq = get_arm_bus_freq_hz();

	ambarella_timer_suspend(1);
	result = amb_set_operating_mode(HAL_BASE_VP, popmode);
	if (result != AMB_HAL_SUCCESS) {
		pr_err("%s: amb_set_operating_mode failed(%d)\n",
			__func__, result);
		retval = -EPERM;
	}
	ambarella_timer_resume(1);

	newfreq = get_arm_bus_freq_hz();
	ambarella_adjust_jiffies(AMBA_EVENT_POST_CPUFREQ, oldfreq, newfreq);

#if defined(CONFIG_SMP)
	percpu_timer_update_rate(amb_get_axi_clock_frequency(HAL_BASE_VP));
#endif

	retval = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_POST_CPUFREQ, NULL));
	if (retval) {
		pr_err("%s: AMBA_EVENT_POST_CPUFREQ failed(%d)\n",
			__func__, retval);
	}

	local_irq_restore(flags);
	enable_nonboot_cpus();

	retval = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_POST_CPUFREQ, NULL));
	if (retval) {
		pr_err("%s: AMBA_EVENT_POST_CPUFREQ failed(%d)\n",
			__func__, retval);
	}

	result = amb_get_operating_mode(HAL_BASE_VP, popmode);
	if (result != AMB_HAL_SUCCESS) {
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		retval = -EPERM;
	}

	return retval;
}

static int ambarella_mode_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	int					retval = 0;
	char					str[AMBPLL_MAX_CMD_LENGTH];
	int					mode, i;
	amb_hal_success_t			result;

	i = (count < AMBPLL_MAX_CMD_LENGTH) ? count : AMBPLL_MAX_CMD_LENGTH;
	if (copy_from_user(str, buffer, i)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		retval = -EFAULT;
		goto ambarella_mode_proc_write_exit;
	}
	str[i - 1] = 0;

	mode = AMB_OPERATING_MODE_END;
	for (i = 0; i < ARRAY_SIZE(mode_list); i++) {
		if (strlen(str) == strlen(mode_list[i].name)
			&& strcmp(str, mode_list[i].name) == 0) {
			mode = mode_list[i].mode;
			break;
		}
	}
	if (mode >= AMB_OPERATING_MODE_END) {
		pr_err("%s: invalid mode (%s)!\n", __func__, str);
		retval = -EINVAL;
		goto ambarella_mode_proc_write_exit;
	}

	result = amb_get_operating_mode(HAL_BASE_VP, &pll_info.operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		retval = -EPERM;
		goto ambarella_mode_proc_write_exit;
	}

	if (pll_info.operating_mode.mode != mode) {
		pll_info.operating_mode.mode = mode;
		retval = ambarella_set_operating_mode(&pll_info.operating_mode);
	}
	if (!retval)
		retval = count;

ambarella_mode_proc_write_exit:
	return retval;
}

static int ambarella_performance_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	int					retval = 0;
	char					str[AMBPLL_MAX_CMD_LENGTH];
	int					performance, i;
	amb_hal_success_t			result;

	i = (count < AMBPLL_MAX_CMD_LENGTH) ? count : AMBPLL_MAX_CMD_LENGTH;
	if (copy_from_user(str, buffer, i)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		retval = -EFAULT;
		goto ambarella_performance_proc_write_exit;
	}
	str[i - 1] = 0;

	performance = AMB_OPERATING_PERFORMANCE_END;
	for (i = 0; i < ARRAY_SIZE(performance_list); i++) {
		if (strlen(str) == strlen(performance_list[i].name)
			&& strcmp(str, performance_list[i].name) == 0) {
			performance = performance_list[i].performance;
			break;
		}
	}
	if (performance >= AMB_OPERATING_PERFORMANCE_END){
		pr_err("\n%s: invalid performance (%s)!\n", __func__, str);
		retval = -EINVAL;
		goto ambarella_performance_proc_write_exit;
	}

	result = amb_get_operating_mode(HAL_BASE_VP, &pll_info.operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		retval = -EPERM;
		goto ambarella_performance_proc_write_exit;
	}

	if (pll_info.operating_mode.performance != performance) {
		pll_info.operating_mode.performance = performance;
		retval = ambarella_set_operating_mode(&pll_info.operating_mode);
	}
	if (!retval)
		retval = count;

ambarella_performance_proc_write_exit:
	return retval;
}

static int __init ambarella_init_pll_hal(void)
{
	amb_hal_success_t			result;
	int					retval = 0;

	result = amb_get_operating_mode(HAL_BASE_VP, &pll_info.operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: get operating mode failed(%d)\n",__func__, result);
		retval = -EPERM;
		goto ambarella_init_pll_hal_exit;
	}

	mode_file = create_proc_entry("mode", S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
	if (mode_file == NULL) {
		retval = -ENOMEM;
		pr_err("%s: create proc file (mode) fail!\n", __func__);
		goto ambarella_init_pll_hal_exit;
	} else {
		mode_file->read_proc = ambarella_pll_proc_read;
		mode_file->write_proc = ambarella_mode_proc_write;
	}

	performance_file = create_proc_entry("performance", S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
	if (performance_file == NULL) {
		retval = -ENOMEM;
		pr_err("%s: create proc file (performance) fail!\n", __func__);
		goto ambarella_init_pll_hal_exit;
	} else {
		performance_file->read_proc = ambarella_pll_proc_read;
		performance_file->write_proc = ambarella_performance_proc_write;
	}

ambarella_init_pll_hal_exit:
	return retval;
}

int __init ambarella_init_pll(void)
{
	int					retval = 0;

	retval = ambarella_init_pll_hal();

	return retval;
}

/* ==========================================================================*/
u32 ambarella_pll_suspend(u32 level)
{
	return 0;
}

u32 ambarella_pll_resume(u32 level)
{
#if defined(CONFIG_PLAT_AMBARELLA_I1)
	return 0;
#else
	return amb_set_operating_mode(HAL_BASE_VP, &pll_info.operating_mode);
#endif
}

