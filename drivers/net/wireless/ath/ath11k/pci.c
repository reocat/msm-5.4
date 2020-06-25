// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/msi.h>
#include <linux/pci.h>

#include "pci.h"
#include "core.h"
#include "hif.h"
#include "mhi.h"
#include "debug.h"

#define QCA6390_DEVICE_ID		0x1101

static const u8 ath11k_tx_ring_mask[ATH11K_EXT_IRQ_GRP_NUM_MAX] = {
	ATH11K_TX_RING_MASK_0,
	ATH11K_TX_RING_MASK_1,
	ATH11K_TX_RING_MASK_2,
};

static const u8 ath11k_rx_mon_status_ring_mask[ATH11K_EXT_IRQ_GRP_NUM_MAX] = {
	0, 0, 0, 0,
	ATH11K_RX_MON_STATUS_RING_MASK_0,
	ATH11K_RX_MON_STATUS_RING_MASK_1,
	ATH11K_RX_MON_STATUS_RING_MASK_2,
};

static const u8 ath11k_rx_ring_mask[ATH11K_EXT_IRQ_GRP_NUM_MAX] = {
	0, 0, 0, 0, 0, 0, 0,
	ATH11K_RX_RING_MASK_0,
	ATH11K_RX_RING_MASK_1,
	ATH11K_RX_RING_MASK_2,
	ATH11K_RX_RING_MASK_3,
};

static const u8 ath11k_rx_err_ring_mask[ATH11K_EXT_IRQ_GRP_NUM_MAX] = {
	ATH11K_RX_ERR_RING_MASK_0,
};

static const u8 ath11k_rx_wbm_rel_ring_mask[ATH11K_EXT_IRQ_GRP_NUM_MAX] = {
	ATH11K_RX_WBM_REL_RING_MASK_0,
};

static const u8 ath11k_reo_status_ring_mask[ATH11K_EXT_IRQ_GRP_NUM_MAX] = {
	ATH11K_REO_STATUS_RING_MASK_0,
};

static const u8 ath11k_rxdma2host_ring_mask[ATH11K_EXT_IRQ_GRP_NUM_MAX] = {
	ATH11K_RXDMA2HOST_RING_MASK_0,
	ATH11K_RXDMA2HOST_RING_MASK_1,
	ATH11K_RXDMA2HOST_RING_MASK_2,
};

static const u8 ath11k_host2rxdma_ring_mask[ATH11K_EXT_IRQ_GRP_NUM_MAX] = {
	0,
};

static const struct pci_device_id ath11k_pci_id_table[] = {
	{ PCI_VDEVICE(QCOM, QCA6390_DEVICE_ID) },
	{0}
};

MODULE_DEVICE_TABLE(pci, ath11k_pci_id_table);

static struct ath11k_msi_config msi_config = {
	.total_vectors = 32,
	.total_users = 4,
	.users = (struct ath11k_msi_user[]) {
		{ .name = "MHI", .num_vectors = 3, .base_vector = 0 },
		{ .name = "CE", .num_vectors = 10, .base_vector = 3 },
		{ .name = "WAKE", .num_vectors = 1, .base_vector = 13 },
		{ .name = "DP", .num_vectors = 18, .base_vector = 14 },
	},
};

/* Target firmware's Copy Engine configuration. */
static const struct ce_pipe_config target_ce_config_wlan[] = {
	/* CE0: host->target HTC control and raw streams */
	{
		.pipenum = __cpu_to_le32(0),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE1: target->host HTT + HTC control */
	{
		.pipenum = __cpu_to_le32(1),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE2: target->host WMI */
	{
		.pipenum = __cpu_to_le32(2),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE3: host->target WMI */
	{
		.pipenum = __cpu_to_le32(3),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE4: host->target HTT */
	{
		.pipenum = __cpu_to_le32(4),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(256),
		.nbytes_max = __cpu_to_le32(256),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = __cpu_to_le32(0),
	},

	/* CE5: target->host Pktlog */
	{
		.pipenum = __cpu_to_le32(5),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE6: Reserved for target autonomous hif_memcpy */
	{
		.pipenum = __cpu_to_le32(6),
		.pipedir = __cpu_to_le32(PIPEDIR_INOUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(16384),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE7 used only by Host */
	{
		.pipenum = __cpu_to_le32(7),
		.pipedir = __cpu_to_le32(PIPEDIR_INOUT_H2H),
		.nentries = __cpu_to_le32(0),
		.nbytes_max = __cpu_to_le32(0),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = __cpu_to_le32(0),
	},

	/* CE8 target->host used only by IPA */
	{
		.pipenum = __cpu_to_le32(8),
		.pipedir = __cpu_to_le32(PIPEDIR_INOUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(16384),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},
	/* CE 9, 10, 11 are used by MHI driver */
};

/* Map from service/endpoint to Copy Engine.
 * This table is derived from the CE_PCI TABLE, above.
 * It is passed to the Target at startup for use by firmware.
 */
static const struct service_to_pipe target_service_to_ce_map_wlan[] = {
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_WMI_DATA_VO),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_WMI_DATA_VO),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_WMI_DATA_BK),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_WMI_DATA_BK),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_WMI_DATA_BE),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_WMI_DATA_BE),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_WMI_DATA_VI),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_WMI_DATA_VI),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_WMI_CONTROL),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_WMI_CONTROL),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},

	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_RSVD_CTRL),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(0),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_RSVD_CTRL),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},

	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_HTT_DATA_MSG),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(4),
	},
	{
		__cpu_to_le32(ATH11K_HTC_SVC_ID_HTT_DATA_MSG),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(1),
	},

	/* (Additions here) */

	{ /* must be last */
		__cpu_to_le32(0),
		__cpu_to_le32(0),
		__cpu_to_le32(0),
	},
};

