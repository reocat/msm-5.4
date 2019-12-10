// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mhi.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include "mhi_internal.h"

const char * const mhi_ee_str[MHI_EE_MAX] = {
	[MHI_EE_PBL] = "PBL",
	[MHI_EE_SBL] = "SBL",
	[MHI_EE_AMSS] = "AMSS",
	[MHI_EE_BHIE] = "BHIE",
	[MHI_EE_RDDM] = "RDDM",
	[MHI_EE_PTHRU] = "PASS THRU",
	[MHI_EE_EDL] = "EDL",
	[MHI_EE_DISABLE_TRANSITION] = "DISABLE",
};

const char * const dev_state_tran_str[DEV_ST_TRANSITION_MAX] = {
	[DEV_ST_TRANSITION_PBL] = "PBL",
	[DEV_ST_TRANSITION_READY] = "READY",
	[DEV_ST_TRANSITION_SBL] = "SBL",
	[DEV_ST_TRANSITION_AMSS] = "AMSS",
	[DEV_ST_TRANSITION_BHIE] = "BHIE",
};

const char * const mhi_state_str[MHI_STATE_MAX] = {
	[MHI_STATE_RESET] = "RESET",
	[MHI_STATE_READY] = "READY",
	[MHI_STATE_M0] = "M0",
	[MHI_STATE_M1] = "M1",
	[MHI_STATE_M2] = "M2",
	[MHI_STATE_M3] = "M3",
	[MHI_STATE_BHI] = "BHI",
	[MHI_STATE_SYS_ERR] = "SYS_ERR",
};

static const char * const mhi_pm_state_str[] = {
	[MHI_PM_STATE_DISABLE] = "DISABLE",
	[MHI_PM_STATE_POR] = "POR",
	[MHI_PM_STATE_M0] = "M0",
	[MHI_PM_STATE_M2] = "M2",
	[MHI_PM_STATE_M3_ENTER] = "M?->M3",
	[MHI_PM_STATE_M3] = "M3",
	[MHI_PM_STATE_M3_EXIT] = "M3->M0",
	[MHI_PM_STATE_FW_DL_ERR] = "FW DL Error",
	[MHI_PM_STATE_SYS_ERR_DETECT] = "SYS_ERR Detect",
	[MHI_PM_STATE_SYS_ERR_PROCESS] = "SYS_ERR Process",
	[MHI_PM_STATE_SHUTDOWN_PROCESS] = "SHUTDOWN Process",
	[MHI_PM_STATE_LD_ERR_FATAL_DETECT] = "LD or Error Fatal Detect",
};

const char *to_mhi_pm_state_str(enum mhi_pm_state state)
{
	int index = find_last_bit((unsigned long *)&state, 32);

	if (index >= ARRAY_SIZE(mhi_pm_state_str))
		return "Invalid State";

	return mhi_pm_state_str[index];
}

/* MHI protocol requires the transfer ring to be aligned with ring length */
static int mhi_alloc_aligned_ring(struct mhi_controller *mhi_cntrl,
				  struct mhi_ring *ring,
				  u64 len)
{
	ring->alloc_size = len + (len - 1);
	ring->pre_aligned = mhi_alloc_coherent(mhi_cntrl, ring->alloc_size,
					       &ring->dma_handle, GFP_KERNEL);
	if (!ring->pre_aligned)
		return -ENOMEM;

	ring->iommu_base = (ring->dma_handle + (len - 1)) & ~(len - 1);
	ring->base = ring->pre_aligned + (ring->iommu_base - ring->dma_handle);

	return 0;
}

void mhi_deinit_free_irq(struct mhi_controller *mhi_cntrl)
{
	int i;
	struct mhi_event *mhi_event = mhi_cntrl->mhi_event;

	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		if (mhi_event->offload_ev)
			continue;

		free_irq(mhi_cntrl->irq[mhi_event->irq], mhi_event);
	}

	free_irq(mhi_cntrl->irq[0], mhi_cntrl);
}

int mhi_init_irq_setup(struct mhi_controller *mhi_cntrl)
{
	int i;
	int ret;
	struct mhi_event *mhi_event = mhi_cntrl->mhi_event;

	/* Setup BHI_INTVEC IRQ */
	ret = request_threaded_irq(mhi_cntrl->irq[0], mhi_intvec_handler,
				   mhi_intvec_threaded_handler, IRQF_SHARED,
				   "bhi", mhi_cntrl);
	if (ret)
		return ret;

	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		if (mhi_event->offload_ev)
			continue;

		ret = request_irq(mhi_cntrl->irq[mhi_event->irq],
				  mhi_irq_handler, IRQF_SHARED, "mhi",
				  mhi_event);
		if (ret) {
			dev_err(mhi_cntrl->dev,
				"Error requesting irq:%d for ev:%d\n",
				mhi_cntrl->irq[mhi_event->irq], i);
			goto error_request;
		}
	}

	return 0;

error_request:
	for (--i, --mhi_event; i >= 0; i--, mhi_event--) {
		if (mhi_event->offload_ev)
			continue;

		free_irq(mhi_cntrl->irq[mhi_event->irq], mhi_event);
	}
	free_irq(mhi_cntrl->irq[0], mhi_cntrl);

	return ret;
}

