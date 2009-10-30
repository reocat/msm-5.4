/*
 * drivers/input/misc/ambarella_input_ir.c
 *
 * Author: Qiao Wang <qwang@ambarella.com>
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

/* ========================================================================= */
static int ir_protocol = AMBA_IR_PROTOCOL_NEC;
MODULE_PARM_DESC(ir_protocol, "Ambarella IR Protocol ID");

static struct ambarella_ir_info	*local_info = NULL;

/* ========================================================================= */
static void ambarella_report_key(struct ambarella_ir_info *pinfo,
	unsigned int code, int value)
{
	input_report_key(pinfo->dev, code, value);
	AMBI_CAN_SYNC();
}

static int __devinit ambarella_setup_keymap(struct ambarella_ir_info *pinfo)
{
	int				i;
	int				vi_enabled = 0;
	int				errorCode;
	const struct firmware		*ext_key_map;

	if (keymap_name != NULL) {
		errorCode = request_firmware(&ext_key_map,
			keymap_name, pinfo->dev->dev.parent);
		if (errorCode) {
			ambi_err("Can't load firmware, use default\n");
			goto ambarella_setup_keymap_init;
		}
		ambi_notice("Load %s, size = %d\n",
			keymap_name, ext_key_map->size);
		memset(default_ambarella_key_table, AMBA_INPUT_END,
			sizeof(default_ambarella_key_table));
		memcpy(default_ambarella_key_table,
			ext_key_map->data, ext_key_map->size);
	}

	for (i = 0; i < ADC_MAX_INSTANCES; i++) {
		pinfo->adc_channel_used[i] = 0;
	}

ambarella_setup_keymap_init:
	pinfo->pkeymap = default_ambarella_key_table;

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		switch (pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) {
		case AMBA_INPUT_SOURCE_IR:
			break;

		case AMBA_INPUT_SOURCE_ADC:
			/* here, we record the adc channels that we will */
			if (pinfo->pkeymap[i].adc_key.chan < ADC_MAX_INSTANCES)
				pinfo->adc_channel_used[pinfo->pkeymap[i].adc_key.chan] = 1;
			break;

		case AMBA_INPUT_SOURCE_GPIO:
			ambarella_gpio_config(pinfo->pkeymap[i].gpio_key.id,
				GPIO_FUNC_SW_INPUT);
			errorCode = request_irq(
				GPIO_INT_VEC(pinfo->pkeymap[i].gpio_key.id),
				ambarella_gpio_irq,
				pinfo->pkeymap[i].gpio_key.irq_mode,
				"ambarella_input_gpio", pinfo);
			if (errorCode)
				ambi_err("Request GPIO%d IRQ failed!\n",
					pinfo->pkeymap[i].gpio_key.id);
			if (pinfo->pkeymap[i].gpio_key.can_wakeup) {
				errorCode = set_irq_wake(GPIO_INT_VEC(
					pinfo->pkeymap[i].gpio_key.id), 1);
				if (errorCode)
					ambi_err("set_irq_wake %d failed!\n",
						pinfo->pkeymap[i].gpio_key.id);
			}
			errorCode = 0;	//Continue with error...
			break;

		case AMBA_INPUT_SOURCE_VI:
			vi_enabled = 1;
			break;

		default:
			ambi_warn("Unknown AMBA_INPUT_SOURCE %d\n",
				(pinfo->pkeymap[i].type &
				AMBA_INPUT_SOURCE_MASK));
			break;
		}

		switch (pinfo->pkeymap[i].type & AMBA_INPUT_TYPE_MASK) {
		case AMBA_INPUT_TYPE_KEY:
			set_bit(EV_KEY, pinfo->dev->evbit);
			set_bit(pinfo->pkeymap[i].ir_key.key_code,
				pinfo->dev->keybit);
			break;

		case AMBA_INPUT_TYPE_REL:
			set_bit(EV_KEY, pinfo->dev->evbit);
			set_bit(EV_REL, pinfo->dev->evbit);
			set_bit(BTN_LEFT, pinfo->dev->keybit);
			set_bit(BTN_RIGHT, pinfo->dev->keybit);
			set_bit(REL_X, pinfo->dev->relbit);
			set_bit(REL_Y, pinfo->dev->relbit);
			break;

		case AMBA_INPUT_TYPE_ABS:
			set_bit(EV_KEY, pinfo->dev->evbit);
			set_bit(EV_ABS, pinfo->dev->evbit);
			set_bit(BTN_LEFT, pinfo->dev->keybit);
			set_bit(BTN_RIGHT, pinfo->dev->keybit);
			set_bit(BTN_TOUCH, pinfo->dev->keybit);
			set_bit(ABS_X, pinfo->dev->absbit);
			set_bit(ABS_Y, pinfo->dev->absbit);
			set_bit(ABS_PRESSURE, pinfo->dev->absbit);
			set_bit(ABS_TOOL_WIDTH, pinfo->dev->absbit);
			input_set_abs_params(pinfo->dev, ABS_X,
				0, abx_max_x, 0, 0);
			input_set_abs_params(pinfo->dev, ABS_Y,
				0, abx_max_y, 0, 0);
			input_set_abs_params(pinfo->dev, ABS_PRESSURE,
				0, abx_max_pressure, 0, 0);
			input_set_abs_params(pinfo->dev, ABS_TOOL_WIDTH,
				0, abx_max_width, 0, 0);
			break;

		default:
			ambi_warn("Unknown AMBA_INPUT_TYPE %d\n",
				(pinfo->pkeymap[i].type &
				AMBA_INPUT_TYPE_MASK));
			break;
		}
	}

	if (vi_enabled)
		for (i = 0; i < 0x100; i++)
			set_bit(i, pinfo->dev->keybit);

	return errorCode;
}