/* FIXME_KVALO: this should be in hw_params */
static __maybe_unused const char *ath11k_pci_irq_name[ATH11K_IRQ_NUM_MAX] = {
	"bhi",
	"mhi-er0",
	"mhi-er1",
	"ce0",
	"ce1",
	"ce2",
	"ce3",
	"ce4",
	"ce5",
	"ce6",
	"ce7",
	"ce8",
	"ce9",
	"ce10",
	"ce11",
	"host2wbm-desc-feed",
	"host2reo-re-injection",
	"host2reo-command",
	"host2rxdma-monitor-ring3",
	"host2rxdma-monitor-ring2",
	"host2rxdma-monitor-ring1",
	"reo2ost-exception",
	"wbm2host-rx-release",
	"reo2host-status",
	"reo2host-destination-ring4",
	"reo2host-destination-ring3",
	"reo2host-destination-ring2",
	"reo2host-destination-ring1",
	"rxdma2host-monitor-destination-mac3",
	"rxdma2host-monitor-destination-mac2",
	"rxdma2host-monitor-destination-mac1",
	"ppdu-end-interrupts-mac3",
	"ppdu-end-interrupts-mac2",
	"ppdu-end-interrupts-mac1",
	"rxdma2host-monitor-status-ring-mac3",
	"rxdma2host-monitor-status-ring-mac2",
	"rxdma2host-monitor-status-ring-mac1",
	"host2rxdma-host-buf-ring-mac3",
	"host2rxdma-host-buf-ring-mac2",
	"host2rxdma-host-buf-ring-mac1",
	"rxdma2host-destination-ring-mac3",
	"rxdma2host-destination-ring-mac2",
	"rxdma2host-destination-ring-mac1",
	"host2tcl-input-ring4",
	"host2tcl-input-ring3",
	"host2tcl-input-ring2",
	"host2tcl-input-ring1",
	"wbm2host-tx-completions-ring3",
	"wbm2host-tx-completions-ring2",
	"wbm2host-tx-completions-ring1",
	"tcl2host-status-ring",
};

static inline void ath11k_pci_select_window(struct ath11k_pci *ab_pci, u32 offset)
{
	u32 window = (offset >> WINDOW_SHIFT) & WINDOW_VALUE_MASK;

	if (window != ab_pci->register_window) {
		iowrite32(WINDOW_ENABLE_BIT | window,
			  ab_pci->mem + WINDOW_REG_ADDRESS);
		ab_pci->register_window = window;
	}
}

static inline struct ath11k_pci *ath11k_pci_priv(struct ath11k_base *ab)
{
	return (struct ath11k_pci *)ab->drv_priv;
}

void ath11k_pci_write32(struct ath11k_base *ab, u32 offset, u32 value)
{
	struct ath11k_pci *ab_pci = ath11k_pci_priv(ab);

	if (!ab->use_register_windowing || offset < MAX_UNWINDOWED_ADDRESS) {
		iowrite32(value, ab_pci->mem  + offset);
	} else {
		spin_lock_bh(&ab_pci->window_lock);
		ath11k_pci_select_window(ab_pci, offset);
		iowrite32(value, ab_pci->mem + WINDOW_START + (offset & WINDOW_RANGE_MASK));
		spin_unlock_bh(&ab_pci->window_lock);
	}
}

u32 ath11k_pci_read32(struct ath11k_base *ab, u32 offset)
{
	struct ath11k_pci *ab_pci = ath11k_pci_priv(ab);
	u32 val;

	if (!ab->use_register_windowing || offset < MAX_UNWINDOWED_ADDRESS) {
		val = ioread32(ab_pci->mem + offset);
	} else {
		spin_lock_bh(&ab_pci->window_lock);
		ath11k_pci_select_window(ab_pci, offset);
		val = ioread32(ab_pci->mem + WINDOW_START + (offset & WINDOW_RANGE_MASK));
		spin_unlock_bh(&ab_pci->window_lock);
	}

	return val;
}

static void ath11k_pci_soc_global_reset(struct ath11k_base *ab)
{
	u32 val;
	u32 delay;

	val = ath11k_pci_read32(ab, PCIE_SOC_GLOBAL_RESET);

	val |= PCIE_SOC_GLOBAL_RESET_V;

	ath11k_pci_write32(ab, PCIE_SOC_GLOBAL_RESET, val);

	/* TODO: exact time to sleep is uncertain */
	delay = 10;
	mdelay(delay);

	/* Need to toggle V bit back otherwise stuck in reset status */
	val &= ~PCIE_SOC_GLOBAL_RESET_V;

	ath11k_pci_write32(ab, PCIE_SOC_GLOBAL_RESET, val);

	mdelay(delay);

	val = ath11k_pci_read32(ab, PCIE_SOC_GLOBAL_RESET);
	if (val == 0xffffffff)
		ath11k_err(ab, "%s link down error\n", __func__);
}