void mhi_deinit_dev_ctxt(struct mhi_controller *mhi_cntrl)
{
	int i;
	struct mhi_ctxt *mhi_ctxt = mhi_cntrl->mhi_ctxt;
	struct mhi_cmd *mhi_cmd;
	struct mhi_event *mhi_event;
	struct mhi_ring *ring;

	mhi_cmd = mhi_cntrl->mhi_cmd;
	for (i = 0; i < NR_OF_CMD_RINGS; i++, mhi_cmd++) {
		ring = &mhi_cmd->ring;
		mhi_free_coherent(mhi_cntrl, ring->alloc_size,
				  ring->pre_aligned, ring->dma_handle);
		ring->base = NULL;
		ring->iommu_base = 0;
	}

	mhi_free_coherent(mhi_cntrl,
			  sizeof(*mhi_ctxt->cmd_ctxt) * NR_OF_CMD_RINGS,
			  mhi_ctxt->cmd_ctxt, mhi_ctxt->cmd_ctxt_addr);

	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		if (mhi_event->offload_ev)
			continue;

		ring = &mhi_event->ring;
		mhi_free_coherent(mhi_cntrl, ring->alloc_size,
				  ring->pre_aligned, ring->dma_handle);
		ring->base = NULL;
		ring->iommu_base = 0;
	}

	mhi_free_coherent(mhi_cntrl, sizeof(*mhi_ctxt->er_ctxt) *
			  mhi_cntrl->total_ev_rings, mhi_ctxt->er_ctxt,
			  mhi_ctxt->er_ctxt_addr);

	mhi_free_coherent(mhi_cntrl, sizeof(*mhi_ctxt->chan_ctxt) *
			  mhi_cntrl->max_chan, mhi_ctxt->chan_ctxt,
			  mhi_ctxt->chan_ctxt_addr);

	kfree(mhi_ctxt);
	mhi_cntrl->mhi_ctxt = NULL;
}

int mhi_init_dev_ctxt(struct mhi_controller *mhi_cntrl)
{
	struct mhi_ctxt *mhi_ctxt;
	struct mhi_chan_ctxt *chan_ctxt;
	struct mhi_event_ctxt *er_ctxt;
	struct mhi_cmd_ctxt *cmd_ctxt;
	struct mhi_chan *mhi_chan;
	struct mhi_event *mhi_event;
	struct mhi_cmd *mhi_cmd;
	int ret = -ENOMEM, i;

	atomic_set(&mhi_cntrl->dev_wake, 0);
	atomic_set(&mhi_cntrl->alloc_size, 0);

	mhi_ctxt = kzalloc(sizeof(*mhi_ctxt), GFP_KERNEL);
	if (!mhi_ctxt)
		return -ENOMEM;

	/* Setup channel ctxt */
	mhi_ctxt->chan_ctxt = mhi_alloc_coherent(mhi_cntrl,
						 sizeof(*mhi_ctxt->chan_ctxt) *
						 mhi_cntrl->max_chan,
						 &mhi_ctxt->chan_ctxt_addr,
						 GFP_KERNEL);
	if (!mhi_ctxt->chan_ctxt)
		goto error_alloc_chan_ctxt;

	mhi_chan = mhi_cntrl->mhi_chan;
	chan_ctxt = mhi_ctxt->chan_ctxt;
	for (i = 0; i < mhi_cntrl->max_chan; i++, chan_ctxt++, mhi_chan++) {
		/* Skip if it is an offload channel */
		if (mhi_chan->offload_ch)
			continue;

		chan_ctxt->chstate = MHI_CH_STATE_DISABLED;
		chan_ctxt->brstmode = mhi_chan->db_cfg.brstmode;
		chan_ctxt->pollcfg = mhi_chan->db_cfg.pollcfg;
		chan_ctxt->chtype = mhi_chan->dir;
		chan_ctxt->erindex = mhi_chan->er_index;

		mhi_chan->ch_state = MHI_CH_STATE_DISABLED;
		mhi_chan->tre_ring.db_addr = &chan_ctxt->wp;
	}

	/* Setup event context */
	mhi_ctxt->er_ctxt = mhi_alloc_coherent(mhi_cntrl,
					       sizeof(*mhi_ctxt->er_ctxt) *
					       mhi_cntrl->total_ev_rings,
					       &mhi_ctxt->er_ctxt_addr,
					       GFP_KERNEL);
	if (!mhi_ctxt->er_ctxt)
		goto error_alloc_er_ctxt;

	er_ctxt = mhi_ctxt->er_ctxt;
	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, er_ctxt++,
		     mhi_event++) {
		struct mhi_ring *ring = &mhi_event->ring;

		/* Skip if it is an offload event */
		if (mhi_event->offload_ev)
			continue;

		er_ctxt->intmodc = 0;
		er_ctxt->intmodt = mhi_event->intmod;
		er_ctxt->ertype = MHI_ER_TYPE_VALID;
		er_ctxt->msivec = mhi_event->irq;
		mhi_event->db_cfg.db_mode = true;

		ring->el_size = sizeof(struct mhi_tre);
		ring->len = ring->el_size * ring->elements;
		ret = mhi_alloc_aligned_ring(mhi_cntrl, ring, ring->len);
		if (ret)
			goto error_alloc_er;

		/*
		 * If the read pointer equals to the write pointer, then the
		 * ring is empty
		 */
		ring->rp = ring->wp = ring->base;
		er_ctxt->rbase = ring->iommu_base;
		er_ctxt->rp = er_ctxt->wp = er_ctxt->rbase;
		er_ctxt->rlen = ring->len;
		ring->ctxt_wp = &er_ctxt->wp;
	}

	/* Setup cmd context */
	mhi_ctxt->cmd_ctxt = mhi_alloc_coherent(mhi_cntrl,
						sizeof(*mhi_ctxt->cmd_ctxt) *
						NR_OF_CMD_RINGS,
						&mhi_ctxt->cmd_ctxt_addr,
						GFP_KERNEL);
	if (!mhi_ctxt->cmd_ctxt)
		goto error_alloc_er;

	mhi_cmd = mhi_cntrl->mhi_cmd;
	cmd_ctxt = mhi_ctxt->cmd_ctxt;
	for (i = 0; i < NR_OF_CMD_RINGS; i++, mhi_cmd++, cmd_ctxt++) {
		struct mhi_ring *ring = &mhi_cmd->ring;

		ring->el_size = sizeof(struct mhi_tre);
		ring->elements = CMD_EL_PER_RING;
		ring->len = ring->el_size * ring->elements;
		ret = mhi_alloc_aligned_ring(mhi_cntrl, ring, ring->len);
		if (ret)
			goto error_alloc_cmd;

		ring->rp = ring->wp = ring->base;
		cmd_ctxt->rbase = ring->iommu_base;
		cmd_ctxt->rp = cmd_ctxt->wp = cmd_ctxt->rbase;
		cmd_ctxt->rlen = ring->len;
		ring->ctxt_wp = &cmd_ctxt->wp;
	}

	mhi_cntrl->mhi_ctxt = mhi_ctxt;

	return 0;