static void ambarella_free_keymap(struct ambarella_ir_info *pinfo)
{
	int				i;

	if (!pinfo->pkeymap)
		return;

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		switch (pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) {
		case AMBA_INPUT_SOURCE_IR:
			break;

		case AMBA_INPUT_SOURCE_ADC:
			break;

		case AMBA_INPUT_SOURCE_GPIO:
			if (pinfo->pkeymap[i].gpio_key.can_wakeup)
				set_irq_wake(GPIO_INT_VEC(
					pinfo->pkeymap[i].gpio_key.id), 0);
			free_irq(GPIO_INT_VEC(pinfo->pkeymap[i].gpio_key.id),
				pinfo);
			break;

		default:
			ambi_warn("Unknown AMBA_INPUT_SOURCE %d\n",
				(pinfo->pkeymap[i].type &
				AMBA_INPUT_SOURCE_MASK));
			break;
		}
	}
}

#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
static int ambarella_input_report_ir(struct ambarella_ir_info *pinfo, u32 uid)
{
	int				i;

	if (!pinfo->pkeymap)
		return -1;

	if ((pinfo->last_ir_uid == uid) && (pinfo->last_ir_flag)) {
		pinfo->last_ir_flag--;
		return 0;
	}

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		if ((pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) !=
			AMBA_INPUT_SOURCE_IR)
			continue;

		if ((pinfo->pkeymap[i].type == AMBA_INPUT_IR_KEY) &&
			(pinfo->pkeymap[i].ir_key.raw_id == uid)) {
			ambarella_report_key(pinfo,
				pinfo->pkeymap[i].ir_key.key_code, 1);
			ambarella_report_key(pinfo,
				pinfo->pkeymap[i].ir_key.key_code, 0);
			ambi_dbg("IR_KEY [%d]\n",
				pinfo->pkeymap[i].ir_key.key_code);
			pinfo->last_ir_flag = pinfo->pkeymap[i].ir_key.key_flag;
			break;
		}

		if ((pinfo->pkeymap[i].type == AMBA_INPUT_IR_REL) &&
			(pinfo->pkeymap[i].ir_rel.raw_id == uid)) {
			if (pinfo->pkeymap[i].ir_rel.key_code == REL_X) {
				input_report_rel(pinfo->dev,
					REL_X,
					pinfo->pkeymap[i].ir_rel.rel_step);
				input_report_rel(pinfo->dev,
					REL_Y, 0);
				AMBI_MUST_SYNC();
				ambi_dbg("IR_REL X[%d]:Y[%d]\n",
					pinfo->pkeymap[i].ir_rel.rel_step, 0);
			} else
			if (pinfo->pkeymap[i].ir_rel.key_code == REL_Y) {
				input_report_rel(pinfo->dev,
					REL_X, 0);
				input_report_rel(pinfo->dev,
					REL_Y,
					pinfo->pkeymap[i].ir_rel.rel_step);
				AMBI_MUST_SYNC();
				ambi_dbg("IR_REL X[%d]:Y[%d]\n", 0,
					pinfo->pkeymap[i].ir_rel.rel_step);
			}
			pinfo->last_ir_flag = 0;
			break;
		}

		if ((pinfo->pkeymap[i].type == AMBA_INPUT_IR_ABS) &&
			(pinfo->pkeymap[i].ir_abs.raw_id == uid)) {
			input_report_abs(pinfo->dev,
				ABS_X, pinfo->pkeymap[i].ir_abs.abs_x);
			input_report_abs(pinfo->dev,
				ABS_Y, pinfo->pkeymap[i].ir_abs.abs_y);
			AMBI_MUST_SYNC();
			ambi_dbg("IR_ABS X[%d]:Y[%d]\n",
				pinfo->pkeymap[i].ir_abs.abs_x,
				pinfo->pkeymap[i].ir_abs.abs_y);
			pinfo->last_ir_flag = 0;
			break;
		}
	}

	return 0;
}
#endif
static inline int ambarella_ir_get_tick_size(struct ambarella_ir_info *pinfo)
{
	int				size = 0;

	if (pinfo->ir_pread > pinfo->ir_pwrite)
		size = MAX_IR_BUFFER - pinfo->ir_pread + pinfo->ir_pwrite;
	else
		size = pinfo->ir_pwrite - pinfo->ir_pread;

	return size;
}