static void ath11k_pci_clear_dbg_registers(struct ath11k_base *ab)
{
	u32 val;

	/* read cookie */
	val = ath11k_pci_read32(ab, PCIE_Q6_COOKIE_ADDR);
	ath11k_dbg(ab, ATH11K_DBG_PCI, "cookie:0x%x\n", val);

	val = ath11k_pci_read32(ab, WLAON_WARM_SW_ENTRY);
	ath11k_dbg(ab, ATH11K_DBG_PCI, "WLAON_WARM_SW_ENTRY 0x%x\n", val);

	/* TODO: exact time to sleep is uncertain */
	mdelay(10);

	/* write 0 to WLAON_WARM_SW_ENTRY to prevent Q6 from
	 * continuing warm path and entering dead loop.
	 */
	ath11k_pci_write32(ab, WLAON_WARM_SW_ENTRY, 0);
	mdelay(10);

	val = ath11k_pci_read32(ab, WLAON_WARM_SW_ENTRY);
	ath11k_dbg(ab, ATH11K_DBG_PCI, "WLAON_WARM_SW_ENTRY 0x%x\n", val);

	/* A read clear register. clear the register to prevent
	 * Q6 from entering wrong code path.
	 */
	val = ath11k_pci_read32(ab, WLAON_SOC_RESET_CAUSE_REG);
	ath11k_dbg(ab, ATH11K_DBG_PCI, "soc reset cause:%d\n", val);
}

static void ath11k_pci_force_wake(struct ath11k_base *ab)
{
	ath11k_pci_write32(ab, PCIE_SOC_WAKE_PCIE_LOCAL_REG, 1);
	mdelay(5);
}

static void ath11k_pci_sw_reset(struct ath11k_base *ab)
{
	ath11k_pci_soc_global_reset(ab);
	ath11k_mhi_clear_vector(ab);
	ath11k_pci_soc_global_reset(ab);
	ath11k_mhi_set_mhictrl_reset(ab);
	ath11k_pci_clear_dbg_registers(ab);
}

int ath11k_pci_get_msi_irq(struct device *dev, unsigned int vector)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);

	int irq_num;

	irq_num = pci_irq_vector(pci_dev, vector);

	return irq_num;
}

static void ath11k_pci_get_msi_address(struct ath11k_base *ab, u32 *msi_addr_lo,
				       u32 *msi_addr_hi)
{
	struct pci_dev *pci_dev = to_pci_dev(ab->dev);

	pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_LO,
			      msi_addr_lo);

	pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_HI,
			      msi_addr_hi);
}

int ath11k_pci_get_user_msi_assignment(struct ath11k_pci *ab_pci, char *user_name,
				       int *num_vectors, u32 *user_base_data,
				       u32 *base_vector)
{
	struct ath11k_base *ab = ab_pci->ab;
	struct ath11k_msi_config *msi_config;
	int idx;

	msi_config = ab_pci->msi_config;
	if (!msi_config) {
		ath11k_err(ab, "MSI is not supported.\n");
		return -EINVAL;
	}

	for (idx = 0; idx < msi_config->total_users; idx++) {
		if (strcmp(user_name, msi_config->users[idx].name) == 0) {
			*num_vectors = msi_config->users[idx].num_vectors;
			*user_base_data = msi_config->users[idx].base_vector
				+ ab_pci->msi_ep_base_data;
			*base_vector = msi_config->users[idx].base_vector;

			ath11k_dbg(ab, ATH11K_DBG_PCI, "Assign MSI to user: %s, num_vectors: %d, user_base_data: %u, base_vector: %u\n",
				   user_name, *num_vectors, *user_base_data,
				   *base_vector);

			return 0;
		}
	}

	ath11k_err(ab, "Failed to find MSI assignment for %s!\n", user_name);

	return -EINVAL;
}

static int ath11k_get_user_msi_assignment(struct ath11k_base *ab, char *user_name,
					  int *num_vectors, u32 *user_base_data,
					  u32 *base_vector)
{
	struct ath11k_pci *ab_pci = ath11k_pci_priv(ab);

	return ath11k_pci_get_user_msi_assignment(ab_pci, user_name,
						  num_vectors, user_base_data,
						  base_vector);
}

static void ath11k_pci_free_ext_irq(struct ath11k_base *ab)
{
	int i, j;

	for (i = 0; i < ATH11K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath11k_ext_irq_grp *irq_grp = &ab->ext_irq_grp[i];

		for (j = 0; j < irq_grp->num_irq; j++)
			free_irq(ab->irq_num[irq_grp->irqs[j]], irq_grp);

		netif_napi_del(&irq_grp->napi);
	}
}