error_alloc_cmd:
	for (--i, --mhi_cmd; i >= 0; i--, mhi_cmd--) {
		struct mhi_ring *ring = &mhi_cmd->ring;

		mhi_free_coherent(mhi_cntrl, ring->alloc_size,
				  ring->pre_aligned, ring->dma_handle);
	}
	mhi_free_coherent(mhi_cntrl,
			  sizeof(*mhi_ctxt->cmd_ctxt) * NR_OF_CMD_RINGS,
			  mhi_ctxt->cmd_ctxt, mhi_ctxt->cmd_ctxt_addr);
	i = mhi_cntrl->total_ev_rings;
	mhi_event = mhi_cntrl->mhi_event + i;

error_alloc_er:
	for (--i, --mhi_event; i >= 0; i--, mhi_event--) {
		struct mhi_ring *ring = &mhi_event->ring;

		if (mhi_event->offload_ev)
			continue;

		mhi_free_coherent(mhi_cntrl, ring->alloc_size,
				  ring->pre_aligned, ring->dma_handle);
	}
	mhi_free_coherent(mhi_cntrl, sizeof(*mhi_ctxt->er_ctxt) *
			  mhi_cntrl->total_ev_rings, mhi_ctxt->er_ctxt,
			  mhi_ctxt->er_ctxt_addr);

error_alloc_er_ctxt:
	mhi_free_coherent(mhi_cntrl, sizeof(*mhi_ctxt->chan_ctxt) *
			  mhi_cntrl->max_chan, mhi_ctxt->chan_ctxt,
			  mhi_ctxt->chan_ctxt_addr);

error_alloc_chan_ctxt:
	kfree(mhi_ctxt);

	return ret;
}