int ambarella_ir_inc_read_ptr(struct ambarella_ir_info *pinfo)
{
	if (pinfo->ir_pread == pinfo->ir_pwrite)
		return -1;

	pinfo->ir_pread++;
	if (pinfo->ir_pread >= MAX_IR_BUFFER)
		pinfo->ir_pread = 0;

	return 0;
}

int ambarella_ir_move_read_ptr(struct ambarella_ir_info *pinfo, int offset)
{
	int				rval = 0;

	for (; offset > 0; offset--) {
		rval = ambarella_ir_inc_read_ptr(pinfo);
		if (rval < 0)
			return rval;
	}
	return rval;
}

static inline void ambarella_ir_write_data(struct ambarella_ir_info *pinfo,
	u16 val)
{
	BUG_ON(pinfo->ir_pwrite < 0);
	BUG_ON(pinfo->ir_pwrite >= MAX_IR_BUFFER);

	pinfo->tick_buf[pinfo->ir_pwrite] = val;

	pinfo->ir_pwrite++;

	if (pinfo->ir_pwrite >= MAX_IR_BUFFER)
		pinfo->ir_pwrite = 0;
}

u16 ambarella_ir_read_data(struct ambarella_ir_info *pinfo, int pointer)
{
	BUG_ON(pointer < 0);
	BUG_ON(pointer >= MAX_IR_BUFFER);

	if (pointer == pinfo->ir_pwrite)
		return 0;

	return pinfo->tick_buf[pointer];
}