static void ath11k_pci_free_irq(struct ath11k_base *ab)
{
	int irq_idx;
	int i;

	for (i = 0; i < CE_COUNT; i++) {
		if (ath11k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;
		irq_idx = ATH11K_IRQ_CE0_OFFSET + i;
		free_irq(ab->irq_num[irq_idx], &ab->ce.ce_pipe[i]);
	}

	ath11k_pci_free_ext_irq(ab);
}

static void ath11k_pci_ce_irq_enable(struct ath11k_base *ab, u16 ce_id)
{
	u32 irq_idx;

	irq_idx = ATH11K_IRQ_CE0_OFFSET + ce_id;
	enable_irq(ab->irq_num[irq_idx]);
}

static void ath11k_pci_ce_irq_disable(struct ath11k_base *ab, u16 ce_id)
{
	u32 irq_idx;

	irq_idx = ATH11K_IRQ_CE0_OFFSET + ce_id;
	disable_irq_nosync(ab->irq_num[irq_idx]);
}

static void ath11k_pci_ce_irqs_disable(struct ath11k_base *ab)
{
	int i;

	for (i = 0; i < CE_COUNT; i++) {
		if (ath11k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;
		ath11k_pci_ce_irq_disable(ab, i);
	}
}

static void ath11k_pci_sync_ce_irqs(struct ath11k_base *ab)
{
	int i;
	int irq_idx;

	for (i = 0; i < CE_COUNT; i++) {
		if (ath11k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		irq_idx = ATH11K_IRQ_CE0_OFFSET + i;
		synchronize_irq(ab->irq_num[irq_idx]);
	}
}

static void ath11k_pci_ce_tasklet(unsigned long data)
{
	struct ath11k_ce_pipe *ce_pipe = (struct ath11k_ce_pipe *)data;

	ath11k_ce_per_engine_service(ce_pipe->ab, ce_pipe->pipe_num);

	ath11k_pci_ce_irq_enable(ce_pipe->ab, ce_pipe->pipe_num);
}

static irqreturn_t ath11k_pci_ce_interrupt_handler(int irq, void *arg)
{
	struct ath11k_ce_pipe *ce_pipe = arg;

	ath11k_pci_ce_irq_disable(ce_pipe->ab, ce_pipe->pipe_num);
	tasklet_schedule(&ce_pipe->intr_tq);

	return IRQ_HANDLED;
}

static void ath11k_pci_ext_grp_disable(struct ath11k_ext_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		disable_irq_nosync(irq_grp->ab->irq_num[irq_grp->irqs[i]]);
}

static void __ath11k_pci_ext_irq_disable(struct ath11k_base *sc)
{
	int i;

	for (i = 0; i < ATH11K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath11k_ext_irq_grp *irq_grp = &sc->ext_irq_grp[i];

		ath11k_pci_ext_grp_disable(irq_grp);

		napi_synchronize(&irq_grp->napi);
		napi_disable(&irq_grp->napi);
	}
}

static void ath11k_pci_ext_grp_enable(struct ath11k_ext_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		enable_irq(irq_grp->ab->irq_num[irq_grp->irqs[i]]);
}

static void ath11k_pci_ext_irq_enable(struct ath11k_base *ab)
{
	int i;

	for (i = 0; i < ATH11K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath11k_ext_irq_grp *irq_grp = &ab->ext_irq_grp[i];

		napi_enable(&irq_grp->napi);
		ath11k_pci_ext_grp_enable(irq_grp);
	}
}

static void ath11k_pci_sync_ext_irqs(struct ath11k_base *ab)
{
	int i, j;
	int irq_idx;

	for (i = 0; i < ATH11K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath11k_ext_irq_grp *irq_grp = &ab->ext_irq_grp[i];

		for (j = 0; j < irq_grp->num_irq; j++) {
			irq_idx = irq_grp->irqs[j];
			synchronize_irq(ab->irq_num[irq_idx]);
		}
	}
}

static void ath11k_pci_ext_irq_disable(struct ath11k_base *ab)
{
	__ath11k_pci_ext_irq_disable(ab);
	ath11k_pci_sync_ext_irqs(ab);
}

static int ath11k_pci_ext_grp_napi_poll(struct napi_struct *napi, int budget)
{
	struct ath11k_ext_irq_grp *irq_grp = container_of(napi,
						struct ath11k_ext_irq_grp,
						napi);
	struct ath11k_base *ab = irq_grp->ab;
	int work_done;

	work_done = ath11k_dp_service_srng(ab, irq_grp, budget);
	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		ath11k_pci_ext_grp_enable(irq_grp);
	}

	if (work_done > budget)
		work_done = budget;

	return work_done;
}

static irqreturn_t ath11k_pci_ext_interrupt_handler(int irq, void *arg)
{
	struct ath11k_ext_irq_grp *irq_grp = arg;

	ath11k_dbg(irq_grp->ab, ATH11K_DBG_PCI, "ext irq:%d\n", irq);

	ath11k_pci_ext_grp_disable(irq_grp);

	napi_schedule(&irq_grp->napi);

	return IRQ_HANDLED;
}

static void ath11k_pci_ext_ring_mask_config(struct ath11k_base *ab)
{
	int i;

	for (i = 0; i < ATH11K_EXT_IRQ_GRP_NUM_MAX; i++) {
		ab->ring_mask.tx_ring_mask[i] = ath11k_tx_ring_mask[i];
		ab->ring_mask.rx_mon_status_ring_mask[i] =
			ath11k_rx_mon_status_ring_mask[i];
		ab->ring_mask.rx_ring_mask[i] = ath11k_rx_ring_mask[i];
		ab->ring_mask.rx_err_ring_mask[i] = ath11k_rx_err_ring_mask[i];
		ab->ring_mask.rx_wbm_rel_ring_mask[i] = ath11k_rx_wbm_rel_ring_mask[i];
		ab->ring_mask.reo_status_ring_mask[i] = ath11k_reo_status_ring_mask[i];
		ab->ring_mask.rxdma2host_ring_mask[i] = ath11k_rxdma2host_ring_mask[i];
		ab->ring_mask.host2rxdma_ring_mask[i] = ath11k_host2rxdma_ring_mask[i];
	}
}

static int ath11k_pci_ext_irq_config(struct ath11k_base *ab)
{
	int i, j;
	int ret;
	int num_vectors = 0;
	u32 user_base_data = 0, base_vector = 0;

	ath11k_pci_ext_ring_mask_config(ab);

	ath11k_pci_get_user_msi_assignment(ath11k_pci_priv(ab), "DP",
					   &num_vectors, &user_base_data,
					   &base_vector);

	for (i = 0; i < ATH11K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath11k_ext_irq_grp *irq_grp = &ab->ext_irq_grp[i];
		u32 num_irq = 0;

		irq_grp->ab = ab;
		irq_grp->grp_id = i;
		init_dummy_netdev(&irq_grp->napi_ndev);
		netif_napi_add(&irq_grp->napi_ndev, &irq_grp->napi,
			       ath11k_pci_ext_grp_napi_poll, NAPI_POLL_WEIGHT);

		if (ath11k_tx_ring_mask[i] ||
		    ath11k_rx_ring_mask[i] ||
		    ath11k_rx_err_ring_mask[i] ||
		    ath11k_rx_wbm_rel_ring_mask[i] ||
		    ath11k_reo_status_ring_mask[i] ||
		    ath11k_rxdma2host_ring_mask[i] ||
		    ath11k_host2rxdma_ring_mask[i] ||
		    ath11k_rx_mon_status_ring_mask[i]) {
			num_irq = 1;
		}

		irq_grp->num_irq = num_irq;
		irq_grp->irqs[0] = base_vector + i;

		for (j = 0; j < irq_grp->num_irq; j++) {
			int irq_idx = irq_grp->irqs[j];
			int vector = (i % num_vectors) + base_vector;
			int irq = ath11k_pci_get_msi_irq(ab->dev, vector);

			ab->irq_num[irq_idx] = irq;

			ath11k_dbg(ab, ATH11K_DBG_PCI,
				   "irq:%d group:%d\n", irq, i);
			ret = request_irq(irq, ath11k_pci_ext_interrupt_handler,
					  IRQF_SHARED,
					  "DP_EXT_IRQ", irq_grp);
			if (ret) {
				ath11k_err(ab, "failed request_irq for %d\n",
					   vector);
			}

			disable_irq_nosync(ab->irq_num[irq_idx]);
		}
	}

	return 0;
}

static int ath11k_pci_config_irq(struct ath11k_base *ab)
{
	struct ath11k_ce_pipe *ce_pipe;
	u32 msi_data_start;
	u32 msi_data_count;
	u32 msi_irq_start;
	unsigned int msi_data;
	int irq, i, ret, irq_idx;

	ret = ath11k_pci_get_user_msi_assignment(ath11k_pci_priv(ab),
						 "CE", &msi_data_count,
						 &msi_data_start, &msi_irq_start);

	/* Configure CE irqs */
	for (i = 0; i < CE_COUNT; i++) {
		msi_data = (i % msi_data_count) +
				msi_irq_start;
		irq = ath11k_pci_get_msi_irq(ab->dev, msi_data);
		ce_pipe = &ab->ce.ce_pipe[i];

		if (ath11k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		irq_idx = ATH11K_IRQ_CE0_OFFSET + i;

		ath11k_info(ab, "%s irq:%d, irq_idx:%d\n",
			    __func__, irq, irq_idx);

		tasklet_init(&ce_pipe->intr_tq, ath11k_pci_ce_tasklet,
			     (unsigned long)ce_pipe);

		ret = request_irq(irq, ath11k_pci_ce_interrupt_handler,
				  IRQF_SHARED, ath11k_irq_name[irq_idx],
				  ce_pipe);
		if (ret)
			return ret;
		ab->irq_num[irq_idx] = irq;
		ath11k_pci_ce_irq_disable(ab, i);
	}

	/* To Do Configure external interrupts */
	ath11k_pci_ext_irq_config(ab);

	return ret;
}

static void ath11k_pci_init_qmi_ce_config(struct ath11k_base *ab)
{
	struct ath11k_qmi_ce_cfg *cfg = &ab->qmi.ce_cfg;

	cfg->tgt_ce = target_ce_config_wlan;
	cfg->tgt_ce_len = ARRAY_SIZE(target_ce_config_wlan);

	cfg->svc_to_ce_map = target_service_to_ce_map_wlan;
	cfg->svc_to_ce_map_len = ARRAY_SIZE(target_service_to_ce_map_wlan);
	ab->qmi.service_ins_id = ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_QCA6x90;
}

static void ath11k_pci_ce_irqs_enable(struct ath11k_base *ab)
{
	int i;

	for (i = 0; i < CE_COUNT; i++) {
		if (ath11k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;
		ath11k_pci_ce_irq_enable(ab, i);
	}
}

static int ath11k_pci_qca6x90_powerup(struct ath11k_pci *ab_pci)
{
	ath11k_pci_sw_reset(ab_pci->ab);

	return ath11k_pci_start_mhi(ab_pci);
}

static void ath11k_pci_qca6x90_powerdown(struct ath11k_pci *ab_pci)
{
	ath11k_pci_stop_mhi(ab_pci);
	ath11k_pci_force_wake(ab_pci->ab);
	ath11k_pci_sw_reset(ab_pci->ab);
}

static int ath11k_pci_get_msi_assignment(struct ath11k_pci *ab_pci)
{
	ab_pci->msi_config = &msi_config;

	return 0;
}

static int ath11k_pci_enable_msi(struct ath11k_pci *ab_pci)
{
	struct ath11k_base *ab = ab_pci->ab;
	struct ath11k_msi_config *msi_config;
	struct msi_desc *msi_desc;
	int num_vectors;
	int ret;

	ret = ath11k_pci_get_msi_assignment(ab_pci);
	if (ret) {
		ath11k_err(ab, "failed to get MSI assignment, err = %d\n", ret);
		goto out;
	}

	msi_config = ab_pci->msi_config;
	if (!msi_config) {
		ath11k_err(ab, "msi_config is NULL!\n");
		ret = -EINVAL;
		goto out;
	}

	num_vectors = pci_alloc_irq_vectors(ab_pci->pdev,
					    msi_config->total_vectors,
					    msi_config->total_vectors,
					    PCI_IRQ_MSI);
	if (num_vectors != msi_config->total_vectors) {
		ath11k_err(ab, "failed to get enough MSI vectors (%d), available vectors = %d",
			   msi_config->total_vectors, num_vectors);

		if (num_vectors >= 0)
			ret = -EINVAL;
		else
			ret = num_vectors;

		goto reset_msi_config;
	}

	msi_desc = irq_get_msi_desc(ab_pci->pdev->irq);
	if (!msi_desc) {
		ath11k_err(ab, "msi_desc is NULL!\n");
		ret = -EINVAL;
		goto free_msi_vector;
	}

	ab_pci->msi_ep_base_data = msi_desc->msg.data;

	ath11k_dbg(ab, ATH11K_DBG_PCI, "msi base data is %d\n", ab_pci->msi_ep_base_data);

	return 0;

free_msi_vector:
	pci_free_irq_vectors(ab_pci->pdev);
reset_msi_config:
	ab_pci->msi_config = NULL;
out:
	return ret;
}

static void ath11k_pci_disable_msi(struct ath11k_pci *ab_pci)
{
	pci_free_irq_vectors(ab_pci->pdev);
}

static int ath11k_pci_claim(struct ath11k_pci *ab_pci, struct pci_dev *pdev)
{
	u32 pci_dma_mask = PCI_DMA_MASK_32_BIT;
	struct ath11k_base *ab = ab_pci->ab;
	u16 device_id;
	int ret = 0;

	pci_read_config_word(pdev, PCI_DEVICE_ID, &device_id);
	if (device_id != ab_pci->dev_id)  {
		ath11k_err(ab, "pci device id mismatch, config ID: 0x%x, probe ID: 0x%x\n",
			   device_id, ab_pci->dev_id);
		ret = -EIO;
		goto out;
	}

	ret = pci_assign_resource(pdev, PCI_BAR_NUM);
	if (ret) {
		ath11k_err(ab, "failed to assign pci resource, err = %d\n", ret);
		goto out;
	}

	ret = pci_enable_device(pdev);
	if (ret) {
		ath11k_err(ab, "failed to enable pci device, err = %d\n", ret);
		goto out;
	}

	ret = pci_request_region(pdev, PCI_BAR_NUM, "ath11k_pci");
	if (ret) {
		ath11k_err(ab, "failed to request pci region, err = %d\n", ret);
		goto disable_device;
	}

	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(pci_dma_mask));
	if (ret) {
		ath11k_err(ab, "failed to set pci dma mask (%d), err = %d\n",
			   ret, pci_dma_mask);
		goto release_region;
	}

	ret = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(pci_dma_mask));
	if (ret) {
		ath11k_err(ab, "failed to set pci consistent dma mask (%d), err = %d\n",
			   ret, pci_dma_mask);
		goto release_region;
	}

	pci_set_master(pdev);

	ab_pci->mem_len = pci_resource_len(pdev, PCI_BAR_NUM);
	ab_pci->mem = pci_iomap(pdev, PCI_BAR_NUM, 0);
	if (!ab_pci->mem) {
		ath11k_err(ab, "failed to map pci bar, bar = %d\n", PCI_BAR_NUM);
		ret = -EIO;
		goto clear_master;
	}

	ath11k_dbg(ab, ATH11K_DBG_BOOT, "boot pci_mem 0x%pK\n", ab_pci->mem);
	return 0;

clear_master:
	pci_clear_master(pdev);
release_region:
	pci_release_region(pdev, PCI_BAR_NUM);
disable_device:
	pci_disable_device(pdev);
out:
	return ret;
}

static void ath11k_pci_free_region(struct ath11k_pci *ab_pci)
{
	struct pci_dev *pci_dev = ab_pci->pdev;

	pci_iounmap(pci_dev, ab_pci->mem);
	ab_pci->mem = NULL;
	pci_clear_master(pci_dev);
	pci_release_region(pci_dev, PCI_BAR_NUM);
	if (pci_is_enabled(pci_dev))
		pci_disable_device(pci_dev);
}

static void ath11k_pci_read_mhi_version(struct ath11k_base *ab)
{
	struct ath11k_pci *ab_pci;
	u32 val;
	u32 major_v, minor_v;

	ab_pci = ath11k_pci_priv(ab);

	val     = ioread32(ab_pci->mem + TCSR_SOC_HW_VERSION);
	major_v = (val & HW_MAJOR_VERSION_MASK) >> HW_MAJOR_VERSION_SHIFT;
	minor_v = (val & HW_MINOR_VERSION_MASK) >> HW_MINOR_VERSION_SHIFT;

	ath11k_info(ab, "Read HST HW Major Version %d, minor revision %d\n",
		    major_v, minor_v);
}

static int ath11k_pci_power_up(struct ath11k_base *ab)
{
	struct ath11k_pci *ab_pci;
	u8 aspm;
	int ret;

	ath11k_pci_read_mhi_version(ab);

	ab_pci = ath11k_pci_priv(ab);

	pci_read_config_byte(ab_pci->pdev, 0x80, &aspm);
	pci_write_config_byte(ab_pci->pdev, 0x80, aspm & 0xfd);

	ath11k_info(ab, "aspm 0x%x changed to 0x%x\n",
		    aspm, aspm & 0xfd);

	ret = ath11k_pci_qca6x90_powerup(ab_pci);
	if (ret)
		ath11k_err(ab, "failed to power on  mhi: %d\n", ret);

	return ret;
}

static void ath11k_pci_power_down(struct ath11k_base *ab)
{
	struct ath11k_pci *ab_pci;

	ab_pci = ath11k_pci_priv(ab);
	ath11k_pci_qca6x90_powerdown(ab_pci);
}

static void ath11k_pci_kill_tasklets(struct ath11k_base *ab)
{
	int i;

	for (i = 0; i < CE_COUNT; i++) {
		struct ath11k_ce_pipe *ce_pipe = &ab->ce.ce_pipe[i];

		if (ath11k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		tasklet_kill(&ce_pipe->intr_tq);
	}
}

static void ath11k_pci_stop(struct ath11k_base *ab)
{
	ath11k_pci_ce_irqs_disable(ab);
	ath11k_pci_sync_ce_irqs(ab);
	ath11k_pci_kill_tasklets(ab);
	ath11k_ce_cleanup_pipes(ab);
	/* Shutdown other components as appropriate */
}

static int ath11k_pci_start(struct ath11k_base *ab)
{
	ath11k_pci_ce_irqs_enable(ab);
	ath11k_ce_rx_post_buf(ab);

	/* Bring up other components as appropriate */
	return 0;
}

static int ath11k_pci_map_service_to_pipe(struct ath11k_base *ab, u16 service_id,
					  u8 *ul_pipe, u8 *dl_pipe)
{
	const struct service_to_pipe *entry;
	bool ul_set = false, dl_set = false;
	int i;

	for (i = 0; i < ARRAY_SIZE(target_service_to_ce_map_wlan); i++) {
		entry = &target_service_to_ce_map_wlan[i];

		if (__le32_to_cpu(entry->service_id) != service_id)
			continue;

		switch (__le32_to_cpu(entry->pipedir)) {
		case PIPEDIR_NONE:
			break;
		case PIPEDIR_IN:
			WARN_ON(dl_set);
			*dl_pipe = __le32_to_cpu(entry->pipenum);
			dl_set = true;
			break;
		case PIPEDIR_OUT:
			WARN_ON(ul_set);
			*ul_pipe = __le32_to_cpu(entry->pipenum);
			ul_set = true;
			break;
		case PIPEDIR_INOUT:
			WARN_ON(dl_set);
			WARN_ON(ul_set);
			*dl_pipe = __le32_to_cpu(entry->pipenum);
			*ul_pipe = __le32_to_cpu(entry->pipenum);
			dl_set = true;
			ul_set = true;
			break;
		}
	}

	if (WARN_ON(!ul_set || !dl_set))
		return -ENOENT;

	return 0;
}

static const struct ath11k_hif_ops ath11k_pci_hif_ops = {
	.start = ath11k_pci_start,
	.stop = ath11k_pci_stop,
	.read32 = ath11k_pci_read32,
	.write32 = ath11k_pci_write32,
	.power_down = ath11k_pci_power_down,
	.power_up = ath11k_pci_power_up,
	.irq_enable = ath11k_pci_ext_irq_enable,
	.irq_disable = ath11k_pci_ext_irq_disable,
	.get_msi_address =  ath11k_pci_get_msi_address,
	.get_user_msi_vector = ath11k_get_user_msi_assignment,
	.map_service_to_pipe = ath11k_pci_map_service_to_pipe,
};

static int ath11k_pci_probe(struct pci_dev *pdev,
			    const struct pci_device_id *pci_dev)
{
	struct ath11k_base *ab;
	struct ath11k_pci *ab_pci;
	enum ath11k_hw_rev hw_rev;
	int ret;

	switch (pci_dev->device) {
	case QCA6390_DEVICE_ID:
		hw_rev = ATH11K_HW_QCA6390;
		break;
	default:
		dev_err(&pdev->dev, "Unknown PCI device found: 0x%x\n",
			pci_dev->device);
		return -ENOTSUPP;
	}

	ab = ath11k_core_alloc(&pdev->dev, sizeof(*ab_pci), ATH11K_BUS_PCI);
	if (!ab) {
		dev_err(&pdev->dev, "failed to allocate ath11k base\n");
		return -ENOMEM;
	}

	ab->dev = &pdev->dev;
	ab->hw_rev = hw_rev;
	pci_set_drvdata(pdev, ab);
	ab_pci = ath11k_pci_priv(ab);
	ab_pci->dev_id = pci_dev->device;
	ab_pci->ab = ab;
	ab_pci->dev = &pdev->dev;
	ab_pci->pdev = pdev;
	ab->dev = &pdev->dev;
	ab->hw_rev = hw_rev;
	ab->hif.ops = &ath11k_pci_hif_ops;
	pci_set_drvdata(pdev, ab);
	ab->mhi_support = true;
	ab->fixed_bdf_addr = false;
	ab->m3_fw_support = true;
	ab->fixed_mem_region = false;
	ab->use_register_windowing = true;
	spin_lock_init(&ab_pci->window_lock);

	ret = ath11k_pci_claim(ab_pci, pdev);
	if (ret) {
		ath11k_err(ab, "failed to claim device: %d\n", ret);
		goto err_free_core;
	}

	ab->mem = ab_pci->mem;
	ab->mem_len = ab_pci->mem_len;

	ret = ath11k_pci_enable_msi(ab_pci);
	if (ret) {
		ath11k_err(ab, "failed to enable  msi: %d\n", ret);
		goto err_pci_free_region;
	}

	ret = ath11k_pci_register_mhi(ab_pci);
	if (ret) {
		ath11k_err(ab, "failed to register  mhi: %d\n", ret);
		goto err_pci_disable_msi;
	}

	ret = ath11k_core_pre_init(ab);
	if (ret)
		goto err_pci_unregister_mhi;

	ret = ath11k_hal_srng_init(ab);
	if (ret)
		goto err_pci_unregister_mhi;

	ret = ath11k_ce_alloc_pipes(ab);
	if (ret) {
		ath11k_err(ab, "failed to allocate ce pipes: %d\n", ret);
		goto err_hal_srng_deinit;
	}

	ath11k_pci_init_qmi_ce_config(ab);
	ath11k_pci_config_irq(ab);
	if (ret) {
		ath11k_err(ab, "failed to config irq: %d\n", ret);
		goto err_ce_free;
	}
	ret = ath11k_core_init(ab);
	if (ret) {
		ath11k_err(ab, "failed to init core: %d\n", ret);
		goto err_free_irq;
	}
	return 0;

err_free_irq:
	ath11k_pci_free_irq(ab);

err_ce_free:
	ath11k_ce_free_pipes(ab);

err_hal_srng_deinit:
	ath11k_hal_srng_deinit(ab);

err_pci_unregister_mhi:
	ath11k_pci_unregister_mhi(ab_pci);

err_pci_disable_msi:
	ath11k_pci_disable_msi(ab_pci);

err_pci_free_region:
	ath11k_pci_free_region(ab_pci);

err_free_core:
	ath11k_core_free(ab);

	return ret;
}

static void ath11k_pci_remove(struct pci_dev *pdev)
{
	struct ath11k_base *ab = pci_get_drvdata(pdev);
	struct ath11k_pci *ab_pci = ath11k_pci_priv(ab);

	set_bit(ATH11K_FLAG_UNREGISTERING, &ab->dev_flags);
	ath11k_core_deinit(ab);

	ath11k_pci_unregister_mhi(ab_pci);

	ath11k_pci_free_irq(ab);
	ath11k_pci_disable_msi(ab_pci);
	ath11k_pci_free_region(ab_pci);

	ath11k_hal_srng_deinit(ab);
	ath11k_ce_free_pipes(ab);
	ath11k_core_free(ab);
}

static void ath11k_pci_shutdown(struct pci_dev *pdev)
{
	struct ath11k_base *ab = pci_get_drvdata(pdev);

	ath11k_pci_power_down(ab);
}

static struct pci_driver ath11k_pci_driver = {
	.name = "ath11k_pci",
	.id_table = ath11k_pci_id_table,
	.probe = ath11k_pci_probe,
	.remove = ath11k_pci_remove,
	.shutdown = ath11k_pci_shutdown,
};

static int ath11k_pci_init(void)
{
	int ret;

	ret = pci_register_driver(&ath11k_pci_driver);
	if (ret)
		pr_err("failed to register ath11k pci driver: %d\n",
		       ret);

	return ret;
}
module_init(ath11k_pci_init);

static void ath11k_pci_exit(void)
{
	pci_unregister_driver(&ath11k_pci_driver);
}

module_exit(ath11k_pci_exit);

MODULE_DESCRIPTION("Driver support for Qualcomm Technologies 802.11ax WLAN PCIe devices");
MODULE_LICENSE("Dual BSD/GPL");