int mhi_init_mmio(struct mhi_controller *mhi_cntrl)
{
	u32 val;
	int i, ret;
	struct mhi_chan *mhi_chan;
	struct mhi_event *mhi_event;
	void __iomem *base = mhi_cntrl->regs;
	struct {
		u32 offset;
		u32 mask;
		u32 shift;
		u32 val;
	} reg_info[] = {
		{
			CCABAP_HIGHER, U32_MAX, 0,
			upper_32_bits(mhi_cntrl->mhi_ctxt->chan_ctxt_addr),
		},
		{
			CCABAP_LOWER, U32_MAX, 0,
			lower_32_bits(mhi_cntrl->mhi_ctxt->chan_ctxt_addr),
		},
		{
			ECABAP_HIGHER, U32_MAX, 0,
			upper_32_bits(mhi_cntrl->mhi_ctxt->er_ctxt_addr),
		},
		{
			ECABAP_LOWER, U32_MAX, 0,
			lower_32_bits(mhi_cntrl->mhi_ctxt->er_ctxt_addr),
		},
		{
			CRCBAP_HIGHER, U32_MAX, 0,
			upper_32_bits(mhi_cntrl->mhi_ctxt->cmd_ctxt_addr),
		},
		{
			CRCBAP_LOWER, U32_MAX, 0,
			lower_32_bits(mhi_cntrl->mhi_ctxt->cmd_ctxt_addr),
		},
		{
			MHICFG, MHICFG_NER_MASK, MHICFG_NER_SHIFT,
			mhi_cntrl->total_ev_rings,
		},
		{
			MHICFG, MHICFG_NHWER_MASK, MHICFG_NHWER_SHIFT,
			mhi_cntrl->hw_ev_rings,
		},
		{
			MHICTRLBASE_HIGHER, U32_MAX, 0,
			upper_32_bits(mhi_cntrl->iova_start),
		},
		{
			MHICTRLBASE_LOWER, U32_MAX, 0,
			lower_32_bits(mhi_cntrl->iova_start),
		},
		{
			MHIDATABASE_HIGHER, U32_MAX, 0,
			upper_32_bits(mhi_cntrl->iova_start),
		},
		{
			MHIDATABASE_LOWER, U32_MAX, 0,
			lower_32_bits(mhi_cntrl->iova_start),
		},
		{
			MHICTRLLIMIT_HIGHER, U32_MAX, 0,
			upper_32_bits(mhi_cntrl->iova_stop),
		},
		{
			MHICTRLLIMIT_LOWER, U32_MAX, 0,
			lower_32_bits(mhi_cntrl->iova_stop),
		},
		{
			MHIDATALIMIT_HIGHER, U32_MAX, 0,
			upper_32_bits(mhi_cntrl->iova_stop),
		},
		{
			MHIDATALIMIT_LOWER, U32_MAX, 0,
			lower_32_bits(mhi_cntrl->iova_stop),
		},
		{ 0, 0, 0 }
	};

	dev_info(mhi_cntrl->dev, "Initializing MHI registers\n");

	/* Read channel db offset */
	ret = mhi_read_reg_field(mhi_cntrl, base, CHDBOFF, CHDBOFF_CHDBOFF_MASK,
				 CHDBOFF_CHDBOFF_SHIFT, &val);
	if (ret) {
		dev_err(mhi_cntrl->dev, "Unable to read CHDBOFF register\n");
		return -EIO;
	}

	/* Setup wake db */
	mhi_cntrl->wake_db = base + val + (8 * MHI_DEV_WAKE_DB);
	mhi_write_reg(mhi_cntrl, mhi_cntrl->wake_db, 4, 0);
	mhi_write_reg(mhi_cntrl, mhi_cntrl->wake_db, 0, 0);
	mhi_cntrl->wake_set = false;

	/* Setup channel db address for each channel in tre_ring */
	mhi_chan = mhi_cntrl->mhi_chan;
	for (i = 0; i < mhi_cntrl->max_chan; i++, val += 8, mhi_chan++)
		mhi_chan->tre_ring.db_addr = base + val;

	/* Read event ring db offset */
	ret = mhi_read_reg_field(mhi_cntrl, base, ERDBOFF, ERDBOFF_ERDBOFF_MASK,
				 ERDBOFF_ERDBOFF_SHIFT, &val);
	if (ret) {
		dev_err(mhi_cntrl->dev, "Unable to read ERDBOFF register\n");
		return -EIO;
	}

	/* Setup event db address for each ev_ring */
	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, val += 8, mhi_event++) {
		if (mhi_event->offload_ev)
			continue;

		mhi_event->ring.db_addr = base + val;
	}

	/* Setup DB register for primary CMD rings */
	mhi_cntrl->mhi_cmd[PRIMARY_CMD_RING].ring.db_addr = base + CRDB_LOWER;

	/* Write to MMIO registers */
	for (i = 0; reg_info[i].offset; i++)
		mhi_write_reg_field(mhi_cntrl, base, reg_info[i].offset,
				    reg_info[i].mask, reg_info[i].shift,
				    reg_info[i].val);

	return 0;
}

void mhi_deinit_chan_ctxt(struct mhi_controller *mhi_cntrl,
			  struct mhi_chan *mhi_chan)
{
	struct mhi_ring *buf_ring;
	struct mhi_ring *tre_ring;
	struct mhi_chan_ctxt *chan_ctxt;

	buf_ring = &mhi_chan->buf_ring;
	tre_ring = &mhi_chan->tre_ring;
	chan_ctxt = &mhi_cntrl->mhi_ctxt->chan_ctxt[mhi_chan->chan];

	mhi_free_coherent(mhi_cntrl, tre_ring->alloc_size,
			  tre_ring->pre_aligned, tre_ring->dma_handle);
	kfree(buf_ring->base);

	buf_ring->base = tre_ring->base = NULL;
	chan_ctxt->rbase = 0;
}

int mhi_init_chan_ctxt(struct mhi_controller *mhi_cntrl,
		       struct mhi_chan *mhi_chan)
{
	struct mhi_ring *buf_ring;
	struct mhi_ring *tre_ring;
	struct mhi_chan_ctxt *chan_ctxt;
	int ret;

	buf_ring = &mhi_chan->buf_ring;
	tre_ring = &mhi_chan->tre_ring;
	tre_ring->el_size = sizeof(struct mhi_tre);
	tre_ring->len = tre_ring->el_size * tre_ring->elements;
	chan_ctxt = &mhi_cntrl->mhi_ctxt->chan_ctxt[mhi_chan->chan];
	ret = mhi_alloc_aligned_ring(mhi_cntrl, tre_ring, tre_ring->len);
	if (ret)
		return -ENOMEM;

	buf_ring->el_size = sizeof(struct mhi_buf_info);
	buf_ring->len = buf_ring->el_size * buf_ring->elements;
	buf_ring->base = kzalloc(buf_ring->len, GFP_KERNEL);

	if (!buf_ring->base) {
		mhi_free_coherent(mhi_cntrl, tre_ring->alloc_size,
				  tre_ring->pre_aligned, tre_ring->dma_handle);
		return -ENOMEM;
	}

	chan_ctxt->chstate = MHI_CH_STATE_ENABLED;
	chan_ctxt->rbase = tre_ring->iommu_base;
	chan_ctxt->rp = chan_ctxt->wp = chan_ctxt->rbase;
	chan_ctxt->rlen = tre_ring->len;
	tre_ring->ctxt_wp = &chan_ctxt->wp;

	tre_ring->rp = tre_ring->wp = tre_ring->base;
	buf_ring->rp = buf_ring->wp = buf_ring->base;
	mhi_chan->db_cfg.db_mode = 1;

	/* Update to all cores */
	smp_wmb();

	return 0;
}

static int parse_ev_cfg(struct mhi_controller *mhi_cntrl,
			struct mhi_controller_config *config)
{
	int i, num = 0;
	struct mhi_event *mhi_event;
	struct mhi_event_config *event_cfg;