static inline int ambarella_ir_update_buffer(struct ambarella_ir_info *pinfo)
{
	int				count;
	int				size;

	count = amba_readl(pinfo->regbase + IR_STATUS_OFFSET);
	for (; count > 0; count--) {
		ambarella_ir_write_data(pinfo,
			amba_readl(pinfo->regbase + IR_DATA_OFFSET));
	}
	size = ambarella_ir_get_tick_size(pinfo);

	return size;
}
#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
static irqreturn_t ambarella_ir_irq(int irq, void *devid)
{
	struct ambarella_ir_info	*pinfo;
	int				size;
	int				cur_ptr;
	int				temp_ptr;
	int				rval;
	u32				uid;

	pinfo = (struct ambarella_ir_info *)devid;

	BUG_ON(pinfo->ir_pread < 0);
	BUG_ON(pinfo->ir_pread >= MAX_IR_BUFFER);

	if (amba_readl(pinfo->regbase + IR_CONTROL_OFFSET) &
		IR_CONTROL_FIFO_OV) {
		while (amba_readl(pinfo->regbase + IR_STATUS_OFFSET) > 0) {
			amba_readl(pinfo->regbase + IR_DATA_OFFSET);
		}

		goto ambarella_ir_irq_exit;
	}

	cur_ptr = pinfo->ir_pread;

	size = ambarella_ir_update_buffer(pinfo);
	for (; size > 0; size--) {
		temp_ptr = pinfo->ir_pread;

		rval = pinfo->ir_parse(pinfo, &uid);
		if (rval < 0) {
			pinfo->ir_pread = temp_ptr;
			rval = ambarella_ir_inc_read_ptr(pinfo);
			if (rval < 0)
				goto ambarella_ir_irq_exit;

			continue;
		}

		if (print_keycode)
			printk("uid = 0x%08x\n", uid);
		ambarella_input_report_ir(pinfo, uid);

		temp_ptr = pinfo->ir_pread;
		cur_ptr  = pinfo->ir_pread;

		size = ambarella_ir_update_buffer(pinfo);
	}

	pinfo->ir_pread = cur_ptr;

ambarella_ir_irq_exit:

	amba_writel(pinfo->regbase + IR_CONTROL_OFFSET,
		(amba_readl(pinfo->regbase + IR_CONTROL_OFFSET) |
		IR_CONTROL_LEVINT));

	return IRQ_HANDLED;
}

void ambarella_ir_enable(struct ambarella_ir_info *pinfo)
{
	amba_writel(pinfo->regbase + IR_CONTROL_OFFSET, IR_CONTROL_RESET);
	amba_setbitsl(pinfo->regbase + IR_CONTROL_OFFSET,
		IR_CONTROL_ENB | IR_CONTROL_INTLEV(24) | IR_CONTROL_INTENB);

	enable_irq(pinfo->irq);
}

void ambarella_ir_disable(struct ambarella_ir_info *pinfo)
{
	disable_irq(pinfo->irq);
}

void ambarella_ir_set_protocol(struct ambarella_ir_info *pinfo,
	enum ambarella_ir_protocol protocol_id)
{
	memset(pinfo->tick_buf, 0x0, sizeof(pinfo->tick_buf));
	pinfo->ir_pread  = 0;
	pinfo->ir_pwrite = 0;

	switch (protocol_id) {
	case AMBA_IR_PROTOCOL_NEC:
		ambi_notice("Protocol NEC[%d]\n", protocol_id);
		pinfo->ir_parse = ambarella_ir_nec_parse;
		break;
	case AMBA_IR_PROTOCOL_PANASONIC:
		ambi_notice("Protocol PANASONIC[%d]\n", protocol_id);
		pinfo->ir_parse = ambarella_ir_panasonic_parse;
		break;
	case AMBA_IR_PROTOCOL_SONY:
		ambi_notice("Protocol SONY[%d]\n", protocol_id);
		pinfo->ir_parse = ambarella_ir_sony_parse;
		break;
	case AMBA_IR_PROTOCOL_PHILIPS:
		ambi_notice("Protocol PHILIPS[%d]\n", protocol_id);
		pinfo->ir_parse = ambarella_ir_philips_parse;
		break;
	default:
		ambi_notice("Protocol default NEC[%d]\n", protocol_id);
		pinfo->ir_parse = ambarella_ir_nec_parse;
		break;
	}
}
#endif
void ambarella_ir_init(struct ambarella_ir_info *pinfo)
{
#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
	ambarella_ir_disable(pinfo);

	pinfo->pcontroller_info->set_pll();

#endif
	ambarella_gpio_config(pinfo->gpio_id, GPIO_FUNC_HW);

#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
	ambarella_ir_set_protocol(pinfo, ir_protocol);

	ambarella_ir_enable(pinfo);
#endif
}