	num = config->num_events;
	mhi_cntrl->total_ev_rings = num;
	mhi_cntrl->mhi_event = kcalloc(num, sizeof(*mhi_cntrl->mhi_event),
				       GFP_KERNEL);
	if (!mhi_cntrl->mhi_event)
		return -ENOMEM;

	/* Populate event ring */
	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < num; ++i) {
		event_cfg = &config->event_cfg[i];

		mhi_event->er_index = i++;
		mhi_event->ring.elements = event_cfg->num_elements;
		mhi_event->intmod = event_cfg->irq_moderation_ms;
		mhi_event->irq = event_cfg->irq;

		if (event_cfg->channel != U32_MAX) {
			/* This event ring has a dedicated channel */
			mhi_event->chan = event_cfg->channel;
			if (mhi_event->chan >= mhi_cntrl->max_chan) {
				dev_err(mhi_cntrl->dev,
					"Event Ring channel not available\n");
				goto error_ev_cfg;
			}

			mhi_event->mhi_chan =
				&mhi_cntrl->mhi_chan[mhi_event->chan];
		}

		/* Priority is fixed to 1 for now */
		mhi_event->priority = 1;

		mhi_event->db_cfg.brstmode = event_cfg->mode;
		if (MHI_INVALID_BRSTMODE(mhi_event->db_cfg.brstmode))
			goto error_ev_cfg;

		mhi_event->db_cfg.process_db =
			(mhi_event->db_cfg.brstmode == MHI_DB_BRST_ENABLE) ?
			mhi_db_brstmode : mhi_db_brstmode_disable;

		mhi_event->data_type = event_cfg->data_type;

		switch (mhi_event->data_type) {
		case MHI_ER_DATA:
			mhi_event->process_event = mhi_process_data_event_ring;
			break;
		case MHI_ER_CTRL:
			mhi_event->process_event = mhi_process_ctrl_ev_ring;
			break;
		default:
			dev_err(mhi_cntrl->dev,
				"Event Ring type not supported\n");
			goto error_ev_cfg;
		}

		mhi_event->hw_ring = event_cfg->hardware_event;
		if (mhi_event->hw_ring)
			mhi_cntrl->hw_ev_rings++;
		else
			mhi_cntrl->sw_ev_rings++;

		mhi_event->cl_manage = event_cfg->client_managed;
		mhi_event->offload_ev = event_cfg->offload_channel;
		mhi_event++;
	}

	/* We need IRQ for each event ring + additional one for BHI */
	mhi_cntrl->nr_irqs_req = mhi_cntrl->total_ev_rings + 1;

	return 0;

error_ev_cfg:

	kfree(mhi_cntrl->mhi_event);
	return -EINVAL;
}

static int parse_ch_cfg(struct mhi_controller *mhi_cntrl,
			struct mhi_controller_config *config)
{
	int i;
	u32 chan;
	struct mhi_channel_config *ch_cfg;

	mhi_cntrl->max_chan = config->max_channels;

	mhi_cntrl->mhi_chan = kcalloc(mhi_cntrl->max_chan,
				      sizeof(*mhi_cntrl->mhi_chan), GFP_KERNEL);
	if (!mhi_cntrl->mhi_chan)
		return -ENOMEM;

	INIT_LIST_HEAD(&mhi_cntrl->lpm_chans);

	/* Populate channel configurations */
	for (i = 0; i < config->num_channels; ++i) {
		struct mhi_chan *mhi_chan;

		ch_cfg = &config->ch_cfg[i];

		chan = ch_cfg->num;
		if (chan >= mhi_cntrl->max_chan) {
			dev_err(mhi_cntrl->dev,
				"Channel %d not available\n", chan);
			goto error_chan_cfg;
		}

		mhi_chan = &mhi_cntrl->mhi_chan[chan];
		mhi_chan->name = ch_cfg->name;
		mhi_chan->chan = chan;

		mhi_chan->buf_ring.elements = ch_cfg->num_elements;
		if (!mhi_chan->buf_ring.elements)
			goto error_chan_cfg;

		mhi_chan->tre_ring.elements = mhi_chan->buf_ring.elements;
		mhi_chan->er_index = ch_cfg->event_ring;
		mhi_chan->dir = ch_cfg->dir;

		mhi_chan->ee = ch_cfg->ee;
		if (mhi_chan->ee >= MHI_EE_MAX_SUPPORTED)
			goto error_chan_cfg;

		mhi_chan->db_cfg.pollcfg = ch_cfg->pollcfg;
		mhi_chan->xfer_type = ch_cfg->data_type;

		switch (mhi_chan->xfer_type) {
		case MHI_BUF_RAW:
			mhi_chan->gen_tre = mhi_gen_tre;
			mhi_chan->queue_xfer = mhi_queue_buf;
			break;
		case MHI_BUF_SKB:
			mhi_chan->queue_xfer = mhi_queue_skb;
			break;
		case MHI_BUF_SCLIST:
			mhi_chan->gen_tre = mhi_gen_tre;
			mhi_chan->queue_xfer = mhi_queue_sclist;
			break;
		case MHI_BUF_NOP:
			mhi_chan->queue_xfer = mhi_queue_nop;
			break;
		default:
			dev_err(mhi_cntrl->dev,
				"Channel datatype not supported\n");
			goto error_chan_cfg;
		}

		mhi_chan->lpm_notify = ch_cfg->lpm_notify;
		mhi_chan->offload_ch = ch_cfg->offload_channel;
		mhi_chan->db_cfg.reset_req = ch_cfg->doorbell_mode_switch;
		mhi_chan->pre_alloc = ch_cfg->auto_queue;
		mhi_chan->auto_start = ch_cfg->auto_start;

		/*
		 * If MHI host allocates buffers, then the channel direction
		 * should be DMA_FROM_DEVICE and the buffer type should be
		 * MHI_BUF_RAW
		 */
		if (mhi_chan->pre_alloc && (mhi_chan->dir != DMA_FROM_DEVICE ||
				mhi_chan->xfer_type != MHI_BUF_RAW)) {
			dev_err(mhi_cntrl->dev,
				"Invalid channel configuration\n");
			goto error_chan_cfg;
		}

		/*
		 * Bi-directional and direction less channel must be an
		 * offload channel
		 */
		if ((mhi_chan->dir == DMA_BIDIRECTIONAL ||
		     mhi_chan->dir == DMA_NONE) && !mhi_chan->offload_ch) {
			dev_err(mhi_cntrl->dev,
				"Invalid channel configuration\n");
			goto error_chan_cfg;
		}

		/* If MHI host allocates buffers then client cannot queue */
		if (mhi_chan->pre_alloc)
			mhi_chan->queue_xfer = mhi_queue_nop;

		if (!mhi_chan->offload_ch) {
			mhi_chan->db_cfg.brstmode = ch_cfg->doorbell;
			if (MHI_INVALID_BRSTMODE(mhi_chan->db_cfg.brstmode)) {
				dev_err(mhi_cntrl->dev,
					"Invalid Door bell mode\n");
				goto error_chan_cfg;
			}

			mhi_chan->db_cfg.process_db =
				(mhi_chan->db_cfg.brstmode ==
				 MHI_DB_BRST_ENABLE) ?
				mhi_db_brstmode : mhi_db_brstmode_disable;
		}

		mhi_chan->configured = true;

		if (mhi_chan->lpm_notify)
			list_add_tail(&mhi_chan->node, &mhi_cntrl->lpm_chans);
	}