static int __devinit ambarella_ir_probe(struct platform_device *pdev)
{
	int				errorCode;
	struct ambarella_ir_info	*pinfo;
	struct resource 		*irq;
	struct resource 		*mem;
	struct resource 		*io;
	struct resource 		*ioarea;
	struct input_dev		*input_dev;
	struct proc_dir_entry		*input_file;

	pinfo = kmalloc(sizeof(struct ambarella_ir_info), GFP_KERNEL);
	if (!pinfo) {
		dev_err(&pdev->dev, "Failed to allocate pinfo!\n");
		errorCode = -ENOMEM;
		goto ir_errorCode_na;
	}
	pinfo->pcontroller_info =
		(struct ambarella_ir_controller *)pdev->dev.platform_data;
	if ((pinfo->pcontroller_info == NULL) ||
		(pinfo->pcontroller_info->set_pll == NULL) ||
		(pinfo->pcontroller_info->get_pll == NULL) ) {
		dev_err(&pdev->dev, "Platform data is NULL!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_pinfo;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&pdev->dev, "Failed to allocate input_dev!\n");
		errorCode = -ENOMEM;
		goto ir_errorCode_pinfo;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(&pdev->dev, "Get mem resource failed!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_free_input_dev;
	}

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq == NULL) {
		dev_err(&pdev->dev, "Get irq resource failed!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_free_input_dev;
	}

	io = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (io == NULL) {
		dev_err(&pdev->dev, "Get GPIO resource failed!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_free_input_dev;
	}

	ioarea = request_mem_region(mem->start,
			(mem->end - mem->start) + 1, pdev->name);
	if (ioarea == NULL) {
		dev_err(&pdev->dev, "Request ioarea failed!\n");
		errorCode = -EBUSY;
		goto ir_errorCode_free_input_dev;
	}

	snprintf(pinfo->name, CONFIG_AMBARELLA_INPUT_STR_SIZE,
		"%s", pdev->name);
	input_dev->name = pinfo->name;
	snprintf(pinfo->phys, CONFIG_AMBARELLA_INPUT_STR_SIZE,
		"ambarella/input%d", pdev->id);
	input_dev->phys = pinfo->phys;
	snprintf(pinfo->uniq, CONFIG_AMBARELLA_INPUT_STR_SIZE,
		"ambarella input input%d", pdev->id);
	input_dev->uniq = pinfo->uniq;

	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = MACH_TYPE_AMBARELLA;
	input_dev->id.product = AMBARELLA_BOARD_CHIP(system_rev);
	input_dev->id.version = AMBARELLA_BOARD_REV(system_rev);
	input_dev->dev.parent = &pdev->dev;

	pinfo->regbase = (unsigned char __iomem *)mem->start;
	pinfo->id = pdev->id;
	pinfo->dev = input_dev;
	pinfo->mem = mem;
	pinfo->irq = irq->start;
	pinfo->gpio_id = io->start;
	pinfo->last_ir_uid = 0;
	pinfo->last_ir_flag = 0;

	local_info = pinfo;
	platform_set_drvdata(pdev, pinfo);

	errorCode = ambarella_setup_keymap(pinfo);
	if (errorCode) {
		dev_err(&pdev->dev, "Setup key map failed!\n");
		goto ir_errorCode_free_platform;
	}

#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
	errorCode = request_irq(pinfo->irq,
		ambarella_ir_irq, IRQF_TRIGGER_HIGH,
		pdev->name, pinfo);
	if (errorCode) {
		dev_err(&pdev->dev, "Request IRQ failed!\n");
		goto ir_errorCode_free_keymap;
	}
#endif
	ambarella_ir_init(pinfo);

	errorCode = input_register_device(input_dev);
	if (errorCode) {
		dev_err(&pdev->dev, "Register input_dev failed!\n");
		goto ir_errorCode_free_irq;
	}

	input_file = create_proc_entry(dev_name(&pdev->dev),
		S_IRUGO | S_IWUSR, get_ambarella_proc_dir());
	if (input_file == NULL) {
		dev_err(&pdev->dev, "Register %s failed!\n",
			dev_name(&pdev->dev));
		errorCode = -ENOMEM;
		goto ir_errorCode_unregister_device;
	} else {
		input_file->write_proc = ambarella_vi_proc_write;
		input_file->owner = THIS_MODULE;
		input_file->data = pinfo;
	}

	dev_notice(&pdev->dev,
		"Ambarella Media Processor IR Host Controller %d probed!\n",
		pdev->id);

	goto ir_errorCode_na;

ir_errorCode_unregister_device:
	input_unregister_device(pinfo->dev);

ir_errorCode_free_irq:
#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
	free_irq(pinfo->irq, pinfo);
ir_errorCode_free_keymap:
#endif
	ambarella_free_keymap(pinfo);

ir_errorCode_free_platform:
	platform_set_drvdata(pdev, NULL);

ir_errorCode_free_input_dev:
	input_free_device(input_dev);

ir_errorCode_pinfo:
	kfree(pinfo);

ir_errorCode_na:
	return errorCode;
}

static int __devexit ambarella_ir_remove(struct platform_device *pdev)
{
	struct ambarella_ir_info	*pinfo;
	int				errorCode = 0;

	pinfo = platform_get_drvdata(pdev);
	local_info = NULL;

	if (pinfo) {
		remove_proc_entry(dev_name(&pdev->dev),
			get_ambarella_proc_dir());
		input_unregister_device(pinfo->dev);
#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
		free_irq(pinfo->irq, pinfo);
#endif
		ambarella_free_keymap(pinfo);
		platform_set_drvdata(pdev, NULL);
		input_free_device(pinfo->dev);
		release_mem_region(pinfo->mem->start,
			(pinfo->mem->end - pinfo->mem->start) + 1);
		kfree(pinfo);
	}

	dev_notice(&pdev->dev,
		"Remove Ambarella Media Processor IR Host Controller.\n");

	return errorCode;
}

#if (defined CONFIG_PM) && ((defined CONFIG_INPUT_AMBARELLA_IR_MODULE)\
	|| (defined CONFIG_INPUT_AMBARELLA_IR))
static int ambarella_ir_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int					errorCode = 0;
	struct ambarella_ir_info		*pinfo;

	pinfo = platform_get_drvdata(pdev);

	if (!device_may_wakeup(&pdev->dev)) {
		amba_clrbitsl(pinfo->regbase + IR_CONTROL_OFFSET,
			IR_CONTROL_INTENB);
		disable_irq(pinfo->irq);
	}

	dev_info(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, errorCode, state.event);
	return errorCode;
}

static int ambarella_ir_resume(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct ambarella_ir_info		*pinfo;

	pinfo = platform_get_drvdata(pdev);

	if (!device_may_wakeup(&pdev->dev)) {
		amba_setbitsl(pinfo->regbase + IR_CONTROL_OFFSET,
			IR_CONTROL_INTENB);
		ambarella_ir_enable(pinfo);
	}

	dev_info(&pdev->dev, "%s exit with %d\n", __func__, errorCode);

	return errorCode;
}
#endif

static struct platform_driver ambarella_ir_driver = {
	.probe		= ambarella_ir_probe,
	.remove		= __devexit_p(ambarella_ir_remove),
#if (defined CONFIG_PM) && ((defined CONFIG_INPUT_AMBARELLA_IR_MODULE)\
	|| (defined CONFIG_INPUT_AMBARELLA_IR))
	.suspend	= ambarella_ir_suspend,
	.resume		= ambarella_ir_resume,
#endif
	.driver		= {
		.name	= "ambarella-ir",
		.owner	= THIS_MODULE,
	},
};

static int ambarella_input_set_ir_protocol(const char *val,
	struct kernel_param *kp)
{
	int				errorCode = 0;

	errorCode = param_set_int(val, kp);

	if (local_info)
		ambarella_ir_init(local_info);

	return errorCode;
}

module_param_call(ir_protocol, ambarella_input_set_ir_protocol,
	param_get_int, &ir_protocol, 0644);