	return 0;

error_chan_cfg:
	kfree(mhi_cntrl->mhi_chan);

	return -EINVAL;
}

static int parse_config(struct mhi_controller *mhi_cntrl,
			struct mhi_controller_config *config)
{
	int ret;

	/* Parse MHI channel configuration */
	ret = parse_ch_cfg(mhi_cntrl, config);
	if (ret)
		return ret;

	/* Parse MHI event configuration */
	ret = parse_ev_cfg(mhi_cntrl, config);
	if (ret)
		goto error_ev_cfg;

	mhi_cntrl->timeout_ms = config->timeout_ms;
	if (!mhi_cntrl->timeout_ms)
		mhi_cntrl->timeout_ms = MHI_TIMEOUT_MS;

	mhi_cntrl->bounce_buf = config->use_bounce_buf;
	mhi_cntrl->buffer_len = config->buf_len;
	if (!mhi_cntrl->buffer_len)
		mhi_cntrl->buffer_len = MHI_MAX_MTU;

	return 0;

error_ev_cfg:
	kfree(mhi_cntrl->mhi_chan);

	return ret;
}

int mhi_register_controller(struct mhi_controller *mhi_cntrl,
			    struct mhi_controller_config *config)
{
	int ret;
	int i;
	struct mhi_event *mhi_event;
	struct mhi_chan *mhi_chan;
	struct mhi_cmd *mhi_cmd;
	struct mhi_device *mhi_dev;

	if (!mhi_cntrl->runtime_get || !mhi_cntrl->runtime_put)
		return -EINVAL;

	if (!mhi_cntrl->status_cb || !mhi_cntrl->link_status)
		return -EINVAL;

	ret = parse_config(mhi_cntrl, config);
	if (ret)
		return -EINVAL;

	mhi_cntrl->mhi_cmd = kcalloc(NR_OF_CMD_RINGS,
				     sizeof(*mhi_cntrl->mhi_cmd), GFP_KERNEL);
	if (!mhi_cntrl->mhi_cmd) {
		ret = -ENOMEM;
		goto error_alloc_cmd;
	}

	INIT_LIST_HEAD(&mhi_cntrl->transition_list);
	mutex_init(&mhi_cntrl->pm_mutex);
	rwlock_init(&mhi_cntrl->pm_lock);
	spin_lock_init(&mhi_cntrl->transition_lock);
	spin_lock_init(&mhi_cntrl->wlock);
	INIT_WORK(&mhi_cntrl->st_worker, mhi_pm_st_worker);
	INIT_WORK(&mhi_cntrl->fw_worker, mhi_fw_load_worker);
	INIT_WORK(&mhi_cntrl->syserr_worker, mhi_pm_sys_err_worker);
	init_waitqueue_head(&mhi_cntrl->state_event);

	mhi_cmd = mhi_cntrl->mhi_cmd;
	for (i = 0; i < NR_OF_CMD_RINGS; i++, mhi_cmd++)
		spin_lock_init(&mhi_cmd->lock);

	mhi_event = mhi_cntrl->mhi_event;
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		/* Skip for offload events */
		if (mhi_event->offload_ev)
			continue;

		mhi_event->mhi_cntrl = mhi_cntrl;
		spin_lock_init(&mhi_event->lock);
		if (mhi_event->data_type == MHI_ER_CTRL)
			tasklet_init(&mhi_event->task, mhi_ctrl_ev_task,
				     (ulong)mhi_event);
		else
			tasklet_init(&mhi_event->task, mhi_ev_task,
				     (ulong)mhi_event);
	}

	mhi_chan = mhi_cntrl->mhi_chan;
	for (i = 0; i < mhi_cntrl->max_chan; i++, mhi_chan++) {
		mutex_init(&mhi_chan->mutex);
		init_completion(&mhi_chan->completion);
		rwlock_init(&mhi_chan->lock);
	}

	if (mhi_cntrl->bounce_buf) {
		mhi_cntrl->map_single = mhi_map_single_use_bb;
		mhi_cntrl->unmap_single = mhi_unmap_single_use_bb;
	} else {
		mhi_cntrl->map_single = mhi_map_single_no_bb;
		mhi_cntrl->unmap_single = mhi_unmap_single_no_bb;
	}

	/* Register controller with MHI bus */
	mhi_dev = mhi_alloc_device(mhi_cntrl);
	if (!mhi_dev) {
		ret = -ENOMEM;
		goto error_alloc_dev;
	}

	mhi_dev->dev_type = MHI_DEVICE_CONTROLLER;
	mhi_dev->mhi_cntrl = mhi_cntrl;
	dev_set_name(&mhi_dev->dev, "%s", mhi_cntrl->name);
	ret = device_add(&mhi_dev->dev);
	if (ret)
		goto error_add_dev;

	mhi_cntrl->mhi_dev = mhi_dev;

	return 0;

error_add_dev:
	mhi_dealloc_device(mhi_cntrl, mhi_dev);

error_alloc_dev:
	kfree(mhi_cntrl->mhi_cmd);

error_alloc_cmd:
	kfree(mhi_cntrl->mhi_chan);
	kfree(mhi_cntrl->mhi_event);

	return ret;
}
EXPORT_SYMBOL_GPL(mhi_register_controller);

void mhi_unregister_controller(struct mhi_controller *mhi_cntrl)
{
	struct mhi_device *mhi_dev = mhi_cntrl->mhi_dev;

	kfree(mhi_cntrl->mhi_cmd);
	kfree(mhi_cntrl->mhi_event);
	kfree(mhi_cntrl->mhi_chan);

	device_del(&mhi_dev->dev);
	put_device(&mhi_dev->dev);
}
EXPORT_SYMBOL_GPL(mhi_unregister_controller);

struct mhi_controller *mhi_alloc_controller(void)
{
	struct mhi_controller *mhi_cntrl;

	mhi_cntrl = kzalloc(sizeof(*mhi_cntrl), GFP_KERNEL);

	return mhi_cntrl;
}
EXPORT_SYMBOL_GPL(mhi_alloc_controller);

int mhi_prepare_for_power_up(struct mhi_controller *mhi_cntrl)
{
	int ret;

	mutex_lock(&mhi_cntrl->pm_mutex);

	ret = mhi_init_dev_ctxt(mhi_cntrl);
	if (ret)
		goto error_dev_ctxt;

	ret = mhi_init_irq_setup(mhi_cntrl);
	if (ret)
		goto error_setup_irq;

	/*
	 * Allocate RDDM table if specified, this table is for debug purpose
	 * so we'll ignore erros
	 */
	if (mhi_cntrl->rddm_size)
		mhi_alloc_bhie_table(mhi_cntrl, &mhi_cntrl->rddm_image,
				     mhi_cntrl->rddm_size);

	mhi_cntrl->pre_init = true;

	mutex_unlock(&mhi_cntrl->pm_mutex);

	return 0;

error_setup_irq:
	mhi_deinit_dev_ctxt(mhi_cntrl);

error_dev_ctxt:
	mutex_unlock(&mhi_cntrl->pm_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(mhi_prepare_for_power_up);

void mhi_unprepare_after_power_down(struct mhi_controller *mhi_cntrl)
{
	if (mhi_cntrl->fbc_image) {
		mhi_free_bhie_table(mhi_cntrl, mhi_cntrl->fbc_image);
		mhi_cntrl->fbc_image = NULL;
	}

	if (mhi_cntrl->rddm_image) {
		mhi_free_bhie_table(mhi_cntrl, mhi_cntrl->rddm_image);
		mhi_cntrl->rddm_image = NULL;
	}

	mhi_deinit_free_irq(mhi_cntrl);
	mhi_deinit_dev_ctxt(mhi_cntrl);
	mhi_cntrl->pre_init = false;
}

/* Match MHI device to driver */
static int mhi_match(struct device *dev, struct device_driver *drv)
{
	struct mhi_device *mhi_dev = to_mhi_device(dev);
	struct mhi_driver *mhi_drv = to_mhi_driver(drv);
	const struct mhi_device_id *id;

	/*
	 * If the device is a controller type then there is no client driver
	 * associated with it
	 */
	if (mhi_dev->dev_type == MHI_DEVICE_CONTROLLER)
		return 0;

	for (id = mhi_drv->id_table; id->chan[0]; id++)
		if (!strcmp(mhi_dev->chan_name, id->chan)) {
			mhi_dev->id = id;
			return 1;
		}

	return 0;
};

static void mhi_release_device(struct device *dev)
{
	struct mhi_device *mhi_dev = to_mhi_device(dev);

	kfree(mhi_dev);
}

struct bus_type mhi_bus_type = {
	.name = "mhi",
	.dev_name = "mhi",
	.match = mhi_match,
};

static int mhi_driver_probe(struct device *dev)
{
	struct mhi_device *mhi_dev = to_mhi_device(dev);
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	struct device_driver *drv = dev->driver;
	struct mhi_driver *mhi_drv = to_mhi_driver(drv);
	struct mhi_event *mhi_event;
	struct mhi_chan *ul_chan = mhi_dev->ul_chan;
	struct mhi_chan *dl_chan = mhi_dev->dl_chan;

	if (ul_chan) {
		/*
		 * If channel supports LPM notifications then status_cb should
		 * be provided
		 */
		if (ul_chan->lpm_notify && !mhi_drv->status_cb)
			return -EINVAL;

		/* For non-offload channels then xfer_cb should be provided */
		if (!ul_chan->offload_ch && !mhi_drv->ul_xfer_cb)
			return -EINVAL;

		ul_chan->xfer_cb = mhi_drv->ul_xfer_cb;
	}

	if (dl_chan) {
		/*
		 * If channel supports LPM notifications then status_cb should
		 * be provided
		 */
		if (dl_chan->lpm_notify && !mhi_drv->status_cb)
			return -EINVAL;

		/* For non-offload channels then xfer_cb should be provided */
		if (!dl_chan->offload_ch && !mhi_drv->dl_xfer_cb)
			return -EINVAL;

		mhi_event = &mhi_cntrl->mhi_event[dl_chan->er_index];

		/*
		 * If the channel event ring is managed by client, then
		 * status_cb must be provided so that the framework can
		 * notify pending data
		 */
		if (mhi_event->cl_manage && !mhi_drv->status_cb)
			return -EINVAL;

		dl_chan->xfer_cb = mhi_drv->dl_xfer_cb;
	}

	/* Call the user provided probe function */
	return mhi_drv->probe(mhi_dev, mhi_dev->id);
}

static int mhi_driver_remove(struct device *dev)
{
	struct mhi_device *mhi_dev = to_mhi_device(dev);
	struct mhi_driver *mhi_drv = to_mhi_driver(dev->driver);
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	struct mhi_chan *mhi_chan;
	enum mhi_ch_state ch_state[] = {
		MHI_CH_STATE_DISABLED,
		MHI_CH_STATE_DISABLED
	};
	int dir;

	/* Skip if it is a controller device */
	if (mhi_dev->dev_type == MHI_DEVICE_CONTROLLER)
		return 0;

	/* Reset both channels */
	for (dir = 0; dir < 2; dir++) {
		mhi_chan = dir ? mhi_dev->ul_chan : mhi_dev->dl_chan;

		if (!mhi_chan)
			continue;

		/* Wake all threads waiting for completion */
		write_lock_irq(&mhi_chan->lock);
		mhi_chan->ccs = MHI_EV_CC_INVALID;
		complete_all(&mhi_chan->completion);
		write_unlock_irq(&mhi_chan->lock);

		/* Set the channel state to disabled */
		mutex_lock(&mhi_chan->mutex);
		write_lock_irq(&mhi_chan->lock);
		ch_state[dir] = mhi_chan->ch_state;
		mhi_chan->ch_state = MHI_CH_STATE_DISABLED;
		write_unlock_irq(&mhi_chan->lock);
	}

	mhi_drv->remove(mhi_dev);

	/* De-init channel if it was enabled */
	for (dir = 0; dir < 2; dir++) {
		mhi_chan = dir ? mhi_dev->ul_chan : mhi_dev->dl_chan;

		if (!mhi_chan)
			continue;

		if (ch_state[dir] == MHI_CH_STATE_ENABLED &&
		    !mhi_chan->offload_ch)
			mhi_deinit_chan_ctxt(mhi_cntrl, mhi_chan);

		/* Remove associated device */
		mhi_chan->mhi_dev = NULL;

		mutex_unlock(&mhi_chan->mutex);
	}

	read_lock_bh(&mhi_cntrl->pm_lock);
	while (atomic_read(&mhi_dev->dev_wake))
		mhi_device_put(mhi_dev);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	return 0;
}

int mhi_driver_register(struct mhi_driver *mhi_drv)
{
	struct device_driver *driver = &mhi_drv->driver;

	if (!mhi_drv->probe || !mhi_drv->remove)
		return -EINVAL;

	driver->bus = &mhi_bus_type;
	driver->probe = mhi_driver_probe;
	driver->remove = mhi_driver_remove;

	return driver_register(driver);
}
EXPORT_SYMBOL_GPL(mhi_driver_register);

void mhi_driver_unregister(struct mhi_driver *mhi_drv)
{
	driver_unregister(&mhi_drv->driver);
}
EXPORT_SYMBOL_GPL(mhi_driver_unregister);

struct mhi_device *mhi_alloc_device(struct mhi_controller *mhi_cntrl)
{
	struct mhi_device *mhi_dev = kzalloc(sizeof(*mhi_dev), GFP_KERNEL);
	struct device *dev;

	if (!mhi_dev)
		return NULL;

	dev = &mhi_dev->dev;
	device_initialize(dev);
	dev->bus = &mhi_bus_type;
	dev->release = mhi_release_device;
	dev->parent = mhi_cntrl->dev;
	mhi_dev->mhi_cntrl = mhi_cntrl;
	atomic_set(&mhi_dev->dev_wake, 0);

	return mhi_dev;
}

static int __init mhi_init(void)
{
	return bus_register(&mhi_bus_type);
}

static void __exit mhi_exit(void)
{
	bus_unregister(&mhi_bus_type);
}

postcore_initcall(mhi_init);
module_exit(mhi_exit);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("MHI_CORE");
MODULE_DESCRIPTION("MHI Host Interface");
