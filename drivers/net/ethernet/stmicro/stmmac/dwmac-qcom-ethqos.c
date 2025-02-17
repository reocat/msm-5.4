// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2018-19 Linaro Limited
/* Copyright (c) 2021, The Linux Foundation. All rights reserved. */
/*Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.*/

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mii.h>
#include <linux/of_mdio.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/debugfs.h>
#include <linux/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/micrel_phy.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/rtnetlink.h>
#include <asm-generic/io.h>
#include <linux/kthread.h>
#include <linux/io-64-nonatomic-hi-lo.h>
#include <linux/if_vlan.h>
#include <linux/msm_eth.h>
#include <soc/qcom/sb_notification.h>
#include "stmmac.h"
#include "stmmac_platform.h"
#include "dwmac-qcom-ethqos.h"
#include "stmmac_ptp.h"
#include "dwmac-qcom-ipa-offload.h"
#include <linux/dwmac-qcom-eth-autosar.h>

void *ipc_emac_log_ctxt;
void __iomem *tlmm_rgmii_pull_ctl1_base;
void __iomem *tlmm_rgmii_rx_ctr_base;
int open_not_called;

#define PHY_LOOPBACK_1000 0x4140
#define PHY_LOOPBACK_100 0x6100
#define PHY_LOOPBACK_10 0x4100
#define PHY_OFF_SYSFS_DEV_ATTR_PERMS 0644
#define BUFF_SZ 256

static void ethqos_rgmii_io_macro_loopback(struct qcom_ethqos *ethqos,
					   int mode);
static int phy_digital_loopback_config(struct qcom_ethqos *ethqos, int speed, int config);
static void __iomem *tlmm_mdc_mdio_hdrv_pull_ctl_base;
static struct emac_emb_smmu_cb_ctx emac_emb_smmu_ctx = {0};
struct plat_stmmacenet_data *plat_dat;
static struct qcom_ethqos *pethqos;

static char err_names[10][14] = {"PHY_RW_ERR",
	"PHY_DET_ERR",
	"CRC_ERR",
	"RECEIVE_ERR",
	"OVERFLOW_ERR",
	"FBE_ERR",
	"RBU_ERR",
	"TDU_ERR",
	"DRIBBLE_ERR",
	"WDT_ERR",
};

static unsigned char dev_addr[ETH_ALEN] = {
	0, 0x55, 0x7b, 0xb5, 0x7d, 0xf7};
static struct ip_params pparams;

struct qcom_ethqos *get_pethqos(void)
{
	return pethqos;
}

static DECLARE_WAIT_QUEUE_HEAD(mac_rec_wq);
static bool mac_rec_wq_flag;

inline unsigned int dwmac_qcom_get_eth_type(unsigned char *buf)
{
	return
		((((u16)buf[QTAG_ETH_TYPE_OFFSET] << 8) |
		  buf[QTAG_ETH_TYPE_OFFSET + 1]) == ETH_P_8021Q) ?
		(((u16)buf[QTAG_VLAN_ETH_TYPE_OFFSET] << 8) |
		 buf[QTAG_VLAN_ETH_TYPE_OFFSET + 1]) :
		 (((u16)buf[QTAG_ETH_TYPE_OFFSET] << 8) |
		  buf[QTAG_ETH_TYPE_OFFSET + 1]);
}

static inline unsigned int dwmac_qcom_get_vlan_ucp(unsigned char  *buf)
{
	return
		(((u16)buf[QTAG_UCP_FIELD_OFFSET] << 8)
		 | buf[QTAG_UCP_FIELD_OFFSET + 1]);
}

static struct ethqos_emac_por emac_por[] = {
	{ .offset = RGMII_IO_MACRO_CONFIG,	.value = 0x0 },
	{ .offset = SDCC_HC_REG_DLL_CONFIG,	.value = 0x0 },
	{ .offset = SDCC_HC_REG_DDR_CONFIG,	.value = 0x0 },
	{ .offset = SDCC_HC_REG_DLL_CONFIG2,	.value = 0x0 },
	{ .offset = SDCC_USR_CTL,		.value = 0x0 },
	{ .offset = RGMII_IO_MACRO_CONFIG2,	.value = 0x0},
};

static struct ethqos_emac_driver_data emac_por_data = {
	.por = emac_por,
	.num_por = ARRAY_SIZE(emac_por),
};

static unsigned char config_dev_addr[ETH_ALEN] = {0};
static void qcom_ethqos_read_iomacro_por_values(struct qcom_ethqos *ethqos)
{
	int i;

	ethqos->por = emac_por_data.por;
	ethqos->num_por = emac_por_data.num_por;

	/* Read to POR values and enable clk */
	for (i = 0; i < ethqos->num_por; i++)
		ethqos->por[i].value =
			readl_relaxed
			(ethqos->rgmii_base + ethqos->por[i].offset);
}

u16 dwmac_qcom_select_queue(struct net_device *dev,
			    struct sk_buff *skb,
			    struct net_device *sb_dev)
{
	u16 txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
	unsigned int eth_type, priority, vlan_id;
	struct stmmac_priv *priv = netdev_priv(dev);
	bool ipa_enabled = pethqos->ipa_enabled;

	/* Retrieve ETH type */
	eth_type = dwmac_qcom_get_eth_type(skb->data);

	if (eth_type == ETH_P_TSN && pethqos->cv2x_mode == CV2X_MODE_DISABLE) {
		/* Read VLAN priority field from skb->data */
		priority = dwmac_qcom_get_vlan_ucp(skb->data);

		priority >>= VLAN_TAG_UCP_SHIFT;
		if (priority == CLASS_A_TRAFFIC_UCP)
			txqueue_select = CLASS_A_TRAFFIC_TX_CHANNEL;
		else if (priority == CLASS_B_TRAFFIC_UCP) {
			txqueue_select = CLASS_B_TRAFFIC_TX_CHANNEL;
		} else {
			if (ipa_enabled)
				txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
		else
			txqueue_select = ALL_OTHER_TX_TRAFFIC_IPA_DISABLED;
		}
	} else if (eth_type == ETH_P_1588) {
		/*gPTP seelct tx queue 1*/
		txqueue_select = NON_TAGGED_IP_TRAFFIC_TX_CHANNEL;
	} else if (skb_vlan_tag_present(skb)) {
		vlan_id = skb_vlan_tag_get_id(skb);

		if (pethqos->cv2x_mode == CV2X_MODE_AP &&
		    vlan_id == pethqos->cv2x_vlan.vlan_id) {
			txqueue_select = CV2X_TAG_TX_CHANNEL;
		} else if (pethqos->qoe_mode &&
			 vlan_id == pethqos->qoe_vlan.vlan_id){
			txqueue_select = QMI_TAG_TX_CHANNEL;
		} else {
			if (ipa_enabled)
				txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
			else
				txqueue_select =
				ALL_OTHER_TX_TRAFFIC_IPA_DISABLED;
		}
	} else {
		/* VLAN tagged IP packet or any other non vlan packets (PTP)*/
		txqueue_select = ALL_OTHER_TX_TRAFFIC_IPA_DISABLED;
		if (plat_dat->c45_marvell_en || priv->tx_queue[txqueue_select].skip_sw)
			txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
	}

	/* use better macro, cannot afford function call here */
	if (ipa_enabled && (txqueue_select == IPA_DMA_TX_CH_BE ||
			    (pethqos->cv2x_mode != CV2X_MODE_DISABLE &&
			     txqueue_select == IPA_DMA_TX_CH_CV2X))) {
		ETHQOSERR("TX Channel [%d] is not a valid for SW path\n",
			  txqueue_select);
		WARN_ON(1);
	}
	return txqueue_select;
}

void dwmac_qcom_program_avb_algorithm(struct stmmac_priv *priv,
				      struct ifr_data_struct *req)
{
	struct dwmac_qcom_avb_algorithm l_avb_struct, *u_avb_struct =
		(struct dwmac_qcom_avb_algorithm *)req->ptr;
	struct dwmac_qcom_avb_algorithm_params *avb_params;

	ETHQOSDBG("\n");

	if (copy_from_user(&l_avb_struct, (void __user *)u_avb_struct,
			   sizeof(struct dwmac_qcom_avb_algorithm))) {
		ETHQOSERR("Failed to fetch AVB Struct\n");
		return;
	}

	if (priv->speed == SPEED_1000)
		avb_params = &l_avb_struct.speed1000params;
	else
		avb_params = &l_avb_struct.speed100params;

	/* Application uses 1 for CLASS A traffic and
	 * 2 for CLASS B traffic
	 * Configure right channel accordingly
	 */
	if (l_avb_struct.qinx == 1) {
		l_avb_struct.qinx = CLASS_A_TRAFFIC_TX_CHANNEL;
	} else if (l_avb_struct.qinx == 2) {
		l_avb_struct.qinx = CLASS_B_TRAFFIC_TX_CHANNEL;
	} else {
		ETHQOSERR("Invalid index [%u] in AVB struct from user\n",
			  l_avb_struct.qinx);
		return;
	}

	priv->plat->tx_queues_cfg[l_avb_struct.qinx].mode_to_use =
		MTL_QUEUE_AVB;
	priv->plat->tx_queues_cfg[l_avb_struct.qinx].send_slope =
		avb_params->send_slope,
	priv->plat->tx_queues_cfg[l_avb_struct.qinx].idle_slope =
		avb_params->idle_slope,
	priv->plat->tx_queues_cfg[l_avb_struct.qinx].high_credit =
		avb_params->hi_credit,
	priv->plat->tx_queues_cfg[l_avb_struct.qinx].low_credit =
		avb_params->low_credit,

	priv->hw->mac->config_cbs(priv->hw,
	priv->plat->tx_queues_cfg[l_avb_struct.qinx].send_slope,
	   priv->plat->tx_queues_cfg[l_avb_struct.qinx].idle_slope,
	   priv->plat->tx_queues_cfg[l_avb_struct.qinx].high_credit,
	   priv->plat->tx_queues_cfg[l_avb_struct.qinx].low_credit,
	   l_avb_struct.qinx);

	ETHQOSDBG("\n");
}

unsigned int dwmac_qcom_get_plat_tx_coal_frames(struct sk_buff *skb)
{
	unsigned int eth_type;

	eth_type = dwmac_qcom_get_eth_type(skb->data);

#ifdef CONFIG_PTPSUPPORT_OBJ
	if (eth_type == ETH_P_1588)
		return PTP_INT_MOD;
#endif

	if (eth_type == ETH_P_TSN)
		return AVB_INT_MOD;
	if (eth_type == ETH_P_IP || eth_type == ETH_P_IPV6) {
#ifdef CONFIG_PTPSUPPORT_OBJ
		bool is_udp = (((eth_type == ETH_P_IP) &&
				   (ip_hdr(skb)->protocol ==
					IPPROTO_UDP)) ||
				  ((eth_type == ETH_P_IPV6) &&
				   (ipv6_hdr(skb)->nexthdr ==
					IPPROTO_UDP)));

		if (is_udp && ((udp_hdr(skb)->dest ==
			htons(PTP_UDP_EV_PORT)) ||
			(udp_hdr(skb)->dest ==
			  htons(PTP_UDP_GEN_PORT))))
			return PTP_INT_MOD;
#endif
		return IP_PKT_INT_MOD;
	}
	return DEFAULT_INT_MOD;
}

static int ethqos_handle_prv_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct stmmac_priv *pdata = netdev_priv(dev);
	struct ifr_data_struct req;
	struct pps_cfg eth_pps_cfg;
	int ret = 0;

	if (copy_from_user(&req, ifr->ifr_ifru.ifru_data,
			   sizeof(struct ifr_data_struct)))
		return -EFAULT;

	switch (req.cmd) {
	case ETHQOS_CONFIG_PPSOUT_CMD:
		if (copy_from_user(&eth_pps_cfg, (void __user *)req.ptr,
				   sizeof(struct pps_cfg)))
			return -EFAULT;

		if (eth_pps_cfg.ppsout_ch < 0 ||
		    eth_pps_cfg.ppsout_ch >= pdata->dma_cap.pps_out_num)
			ret = -EOPNOTSUPP;
		else if ((eth_pps_cfg.ppsout_align == 1) &&
			 ((eth_pps_cfg.ppsout_ch != DWC_ETH_QOS_PPS_CH_0) &&
			 (eth_pps_cfg.ppsout_ch != DWC_ETH_QOS_PPS_CH_3)))
			ret = -EOPNOTSUPP;
		else
			ret = ppsout_config(pdata, &eth_pps_cfg);
		break;
	case ETHQOS_AVB_ALGORITHM:
		dwmac_qcom_program_avb_algorithm(pdata, &req);
		break;
	default:
		break;
	}
	return ret;
}

#ifndef MODULE
static int __init set_early_ethernet_ipv4(char *ipv4_addr_in)
{
	int ret = 1;

	pparams.is_valid_ipv4_addr = false;

	if (!ipv4_addr_in)
		return ret;

	strlcpy(pparams.ipv4_addr_str,
		ipv4_addr_in, sizeof(pparams.ipv4_addr_str));
	ETHQOSDBG("Early ethernet IPv4 addr: %s\n", pparams.ipv4_addr_str);

	ret = in4_pton(pparams.ipv4_addr_str, -1,
		       (u8 *)&pparams.ipv4_addr.s_addr, -1, NULL);
	if (ret != 1 || pparams.ipv4_addr.s_addr == 0) {
		ETHQOSERR("Invalid ipv4 address programmed: %s\n",
			  ipv4_addr_in);
		return ret;
	}

	pparams.is_valid_ipv4_addr = true;
	return ret;
}

__setup("eipv4=", set_early_ethernet_ipv4);

static int __init set_early_ethernet_ipv6(char *ipv6_addr_in)
{
	int ret = 1;

	pparams.is_valid_ipv6_addr = false;

	if (!ipv6_addr_in)
		return ret;

	strlcpy(pparams.ipv6_addr_str,
		ipv6_addr_in, sizeof(pparams.ipv6_addr_str));
	ETHQOSDBG("Early ethernet IPv6 addr: %s\n", pparams.ipv6_addr_str);

	ret = in6_pton(pparams.ipv6_addr_str, -1,
		       (u8 *)&pparams.ipv6_addr.ifr6_addr.s6_addr32, -1, NULL);
	if (ret != 1 || !pparams.ipv6_addr.ifr6_addr.s6_addr32)  {
		ETHQOSERR("Invalid ipv6 address programmed: %s\n",
			  ipv6_addr_in);
		return ret;
	}

	pparams.is_valid_ipv6_addr = true;
	return ret;
}

__setup("eipv6=", set_early_ethernet_ipv6);

static int __init set_early_ethernet_mac(char *mac_addr)
{
	bool valid_mac = false;

	pparams.is_valid_mac_addr = false;
	if (!mac_addr)
		return 1;

	valid_mac = mac_pton(mac_addr, pparams.mac_addr);
	if (!valid_mac)
		goto fail;

	valid_mac = is_valid_ether_addr(pparams.mac_addr);
	if (!valid_mac)
		goto fail;

	pparams.is_valid_mac_addr = true;
	return 0;

fail:
	ETHQOSERR("Invalid Mac address programmed: %s\n", mac_addr);
	return 1;
}

__setup("ermac=", set_early_ethernet_mac);
#endif

static int qcom_ethqos_add_ipaddr(struct ip_params *ip_info,
				  struct net_device *dev)
{
	int res = 0;
	struct ifreq ir;
	struct sockaddr_in *sin = (void *)&ir.ifr_ifru.ifru_addr;
	struct net *net = dev_net(dev);

	if (!net || !net->genl_sock || !net->genl_sock->sk_socket) {
		ETHQOSINFO("Sock is null, unable to assign ipv4 address\n");
		return res;
	}

	if (!net->ipv4.devconf_dflt) {
		ETHQOSERR("ipv4.devconf_dflt is null, schedule wq\n");
		schedule_delayed_work(&pethqos->ipv4_addr_assign_wq,
				      msecs_to_jiffies(1000));
		return res;
	}
	/*For valid Ipv4 address*/
	memset(&ir, 0, sizeof(ir));
	memcpy(&sin->sin_addr.s_addr, &ip_info->ipv4_addr,
	       sizeof(sin->sin_addr.s_addr));

	strlcpy(ir.ifr_ifrn.ifrn_name,
		dev->name, sizeof(ir.ifr_ifrn.ifrn_name));
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	res = inet_ioctl(net->genl_sock->sk_socket,
			 SIOCSIFADDR, (unsigned long)(void *)&ir);
		if (res) {
			ETHQOSERR("can't setup IPv4 address!: %d\r\n", res);
		} else {
			ETHQOSINFO("Assigned IPv4 address: %s\r\n",
				   ip_info->ipv4_addr_str);
#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER
place_marker("M - Etherent Assigned IPv4 address");
#endif
		}
	return res;
}

#ifdef CONFIG_IPV6
static int qcom_ethqos_add_ipv6addr(struct ip_params *ip_info,
				    struct net_device *dev)
{
	int ret = -EFAULT;
	struct in6_ifreq ir6;
	char *prefix;
	struct net *net = dev_net(dev);
	/*For valid IPv6 address*/

	if (!net || !net->genl_sock || !net->genl_sock->sk_socket) {
		ETHQOSERR("Sock is null, unable to assign ipv6 address\n");
		return -EFAULT;
	}

	if (!net->ipv6.devconf_dflt) {
		ETHQOSERR("ipv6.devconf_dflt is null, schedule wq\n");
		schedule_delayed_work(&pethqos->ipv6_addr_assign_wq,
				      msecs_to_jiffies(1000));
		return ret;
	}
	memset(&ir6, 0, sizeof(ir6));
	memcpy(&ir6, &ip_info->ipv6_addr, sizeof(struct in6_ifreq));
	ir6.ifr6_ifindex = dev->ifindex;

	prefix = strnchr(ip_info->ipv6_addr_str,
			 strlen(ip_info->ipv6_addr_str), '/');

	if (!prefix) {
		ir6.ifr6_prefixlen = 0;
	} else {
		ret = kstrtoul(prefix + 1, 0,
			       (unsigned long *)&ir6.ifr6_prefixlen);
		if (ret)
			ETHQOSDBG("kstrtoul failed");
		if (ir6.ifr6_prefixlen > 128)
			ir6.ifr6_prefixlen = 0;
	}
	ret = inet6_ioctl(net->genl_sock->sk_socket,
			  SIOCSIFADDR, (unsigned long)(void *)&ir6);
		if (ret) {
			ETHQOSDBG("Can't setup IPv6 address!\r\n");
		} else {
			ETHQOSDBG("Assigned IPv6 address: %s\r\n",
				  ip_info->ipv6_addr_str);
#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER
		place_marker("M - Ethernet Assigned IPv6 address");
#endif
		}
	return ret;
}
#endif

static int rgmii_readl(struct qcom_ethqos *ethqos, unsigned int offset)
{
	return readl(ethqos->rgmii_base + offset);
}

static void rgmii_writel(struct qcom_ethqos *ethqos,
			 int value, unsigned int offset)
{
	writel(value, ethqos->rgmii_base + offset);
}

static void rgmii_updatel(struct qcom_ethqos *ethqos,
			  int mask, int val, unsigned int offset)
{
	unsigned int temp;

	temp =  rgmii_readl(ethqos, offset);
	temp = (temp & ~(mask)) | val;
	rgmii_writel(ethqos, temp, offset);
}

static void rgmii_loopback_config(void *priv_n, int loopback_en)
{
	struct stmmac_priv *priv = priv_n;
	struct qcom_ethqos *ethqos;

	if (!priv->plat->bsp_priv)
		return;

	ethqos = (struct qcom_ethqos *)priv->plat->bsp_priv;

	if (loopback_en) {
		rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
			      RGMII_CONFIG_LOOPBACK_EN,
			      RGMII_IO_MACRO_CONFIG);
		ETHQOSINFO("Loopback EN Enabled\n");
	} else {
		rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
			      0, RGMII_IO_MACRO_CONFIG);
		ETHQOSINFO("Loopback EN Disabled\n");
	}
}
static void rgmii_dump(struct qcom_ethqos *ethqos)
{
	dev_dbg(&ethqos->pdev->dev, "Rgmii register dump\n");
	dev_dbg(&ethqos->pdev->dev, "RGMII_IO_MACRO_CONFIG: %x\n",
		rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG));
	dev_dbg(&ethqos->pdev->dev, "SDCC_HC_REG_DLL_CONFIG: %x\n",
		rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG));
	dev_dbg(&ethqos->pdev->dev, "SDCC_HC_REG_DDR_CONFIG: %x\n",
		rgmii_readl(ethqos, SDCC_HC_REG_DDR_CONFIG));
	dev_dbg(&ethqos->pdev->dev, "SDCC_HC_REG_DLL_CONFIG2: %x\n",
		rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG2));
	dev_dbg(&ethqos->pdev->dev, "SDC4_STATUS: %x\n",
		rgmii_readl(ethqos, SDC4_STATUS));
	dev_dbg(&ethqos->pdev->dev, "SDCC_USR_CTL: %x\n",
		rgmii_readl(ethqos, SDCC_USR_CTL));
	dev_dbg(&ethqos->pdev->dev, "RGMII_IO_MACRO_CONFIG2: %x\n",
		rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG2));
	dev_dbg(&ethqos->pdev->dev, "RGMII_IO_MACRO_DEBUG1: %x\n",
		rgmii_readl(ethqos, RGMII_IO_MACRO_DEBUG1));
	dev_dbg(&ethqos->pdev->dev, "EMAC_SYSTEM_LOW_POWER_DEBUG: %x\n",
		rgmii_readl(ethqos, EMAC_SYSTEM_LOW_POWER_DEBUG));
}

/* Clock rates */
#define RGMII_1000_NOM_CLK_FREQ			(250 * 1000 * 1000UL)
#define RGMII_ID_MODE_100_LOW_SVS_CLK_FREQ	 (50 * 1000 * 1000UL)
#define RGMII_ID_MODE_10_LOW_SVS_CLK_FREQ	  (5 * 1000 * 1000UL)

static void
ethqos_update_rgmii_clk_and_bus_cfg(struct qcom_ethqos *ethqos,
				    unsigned int speed)
{
	switch (speed) {
	case SPEED_1000:
		ethqos->rgmii_clk_rate =  RGMII_1000_NOM_CLK_FREQ;
		break;

	case SPEED_100:
		ethqos->rgmii_clk_rate =  RGMII_ID_MODE_100_LOW_SVS_CLK_FREQ;
		break;

	case SPEED_10:
		ethqos->rgmii_clk_rate =  RGMII_ID_MODE_10_LOW_SVS_CLK_FREQ;
		break;
	}

	switch (speed) {
	case SPEED_1000:
		ethqos->vote_idx = VOTE_IDX_1000MBPS;
		break;
	case SPEED_100:
		ethqos->vote_idx = VOTE_IDX_100MBPS;
		break;
	case SPEED_10:
		ethqos->vote_idx = VOTE_IDX_10MBPS;
		break;
	case 0:
		ethqos->vote_idx = VOTE_IDX_0MBPS;
		ethqos->rgmii_clk_rate = 0;
		break;
	}
	clk_set_rate(ethqos->rgmii_clk, ethqos->rgmii_clk_rate);
}

static void ethqos_set_func_clk_en(struct qcom_ethqos *ethqos)
{
	rgmii_updatel(ethqos, RGMII_CONFIG_FUNC_CLK_EN,
		      RGMII_CONFIG_FUNC_CLK_EN, RGMII_IO_MACRO_CONFIG);
}

static int ethqos_dll_configure(struct qcom_ethqos *ethqos)
{
	/* Set CDR_EN */
	if (!ethqos->io_macro.config_cdr_en)
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CDR_EN,
			      0, SDCC_HC_REG_DLL_CONFIG);
	else
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CDR_EN,
			      SDCC_DLL_CONFIG_CDR_EN, SDCC_HC_REG_DLL_CONFIG);

	/* Set CDR_EXT_EN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CDR_EXT_EN,
		      SDCC_DLL_CONFIG_CDR_EXT_EN, SDCC_HC_REG_DLL_CONFIG);

	if (ethqos->io_macro.mclk_gating_en)
		rgmii_updatel(ethqos, SDCC_DLL_MCLK_GATING_EN,
			      SDCC_DLL_MCLK_GATING_EN, SDCC_HC_REG_DLL_CONFIG);
	else
		rgmii_updatel(ethqos, SDCC_DLL_MCLK_GATING_EN,
			      0, SDCC_HC_REG_DLL_CONFIG);

	if (ethqos->io_macro.cdr_fine_phase)
		rgmii_updatel(ethqos, SDCC_DLL_CDR_FINE_PHASE,
			      SDCC_DLL_CDR_FINE_PHASE, SDCC_HC_REG_DLL_CONFIG);
	else
		rgmii_updatel(ethqos, SDCC_DLL_CDR_FINE_PHASE,
			      0, SDCC_HC_REG_DLL_CONFIG);

	if (ethqos->io_macro.ddr_cal_en)
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_CAL_EN,
			      SDCC_DLL_CONFIG2_DDR_CAL_EN,
			      SDCC_HC_REG_DLL_CONFIG2);
	else
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_CAL_EN,
			      0, SDCC_HC_REG_DLL_CONFIG2);

	if (ethqos->io_macro.dll_clock_dis)
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DLL_CLOCK_DIS,
			      SDCC_DLL_CONFIG2_DLL_CLOCK_DIS,
			      SDCC_HC_REG_DLL_CONFIG2);
	else
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DLL_CLOCK_DIS,
			      0, SDCC_HC_REG_DLL_CONFIG2);

	if (ethqos->io_macro.mclk_freq_calc)
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_MCLK_FREQ_CALC,
			      ethqos->io_macro.mclk_freq_calc << 10,
			      SDCC_HC_REG_DLL_CONFIG2);

	if (ethqos->io_macro.ddr_traffic_init_sel)
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SEL,
			      SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SEL,
			      SDCC_HC_REG_DLL_CONFIG2);
	else
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SEL,
			      0, SDCC_HC_REG_DLL_CONFIG2);

	if (ethqos->io_macro.ddr_traffic_init_sw)
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SW,
			      SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SW,
			      SDCC_HC_REG_DLL_CONFIG2);
	else
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SW,
			      0, SDCC_HC_REG_DLL_CONFIG2);

	return 0;
}

void emac_rgmii_io_macro_config_1G(struct qcom_ethqos *ethqos)
{
	rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
		      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
		      0, RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
		      RGMII_CONFIG_POS_NEG_DATA_SEL,
		      RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
		      RGMII_CONFIG_PROG_SWAP, RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
		      0, RGMII_IO_MACRO_CONFIG2);

	if (plat_dat->c45_marvell_en)
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      0, RGMII_IO_MACRO_CONFIG2);
	else
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_IO_MACRO_CONFIG2);

	rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
		      0, RGMII_IO_MACRO_CONFIG2);
	rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
		      RGMII_CONFIG2_RX_PROG_SWAP,
		      RGMII_IO_MACRO_CONFIG2);

	/* Set PRG_RCLK_DLY to 115 */
	if (plat_dat->c45_marvell_en)
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_RCLK_DLY,
			      104, SDCC_HC_REG_DDR_CONFIG);
	else
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_RCLK_DLY,
			      115, SDCC_HC_REG_DDR_CONFIG);
	rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_DLY_EN,
		      SDCC_DDR_CONFIG_PRG_DLY_EN,
		      SDCC_HC_REG_DDR_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
		      0, RGMII_IO_MACRO_CONFIG);
}

void emac_rgmii_io_macro_config_100M(struct qcom_ethqos *ethqos)
{
	rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
		      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
		      RGMII_CONFIG_BYPASS_TX_ID_EN,
		      RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
		      0, RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
		      0, RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
		      0, RGMII_IO_MACRO_CONFIG2);

	if (plat_dat->c45_marvell_en)
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      0, RGMII_IO_MACRO_CONFIG2);
	else
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_IO_MACRO_CONFIG2);
	rgmii_updatel(ethqos, RGMII_CONFIG_MAX_SPD_PRG_2,
		      BIT(6), RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
		      0, RGMII_IO_MACRO_CONFIG2);
	rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
		      RGMII_CONFIG2_RX_PROG_SWAP,
		      RGMII_IO_MACRO_CONFIG2);

	/* Write 0x5 to PRG_RCLK_DLY_CODE */
	rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_CODE,
		      (BIT(29) | BIT(27)), SDCC_HC_REG_DDR_CONFIG);
	rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
		      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
		      SDCC_HC_REG_DDR_CONFIG);
	rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
		      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
		      SDCC_HC_REG_DDR_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
		      0, RGMII_IO_MACRO_CONFIG);
}

void emac_rgmii_io_macro_config_10M(struct qcom_ethqos *ethqos)
{
	rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
		      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
		      RGMII_CONFIG_BYPASS_TX_ID_EN,
		      RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
		      0, RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
		      0, RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
		      0, RGMII_IO_MACRO_CONFIG2);
	rgmii_updatel(ethqos,
		      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
		      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
		      RGMII_IO_MACRO_CONFIG2);
	rgmii_updatel(ethqos, RGMII_CONFIG_MAX_SPD_PRG_9,
		      BIT(12) | GENMASK(9, 8),
		      RGMII_IO_MACRO_CONFIG);
	rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
		      0, RGMII_IO_MACRO_CONFIG2);
	rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
		      RGMII_CONFIG2_RX_PROG_SWAP,
		      RGMII_IO_MACRO_CONFIG2);

	/* Write 0x5 to PRG_RCLK_DLY_CODE */
	rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_CODE,
		      (BIT(29) | BIT(27)), SDCC_HC_REG_DDR_CONFIG);
}

static int ethqos_rgmii_macro_init(struct qcom_ethqos *ethqos)
{
	/* Disable loopback mode */
	rgmii_updatel(ethqos, RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN,
		      0, RGMII_IO_MACRO_CONFIG2);

	/* Select RGMII, write 0 to interface select */
	rgmii_updatel(ethqos, RGMII_CONFIG_INTF_SEL,
		      0, RGMII_IO_MACRO_CONFIG);

	switch (ethqos->speed) {
	case SPEED_1000:
		rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
			      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
			      RGMII_CONFIG_POS_NEG_DATA_SEL,
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
			      RGMII_CONFIG_PROG_SWAP, RGMII_IO_MACRO_CONFIG);
		if (ethqos->io_macro.data_divide_clk_sel)
			rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
				      RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL, RGMII_IO_MACRO_CONFIG2);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
				      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
			      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
			      RGMII_CONFIG2_RX_PROG_SWAP,
			      RGMII_IO_MACRO_CONFIG2);

		if (ethqos->io_macro.tcx0_cycles_dly_line)
			rgmii_updatel(ethqos, SDCC_DDR_CONFIG_TCXO_CYCLES_DLY_LINE,
				      ethqos->io_macro.tcx0_cycles_dly_line << 12,
				      SDCC_HC_REG_DDR_CONFIG);

		if (ethqos->io_macro.tcx0_cycles_cnt)
			rgmii_updatel(ethqos, SDCC_DDR_CONFIG_TCXO_CYCLES_CNT,
				      ethqos->io_macro.tcx0_cycles_cnt << 9,
				      SDCC_HC_REG_DDR_CONFIG);

		if (ethqos->io_macro.prg_rclk_dly)
			rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_RCLK_DLY,
				      ethqos->io_macro.prg_rclk_dly,
				      SDCC_HC_REG_DDR_CONFIG);

		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_DLY_EN,
			      SDCC_DDR_CONFIG_PRG_DLY_EN,
			      SDCC_HC_REG_DDR_CONFIG);

		if (!ethqos->io_macro.loopback_en)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
		break;

	case SPEED_100:
		rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
			      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
			      0, RGMII_IO_MACRO_CONFIG);
		if (!ethqos->io_macro.data_divide_clk_sel)
			rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
				      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG_MAX_SPD_PRG_2,
			      BIT(6), RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
			      0, RGMII_IO_MACRO_CONFIG2);

		if (ethqos->io_macro.rx_prog_swap)
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_IO_MACRO_CONFIG2);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      0, RGMII_IO_MACRO_CONFIG2);
		/* Write 0x5 to PRG_RCLK_DLY_CODE */
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_CODE,
			      (BIT(29) | BIT(27)), SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_HC_REG_DDR_CONFIG);

		if (!ethqos->io_macro.loopback_en)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
		break;

	case SPEED_10:
		rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
			      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
			      0, RGMII_IO_MACRO_CONFIG);
		if (!ethqos->io_macro.data_divide_clk_sel)
			rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
				      0, RGMII_IO_MACRO_CONFIG2);

		if (ethqos->io_macro.tx_clk_phase_shift_en)
			rgmii_updatel(ethqos,
				      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
				      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
				      RGMII_IO_MACRO_CONFIG2);
		else
			rgmii_updatel(ethqos,
				      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
				      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG_MAX_SPD_PRG_9,
			      BIT(12) | GENMASK(9, 8),
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
			      0, RGMII_IO_MACRO_CONFIG2);

		if (ethqos->io_macro.rx_prog_swap)
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_IO_MACRO_CONFIG2);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      0, RGMII_IO_MACRO_CONFIG2);
		/* Write 0x5 to PRG_RCLK_DLY_CODE */
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_CODE,
			      (BIT(29) | BIT(27)), SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_HC_REG_DDR_CONFIG);

		if (!ethqos->io_macro.loopback_en)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
		break;
	default:
		dev_err(&ethqos->pdev->dev,
			"Invalid speed %d\n", ethqos->speed);
		return -EINVAL;
	}

	return 0;
}

static int ethqos_rgmii_macro_init_v3(struct qcom_ethqos *ethqos)
{
	/* Disable loopback mode */
	rgmii_updatel(ethqos, RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN,
		      0, RGMII_IO_MACRO_CONFIG2);

	/* Select RGMII, write 0 to interface select */
	rgmii_updatel(ethqos, RGMII_CONFIG_INTF_SEL,
		      0, RGMII_IO_MACRO_CONFIG);

	switch (ethqos->speed) {
	case SPEED_1000:
		emac_rgmii_io_macro_config_1G(ethqos);
		break;

	case SPEED_100:
		emac_rgmii_io_macro_config_100M(ethqos);
		break;

	case SPEED_10:
		emac_rgmii_io_macro_config_10M(ethqos);
		break;
	default:
		dev_err(&ethqos->pdev->dev,
			"Invalid speed %d\n", ethqos->speed);
		return -EINVAL;
	}

	return 0;
}

static int ethqos_configure(struct qcom_ethqos *ethqos)
{
	volatile unsigned int dll_lock;
	unsigned int i, retry = 1000;
	unsigned int val;

	/* Reset to POR values and enable clk */
	for (i = 0; i < ethqos->num_por; i++)
		rgmii_writel(ethqos, ethqos->por[i].value,
			     ethqos->por[i].offset);
	ethqos_set_func_clk_en(ethqos);

	if (ethqos->speed != SPEED_100 && ethqos->speed != SPEED_10) {
		/* Disable CK_OUT_EN */
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
			      0,
			      SDCC_HC_REG_DLL_CONFIG);

		/* Wait for CK_OUT_EN clear */
		do {
			val = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG);
			val &= SDCC_DLL_CONFIG_CK_OUT_EN;
			if (!val)
				break;
			usleep_range(1000, 1500);
			retry--;
		} while (retry > 0);
		if (!retry)
			dev_err(&ethqos->pdev->dev, "Clear CK_OUT_EN timedout\n");

			/* Set DLL_EN */
			rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_EN,
				      SDCC_DLL_CONFIG_DLL_EN, SDCC_HC_REG_DLL_CONFIG);
	}

	if (ethqos->speed == SPEED_1000) {
		ethqos_dll_configure(ethqos);

		if (ethqos->io_macro.test_ctl)
			rgmii_writel(ethqos, ethqos->io_macro.test_ctl,
				     SDCC_TEST_CTL_RGOFFADDR_OFFSET);

		if (ethqos->io_macro.usr_ctl)
			rgmii_writel(ethqos, ethqos->io_macro.usr_ctl,
				     SDCC_USR_CTL_RGOFFADDR_OFFSET);
		else
			rgmii_updatel(ethqos, GENMASK(26, 24), BIT(26),
				      SDCC_USR_CTL);
	}

	ethqos_rgmii_macro_init(ethqos);

	/* Set DLL_RST */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_RST,
		      SDCC_DLL_CONFIG_DLL_RST, SDCC_HC_REG_DLL_CONFIG);

	/* Set PDN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN,
		      SDCC_DLL_CONFIG_PDN, SDCC_HC_REG_DLL_CONFIG);

	if (ethqos->speed != SPEED_100 && ethqos->speed != SPEED_10)
		usleep_range(1000, 1500);

	/* Clear DLL_RST */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_RST, 0,
		      SDCC_HC_REG_DLL_CONFIG);

	/* Clear PDN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN, 0,
		      SDCC_HC_REG_DLL_CONFIG);

	if (ethqos->speed != SPEED_100 && ethqos->speed != SPEED_10)
		usleep_range(1000, 1500);

	if (ethqos->speed != SPEED_100 && ethqos->speed != SPEED_10) {
		/* Set CK_OUT_EN */
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
			      SDCC_DLL_CONFIG_CK_OUT_EN,
		      SDCC_HC_REG_DLL_CONFIG);

		/* Wait for CK_OUT_EN set */
		retry = 1000;
		do {
			val = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG);
			val &= SDCC_DLL_CONFIG_CK_OUT_EN;
			if (val)
				break;
			usleep_range(1000, 1500);
			retry--;
		} while (retry > 0);
		if (!retry)
			dev_err(&ethqos->pdev->dev, "Set CK_OUT_EN timedout\n");

		/* wait for DLL LOCK */
		retry = 1000;
		do {
			usleep_range(1000, 1500);
			dll_lock = rgmii_readl(ethqos, SDC4_STATUS);
			if (dll_lock & SDC4_STATUS_DLL_LOCK)
				break;
			retry--;
		} while (retry > 0);
		if (!retry)
			dev_err(&ethqos->pdev->dev,
				"Timeout while waiting for DLL lock\n");

		/* Disable CK_OUT_EN */
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
			      0,
			      SDCC_HC_REG_DLL_CONFIG);

		/* Wait for CK_OUT_EN clear */
		do {
			val = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG);
			val &= SDCC_DLL_CONFIG_CK_OUT_EN;
			if (!val)
				break;
			usleep_range(1000, 1500);
			retry--;
		} while (retry > 0);
		if (!retry)
			dev_err(&ethqos->pdev->dev, "Clear CK_OUT_EN timedout\n");
	}
	return 0;
}

/* for EMAC_HW_VER >= 3 */
static int ethqos_configure_mac_v3(struct qcom_ethqos *ethqos)
{
	unsigned int dll_lock;
	unsigned int i, retry = 1000;
	int ret = 0;
	/* Reset to POR values and enable clk */
	for (i = 0; i < ethqos->num_por; i++)
		rgmii_writel(ethqos, ethqos->por[i].value,
			     ethqos->por[i].offset);
	ethqos_set_func_clk_en(ethqos);

	/* Put DLL into Reset and Powerdown */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_RST,
		      SDCC_DLL_CONFIG_DLL_RST, SDCC_HC_REG_DLL_CONFIG);
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN,
		      SDCC_DLL_CONFIG_PDN, SDCC_HC_REG_DLL_CONFIG)
		;
	/*Power on and set DLL, Set->RST & PDN to '0' */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_RST,
		      0, SDCC_HC_REG_DLL_CONFIG);
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN,
		      0, SDCC_HC_REG_DLL_CONFIG);

	/* for 10 or 100Mbps further configuration not required */
	if (ethqos->speed == SPEED_1000) {
		/* Disable DLL output clock */
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
			      0, SDCC_HC_REG_DLL_CONFIG);

		/* Configure SDCC_DLL_TEST_CTRL */
		rgmii_writel(ethqos, HSR_SDCC_DLL_TEST_CTRL, SDCC_TEST_CTL);

		/* Configure SDCC_USR_CTRL */
		rgmii_writel(ethqos, HSR_SDCC_USR_CTRL, SDCC_USR_CTL);

		/* Configure DDR_CONFIG */
		rgmii_writel(ethqos, HSR_DDR_CONFIG, SDCC_HC_REG_DDR_CONFIG);

		/* Configure PRG_RCLK_DLY */
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_RCLK_DLY,
			      DDR_CONFIG_PRG_RCLK_DLY, SDCC_HC_REG_DDR_CONFIG);
		/*Enable PRG_RCLK_CLY */
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_DLY_EN,
			      SDCC_DDR_CONFIG_PRG_DLY_EN, SDCC_HC_REG_DDR_CONFIG);

		/* Configure DLL_CONFIG */
		rgmii_writel(ethqos, HSR_DLL_CONFIG, SDCC_HC_REG_DLL_CONFIG);

		/*Set -> DLL_CONFIG_2 MCLK_FREQ_CALC*/
		rgmii_writel(ethqos, HSR_DLL_CONFIG_2, SDCC_HC_REG_DLL_CONFIG2);

		/*Power Down and Reset DLL*/
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_RST,
			      SDCC_DLL_CONFIG_DLL_RST, SDCC_HC_REG_DLL_CONFIG);
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN,
			      SDCC_DLL_CONFIG_PDN, SDCC_HC_REG_DLL_CONFIG);

		/*wait for 52us*/
		usleep_range(52, 55);

		/*Power on and set DLL, Set->RST & PDN to '0' */
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_RST,
			      0, SDCC_HC_REG_DLL_CONFIG);
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN,
			      0, SDCC_HC_REG_DLL_CONFIG);

		/*Wait for 8000 input clock cycles, 8000 cycles of 100 MHz = 80us*/
		usleep_range(80, 85);

		/* Enable DLL output clock */
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
			      SDCC_DLL_CONFIG_CK_OUT_EN, SDCC_HC_REG_DLL_CONFIG);

		/* Check for DLL lock */
		do {
			udelay(1);
			dll_lock = rgmii_readl(ethqos, SDC4_STATUS);
			if (dll_lock & SDC4_STATUS_DLL_LOCK)
				break;
			retry--;
		} while (retry > 0);
		if (!retry)
			dev_err(&ethqos->pdev->dev,
				"Timeout while waiting for DLL lock\n");
	}

	/* DLL bypass mode for 10Mbps and 100Mbps
	 * 1.   Write 1 to PDN bit of SDCC_HC_REG_DLL_CONFIG register.
	 * 2.   Write 1 to bypass bit of SDCC_USR_CTL register
	 * 3.   Default value of this register is 0x00010800
	 */
	if (ethqos->speed == SPEED_10 || ethqos->speed == SPEED_100) {
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN,
			      SDCC_DLL_CONFIG_PDN, SDCC_HC_REG_DLL_CONFIG);
		rgmii_updatel(ethqos, DLL_BYPASS,
			      DLL_BYPASS, SDCC_USR_CTL);
	}

	ret = ethqos_rgmii_macro_init_v3(ethqos);

	return ret;
}

static void ethqos_fix_mac_speed(void *priv, unsigned int speed)
{
	struct qcom_ethqos *ethqos = priv;
	int ret = 0;

	ethqos->speed = speed;
	ethqos_update_rgmii_clk_and_bus_cfg(ethqos, speed);
	if (ethqos->emac_ver == EMAC_HW_v3_0_0_RG)
		ret = ethqos_configure_mac_v3(ethqos);
	else
		ethqos_configure(ethqos);
	if (ret != 0)
		ETHQOSERR("HSR configuration has failed\n");
}

int ethqos_mdio_read(struct stmmac_priv  *priv, int phyaddr, int phyreg)
{
	unsigned int mii_address = priv->hw->mii.addr;
	unsigned int mii_data = priv->hw->mii.data;
	u32 v;
	int data;
	u32 value = MII_BUSY;
	struct qcom_ethqos *ethqos = priv->plat->bsp_priv;

	if (ethqos->phy_state == PHY_IS_OFF) {
		ETHQOSINFO("Phy is in off state reading is not possible\n");
		return -EOPNOTSUPP;
	}

	value |= (phyaddr << priv->hw->mii.addr_shift)
		& priv->hw->mii.addr_mask;
	value |= (phyreg << priv->hw->mii.reg_shift) & priv->hw->mii.reg_mask;
	value |= (priv->clk_csr << priv->hw->mii.clk_csr_shift)
		& priv->hw->mii.clk_csr_mask;
	if (priv->plat->has_gmac4)
		value |= MII_GMAC4_READ;

	if (readl_poll_timeout(priv->ioaddr + mii_address, v, !(v & MII_BUSY),
			       100, 10000)) {
		if (priv->plat->handle_mac_err)
			priv->plat->handle_mac_err(priv, PHY_RW_ERR, 0);

		return -EBUSY;
	}

	writel_relaxed(value, priv->ioaddr + mii_address);

	if (readl_poll_timeout(priv->ioaddr + mii_address, v, !(v & MII_BUSY),
			       100, 10000)) {
		if (priv->plat->handle_mac_err)
			priv->plat->handle_mac_err(priv, PHY_RW_ERR, 0);

		return -EBUSY;
	}

	/* Read the data from the MII data register */
	data = (int)readl_relaxed(priv->ioaddr + mii_data);

	return data;
}

static int ethqos_phy_intr_config(struct qcom_ethqos *ethqos)
{
	int ret = 0;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	ethqos->phy_intr = platform_get_irq_byname(ethqos->pdev, "phy-intr");

	if (ethqos->phy_intr < 0) {
		if (ethqos->phy_intr != -EPROBE_DEFER) {
			dev_err(&ethqos->pdev->dev,
				"PHY IRQ configuration information not found\n");
		}
		ret = 1;
	} else {
		priv->phy_intr_wol_irq = ethqos->phy_intr;
	}

	return ret;
}

static void ethqos_handle_phy_interrupt(struct qcom_ethqos *ethqos)
{
	int phy_intr_status = 0;
	struct platform_device *pdev = ethqos->pdev;

	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	int micrel_intr_status = 0;

	if ((priv->phydev && (priv->phydev->phy_id &
	     priv->phydev->drv->phy_id_mask)
	     == MICREL_PHY_ID) ||
	    (priv->phydev && (priv->phydev->phy_id &
	     priv->phydev->drv->phy_id_mask)
	     == PHY_ID_KSZ9131)) {
		phy_intr_status = ethqos_mdio_read(priv,
						   priv->plat->phy_addr,
						   DWC_ETH_QOS_BASIC_STATUS);
		ETHQOSDBG("Basic Status Reg (%#x) = %#x\n",
			  DWC_ETH_QOS_BASIC_STATUS, phy_intr_status);
		micrel_intr_status = ethqos_mdio_read(priv,
						      priv->plat->phy_addr,
						      DWC_ETH_QOS_MICREL_PHY_INTCS);
		ETHQOSDBG("MICREL PHY Intr EN Reg (%#x) = %#x\n",
			  DWC_ETH_QOS_MICREL_PHY_INTCS, micrel_intr_status);

		/**
		 * Call ack interrupt to clear the WOL
		 * interrupt status fields
		 */
		if (priv->phydev->drv->ack_interrupt)
			priv->phydev->drv->ack_interrupt(priv->phydev);

		/* Interrupt received for link state change */
		if (phy_intr_status & LINK_STATE_MASK) {
			if (micrel_intr_status & MICREL_LINK_UP_INTR_STATUS)
				ETHQOSDBG("Intr for link UP state\n");
			phy_mac_interrupt(priv->phydev);
		} else if (!(phy_intr_status & LINK_STATE_MASK)) {
			ETHQOSDBG("Intr for link DOWN state\n");
			phy_mac_interrupt(priv->phydev);
		} else if (!(phy_intr_status & AUTONEG_STATE_MASK)) {
			ETHQOSDBG("Intr for link down with auto-neg err\n");
		}
	} else {
		phy_intr_status =
		 ethqos_mdio_read(priv, priv->plat->phy_addr,
				  DWC_ETH_QOS_PHY_INTR_STATUS);

		if (!priv->plat->mac2mac_en) {
			if (phy_intr_status & LINK_UP_STATE)
				phy_mac_interrupt(priv->phydev);
			else if (phy_intr_status & LINK_DOWN_STATE)
				phy_mac_interrupt(priv->phydev);
		}
	}
}

static void ethqos_defer_phy_isr_work(struct work_struct *work)
{
	struct qcom_ethqos *ethqos =
		container_of(work, struct qcom_ethqos, emac_phy_work);

	if (ethqos->clks_suspended)
		wait_for_completion(&ethqos->clk_enable_done);

	ethqos_handle_phy_interrupt(ethqos);
}

static irqreturn_t ETHQOS_PHY_ISR(int irq, void *dev_data)
{
	struct qcom_ethqos *ethqos = (struct qcom_ethqos *)dev_data;

	pm_wakeup_event(&ethqos->pdev->dev, 5000);

	queue_work(system_wq, &ethqos->emac_phy_work);
	return IRQ_HANDLED;
}

static void ethqos_phy_irq_enable(void *priv_n)
{
	struct stmmac_priv *priv = priv_n;
	struct qcom_ethqos *ethqos = priv->plat->bsp_priv;

	if (ethqos->phy_intr) {
		ETHQOSINFO("enabling irq = %d\n", priv->phy_irq_enabled);
		enable_irq(ethqos->phy_intr);
		priv->phy_irq_enabled = true;
	}
}

static void ethqos_phy_irq_disable(void *priv_n)
{
	struct stmmac_priv *priv = priv_n;
	struct qcom_ethqos *ethqos = priv->plat->bsp_priv;

	if (ethqos->phy_intr) {
		ETHQOSINFO("disabling irq = %d\n", priv->phy_irq_enabled);
		disable_irq(ethqos->phy_intr);
		priv->phy_irq_enabled = false;
	}
}

static int ethqos_phy_intr_enable(void *priv_n)
{
	int ret = 0;
	struct stmmac_priv *priv = priv_n;
	struct qcom_ethqos *ethqos = priv->plat->bsp_priv;

	if (ethqos_phy_intr_config(ethqos)) {
		ret = 1;
		return ret;
	}

	INIT_WORK(&ethqos->emac_phy_work, ethqos_defer_phy_isr_work);
	init_completion(&ethqos->clk_enable_done);

	ret = request_irq(ethqos->phy_intr, ETHQOS_PHY_ISR,
			  IRQF_SHARED, "stmmac", ethqos);
	if (ret) {
		ETHQOSERR("Unable to register PHY IRQ %d\n",
			  ethqos->phy_intr);
		return ret;
	}
	priv->plat->phy_intr_en_extn_stm = true;
	priv->phy_irq_enabled = true;
	return ret;
}

static const struct of_device_id qcom_ethqos_match[] = {
	{ .compatible = "qcom,qcs404-ethqos",},
	{ .compatible = "qcom,sdxprairie-ethqos",},
	{ .compatible = "qcom,stmmac-ethqos", },
	{ .compatible = "qcom,emac-smmu-embedded", },
	{ }
};

static void emac_emb_smmu_exit(void)
{
	emac_emb_smmu_ctx.valid = false;
	emac_emb_smmu_ctx.pdev_master = NULL;
	emac_emb_smmu_ctx.smmu_pdev = NULL;
	emac_emb_smmu_ctx.iommu_domain = NULL;
}

static int emac_emb_smmu_cb_probe(struct platform_device *pdev,
				  struct plat_stmmacenet_data *plat_dat)
{
	int result = 0;
	u32 iova_ap_mapping[2];
	struct device *dev = &pdev->dev;
	const char *str = NULL;

	ETHQOSDBG("EMAC EMB SMMU CB probe: smmu pdev=%p\n", pdev);
	result = of_property_read_string(dev->of_node, "qcom,iommu-dma", &str);

	if (result == 0 && !strcmp(str, "bypass")) {
		ETHQOSINFO("iommu-dma-addr-pool not required in bypass mode\n");
	} else {
		result = of_property_read_u32_array(dev->of_node,
						    "qcom,iommu-dma-addr-pool",
						    iova_ap_mapping,
						    ARRAY_SIZE(iova_ap_mapping));
		if (result) {
			ETHQOSERR("Failed to read EMB start/size iova addresses\n");
			return result;
		}
	}

	emac_emb_smmu_ctx.smmu_pdev = pdev;

	if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
	    dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
		ETHQOSERR("DMA set 32bit mask failed\n");
		return -EOPNOTSUPP;
	}

	emac_emb_smmu_ctx.valid = true;

	emac_emb_smmu_ctx.iommu_domain =
		iommu_get_domain_for_dev(&emac_emb_smmu_ctx.smmu_pdev->dev);

	ETHQOSINFO("Successfully attached to IOMMU\n");
	plat_dat->stmmac_emb_smmu_ctx = emac_emb_smmu_ctx;

	if (pethqos && pethqos->early_eth_enabled) {
		ETHQOSINFO("interface up after smmu probe\n");
		queue_work(system_wq, &pethqos->early_eth);
	} else {
		open_not_called = 1;
		ETHQOSINFO("interface up not done by smmu\n");
	}
	if (emac_emb_smmu_ctx.pdev_master)
		goto smmu_probe_done;

smmu_probe_done:
	emac_emb_smmu_ctx.ret = result;
	return result;
}

static void ethqos_pps_irq_config(struct qcom_ethqos *ethqos)
{
	ethqos->pps_class_a_irq =
	platform_get_irq_byname(ethqos->pdev, "ptp_pps_irq_0");
	if (ethqos->pps_class_a_irq < 0) {
		if (ethqos->pps_class_a_irq != -EPROBE_DEFER)
			ETHQOSERR("class_a_irq config info not found\n");
	}
	ethqos->pps_class_b_irq =
	platform_get_irq_byname(ethqos->pdev, "ptp_pps_irq_1");
	if (ethqos->pps_class_b_irq < 0) {
		if (ethqos->pps_class_b_irq != -EPROBE_DEFER)
			ETHQOSERR("class_b_irq config info not found\n");
	}
}

static void qcom_ethqos_phy_suspend_clks(struct qcom_ethqos *ethqos)
{
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);

	ETHQOSINFO("Enter\n");

	if (priv->plat->phy_intr_en_extn_stm)
		reinit_completion(&ethqos->clk_enable_done);

	ethqos->clks_suspended = 1;

	ethqos_update_rgmii_clk_and_bus_cfg(ethqos, 0);

	if (ethqos->phy_wol_supported || !priv->plat->clks_suspended) {
		if (priv->plat->stmmac_clk)
			clk_disable_unprepare(priv->plat->stmmac_clk);

		if (priv->plat->pclk)
			clk_disable_unprepare(priv->plat->pclk);

		if (priv->plat->clk_ptp_ref)
			clk_disable_unprepare(priv->plat->clk_ptp_ref);
		priv->plat->clks_suspended = true;
	}
	if (ethqos->rgmii_clk)
		clk_disable_unprepare(ethqos->rgmii_clk);

	ETHQOSINFO("Exit\n");
}

inline void *qcom_ethqos_get_priv(struct qcom_ethqos *ethqos)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	return priv;
}

static int ethqos_writeback_desc_rec(struct stmmac_priv *priv, int chan)
{
	return -EOPNOTSUPP;
}

static int ethqos_reset_phy_rec(struct stmmac_priv *priv, int int_en)
{
	struct qcom_ethqos *ethqos =   priv->plat->bsp_priv;

	if (int_en && (priv->phydev &&
		       priv->phydev->autoneg == AUTONEG_DISABLE)) {
		ethqos->backup_autoneg = priv->phydev->autoneg;
		ethqos->backup_bmcr = ethqos_mdio_read(priv,
						       priv->plat->phy_addr,
						       MII_BMCR);
	} else {
		ethqos->backup_autoneg = AUTONEG_ENABLE;
	}

	if (int_en) {
		rtnl_lock();
		phylink_stop(priv->phylink);
		phylink_disconnect_phy(priv->phylink);
		rtnl_unlock();
	}
	ethqos_phy_power_off(ethqos);

	ethqos_phy_power_on(ethqos);

	if (int_en) {
		ethqos_reset_phy_enable_interrupt(ethqos);
		rtnl_lock();
		phylink_start(priv->phylink);
		rtnl_unlock();
		if (ethqos->backup_autoneg == AUTONEG_DISABLE && priv->phydev) {
			priv->phydev->autoneg = ethqos->backup_autoneg;
			phy_write(priv->phydev, MII_BMCR, ethqos->backup_bmcr);
		}
	}

	return 1;
}

static int ethqos_tx_clean_rec(struct stmmac_priv *priv, int chan)
{
	struct stmmac_tx_queue *tx_q = &priv->tx_queue[chan];

	if (!tx_q->skip_sw)
		stmmac_tx_clean(priv, DMA_TX_SIZE, chan);
	return 1;
}

static int ethqos_reset_tx_dma_rec(struct stmmac_priv *priv, int chan)
{
	stmmac_tx_err(priv, chan);
	return 1;
}

static int ethqos_schedule_poll(struct stmmac_priv *priv, int chan)
{
	struct stmmac_rx_queue *rx_q = &priv->rx_queue[chan];
	struct stmmac_channel *ch;

	ch = &priv->channel[chan];

	if (!rx_q->skip_sw) {
		if (likely(napi_schedule_prep(&ch->rx_napi))) {
			priv->hw->dma->disable_dma_irq(priv->ioaddr, chan);
			__napi_schedule(&ch->rx_napi);
		}
	}
	return 1;
}

static void ethqos_tdu_rec_wq(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct stmmac_priv *priv;
	struct qcom_ethqos *ethqos;
	int ret;

	ETHQOSDBG("Enter\n");
	dwork = container_of(work, struct delayed_work, work);
	ethqos = container_of(dwork, struct qcom_ethqos, tdu_rec);

	priv = qcom_ethqos_get_priv(ethqos);
	if (!priv)
		return;

	ret = ethqos_tx_clean_rec(priv, ethqos->tdu_chan);
	if (!ret)
		return;

	ethqos->tdu_scheduled = false;
}

static void ethqos_mac_rec_init(struct qcom_ethqos *ethqos)
{
	int threshold[] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10};
	int en[] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
	int i;

	for (i = 0; i < MAC_ERR_CNT; i++) {
		ethqos->mac_rec_threshold[i] = threshold[i];
		ethqos->mac_rec_en[i] = en[i];
		ethqos->mac_err_cnt[i] = 0;
		ethqos->mac_rec_cnt[i] = 0;
	}
	INIT_DELAYED_WORK(&ethqos->tdu_rec,
			  ethqos_tdu_rec_wq);
}

static int dwmac_qcom_handle_mac_err(void *pdata, int type, int chan)
{
	int ret = 1;
	struct qcom_ethqos *ethqos;
	struct stmmac_priv *priv;

	if (!pdata)
		return -EINVAL;
	priv = pdata;
	ethqos = priv->plat->bsp_priv;

	if (!ethqos)
		return -EINVAL;

	ethqos->mac_err_cnt[type]++;

	if (ethqos->mac_rec_en[type]) {
		if (ethqos->mac_rec_cnt[type] >=
		    ethqos->mac_rec_threshold[type]) {
			ETHQOSERR("exceeded recovery threshold for %s",
				  err_names[type]);
			ethqos->mac_rec_en[type] = false;
			ret = 0;
		} else {
			ethqos->mac_rec_cnt[type]++;
			switch (type) {
			case PHY_RW_ERR:
			{
				ret = ethqos_reset_phy_rec(priv, true);
				if (!ret) {
					ETHQOSERR("recovery failed for %s",
						  err_names[type]);
					ethqos->mac_rec_fail[type] = true;
				}
			}
			break;
			case PHY_DET_ERR:
			{
				ret = ethqos_reset_phy_rec(priv, false);
				if (!ret) {
					ETHQOSERR("recovery failed for %s",
						  err_names[type]);
					ethqos->mac_rec_fail[type] = true;
				}
			}
			break;
			case FBE_ERR:
			{
				ret = ethqos_reset_tx_dma_rec(priv, chan);
				if (!ret) {
					ETHQOSERR("recovery failed for %s",
						  err_names[type]);
					ethqos->mac_rec_fail[type] = true;
				}
			}
			break;
			case RBU_ERR:
			{
				ret = ethqos_schedule_poll(priv, chan);
				if (!ret) {
					ETHQOSERR("recovery failed for %s",
						  err_names[type]);
					ethqos->mac_rec_fail[type] = true;
				}
			}
			break;
			case TDU_ERR:
			{
				if (!ethqos->tdu_scheduled) {
					ethqos->tdu_chan = chan;
					schedule_delayed_work
					(&ethqos->tdu_rec,
					 msecs_to_jiffies(3000));
					ethqos->tdu_scheduled = true;
				}
			}
			break;
			case CRC_ERR:
			case RECEIVE_ERR:
			case OVERFLOW_ERR:
			case DRIBBLE_ERR:
			case WDT_ERR:
			{
				ret = ethqos_writeback_desc_rec(priv, chan);
				if (!ret) {
					ETHQOSERR("recovery failed for %s",
						  err_names[type]);
					ethqos->mac_rec_fail[type] = true;
				}
			}
			break;
			default:
			break;
			}
		}
	}
	mac_rec_wq_flag = true;
	wake_up_interruptible(&mac_rec_wq);

	return ret;
}

inline bool qcom_ethqos_is_phy_link_up(struct qcom_ethqos *ethqos)
{
	/* PHY driver initializes phydev->link=1.
	 * So, phydev->link is 1 even on bootup with no PHY connected.
	 * phydev->link is valid only after adjust_link is called once.
	 */
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);

	if (priv->plat->mac2mac_en) {
		return priv->plat->mac2mac_link;
	} else {
		return (priv->dev->phydev &&
			priv->dev->phydev->link);
	}
}

static void qcom_ethqos_phy_resume_clks(struct qcom_ethqos *ethqos)
{
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);

	ETHQOSINFO("Enter\n");

	if (ethqos->phy_wol_supported ||
	    ethqos->current_phy_mode == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		if (priv->plat->stmmac_clk)
			clk_prepare_enable(priv->plat->stmmac_clk);

		if (priv->plat->pclk)
			clk_prepare_enable(priv->plat->pclk);

		if (priv->plat->clk_ptp_ref)
			clk_prepare_enable(priv->plat->clk_ptp_ref);

		priv->plat->clks_suspended = false;
	}

	if (ethqos->rgmii_clk)
		clk_prepare_enable(ethqos->rgmii_clk);

	if (qcom_ethqos_is_phy_link_up(ethqos))
		ethqos_update_rgmii_clk_and_bus_cfg(ethqos, ethqos->speed);
	else
		ethqos_update_rgmii_clk_and_bus_cfg(ethqos, SPEED_10);

	ethqos->clks_suspended = 0;

	if (priv->plat->phy_intr_en_extn_stm)
		complete_all(&ethqos->clk_enable_done);

	ETHQOSINFO("Exit\n");
}

void qcom_ethqos_request_phy_wol(void *plat_n)
{
	struct plat_stmmacenet_data *plat = plat_n;
	struct qcom_ethqos *ethqos;
	struct stmmac_priv *priv;
	int ret = 0;

	if (!plat)
		return;

	ethqos = plat->bsp_priv;
	priv = qcom_ethqos_get_priv(ethqos);

	if (!priv || !priv->en_wol)
		return;

	/* Check if phydev is valid*/
	/* Check and enable Wake-on-LAN functionality in PHY*/
	if (priv->phydev) {
		struct ethtool_wolinfo wol = {.cmd = ETHTOOL_GWOL};
		phy_ethtool_get_wol(priv->phydev, &wol);

		wol.cmd = ETHTOOL_SWOL;
		wol.wolopts = wol.supported;
		ret = phy_ethtool_set_wol(priv->phydev, &wol);

		if (ret) {
			ETHQOSERR("set wol in PHY failed\n");
			return;
		}

		if (ret == EOPNOTSUPP) {
			ETHQOSERR("WOL not supported\n");
			return;
		}

		device_set_wakeup_capable(priv->device, 1);

		enable_irq_wake(ethqos->phy_intr);
		device_set_wakeup_enable(&ethqos->pdev->dev, 1);
	}
}

static void read_rgmii_io_macro_node_setting(struct device_node *np_hw, struct qcom_ethqos *ethqos)
{
	int ret = 0;
	unsigned int rclk_dly_read_ps;

	ret = of_property_read_u32(np_hw, "prg-rclk-dly",
				   &rclk_dly_read_ps);
	if (!ret && rclk_dly_read_ps) {
		ethqos->io_macro.prg_rclk_dly =
		(RGMII_PRG_RCLK_CONST * 1000) / rclk_dly_read_ps;
	} else {
		ethqos->io_macro.prg_rclk_dly = 57;
	}

	ret = of_property_read_u32(np_hw,
				   "config-cdr-en",
				   &ethqos->io_macro.config_cdr_en);
	if (ret) {
		ETHQOSDBG("default config_cdr_en\n");
		ethqos->io_macro.config_cdr_en = 1;
	}

	ret = of_property_read_u32(np_hw,
				   "mclk-gating-en",
				   &ethqos->io_macro.mclk_gating_en);
	if (ret) {
		ETHQOSDBG("default mclk_gating_en\n");
		ethqos->io_macro.mclk_gating_en = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "cdr-fine-phase",
				   &ethqos->io_macro.cdr_fine_phase);
	if (ret) {
		ETHQOSDBG("default cdr_fine_phase\n");
		ethqos->io_macro.cdr_fine_phase = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "skip-calc-traffic",
				   &ethqos->io_macro.skip_calc_traffic);
	if (ret) {
		ETHQOSDBG("default skip_calc_traffic\n");
		ethqos->io_macro.skip_calc_traffic = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "data-divide-clk-sel",
				   &ethqos->io_macro.data_divide_clk_sel);
	if (ret) {
		ETHQOSDBG("default data_divide_clk_sel\n");
		ethqos->io_macro.data_divide_clk_sel = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "loopback-en",
				   &ethqos->io_macro.loopback_en);
	if (ret) {
		ETHQOSDBG("default loopback_en\n");
		ethqos->io_macro.loopback_en = 1;
	}

	ret = of_property_read_u32(np_hw,
				   "rx-prog-swap",
				   &ethqos->io_macro.rx_prog_swap);
	if (ret) {
		ETHQOSDBG("default rx_prog_swap\n");
		ethqos->io_macro.rx_prog_swap = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "tx-clk-phase-shift-en",
				   &ethqos->io_macro.tx_clk_phase_shift_en);
	if (ret) {
		ETHQOSDBG("default tx_clk_phase_shift_en\n");
		ethqos->io_macro.tx_clk_phase_shift_en = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "dll-clock-dis",
				   &ethqos->io_macro.dll_clock_dis);
	if (ret) {
		ETHQOSDBG("default dll_clock_dis\n");
		ethqos->io_macro.dll_clock_dis = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "mclk-freq-calc",
				   &ethqos->io_macro.mclk_freq_calc);
	if (ret) {
		ETHQOSDBG("default mclk_freq_calc\n");
		ethqos->io_macro.mclk_freq_calc = 0x1A;
	}

	ret = of_property_read_u32(np_hw,
				   "ddr-traffic-init-sel",
				   &ethqos->io_macro.ddr_traffic_init_sel);
	if (ret) {
		ETHQOSDBG("default ddr_traffic_init_sel\n");
		ethqos->io_macro.ddr_traffic_init_sel = 1;
	}

	ret = of_property_read_u32(np_hw,
				   "ddr-traffic-init-sw",
				   &ethqos->io_macro.ddr_traffic_init_sw);
	if (ret) {
		ETHQOSDBG("default ddr_traffic_init_sw\n");
		ethqos->io_macro.ddr_traffic_init_sw = 1;
	}

	ret = of_property_read_u32(np_hw,
				   "ddr-cal-en",
				   &ethqos->io_macro.ddr_cal_en);
	if (ret) {
		ETHQOSDBG("default ddr_cal_en\n");
		ethqos->io_macro.ddr_cal_en = 1;
	}

	ret = of_property_read_u32(np_hw,
				   "tcx0-cycles-dly-line",
				   &ethqos->io_macro.tcx0_cycles_dly_line);
	if (ret) {
		ETHQOSDBG("default tcx0_cycles_dly_line\n");
		ethqos->io_macro.tcx0_cycles_dly_line = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "tcx0-cycles-cnt",
				   &ethqos->io_macro.tcx0_cycles_cnt);
	if (ret) {
		ETHQOSDBG("default tcx0_cycles_cnt\n");
		ethqos->io_macro.tcx0_cycles_cnt = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "test-ctl",
				   &ethqos->io_macro.test_ctl);
	if (ret) {
		ETHQOSDBG("default test_ctl\n");
		ethqos->io_macro.test_ctl = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "usr-ctl",
				   &ethqos->io_macro.usr_ctl);
	if (ret) {
		ETHQOSDBG("default usr_ctl\n");
		ethqos->io_macro.usr_ctl = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "pps-create",
				   &ethqos->io_macro.pps_create);
	if (ret) {
		ETHQOSDBG("default pps_create\n");
		ethqos->io_macro.pps_create = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "pps-remove",
				   &ethqos->io_macro.pps_remove);
	if (ret) {
		ETHQOSDBG("default pps_remove\n");
		ethqos->io_macro.pps_remove = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "l3-master-dev",
				   &ethqos->io_macro.l3_master_dev);
	if (ret) {
		ETHQOSDBG("default l3_master_dev\n");
		ethqos->io_macro.l3_master_dev = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "ipv6-wq",
				   &ethqos->io_macro.ipv6_wq);
	if (ret) {
		ETHQOSDBG("default ipv6_wq\n");
		ethqos->io_macro.ipv6_wq = 0;
	}

	ret = of_property_read_u32(np_hw,
				   "rgmii-tx-drv",
				   &ethqos->io_macro.rgmii_tx_drv);
	if (ret) {
		ETHQOSDBG("default rgmii_tx_drv\n");
		ethqos->io_macro.rgmii_tx_drv = 0;
	}
}

static void qcom_ethqos_bringup_iface(struct work_struct *work)
{
	struct platform_device *pdev = NULL;
	struct net_device *ndev = NULL;
	struct qcom_ethqos *ethqos =
		container_of(work, struct qcom_ethqos, early_eth);

	ETHQOSINFO("entry\n");
	if (!ethqos)
		return;
	pdev = ethqos->pdev;
	if (!pdev)
		return;
	ndev = platform_get_drvdata(pdev);
	if (!ndev || netif_running(ndev))
		return;
	rtnl_lock();
	if (dev_change_flags(ndev, ndev->flags | IFF_UP, NULL) < 0)
		ETHQOSINFO("ERROR\n");
	rtnl_unlock();
	ETHQOSINFO("exit\n");
}

static void read_mac_addr_from_fuse_reg(struct device_node *np)
{
	int ret, i, count, x;
	u32 mac_efuse_prop, efuse_size = 8;
	u64 mac_addr;

	/* If the property doesn't exist or empty return */
	count = of_property_count_u32_elems(np, "mac-efuse-addr");
	if (!count || count < 0)
		return;

	/* Loop over all addresses given until we get valid address */
	for (x = 0; x < count; x++) {
		void __iomem *mac_efuse_addr;

		ret = of_property_read_u32_index(np, "mac-efuse-addr",
						 x, &mac_efuse_prop);
		if (!ret) {
			mac_efuse_addr = ioremap(mac_efuse_prop, efuse_size);
			if (!mac_efuse_addr)
				continue;

			mac_addr = readq(mac_efuse_addr);
			ETHQOSINFO("Mac address read: %llx\n", mac_addr);

			/* create byte array out of value read from efuse */
			for (i = 0; i < ETH_ALEN ; i++) {
				pparams.mac_addr[ETH_ALEN - 1 - i] =
					mac_addr & 0xff;
				mac_addr = mac_addr >> 8;
			}

			iounmap(mac_efuse_addr);

			/* if valid address is found set cookie & return */
			pparams.is_valid_mac_addr =
				is_valid_ether_addr(pparams.mac_addr);
			if (pparams.is_valid_mac_addr)
				return;
		}
	}
}

static void qcom_ethqos_handle_ssr_workqueue(struct work_struct *work)
{
	struct stmmac_priv *priv = NULL;

	priv = qcom_ethqos_get_priv(pethqos);

	ETHQOSINFO("%s is executing action: %d\n", __func__, pethqos->action);

	if (priv->hw_offload_enabled) {
		if (pethqos->action == EVENT_REMOTE_STATUS_DOWN) {
			ethqos_ipa_offload_event_handler(NULL, EV_IPA_SSR_DOWN);
		} else {
			if (pethqos->action == EVENT_REMOTE_STATUS_UP)
				ethqos_ipa_offload_event_handler(NULL, EV_IPA_SSR_UP);
		}
	}
}

static int qcom_ethqos_qti_alert(struct notifier_block *nb,
				 unsigned long action, void *dev)
{
	struct stmmac_priv *priv = NULL;

	priv = qcom_ethqos_get_priv(pethqos);
	if (!priv) {
		ETHQOSERR("Unable to alert QTI of SSR status: %s\n", __func__);
		return NOTIFY_DONE;
	}

	switch (action) {
	case EVENT_REMOTE_STATUS_UP:
		ETHQOSINFO("Link up\n");
		pethqos->action = EVENT_REMOTE_STATUS_UP;
		break;
	case EVENT_REMOTE_STATUS_DOWN:
		ETHQOSINFO("Link down\n");
		pethqos->action = EVENT_REMOTE_STATUS_DOWN;
		break;
	default:
		ETHQOSERR("Invalid action passed: %s, %d\n", __func__,
			  action);
		return NOTIFY_DONE;
	}

	INIT_WORK(&pethqos->eth_ssr, qcom_ethqos_handle_ssr_workqueue);
	queue_work(system_wq, &pethqos->eth_ssr);

	return NOTIFY_DONE;
}

static void qcom_ethqos_register_listener(void)
{
	int ret;

	ETHQOSINFO("Registering sb notification listener: %s\n", __func__);

	pethqos->qti_nb.notifier_call = qcom_ethqos_qti_alert;
	ret = sb_register_evt_listener(&pethqos->qti_nb);
	if (ret)
		ETHQOSERR("sb_register_evt_listener failed at: %s\n", __func__);
}

static void ethqos_is_ipv4_NW_stack_ready(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct qcom_ethqos *ethqos;
	struct platform_device *pdev = NULL;
	struct net_device *ndev = NULL;
	int ret;

	ETHQOSDBG("\n");
	dwork = container_of(work, struct delayed_work, work);
	ethqos = container_of(dwork, struct qcom_ethqos, ipv4_addr_assign_wq);

	if (!ethqos)
		return;

	pdev = ethqos->pdev;

	if (!pdev)
		return;

	ndev = platform_get_drvdata(pdev);

	ret = qcom_ethqos_add_ipaddr(&pparams, ndev);
	if (ret)
		return;

	cancel_delayed_work_sync(&ethqos->ipv4_addr_assign_wq);
	flush_delayed_work(&ethqos->ipv4_addr_assign_wq);
}

#ifdef CONFIG_IPV6
static void ethqos_is_ipv6_NW_stack_ready(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct qcom_ethqos *ethqos;
	struct platform_device *pdev = NULL;
	struct net_device *ndev = NULL;
	int ret;

	ETHQOSDBG("\n");
	dwork = container_of(work, struct delayed_work, work);
	ethqos = container_of(dwork, struct qcom_ethqos, ipv6_addr_assign_wq);

	if (!ethqos)
		return;

	pdev = ethqos->pdev;

	if (!pdev)
		return;

	ndev = platform_get_drvdata(pdev);

	ret = qcom_ethqos_add_ipv6addr(&pparams, ndev);
	if (ret)
		return;

	cancel_delayed_work_sync(&ethqos->ipv6_addr_assign_wq);
	flush_delayed_work(&ethqos->ipv6_addr_assign_wq);
}
#endif

static void ethqos_set_early_eth_param(struct stmmac_priv *priv,
				       struct qcom_ethqos *ethqos)
{
	int ret = 0;

	if (priv->plat && priv->plat->mdio_bus_data)
		priv->plat->mdio_bus_data->phy_mask =
		 priv->plat->mdio_bus_data->phy_mask | DUPLEX_FULL | SPEED_100;


	if (pparams.is_valid_ipv4_addr) {
		INIT_DELAYED_WORK(&ethqos->ipv4_addr_assign_wq,
				  ethqos_is_ipv4_NW_stack_ready);
		schedule_delayed_work(&ethqos->ipv4_addr_assign_wq, 0);
	}

#ifdef CONFIG_IPV6
	if (pparams.is_valid_ipv6_addr) {
		INIT_DELAYED_WORK(&ethqos->ipv6_addr_assign_wq,
				  ethqos_is_ipv6_NW_stack_ready);
		if (ethqos->io_macro.ipv6_wq) {
			schedule_delayed_work(&ethqos->ipv6_addr_assign_wq,
					      msecs_to_jiffies(1000));
		} else {
			ret = qcom_ethqos_add_ipv6addr(&pparams, priv->dev);
			if (ret)
				schedule_delayed_work(&ethqos->ipv6_addr_assign_wq,
						      msecs_to_jiffies(1000));
		}
	}
#endif
	return;
}

static ssize_t read_phy_reg_dump(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct qcom_ethqos *ethqos = file->private_data;
	struct platform_device *pdev;
	struct net_device *dev;
	struct stmmac_priv *priv;
	unsigned int len = 0, buf_len = 2000;
	char *buf;
	ssize_t ret_cnt;
	int phydata = 0;
	int i = 0;

	if (!ethqos) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}

	if (ethqos->phy_state == PHY_IS_OFF) {
		ETHQOSINFO("Phy is in off state phy dump is not possible\n");
		return -EOPNOTSUPP;
	}
	pdev = ethqos->pdev;
	dev = platform_get_drvdata(pdev);
	priv = netdev_priv(dev);

	if (!dev->phydev) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len,
					 "\n************* PHY Reg dump *************\n");

	for (i = 0; i < 32; i++) {
		phydata = ethqos_mdio_read(priv, priv->plat->phy_addr, i);
		len += scnprintf(buf + len, buf_len - len,
					 "MII Register (%#x) = %#x\n",
					 i, phydata);
	}

	if (len > buf_len) {
		ETHQOSERR("(len > buf_len) buffer not sufficient\n");
		len = buf_len;
	}

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static ssize_t read_rgmii_reg_dump(struct file *file,
				   char __user *user_buf, size_t count,
				   loff_t *ppos)
{
	struct qcom_ethqos *ethqos = file->private_data;
	unsigned int len = 0, buf_len = 2000;
	char *buf;
	ssize_t ret_cnt;
	int rgmii_data = 0;

	if (!ethqos) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len,
					 "\n************* RGMII Reg dump *************\n");
	rgmii_data = rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG);
	len += scnprintf(buf + len, buf_len - len,
					 "RGMII_IO_MACRO_CONFIG Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_HC_REG_DLL_CONFIG Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_HC_REG_DDR_CONFIG);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_HC_REG_DDR_CONFIG Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG2);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_HC_REG_DLL_CONFIG2 Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDC4_STATUS);
	len += scnprintf(buf + len, buf_len - len,
					 "SDC4_STATUS Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_TEST_CTL);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_TEST_CTL Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_USR_CTL);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_USR_CTL Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG2);
	len += scnprintf(buf + len, buf_len - len,
					 "RGMII_IO_MACRO_CONFIG2 Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, RGMII_IO_MACRO_DEBUG1);
	len += scnprintf(buf + len, buf_len - len,
					 "RGMII_IO_MACRO_DEBUG1 Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, EMAC_SYSTEM_LOW_POWER_DEBUG);
	len += scnprintf(buf + len, buf_len - len,
					 "EMAC_SYSTEM_LOW_POWER_DEBUG Register = %#x\n",
					 rgmii_data);

	if (len > buf_len) {
		ETHQOSERR("(len > buf_len) buffer not sufficient\n");
		len = buf_len;
	}

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static ssize_t read_phy_off(struct device *dev,
			    struct device_attribute *attr,
			    char *user_buf)
{
	struct net_device *netdev = to_net_dev(dev);
	struct stmmac_priv *priv;
	struct qcom_ethqos *ethqos;

	if (!netdev) {
		ETHQOSERR("netdev is NULL\n");
		return -EINVAL;
	}

	priv = netdev_priv(netdev);
	if (!priv) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}

	ethqos = priv->plat->bsp_priv;
	if (!ethqos) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}
	if (ethqos->current_phy_mode == DISABLE_PHY_IMMEDIATELY)
		return scnprintf(user_buf, BUFF_SZ,
				"Disable phy immediately enabled\n");
	else if (ethqos->current_phy_mode == ENABLE_PHY_IMMEDIATELY)
		return scnprintf(user_buf, BUFF_SZ,
				 "Enable phy immediately enabled\n");
	else if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY) {
		return scnprintf(user_buf, BUFF_SZ,
			"%s %s",
			"Disable Phy at suspend\n",
			" & do not enable at resume enabled\n");
	} else if (ethqos->current_phy_mode == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		return scnprintf(user_buf, BUFF_SZ,
			"%s %s",
			"Disable Phy at suspend\n",
			" & enable at resume enabled\n");
	} else if (ethqos->current_phy_mode == DISABLE_PHY_ON_OFF)
		return scnprintf(user_buf, BUFF_SZ,
				 "Disable phy on/off disabled\n");

	return scnprintf(user_buf, BUFF_SZ,
						"Invalid Phy State\n");
}

static ssize_t phy_off_config(struct device *dev, struct device_attribute *attr,
			      const char *user_buf, size_t count)
{
	s8 config = 0;
	struct net_device *net_dev = to_net_dev(dev);
	struct stmmac_priv *priv;
	struct plat_stmmacenet_data *plat;
	struct qcom_ethqos *ethqos;

	if (!net_dev) {
		ETHQOSERR("net_dev is NULL\n");
		return -EINVAL;
	}

	priv = netdev_priv(net_dev);
	if (!priv) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}

	ethqos = priv->plat->bsp_priv;
	if (!ethqos) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}

	plat = priv->plat;
	if (kstrtos8(user_buf, 0, &config)) {
		ETHQOSERR("Error in reading option from user\n");
		return -EINVAL;
	}

	if (config > DISABLE_PHY_ON_OFF || config < DISABLE_PHY_IMMEDIATELY) {
		ETHQOSERR("Invalid option =%d", config);
		return -EINVAL;
	}
	if (config == ethqos->current_phy_mode) {
		ETHQOSERR("No effect as duplicate config");
		return -EPERM;
	}
	if (config == DISABLE_PHY_IMMEDIATELY) {
		ethqos->current_phy_mode = DISABLE_PHY_IMMEDIATELY;
	//make phy off
		if (priv->current_loopback == ENABLE_PHY_LOOPBACK) {
			/* If Phy loopback is enabled
			 *  Disabled It before phy off
			 */
			phy_digital_loopback_config(ethqos,
						    ethqos->loopback_speed, 0);
			ETHQOSDBG("Disable phy Loopback\n");
			priv->current_loopback = ENABLE_PHY_LOOPBACK;
		}
		/*Backup phy related data*/
		if (priv->phydev && priv->phydev->autoneg == AUTONEG_DISABLE) {
			ethqos->backup_autoneg = priv->phydev->autoneg;
			ethqos->backup_bmcr = ethqos_mdio_read(priv,
							       plat->phy_addr,
							       MII_BMCR);
		} else {
			ethqos->backup_autoneg = AUTONEG_ENABLE;
		}
		if (priv->phydev) {
			if (qcom_ethqos_is_phy_link_up(ethqos)) {
				ETHQOSINFO("Post Link down before PHY off\n");
				netif_carrier_off(net_dev);
				phy_mac_interrupt(priv->phydev);
			}
		}
		ethqos_phy_power_off(ethqos);
		plat->is_phy_off = true;
	} else if (config == ENABLE_PHY_IMMEDIATELY) {
		ethqos->current_phy_mode = ENABLE_PHY_IMMEDIATELY;
		//make phy on
		ethqos_phy_power_on(ethqos);
		ethqos_reset_phy_enable_interrupt(ethqos);

		if (ethqos->backup_autoneg == AUTONEG_DISABLE) {
			priv->phydev->autoneg = ethqos->backup_autoneg;
			phy_write(priv->phydev, MII_BMCR, ethqos->backup_bmcr);
		}
		if (priv->current_loopback == ENABLE_PHY_LOOPBACK) {
			/*If Phy loopback is enabled , enabled It again*/
			phy_digital_loopback_config(ethqos,
						    ethqos->loopback_speed, 1);
			ETHQOSDBG("Enabling Phy loopback again");
		}
		plat->is_phy_off = false;
	} else if (config == DISABLE_PHY_AT_SUSPEND_ONLY) {
		ethqos->current_phy_mode = DISABLE_PHY_AT_SUSPEND_ONLY;
		plat->is_phy_off = true;
	} else if (config == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		ethqos->current_phy_mode = DISABLE_PHY_SUSPEND_ENABLE_RESUME;
		plat->is_phy_off = true;
	} else if (config == DISABLE_PHY_ON_OFF) {
		ethqos->current_phy_mode = DISABLE_PHY_ON_OFF;
		plat->is_phy_off = false;
	} else {
		ETHQOSERR("Invalid option\n");
		return -EINVAL;
	}
	return count;
}

static void ethqos_rgmii_io_macro_loopback(struct qcom_ethqos *ethqos, int mode)
{
	/* Set loopback mode */
	if (mode == 1) {
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN,
			      RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN,
			      RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
			      0, RGMII_IO_MACRO_CONFIG2);
	} else {
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN,
			      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
			      RGMII_CONFIG2_RX_PROG_SWAP,
			      RGMII_IO_MACRO_CONFIG2);
	}
}

static void ethqos_mac_loopback(struct qcom_ethqos *ethqos, int mode)
{
	u32 read_value = (u32)readl_relaxed(ethqos->ioaddr + MAC_CONFIGURATION);
	/* Set loopback mode */
	if (mode == 1)
		read_value |= MAC_LM;
	else
		read_value &= ~MAC_LM;
	writel_relaxed(read_value, ethqos->ioaddr + MAC_CONFIGURATION);
}

static int phy_digital_loopback_config(struct qcom_ethqos *ethqos, int speed, int config)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	int phydata = 0;

	if (config == 1) {
		ETHQOSINFO("Request for phy digital loopback enable\n");
		switch (speed) {
		case SPEED_1000:
			phydata = PHY_LOOPBACK_1000;
			break;
		case SPEED_100:
			phydata = PHY_LOOPBACK_100;
			break;
		case SPEED_10:
			phydata = PHY_LOOPBACK_10;
			break;
		default:
			ETHQOSERR("Invalid link speed\n");
			break;
		}
	} else if (config == 0) {
		ETHQOSINFO("Request for phy digital loopback disable\n");
		if (ethqos->bmcr_backup)
			phydata = ethqos->bmcr_backup;
		else
			phydata = 0x1140;
	} else {
		ETHQOSERR("Invalid option\n");
		return -EINVAL;
	}
	if (phydata != 0) {
		if (priv->phydev) {
			phy_write(priv->phydev, MII_BMCR, phydata);
			ETHQOSINFO("write done for phy loopback\n");
		} else {
			ETHQOSINFO("Phy dev is NULL\n");
		}
	}
	return 0;
}

static void print_loopback_detail(enum loopback_mode loopback)
{
	switch (loopback) {
	case DISABLE_LOOPBACK:
		ETHQOSINFO("Loopback is disabled\n");
		break;
	case ENABLE_IO_MACRO_LOOPBACK:
		ETHQOSINFO("Loopback is Enabled as IO MACRO LOOPBACK\n");
		break;
	case ENABLE_MAC_LOOPBACK:
		ETHQOSINFO("Loopback is Enabled as MAC LOOPBACK\n");
		break;
	case ENABLE_PHY_LOOPBACK:
		ETHQOSINFO("Loopback is Enabled as PHY LOOPBACK\n");
		break;
	default:
		ETHQOSINFO("Invalid Loopback=%d\n", loopback);
		break;
	}
}

static void setup_config_registers(struct qcom_ethqos *ethqos,
				   int speed, int duplex, int mode)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	u32 ctrl = 0;

	ETHQOSDBG("Speed=%d,dupex=%d,mode=%d\n", speed, duplex, mode);

	if (mode > DISABLE_LOOPBACK && !qcom_ethqos_is_phy_link_up(ethqos)) {
		/*If Link is Down & need to enable Loopback*/
		ETHQOSDBG("Enable Lower Up Flag & disable phy dev\n");
		ETHQOSDBG("IRQ so that Rx/Tx can happen beforeee Link down\n");
		netif_carrier_on(dev);
		/*Disable phy interrupt by Link/Down by cable plug in/out*/
		priv->plat->phy_irq_disable(priv);
	} else if (mode > DISABLE_LOOPBACK &&
			qcom_ethqos_is_phy_link_up(ethqos)) {
		ETHQOSDBG("Only disable phy irqqq Lin is UP\n");
		/*Since link is up no need to set Lower UP flag*/
		/*Disable phy interrupt by Link/Down by cable plug in/out*/
		priv->plat->phy_irq_disable(priv);
	} else if (mode == DISABLE_LOOPBACK &&
		!qcom_ethqos_is_phy_link_up(ethqos)) {
		ETHQOSDBG("Disable Lower Up as Link is down\n");
		netif_carrier_off(dev);
		priv->plat->phy_irq_enable(priv);
	} else if (mode == DISABLE_LOOPBACK &&
		qcom_ethqos_is_phy_link_up(ethqos)) {
		priv->plat->phy_irq_enable(priv);
	}
	ETHQOSDBG("Old ctrl=%d  dupex full\n", ctrl);
	ctrl = readl_relaxed(priv->ioaddr + MAC_CTRL_REG);
		ETHQOSDBG("Old ctrl=0x%x with mask with flow control\n", ctrl);

	ctrl |= priv->hw->link.duplex;
	priv->dev->phydev->duplex = duplex;
	ctrl &= ~priv->hw->link.speed_mask;
	switch (speed) {
	case SPEED_1000:
		ctrl |= priv->hw->link.speed1000;
		break;
	case SPEED_100:
		ctrl |= priv->hw->link.speed100;
		break;
	case SPEED_10:
		ctrl |= priv->hw->link.speed10;
		break;
	default:
		speed = SPEED_UNKNOWN;
		ETHQOSDBG("unkwon speed\n");
		break;
	}
	writel_relaxed(ctrl, priv->ioaddr + MAC_CTRL_REG);
	ETHQOSDBG("New ctrl=%x priv hw speeed =%d\n", ctrl,
		  priv->hw->link.speed1000);
	priv->dev->phydev->speed = speed;
	priv->speed  = speed;

	if (!ethqos->susp_ipa_offload) {
		if (mode > DISABLE_LOOPBACK && pethqos->ipa_enabled)
			priv->hw->mac->map_mtl_to_dma(priv->hw, EMAC_QUEUE_0,
						      EMAC_CHANNEL_1);

		else
			priv->hw->mac->map_mtl_to_dma(priv->hw, EMAC_QUEUE_0,
						      EMAC_CHANNEL_0);
	}

	if (priv->dev->phydev->speed != SPEED_UNKNOWN)
		ethqos_fix_mac_speed(ethqos, speed);

	if (mode > DISABLE_LOOPBACK) {
		if (mode == ENABLE_MAC_LOOPBACK ||
		    mode == ENABLE_IO_MACRO_LOOPBACK)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
	} else if (mode == DISABLE_LOOPBACK) {
		if (ethqos->emac_ver == EMAC_HW_v2_3_2 ||
		    ethqos->emac_ver == EMAC_HW_v2_1_2)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
	}
	ETHQOSERR("End\n");
}

static ssize_t loopback_handling_config(struct file *file, const char __user *user_buffer,
					size_t count, loff_t *position)
{
	char *in_buf;
	int buf_len = 2000;
	unsigned long ret;
	int config = 0;
	struct qcom_ethqos *ethqos = file->private_data;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	int speed = 0;

	in_buf = kzalloc(buf_len, GFP_KERNEL);
	if (!in_buf)
		return -ENOMEM;

	ret = copy_from_user(in_buf, user_buffer, buf_len);
	if (ret) {
		ETHQOSERR("unable to copy from user\n");
		return -EFAULT;
	}

	ret = sscanf(in_buf, "%d %d", &config,  &speed);
	if (config > DISABLE_LOOPBACK && ret != 2) {
		ETHQOSERR("Speed is also needed while enabling loopback\n");
		return -EINVAL;
	}
	if (config < DISABLE_LOOPBACK || config > ENABLE_PHY_LOOPBACK) {
		ETHQOSERR("Invalid config =%d\n", config);
		return -EINVAL;
	}

	if (priv->current_loopback == ENABLE_PHY_LOOPBACK &&
	    priv->plat->mac2mac_en) {
		ETHQOSINFO("Not supported with Mac2Mac enabled\n");
		return -EOPNOTSUPP;
	}

	if (!priv->dev->phydev)
		return -EOPNOTSUPP;

	if ((config == ENABLE_PHY_LOOPBACK  || priv->current_loopback ==
			ENABLE_PHY_LOOPBACK) &&
			ethqos->current_phy_mode == DISABLE_PHY_IMMEDIATELY) {
		ETHQOSERR("Can't enabled/disable ");
		ETHQOSERR("phy loopback when phy is off\n");
		return -EPERM;
	}

	/*Argument validation*/
	if (config == ENABLE_IO_MACRO_LOOPBACK ||
	    config == ENABLE_MAC_LOOPBACK || config == ENABLE_PHY_LOOPBACK) {
		if (speed != SPEED_1000 && speed != SPEED_100 &&
		    speed != SPEED_10)
			return -EINVAL;
	}

	if (config == priv->current_loopback) {
		switch (config) {
		case DISABLE_LOOPBACK:
			ETHQOSINFO("Loopback is already disabled\n");
			break;
		case ENABLE_IO_MACRO_LOOPBACK:
			ETHQOSINFO("Loopback is already Enabled as ");
			ETHQOSINFO("IO MACRO LOOPBACK\n");
			break;
		case ENABLE_MAC_LOOPBACK:
			ETHQOSINFO("Loopback is already Enabled as ");
			ETHQOSINFO("MAC LOOPBACK\n");
			break;
		case ENABLE_PHY_LOOPBACK:
			ETHQOSINFO("Loopback is already Enabled as ");
			ETHQOSINFO("PHY LOOPBACK\n");
			break;
		}
		return -EINVAL;
	}
	/*If request to enable loopback & some other loopback already enabled*/
	if (config != DISABLE_LOOPBACK &&
	    priv->current_loopback > DISABLE_LOOPBACK) {
		ETHQOSINFO("Loopback is already enabled\n");
		print_loopback_detail(priv->current_loopback);
		return -EINVAL;
	}
	ETHQOSINFO("enable loopback = %d with link speed = %d backup now\n",
		   config, speed);

	/*Backup speed & duplex before Enabling Loopback */
	if (priv->current_loopback == DISABLE_LOOPBACK &&
	    config > DISABLE_LOOPBACK) {
		/*Backup old speed & duplex*/
		ethqos->backup_speed = priv->speed;
		ethqos->backup_duplex = priv->dev->phydev->duplex;
	}
	/*Backup BMCR before Enabling Phy LoopbackLoopback */
	if (priv->current_loopback == DISABLE_LOOPBACK &&
	    config == ENABLE_PHY_LOOPBACK)
		ethqos->bmcr_backup = ethqos_mdio_read(priv,
						       priv->plat->phy_addr,
						       MII_BMCR);

	if (config == DISABLE_LOOPBACK)
		setup_config_registers(ethqos, ethqos->backup_speed,
				       ethqos->backup_duplex, 0);
	else
		setup_config_registers(ethqos, speed, DUPLEX_FULL, config);

	switch (config) {
	case DISABLE_LOOPBACK:
		ETHQOSINFO("Request to Disable Loopback\n");
		if (priv->current_loopback == ENABLE_IO_MACRO_LOOPBACK)
			ethqos_rgmii_io_macro_loopback(ethqos, 0);
		else if (priv->current_loopback == ENABLE_MAC_LOOPBACK)
			ethqos_mac_loopback(ethqos, 0);
		else if (priv->current_loopback == ENABLE_PHY_LOOPBACK)
			phy_digital_loopback_config(ethqos,
						    ethqos->backup_speed, 0);
		break;
	case ENABLE_IO_MACRO_LOOPBACK:
		ETHQOSINFO("Request to Enable IO MACRO LOOPBACK\n");
		ethqos_rgmii_io_macro_loopback(ethqos, 1);
		break;
	case ENABLE_MAC_LOOPBACK:
		ETHQOSINFO("Request to Enable MAC LOOPBACK\n");
		ethqos_mac_loopback(ethqos, 1);
		break;
	case ENABLE_PHY_LOOPBACK:
		ETHQOSINFO("Request to Enable PHY LOOPBACK\n");
		ethqos->loopback_speed = speed;
		phy_digital_loopback_config(ethqos, speed, 1);
		break;
	default:
		ETHQOSINFO("Invalid Loopback=%d\n", config);
		break;
	}

	priv->current_loopback = config;
	kfree(in_buf);
	return count;
}

static ssize_t read_loopback_config(struct file *file,
				    char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	unsigned int len = 0, buf_len = 2000;
	struct qcom_ethqos *ethqos = file->private_data;
	struct platform_device *pdev;
	struct net_device *dev;
	struct stmmac_priv *priv;
	char *buf;
	ssize_t ret_cnt;

	if (!ethqos) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}
	pdev = ethqos->pdev;
	dev = platform_get_drvdata(pdev);
	priv = netdev_priv(dev);

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (priv->current_loopback == DISABLE_LOOPBACK)
		len += scnprintf(buf + len, buf_len - len,
				 "Loopback is Disabled\n");
	else if (priv->current_loopback == ENABLE_IO_MACRO_LOOPBACK)
		len += scnprintf(buf + len, buf_len - len,
				 "Current Loopback is IO MACRO LOOPBACK\n");
	else if (priv->current_loopback == ENABLE_MAC_LOOPBACK)
		len += scnprintf(buf + len, buf_len - len,
				 "Current Loopback is MAC LOOPBACK\n");
	else if (priv->current_loopback == ENABLE_PHY_LOOPBACK)
		len += scnprintf(buf + len, buf_len - len,
				 "Current Loopback is PHY LOOPBACK\n");
	else
		len += scnprintf(buf + len, buf_len - len,
				 "Invalid LOOPBACK Config\n");
	if (len > buf_len)
		len = buf_len;

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static const struct file_operations fops_phy_reg_dump = {
	.read = read_phy_reg_dump,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_rgmii_reg_dump = {
	.read = read_rgmii_reg_dump,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_loopback_config = {
	.read = read_loopback_config,
	.write = loopback_handling_config,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t read_mac_recovery_enable(struct file *filp, char __user *usr_buf,
					size_t count, loff_t *f_pos)
{
	char *buf;
	unsigned int len = 0, buf_len = 6000;
	ssize_t ret_cnt;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "PHY_RW_rec", pethqos->mac_rec_en[PHY_RW_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "PHY_DET_rec", pethqos->mac_rec_en[PHY_DET_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "CRC_rec", pethqos->mac_rec_en[CRC_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "RECEIVE_rec", pethqos->mac_rec_en[RECEIVE_ERR]);
		len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "OVERFLOW_rec", pethqos->mac_rec_en[OVERFLOW_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "FBE_rec", pethqos->mac_rec_en[FBE_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "RBU_rec", pethqos->mac_rec_en[RBU_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "TDU_rec", pethqos->mac_rec_en[TDU_ERR]);
		len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "DRIBBLE_rec", pethqos->mac_rec_en[DRIBBLE_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "WDT_rec", pethqos->mac_rec_en[WDT_ERR]);

	if (len > buf_len)
		len = buf_len;

	ETHQOSDBG("%s", buf);
	ret_cnt = simple_read_from_buffer(usr_buf, count, f_pos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static ssize_t ethqos_mac_recovery_enable(struct file *file,
					  const char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	unsigned char in_buf[15] = {0};
	int i, ret;
	struct qcom_ethqos *ethqos = pethqos;

	if (sizeof(in_buf) < count) {
		ETHQOSERR("emac string is too long - count=%u\n", count);
		return -EFAULT;
	}

	memset(in_buf, 0,  sizeof(in_buf));
	ret = copy_from_user(in_buf, user_buf, count);

	for (i = 0; i < MAC_ERR_CNT; i++) {
		if (in_buf[i] == '1')
			ethqos->mac_rec_en[i] = true;
		else
			ethqos->mac_rec_en[i] = false;
	}
	return count;
}

static ssize_t ethqos_test_mac_recovery(struct file *file,
					const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	unsigned char in_buf[4] = {0};
	int ret, err, chan;
	struct qcom_ethqos *ethqos = pethqos;

	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);

	if (sizeof(in_buf) < count) {
		ETHQOSERR("emac string is too long - count=%u\n", count);
		return -EFAULT;
	}

	memset(in_buf, 0,  sizeof(in_buf));
	ret = copy_from_user(in_buf, user_buf, count);

	err = in_buf[0] - '0';

	chan = in_buf[2] - '0';

	if (err < 0 || err > 9) {
		ETHQOSERR("Invalid error\n");
		return -EFAULT;
	}
	if (err != PHY_DET_ERR && err != PHY_RW_ERR) {
		if (chan < 0 || chan >= priv->plat->tx_queues_to_use) {
			ETHQOSERR("Invalid channel\n");
			return -EFAULT;
		}
	}
	if (priv->plat->handle_mac_err)
		priv->plat->handle_mac_err(priv, err, chan);

	return count;
}

static const struct file_operations fops_mac_rec = {
	.read = read_mac_recovery_enable,
	.write = ethqos_test_mac_recovery,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static DEVICE_ATTR(phy_off, PHY_OFF_SYSFS_DEV_ATTR_PERMS, read_phy_off, phy_off_config);

static int ethqos_phy_off_remove_sysfs(struct qcom_ethqos *ethqos)
{
	struct net_device *net_dev;

	if (!ethqos) {
		ETHQOSERR("ethqos is NULL\n");
		return -EINVAL;
	}
	net_dev = platform_get_drvdata(ethqos->pdev);
	if (!net_dev) {
		ETHQOSERR("netdev is NULL\n");
		return -EINVAL;
	}

	sysfs_remove_file(&net_dev->dev.kobj,
			  &dev_attr_phy_off.attr);
	return 0;
}

static int ethqos_phy_off_sysfs(struct qcom_ethqos *ethqos)
{
	int ret;
	struct net_device *net_dev;

	if (!ethqos) {
		ETHQOSERR("ethqos is NULL\n");
		return -EINVAL;
	}

	net_dev = platform_get_drvdata(ethqos->pdev);
	if (!net_dev) {
		ETHQOSERR("netdev is NULL\n");
		return -EINVAL;
	}

	ret = sysfs_create_file(&net_dev->dev.kobj,
				&dev_attr_phy_off.attr);
	if (ret) {
		ETHQOSERR("unable to create phy_off sysfs node\n");
		goto fail;
	}
	return ret;

fail:
	return ethqos_phy_off_remove_sysfs(ethqos);
}

#ifdef CONFIG_DEBUG_FS
static int ethqos_create_debugfs(struct qcom_ethqos        *ethqos)
{
	static struct dentry *phy_reg_dump;
	static struct dentry *rgmii_reg_dump;
	static struct dentry *loopback_enable_mode;
	static struct dentry *mac_rec;
	struct stmmac_priv *priv;

	if (!ethqos) {
		ETHQOSERR("Null Param\n");
		return -ENOMEM;
	}

	priv = qcom_ethqos_get_priv(ethqos);
	ethqos->debugfs_dir = debugfs_create_dir("eth", NULL);

	if (!ethqos->debugfs_dir || IS_ERR(ethqos->debugfs_dir)) {
		ETHQOSERR("Can't create debugfs dir\n");
		return -ENOMEM;
	}

	phy_reg_dump = debugfs_create_file("phy_reg_dump", 0400,
					   ethqos->debugfs_dir, ethqos,
					   &fops_phy_reg_dump);
	if (!phy_reg_dump || IS_ERR(phy_reg_dump)) {
		ETHQOSERR("Can't create phy_dump %p\n", phy_reg_dump);
		goto fail;
	}

	rgmii_reg_dump = debugfs_create_file("rgmii_reg_dump", 0400,
					     ethqos->debugfs_dir, ethqos,
					     &fops_rgmii_reg_dump);
	if (!rgmii_reg_dump || IS_ERR(rgmii_reg_dump)) {
		ETHQOSERR("Can't create rgmii_dump %p\n", rgmii_reg_dump);
		goto fail;
	}

	mac_rec = debugfs_create_file("test_mac_recovery", 0400,
				      ethqos->debugfs_dir, ethqos,
				      &fops_mac_rec);
	if (!mac_rec || IS_ERR(mac_rec)) {
		ETHQOSERR("Can't create mac_rec directory");
		goto fail;
	}

	loopback_enable_mode = debugfs_create_file("loopback_enable_mode", 0400,
						   ethqos->debugfs_dir, ethqos,
						   &fops_loopback_config);
	if (!loopback_enable_mode || IS_ERR(loopback_enable_mode)) {
		ETHQOSERR("Can't create loopback_enable_mode %x\n",
			  loopback_enable_mode);
		goto fail;
	}

	return 0;

fail:
	debugfs_remove_recursive(ethqos->debugfs_dir);
	return -ENOMEM;
}

static int ethqos_cleanup_debugfs(struct qcom_ethqos *ethqos)
{
	if (!ethqos) {
		ETHQOSERR("Null Param");
		return -ENODEV;
	}

	debugfs_remove_recursive(ethqos->debugfs_dir);
	ethqos->debugfs_dir = NULL;

	ETHQOSDBG("debugfs Deleted Successfully");
	return 0;
}
#endif

static void ethqos_emac_mem_base(struct qcom_ethqos *ethqos)
{
	struct resource *resource = NULL;
	int ret = 0;

	resource = platform_get_resource(ethqos->pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		ETHQOSERR("get emac-base resource failed\n");
		ret = -ENODEV;
		return;
	}
	ethqos->emac_mem_base = resource->start;
	ethqos->emac_mem_size = resource_size(resource);
}

static u32 l3mdev_fib_table1(const struct net_device *dev)
{
	return RT_TABLE_LOCAL;
}

static const struct l3mdev_ops l3mdev_op1 = {.l3mdev_fib_table = l3mdev_fib_table1};

static inline u32 qcom_ethqos_rgmii_io_macro_num_of_regs(u32 emac_hw_version)
{
	switch (emac_hw_version) {
	case EMAC_HW_v2_1_1:
	case EMAC_HW_v2_1_2:
	case EMAC_HW_v2_3_1:
		return 27;
	case EMAC_HW_v2_3_0:
		return 28;
	case EMAC_HW_v2_3_2:
		return 29;
	case EMAC_HW_NONE:
	default:
		return 0;
	}
}

static int qcom_ethqos_init_panic_notifier(struct qcom_ethqos *ethqos)
{
	u32 size_iomacro_regs;
	int ret = 0;

	if (pethqos) {
		size_iomacro_regs =
		qcom_ethqos_rgmii_io_macro_num_of_regs(pethqos->emac_ver) * 4;

		pethqos->emac_reg_base_address =
			kzalloc(pethqos->emac_mem_size, GFP_KERNEL);

		if (!pethqos->emac_reg_base_address)
			ret = -ENOMEM;

		pethqos->rgmii_reg_base_address =
			kzalloc(size_iomacro_regs, GFP_KERNEL);

		if (!pethqos->rgmii_reg_base_address)
			ret = -ENOMEM;
	}
	return ret;
}

static int qcom_ethqos_panic_notifier(struct notifier_block *this,
				      unsigned long event, void *ptr)
{
	u32 size_iomacro_regs;
	struct stmmac_priv *priv = NULL;

	if (pethqos) {
		priv = qcom_ethqos_get_priv(pethqos);

		pr_info("qcom-ethqos: ethqos 0x%p\n", pethqos);
		pr_info("qcom-ethqos: stmmac_priv 0x%p\n", priv);

		pethqos->iommu_domain = priv->plat->stmmac_emb_smmu_ctx.iommu_domain;

		pr_info("qcom-ethqos: emac iommu domain 0x%p\n",
			pethqos->iommu_domain);
		pr_info("qcom-ethqos: emac register mem 0x%p\n",
			pethqos->emac_reg_base_address);

		if (pethqos->emac_reg_base_address)
			memcpy_fromio(pethqos->emac_reg_base_address,
				      pethqos->ioaddr,
				      pethqos->emac_mem_size);

		pr_info("qcom-ethqos: rgmii register mem 0x%p\n",
			pethqos->rgmii_reg_base_address);

		size_iomacro_regs =
		qcom_ethqos_rgmii_io_macro_num_of_regs(pethqos->emac_ver) * 4;

		if (pethqos->rgmii_reg_base_address)
			memcpy(pethqos->rgmii_reg_base_address,
			       __io_virt(pethqos->rgmii_base),
			       size_iomacro_regs);
	}
	return NOTIFY_DONE;
}

static struct notifier_block qcom_ethqos_panic_blk = {
	.notifier_call  = qcom_ethqos_panic_notifier,
};

static int ethqos_update_mdio_drv_strength(struct qcom_ethqos *ethqos,
					   struct device_node *np)
{
	u32 mdio_drv_str[2];
	struct resource *resource = NULL;
	unsigned long tlmm_central_base = 0;
	unsigned long tlmm_central_size = 0;
	int ret = 0;
	unsigned long v;
	size_t size = 4;

	resource = platform_get_resource_byname(ethqos->pdev,
						IORESOURCE_MEM, "tlmm-central-base");

	if (!resource) {
		ETHQOSERR("Resource tlmm-central-base not found\n");
		goto err_out;
	}

	tlmm_central_base = resource->start;
	tlmm_central_size = resource_size(resource);
	ETHQOSDBG("tlmm_central_base = 0x%x, size = 0x%x\n",
		  tlmm_central_base, tlmm_central_size);

	tlmm_mdc_mdio_hdrv_pull_ctl_base = ioremap(tlmm_central_base +
			TLMM_MDC_MDIO_HDRV_PULL_CTL_ADDRESS_OFFSET,
			size);
	if (!tlmm_mdc_mdio_hdrv_pull_ctl_base) {
		ETHQOSERR("cannot map dwc_tlmm_central reg memory, aborting\n");
		ret = -EIO;
		goto err_out;
	}

	if (np && !of_property_read_u32(np, "mdio-drv-str",
					&mdio_drv_str[0])) {
		switch (mdio_drv_str[0]) {
		case 2:
			mdio_drv_str[0] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_2MA;
			break;
		case 4:
			mdio_drv_str[0] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_4MA;
			break;
		case 6:
			mdio_drv_str[0] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_6MA;
			break;
		case 8:
			mdio_drv_str[0] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_8MA;
			break;
		case 10:
			mdio_drv_str[0] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_10MA;
			break;
		case 12:
			mdio_drv_str[0] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_12MA;
			break;
		case 14:
			mdio_drv_str[0] = TLMM_RGMII_HDRV_PL_CTL1_TX_HDRV_14MA;
			break;
		case 16:
			mdio_drv_str[0] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA;
			break;
		default:
			mdio_drv_str[0] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA;
			break;
		}

		TLMM_MDC_MDIO_HDRV_PULL_CTL_RGRD(v);
		v = (v & (unsigned long)(0xFFFFFFF8))
		 | (((mdio_drv_str[0]) & ((unsigned long)(0x7))) << 0);
		TLMM_MDC_MDIO_HDRV_PULL_CTL_RGWR(v);
	}

	if (np && !of_property_read_u32(np, "mdc-drv-str",
					&mdio_drv_str[1])) {
		switch (mdio_drv_str[1]) {
		case 2:
			mdio_drv_str[1] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_2MA;
			break;
		case 4:
			mdio_drv_str[1] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_4MA;
			break;
		case 6:
			mdio_drv_str[1] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_6MA;
			break;
		case 8:
			mdio_drv_str[1] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_8MA;
			break;
		case 10:
			mdio_drv_str[1] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_10MA;
			break;
		case 12:
			mdio_drv_str[1] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_12MA;
			break;
		case 14:
			mdio_drv_str[1] = TLMM_RGMII_HDRV_PL_CTL1_TX_HDRV_14MA;
			break;
		case 16:
			mdio_drv_str[1] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA;
			break;
		default:
			mdio_drv_str[1] = TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA;
			break;
		}

		TLMM_MDC_MDIO_HDRV_PULL_CTL_RGRD(v);
		v = (v & (unsigned long)(0xFFFFFF1F))
		 | (((mdio_drv_str[1]) & (unsigned long)(0x7)) << 5);
		TLMM_MDC_MDIO_HDRV_PULL_CTL_RGWR(v);
	}

err_out:
	if (tlmm_mdc_mdio_hdrv_pull_ctl_base)
		iounmap(tlmm_mdc_mdio_hdrv_pull_ctl_base);
	return ret;
}

static int ethqos_update_rgmii_tx_drv_strength(struct qcom_ethqos *ethqos)
{
	int ret = 0;
	struct resource *resource = NULL;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	u32 tlmm_central_base = 0;
	u32 tlmm_central_size = 0;
	unsigned long reg_rgmii_io_pads_voltage = 0;
	size_t size = 4;

	resource =
	 platform_get_resource_byname(ethqos->pdev, IORESOURCE_MEM, "tlmm-central-base");

	if (!resource) {
		ETHQOSINFO("Resource tlmm-central-base not found\n");
		goto err_out_rgmii_rx_ctr;
	}

	tlmm_central_base = resource->start;
	tlmm_central_size = resource_size(resource);
	ETHQOSDBG("tlmm_central_base = 0x%x, size = 0x%x\n",
		  tlmm_central_base, tlmm_central_size);

	tlmm_rgmii_pull_ctl1_base = ioremap(tlmm_central_base +
					    TLMM_RGMII_HDRV_PULL_CTL1_ADDRESS_OFFSET,
					    size);
	if (!tlmm_rgmii_pull_ctl1_base) {
		ETHQOSERR("cannot map tlmm_rgmii_pull_ctl1_base reg memory, aborting\n");
		ret = -EIO;
		goto err_out_rgmii_ctl1;
	}

	tlmm_rgmii_rx_ctr_base = ioremap(tlmm_central_base +
					 TLMM_RGMII_RX_HV_MODE_CTL_ADDRESS_OFFSET,
					 size);
	if (!tlmm_rgmii_rx_ctr_base) {
		ETHQOSERR("cannot map tlmm_rgmii_rx_ctr_base reg memory, aborting\n");
		ret = -EIO;
		goto err_out_rgmii_rx_ctr;
	}

	ETHQOSDBG("tlmm_rgmii_pull_ctl1_base = %#lx , tlmm_rgmii_rx_ctr_base= %lx\n",
		  tlmm_rgmii_pull_ctl1_base, tlmm_rgmii_rx_ctr_base);

	reg_rgmii_io_pads_voltage =
	regulator_get_voltage(ethqos->reg_rgmii_io_pads);

	ETHQOSINFO("IOMACRO pads voltage: %u uV\n", reg_rgmii_io_pads_voltage);

	switch (reg_rgmii_io_pads_voltage) {
	case 1500000:
	case 1800000: {
		switch (ethqos->emac_ver) {
		case EMAC_HW_v2_0_0:
		case EMAC_HW_v2_2_0:
		case EMAC_HW_v2_3_2: {
		TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_WR(TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA,
						     TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA,
						     TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA);
						     TLMM_RGMII_RX_HV_MODE_CTL_RGWR(0x0);
		}
		break;
		default:
		break;
		}
	}
	break;
	case 2500000: {
		switch (ethqos->emac_ver) {
		case EMAC_HW_v2_0_0:
		case EMAC_HW_v2_2_0:
		if (ethqos->always_on_phy)
			TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_WR(TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA,
							     TLMM_RGMII_HDRV_PL_CTL1_TX_HDRV_14MA,
							     TLMM_RGMII_HDRV_PL_CTL1_TX_HDRV_14MA);
		else if ((dev->phydev) && (dev->phydev->phy_id == ATH8035_PHY_ID))
			TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_WR(TLMM_RGMII_HDRV_PL_CTL1_TX_HDRV_14MA,
							     TLMM_RGMII_HDRV_PL_CTL1_TX_HDRV_14MA,
							     TLMM_RGMII_HDRV_PL_CTL1_TX_HDRV_14MA);
		break;
		default:
		break;
		}
	}
	break;
	default:
	break;
	}

err_out_rgmii_rx_ctr:
	if (tlmm_rgmii_rx_ctr_base)
		iounmap(tlmm_rgmii_rx_ctr_base);

err_out_rgmii_ctl1:
	if (tlmm_rgmii_pull_ctl1_base)
		iounmap(tlmm_rgmii_pull_ctl1_base);

	return ret;
}

static unsigned int ethqos_poll_rec_dev_emac(struct file *file,
					     poll_table *wait)
{
	int mask = 0;

	ETHQOSDBG("\n");

	poll_wait(file, &mac_rec_wq, wait);

	if (mac_rec_wq_flag) {
		mask = POLLIN | POLLRDNORM;
		mac_rec_wq_flag = false;
	}

	ETHQOSDBG("mask %d\n", mask);

	return mask;
}

static ssize_t ethqos_read_rec_dev_emac(struct file *filp, char __user *usr_buf,
					size_t count, loff_t *f_pos)
{
	char *buf;
	unsigned int len = 0, buf_len = 6000;
	ssize_t ret_cnt;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "PHY_RW_ERR", pethqos->mac_err_cnt[PHY_RW_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "PHY_DET_ERR", pethqos->mac_err_cnt[PHY_DET_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "CRC_ERR", pethqos->mac_err_cnt[CRC_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "RECEIVE_ERR", pethqos->mac_err_cnt[RECEIVE_ERR]);
		len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "OVERFLOW_ERR", pethqos->mac_err_cnt[OVERFLOW_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "FBE_ERR", pethqos->mac_err_cnt[FBE_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "RBU_ERR", pethqos->mac_err_cnt[RBU_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "TDU_ERR", pethqos->mac_err_cnt[TDU_ERR]);
		len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "DRIBBLE_ERR", pethqos->mac_err_cnt[DRIBBLE_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "WDT_ERR", pethqos->mac_err_cnt[WDT_ERR]);

	len += scnprintf(buf + len, buf_len - len, "\n\n%s  =  %d\n",
		 "PHY_RW_EN", pethqos->mac_rec_en[PHY_RW_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "PHY_DET_EN", pethqos->mac_rec_en[PHY_DET_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "CRC_EN", pethqos->mac_rec_en[CRC_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "RECEIVE_EN", pethqos->mac_rec_en[RECEIVE_ERR]);
		len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "OVERFLOW_EN", pethqos->mac_rec_en[OVERFLOW_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "FBE_EN", pethqos->mac_rec_en[FBE_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "RBU_EN", pethqos->mac_rec_en[RBU_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "TDU_EN", pethqos->mac_rec_en[TDU_ERR]);
		len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "DRIBBLE_EN", pethqos->mac_rec_en[DRIBBLE_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "WDT_EN", pethqos->mac_rec_en[WDT_ERR]);

	len += scnprintf(buf + len, buf_len - len, "\n\n%s  =  %d\n",
		 "PHY_RW_REC", pethqos->mac_rec_cnt[PHY_RW_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "PHY_DET_REC", pethqos->mac_rec_cnt[PHY_DET_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "CRC_REC", pethqos->mac_rec_cnt[CRC_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "RECEIVE_REC", pethqos->mac_rec_cnt[RECEIVE_ERR]);
		len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "OVERFLOW_REC", pethqos->mac_rec_cnt[OVERFLOW_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "FBE_REC", pethqos->mac_rec_cnt[FBE_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "RBU_REC", pethqos->mac_rec_cnt[RBU_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "TDU_REC", pethqos->mac_rec_cnt[TDU_ERR]);
		len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "DRIBBLE_REC", pethqos->mac_rec_cnt[DRIBBLE_ERR]);
	len += scnprintf(buf + len, buf_len - len, "%s  =  %d\n",
		 "WDT_REC", pethqos->mac_rec_cnt[WDT_ERR]);

	if (len > buf_len)
		len = buf_len;

	ETHQOSDBG("%s", buf);
	ret_cnt = simple_read_from_buffer(usr_buf, count, f_pos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static const struct file_operations emac_rec_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ethqos_read_rec_dev_emac,
	.write = ethqos_mac_recovery_enable,
	.poll = ethqos_poll_rec_dev_emac,
};

static int ethqos_create_emac_rec_device_node(dev_t *emac_dev_t,
					      struct cdev **emac_cdev,
					      struct class **emac_class,
					      char *emac_dev_node_name)
{
	int ret;

	ret = alloc_chrdev_region(emac_dev_t, 0, 1,
				  emac_dev_node_name);
	if (ret) {
		ETHQOSERR("alloc_chrdev_region error for node %s\n",
			  emac_dev_node_name);
		goto alloc_chrdev1_region_fail;
	}

	*emac_cdev = cdev_alloc();
	if (!*emac_cdev) {
		ret = -ENOMEM;
		ETHQOSERR("failed to alloc cdev\n");
		goto fail_alloc_cdev;
	}
	cdev_init(*emac_cdev, &emac_rec_fops);

	ret = cdev_add(*emac_cdev, *emac_dev_t, 1);
	if (ret < 0) {
		ETHQOSERR(":cdev_add err=%d\n", -ret);
		goto cdev1_add_fail;
	}

	*emac_class = class_create(THIS_MODULE, emac_dev_node_name);
	if (!*emac_class) {
		ret = -ENODEV;
		ETHQOSERR("failed to create class\n");
		goto fail_create_class;
	}

	if (!device_create(*emac_class, NULL,
			   *emac_dev_t, NULL, emac_dev_node_name)) {
		ret = -EINVAL;
		ETHQOSERR("failed to create device_create\n");
		goto fail_create_device;
	}

	ETHQOSERR(" mac recovery node opened");
	return 0;

fail_create_device:
	class_destroy(*emac_class);
fail_create_class:
	cdev_del(*emac_cdev);
cdev1_add_fail:
fail_alloc_cdev:
	unregister_chrdev_region(*emac_dev_t, 1);
alloc_chrdev1_region_fail:
		return ret;
}

bool qcom_ethqos_ipa_enabled(void)
{
#ifdef CONFIG_ETH_IPA_OFFLOAD
	return pethqos->ipa_enabled;
#endif
	return false;
}

void qcom_ethqos_serdes_init(struct qcom_ethqos *ethqos, int speed)
{
	int retry = 500;
	unsigned int val;

	/****************MODULE: SGMII_PHY_SGMII_PCS**********************************/
	writel_relaxed(0x01, ethqos->sgmii_base + QSERDES_PCS_SW_RESET);
	writel_relaxed(0x01, ethqos->sgmii_base + QSERDES_PCS_POWER_DOWN_CONTROL);

	/***************** MODULE: QSERDES_COM_SGMII_QMP_PLL*********/
	writel_relaxed(0x0F, ethqos->sgmii_base + QSERDES_COM_PLL_IVCO);
	writel_relaxed(0x06, ethqos->sgmii_base + QSERDES_COM_CP_CTRL_MODE0);
	writel_relaxed(0x16, ethqos->sgmii_base + QSERDES_COM_PLL_RCTRL_MODE0);
	writel_relaxed(0x36, ethqos->sgmii_base + QSERDES_COM_PLL_CCTRL_MODE0);
	writel_relaxed(0x1A, ethqos->sgmii_base + QSERDES_COM_SYSCLK_EN_SEL);
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_COM_LOCK_CMP1_MODE0);
	writel_relaxed(0x1A, ethqos->sgmii_base + QSERDES_COM_LOCK_CMP2_MODE0);
	writel_relaxed(0x82, ethqos->sgmii_base + QSERDES_COM_DEC_START_MODE0);
	writel_relaxed(0x55, ethqos->sgmii_base + QSERDES_COM_DIV_FRAC_START1_MODE0);
	writel_relaxed(0x55, ethqos->sgmii_base + QSERDES_COM_DIV_FRAC_START2_MODE0);
	writel_relaxed(0x03, ethqos->sgmii_base + QSERDES_COM_DIV_FRAC_START3_MODE0);
	writel_relaxed(0x24, ethqos->sgmii_base + QSERDES_COM_VCO_TUNE1_MODE0);

	writel_relaxed(0x02, ethqos->sgmii_base + QSERDES_COM_VCO_TUNE2_MODE0);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_COM_VCO_TUNE_INITVAL2);
	writel_relaxed(0x04, ethqos->sgmii_base + QSERDES_COM_HSCLK_SEL);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_COM_HSCLK_HS_SWITCH_SEL);
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_COM_CORECLK_DIV_MODE0);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_COM_CORE_CLK_EN);
	writel_relaxed(0xB9, ethqos->sgmii_base + QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE0);
	writel_relaxed(0x1E, ethqos->sgmii_base + QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE0);
	writel_relaxed(0x11, ethqos->sgmii_base + QSERDES_COM_BIN_VCOCAL_HSCLK_SEL);

	/******************MODULE: QSERDES_TX0_SGMII_QMP_TX***********************/
	writel_relaxed(0x05, ethqos->sgmii_base + QSERDES_TX_TX_BAND);
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_TX_SLEW_CNTL);
	writel_relaxed(0x09, ethqos->sgmii_base + QSERDES_TX_RES_CODE_LANE_OFFSET_TX);
	writel_relaxed(0x09, ethqos->sgmii_base + QSERDES_TX_RES_CODE_LANE_OFFSET_RX);
	writel_relaxed(0x05, ethqos->sgmii_base + QSERDES_TX_LANE_MODE_1);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_TX_LANE_MODE_3);
	writel_relaxed(0x12, ethqos->sgmii_base + QSERDES_TX_RCV_DETECT_LVL_2);
	writel_relaxed(0x0C, ethqos->sgmii_base + QSERDES_TX_TRAN_DRVR_EMP_EN);

	/*****************MODULE: QSERDES_RX0_SGMII_QMP_RX*******************/
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_RX_UCDR_FO_GAIN);
	writel_relaxed(0x06, ethqos->sgmii_base + QSERDES_RX_UCDR_SO_GAIN);
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_RX_UCDR_FASTLOCK_FO_GAIN);
	writel_relaxed(0x7F, ethqos->sgmii_base + QSERDES_RX_UCDR_SO_SATURATION_AND_ENABLE);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_RX_UCDR_FASTLOCK_COUNT_LOW);
	writel_relaxed(0x01, ethqos->sgmii_base + QSERDES_RX_UCDR_FASTLOCK_COUNT_HIGH);
	writel_relaxed(0x81, ethqos->sgmii_base + QSERDES_RX_UCDR_PI_CONTROLS);
	writel_relaxed(0x80, ethqos->sgmii_base + QSERDES_RX_UCDR_PI_CTRL2);
	writel_relaxed(0x04, ethqos->sgmii_base + QSERDES_RX_RX_TERM_BW);
	writel_relaxed(0x08, ethqos->sgmii_base + QSERDES_RX_VGA_CAL_CNTRL2);
	writel_relaxed(0x0F, ethqos->sgmii_base + QSERDES_RX_GM_CAL);
	writel_relaxed(0x04, ethqos->sgmii_base + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL1);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2);
	writel_relaxed(0x4A, ethqos->sgmii_base + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3);
	writel_relaxed(0x0A, ethqos->sgmii_base + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4);
	writel_relaxed(0x80, ethqos->sgmii_base + QSERDES_RX_RX_IDAC_TSETTLE_LOW);
	writel_relaxed(0x01, ethqos->sgmii_base + QSERDES_RX_RX_IDAC_TSETTLE_HIGH);
	writel_relaxed(0x20, ethqos->sgmii_base + QSERDES_RX_RX_IDAC_MEASURE_TIME);
	writel_relaxed(0x17, ethqos->sgmii_base + QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2);
	writel_relaxed(0x0F, ethqos->sgmii_base + QSERDES_RX_SIGDET_CNTRL);
	writel_relaxed(0x1E, ethqos->sgmii_base + QSERDES_RX_SIGDET_DEGLITCH_CNTRL);
	writel_relaxed(0x05, ethqos->sgmii_base + QSERDES_RX_RX_BAND);
	writel_relaxed(0xE0, ethqos->sgmii_base + QSERDES_RX_RX_MODE_00_LOW);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_00_HIGH);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_00_HIGH2);
	writel_relaxed(0x09, ethqos->sgmii_base + QSERDES_RX_RX_MODE_00_HIGH3);
	writel_relaxed(0xB1, ethqos->sgmii_base + QSERDES_RX_RX_MODE_00_HIGH4);
	writel_relaxed(0xE0, ethqos->sgmii_base + QSERDES_RX_RX_MODE_01_LOW);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_01_HIGH);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_01_HIGH2);
	writel_relaxed(0x09, ethqos->sgmii_base + QSERDES_RX_RX_MODE_01_HIGH3);
	writel_relaxed(0xB1, ethqos->sgmii_base + QSERDES_RX_RX_MODE_01_HIGH4);
	writel_relaxed(0xE0, ethqos->sgmii_base + QSERDES_RX_RX_MODE_10_LOW);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_10_HIGH);
	writel_relaxed(0xC8, ethqos->sgmii_base + QSERDES_RX_RX_MODE_10_HIGH2);
	writel_relaxed(0x3B, ethqos->sgmii_base + QSERDES_RX_RX_MODE_10_HIGH3);
	writel_relaxed(0xB7, ethqos->sgmii_base + QSERDES_RX_RX_MODE_10_HIGH4);
	writel_relaxed(0x0C, ethqos->sgmii_base + QSERDES_RX_DCC_CTRL1);

	/****************MODULE: SGMII_PHY_SGMII_PCS**********************************/
	writel_relaxed(0x0C, ethqos->sgmii_base + QSERDES_PCS_LINE_RESET_TIME);
	writel_relaxed(0x1F, ethqos->sgmii_base + QSERDES_PCS_TX_LARGE_AMP_DRV_LVL);
	writel_relaxed(0x03, ethqos->sgmii_base + QSERDES_PCS_TX_SMALL_AMP_DRV_LVL);
	writel_relaxed(0x83, ethqos->sgmii_base + QSERDES_PCS_TX_MID_TERM_CTRL1);
	writel_relaxed(0x08, ethqos->sgmii_base + QSERDES_PCS_TX_MID_TERM_CTRL2);
	writel_relaxed(0x0C, ethqos->sgmii_base + QSERDES_PCS_SGMII_MISC_CTRL8);
	writel_relaxed(0x00, ethqos->sgmii_base + QSERDES_PCS_SW_RESET);
	msleep(50);
	writel_relaxed(0x01, ethqos->sgmii_base + QSERDES_PCS_PHY_START);
	msleep(50);

	do {
		val = readl_relaxed(ethqos->sgmii_base + QSERDES_COM_C_READY_STATUS);
		val &= BIT(0);
		if (val)
			break;
		usleep_range(1000, 1500);
		retry--;
	} while (retry > 0);
	if (!retry)
		ETHQOSERR("QSERDES_COM_C_READY_STATUS timedout with retry = %d\n", retry);

	retry = 500;
	do {
		val = readl_relaxed(ethqos->sgmii_base + QSERDES_PCS_PCS_READY_STATUS);
		val &= BIT(0);
		if (val)
			break;
		usleep_range(1000, 1500);
		retry--;
	} while (retry > 0);
	if (!retry)
		ETHQOSERR("PCS_READY timedout with retry = %d\n", retry);

	retry = 500;
	do {
		val = readl_relaxed(ethqos->sgmii_base + QSERDES_PCS_PCS_READY_STATUS);
		val &= BIT(7);
		if (val)
			break;
		usleep_range(1000, 1500);
		retry--;
	} while (retry > 0);
	if (!retry)
		ETHQOSERR("SGMIIPHY_READY timedout with retry = %d\n", retry);

	retry = 5000;
	do {
		val = readl_relaxed(ethqos->sgmii_base + QSERDES_COM_CMN_STATUS);
		val &= BIT(1);
		if (val)
			break;
		usleep_range(1000, 1500);
		retry--;
	} while (retry > 0);
	if (!retry)
		ETHQOSERR("PLL Lock Status timedout with retry = %d\n", retry);
}

static ssize_t ethqos_read_dev_emac(struct file *filp, char __user *buf,
				    size_t count, loff_t *f_pos)
{
	struct eth_msg_meta msg;
	u8 status = 0;

	memset(&msg, 0,  sizeof(struct eth_msg_meta));

	if (pethqos && pethqos->ipa_enabled)
		ethqos_ipa_offload_event_handler(&status, EV_QTI_GET_CONN_STATUS);

	msg.msg_type = status;

	ETHQOSDBG("status %02x\n", status);
	ETHQOSDBG("msg.msg_type %02x\n", msg.msg_type);
	ETHQOSDBG("msg.rsvd %02x\n", msg.rsvd);
	ETHQOSDBG("msg.msg_len %d\n", msg.msg_len);

	return copy_to_user(buf, &msg, sizeof(struct eth_msg_meta));
}

static ssize_t ethqos_write_dev_emac(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	unsigned char in_buf[300] = {0};
	unsigned long ret;
	struct qcom_ethqos *ethqos = pethqos;
	struct stmmac_priv *priv = qcom_ethqos_get_priv(pethqos);
	char mac_str[30] = {0};
	char vlan_str[30] = {0};
	char *prefix = NULL;
	u32 err, prio, queue;
	unsigned int number;

	if (sizeof(in_buf) < count) {
		ETHQOSERR("emac string is too long - count=%u\n", count);
		return -EFAULT;
	}

	memset(in_buf, 0,  sizeof(in_buf));
	ret = copy_from_user(in_buf, user_buf, count);

	if (ret)
		return -EFAULT;

	strlcpy(vlan_str, in_buf, sizeof(vlan_str));

	ETHQOSINFO("emac string is %s\n", vlan_str);

	if (strnstr(vlan_str, "QOE", sizeof(vlan_str))) {
		ethqos->qoe_vlan.available = true;
		queue = ethqos->qoe_vlan.rx_queue;
		priv->plat->rx_queues_cfg[queue].use_prio = true;
		prio = priv->plat->rx_queues_cfg[queue].prio;
		priv->hw->mac->rx_queue_prio(priv->hw, prio, queue);
	}

	if (strnstr(vlan_str, "qvlanid=", sizeof(vlan_str))) {
		prefix = strnchr(vlan_str,
				 strlen(vlan_str), '=');
		ETHQOSINFO("vlanid data written is %s\n", prefix + 1);
		if (prefix) {
			err = kstrtouint(prefix + 1, 0, &number);
			if (!err)
				ethqos->qoe_vlan.vlan_id = number;
		}
	}

	if (strnstr(vlan_str, "qvlan_pcp=", strlen(vlan_str))) {
		prefix = strnchr(vlan_str, strlen(vlan_str), '=');
		ETHQOSDBG("QMI vlan_pcp data written is %s\n", prefix + 1);
		if (prefix) {
			err = kstrtouint(prefix + 1, 0, &number);
			if (!err) {
				/* Convert prio to bit format */
				prio = MAC_RXQCTRL_PSRQX_PRIO_SHIFT(number);
				queue = ethqos->qoe_vlan.rx_queue;
				priv->plat->rx_queues_cfg[queue].prio = prio;
			}
		}
	}

	if (strnstr(vlan_str, "Cv2X", strlen(vlan_str))) {
		ETHQOSDBG("Cv2X supported mode is %u\n", ethqos->cv2x_mode);
		ethqos->cv2x_vlan.available = true;
		queue = ethqos->cv2x_vlan.rx_queue;
		priv->plat->rx_queues_cfg[queue].use_prio = true;
		prio = priv->plat->rx_queues_cfg[queue].prio;
		priv->hw->mac->rx_queue_prio(priv->hw, prio, queue);
	}

	if (strnstr(vlan_str, "cvlanid=", strlen(vlan_str))) {
		prefix = strnchr(vlan_str, strlen(vlan_str), '=');
		ETHQOSDBG("Cv2X vlanid data written is %s\n", prefix + 1);
		if (prefix) {
			err = kstrtouint(prefix + 1, 0, &number);
			if (!err)
				ethqos->cv2x_vlan.vlan_id = number;
		}
	}

	if (strnstr(vlan_str, "cvlan_pcp=", strlen(vlan_str))) {
		prefix = strnchr(vlan_str, strlen(vlan_str), '=');
		ETHQOSDBG("Cv2X vlan_pcp data written is %s\n", prefix + 1);
		if (prefix) {
			err = kstrtouint(prefix + 1, 0, &number);
			if (!err) {
				/* Convert prio to bit format */
				prio = MAC_RXQCTRL_PSRQX_PRIO_SHIFT(number);
				queue = ethqos->cv2x_vlan.rx_queue;
				priv->plat->rx_queues_cfg[queue].prio = prio;
			}
		}
	}

	if (strnstr(in_buf, "cmac_id=", strlen(in_buf))) {
		prefix = strnchr(in_buf, strlen(in_buf), '=');
		if (prefix) {
			if (strlcpy(mac_str, (char *)prefix + 1, 30) >= 30) {
				ETHQOSERR("Invalid prefix size\n");
				return -EFAULT;
			}
			if (!mac_pton(mac_str, config_dev_addr)) {
				ETHQOSERR("Invalid mac addr in /dev/emac\n");
				return count;
			}

			if (!is_valid_ether_addr(config_dev_addr)) {
				ETHQOSERR("Invalid/Multcast mac addr found\n");
				return count;
			}

			ether_addr_copy(dev_addr, config_dev_addr);
			memcpy(ethqos->cv2x_dev_addr, dev_addr, ETH_ALEN);
		}
	}

	return count;
}

static void ethqos_get_qoe_dt(struct qcom_ethqos *ethqos,
			      struct device_node *np)
{
	int res;

	res = of_property_read_u32(np, "qcom,qoe_mode", &ethqos->qoe_mode);
	if (res) {
		ETHQOSDBG("qoe_mode not in dtsi\n");
		ethqos->qoe_mode = 0;
	}

	if (ethqos->qoe_mode) {
		res = of_property_read_u32(np, "qcom,qoe-queue",
					   &ethqos->qoe_vlan.rx_queue);
		if (res) {
			ETHQOSERR("qoe-queue not in dtsi for qoe_mode %u\n",
				  ethqos->qoe_mode);
			ethqos->qoe_vlan.rx_queue = QMI_TAG_TX_CHANNEL;
		}

		res = of_property_read_u32(np, "qcom,qoe-vlan-offset",
					   &ethqos->qoe_vlan.vlan_offset);
		if (res) {
			ETHQOSERR("qoe-vlan-offset not in dtsi\n");
			ethqos->qoe_vlan.vlan_offset = 0;
		}
	}
}

static DECLARE_WAIT_QUEUE_HEAD(dev_emac_wait);
#ifdef CONFIG_ETH_IPA_OFFLOAD
void ethqos_wakeup_dev_emac_queue(void)
{
	ETHQOSDBG("\n");
	wake_up_interruptible(&dev_emac_wait);
}
#endif

static unsigned int ethqos_poll_dev_emac(struct file *file, poll_table *wait)
{
	int mask = 0;
	int update = 0;

	ETHQOSDBG("\n");

	poll_wait(file, &dev_emac_wait, wait);

	if (pethqos && pethqos->ipa_enabled && pethqos->cv2x_mode)
		ethqos_ipa_offload_event_handler(&update, EV_QTI_CHECK_CONN_UPDATE);

	if (update)
		mask = POLLIN | POLLRDNORM;

	ETHQOSDBG("mask %d\n", mask);

	return mask;
}

static const struct file_operations emac_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ethqos_read_dev_emac,
	.write = ethqos_write_dev_emac,
	.poll = ethqos_poll_dev_emac,
};

static int ethqos_create_emac_device_node(dev_t *emac_dev_t,
					  struct cdev **emac_cdev,
					  struct class **emac_class,
					  char *emac_dev_node_name)
{
	int ret;

	ret = alloc_chrdev_region(emac_dev_t, 0, 1,
				  emac_dev_node_name);
	if (ret) {
		ETHQOSERR("alloc_chrdev_region error for node %s\n",
			  emac_dev_node_name);
		goto alloc_chrdev1_region_fail;
	}

	*emac_cdev = cdev_alloc();
	if (!*emac_cdev) {
		ret = -ENOMEM;
		ETHQOSERR("failed to alloc cdev\n");
		goto fail_alloc_cdev;
	}
	cdev_init(*emac_cdev, &emac_fops);

	ret = cdev_add(*emac_cdev, *emac_dev_t, 1);
	if (ret < 0) {
		ETHQOSERR(":cdev_add err=%d\n", -ret);
		goto cdev1_add_fail;
	}

	*emac_class = class_create(THIS_MODULE, emac_dev_node_name);
	if (!*emac_class) {
		ret = -ENODEV;
		ETHQOSERR("failed to create class\n");
		goto fail_create_class;
	}

	if (!device_create(*emac_class, NULL,
			   *emac_dev_t, NULL, emac_dev_node_name)) {
		ret = -EINVAL;
		ETHQOSERR("failed to create device_create\n");
		goto fail_create_device;
	}

	return 0;

fail_create_device:
	class_destroy(*emac_class);
fail_create_class:
	cdev_del(*emac_cdev);
cdev1_add_fail:
fail_alloc_cdev:
	unregister_chrdev_region(*emac_dev_t, 1);
alloc_chrdev1_region_fail:
		return ret;
}

static void ethqos_get_cv2x_dt(struct qcom_ethqos *ethqos,
			       struct device_node *np)
{
	int res;

	res = of_property_read_u32(np, "qcom,cv2x_mode", &ethqos->cv2x_mode);
	if (res) {
		ETHQOSDBG("cv2x_mode not in dtsi\n");
		ethqos->cv2x_mode = CV2X_MODE_DISABLE;
	}

	if (ethqos->cv2x_mode != CV2X_MODE_DISABLE) {
		res = of_property_read_u32(np, "qcom,cv2x-queue",
					   &ethqos->cv2x_vlan.rx_queue);
		if (res) {
			ETHQOSERR("cv2x-queue not in dtsi for cv2x_mode %u\n",
				  ethqos->cv2x_mode);
			ethqos->cv2x_vlan.rx_queue = CV2X_TAG_TX_CHANNEL;
		}

		res = of_property_read_u32(np, "qcom,cv2x-vlan-offset",
					   &ethqos->cv2x_vlan.vlan_offset);
		if (res) {
			ETHQOSERR("cv2x-vlan-offset not in dtsi\n");
			ethqos->cv2x_vlan.vlan_offset = 1;
		}
	}
}

static int _qcom_ethqos_probe(void *arg)
{
	struct platform_device *pdev = (struct platform_device *)arg;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *rgmii_io_macro_node = NULL;
	struct stmmac_resources stmmac_res;
	struct qcom_ethqos *ethqos = NULL;
	struct resource *res = NULL;
	int ret, i;
	struct net_device *ndev;
	struct stmmac_priv *priv;

	if (of_device_is_compatible(pdev->dev.of_node,
				    "qcom,emac-smmu-embedded"))
		return emac_emb_smmu_cb_probe(pdev, plat_dat);

#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER
	place_marker("M - Ethernet probe start");
#endif

#if IS_ENABLED(CONFIG_IPC_LOGGING)
	ipc_emac_log_ctxt = ipc_log_context_create(IPCLOG_STATE_PAGES,
						   "emac", 0);
	if (!ipc_emac_log_ctxt)
		ETHQOSERR("Error creating logging context for emac\n");
	else
		ETHQOSINFO("IPC logging has been enabled for emac\n");
#endif

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	ethqos = devm_kzalloc(&pdev->dev, sizeof(*ethqos), GFP_KERNEL);
	if (!ethqos) {
		return -ENOMEM;
	}

	ethqos->pdev = pdev;

	ethqos->phyad_change = false;
	if (of_property_read_bool(np, "qcom,phyad_change")) {
		ethqos->phyad_change = true;
		ETHQOSDBG("qcom,phyad_change present\n");
	}

	ethqos->is_gpio_phy_reset = false;
	if (of_property_read_bool(np, "snps,reset-gpios")) {
		ethqos->is_gpio_phy_reset = true;
		ETHQOSDBG("qcom,phy-reset present\n");
	}

	if (of_property_read_u32(np, "qcom,phyvoltage_min",
				 &ethqos->phyvoltage_min))
		ethqos->phyvoltage_min = 3075000;
	else
		ETHQOSINFO("qcom,phyvoltage_min = %d\n",
			   ethqos->phyvoltage_min);

	if (of_property_read_u32(np, "qcom,phyvoltage_max",
				 &ethqos->phyvoltage_max))
		ethqos->phyvoltage_max = 3200000;
	else
		ETHQOSINFO("qcom,phyvoltage_max = %d\n",
			   ethqos->phyvoltage_max);

	ethqos_init_regulators(ethqos);
	ethqos_init_gpio(ethqos);

	ethqos_get_qoe_dt(ethqos, np);
	ethqos_get_cv2x_dt(ethqos, np);

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat)) {
		dev_err(&pdev->dev, "dt configuration failed\n");
		return PTR_ERR(plat_dat);
	}

	if (ethqos->cv2x_mode == CV2X_MODE_MDM ||
	    ethqos->cv2x_mode == CV2X_MODE_AP) {
		for (i = 0; i < plat_dat->rx_queues_to_use; i++) {
			if (plat_dat->rx_queues_cfg[i].pkt_route ==
			    PACKET_AVCPQ)
				plat_dat->rx_queues_cfg[i].pkt_route = 0;
		}
	}

	if (ethqos->cv2x_mode) {
		if (ethqos->cv2x_vlan.rx_queue >= plat_dat->rx_queues_to_use)
			ethqos->cv2x_vlan.rx_queue = CV2X_TAG_TX_CHANNEL;

		ret = of_property_read_u32(np, "jumbo-mtu",
					   &plat_dat->jumbo_mtu);
		if (!ret) {
			if (plat_dat->jumbo_mtu >
			    MAX_SUPPORTED_JUMBO_MTU) {
				ETHQOSDBG("jumbo mtu %u biger than max val\n",
					  plat_dat->jumbo_mtu);
				ETHQOSDBG("Set it to max supported value %u\n",
					  MAX_SUPPORTED_JUMBO_MTU);
				plat_dat->jumbo_mtu =
					MAX_SUPPORTED_JUMBO_MTU;
			}

			if (plat_dat->jumbo_mtu < MIN_JUMBO_FRAME_SIZE) {
				plat_dat->jumbo_mtu = 0;
			} else {
				/* Store and Forward mode will limit the max
				 * buffer size per rx fifo buffer size
				 * configuration. Use Receive Queue Threshold
				 * Control mode (rtc) for cv2x rx queue to
				 * support the jumbo frame up to 8K.
				 */
				i = ethqos->cv2x_vlan.rx_queue;
				plat_dat->rx_queues_cfg[i].use_rtc = true;
			}
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rgmii");
	ethqos->rgmii_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ethqos->rgmii_base)) {
		dev_err(&pdev->dev, "Can't get rgmii base\n");
		ret = PTR_ERR(ethqos->rgmii_base);
		goto err_mem;
	}

	ethqos->rgmii_clk = devm_clk_get(&pdev->dev, "rgmii");
	if (IS_ERR(ethqos->rgmii_clk)) {
		ret = PTR_ERR(ethqos->rgmii_clk);
		goto err_mem;
	}

	ret = clk_prepare_enable(ethqos->rgmii_clk);
	if (ret)
		goto err_mem;

	/* Read mac address from fuse register */
	read_mac_addr_from_fuse_reg(np);

	/*Initialize Early ethernet to false*/
	ethqos->early_eth_enabled = false;

	/*Check for valid mac, ip address to enable Early eth*/
	if (pparams.is_valid_mac_addr &&
	    (pparams.is_valid_ipv4_addr || pparams.is_valid_ipv6_addr)) {
		/* For 1000BASE-T mode, auto-negotiation is required and
		 * always used to establish a link.
		 * Configure phy and MAC in 100Mbps mode with autoneg
		 * disable as link up takes more time with autoneg
		 * enabled.
		 */
		ethqos->early_eth_enabled = true;
		ETHQOSINFO("Early ethernet is enabled\n");
	}

	ethqos->speed = SPEED_10;
	ethqos_update_rgmii_clk_and_bus_cfg(ethqos, SPEED_10);
	ethqos_set_func_clk_en(ethqos);

	plat_dat->bsp_priv = ethqos;
	plat_dat->fix_mac_speed = ethqos_fix_mac_speed;
	plat_dat->c45_marvell_en = of_property_read_bool(np, "qcom,c45_marvell");
	plat_dat->tx_select_queue = dwmac_qcom_select_queue;
	if (of_property_read_bool(pdev->dev.of_node,
				  "disable-intr-mod") &&
	    !plat_dat->autosar_en)
		ETHQOSINFO("disabling Interrupt moderation\n");
	else
		plat_dat->get_plat_tx_coal_frames =  dwmac_qcom_get_plat_tx_coal_frames;
	plat_dat->has_gmac4 = 1;
	plat_dat->tso_en = of_property_read_bool(np, "snps,tso");
	plat_dat->autosar_en = of_property_read_bool(np, "qcom,autosar");
	plat_dat->force_thresh_dma_mode_q0_en =
		of_property_read_bool(np,
				      "snps,force_thresh_dma_mode_q0");
	plat_dat->early_eth = ethqos->early_eth_enabled;
	plat_dat->handle_prv_ioctl = ethqos_handle_prv_ioctl;
	plat_dat->request_phy_wol = qcom_ethqos_request_phy_wol;
	plat_dat->init_pps = ethqos_init_pps;
	plat_dat->phy_intr_enable = ethqos_phy_intr_enable;
	plat_dat->phy_irq_enable = ethqos_phy_irq_enable;
	plat_dat->phy_irq_disable = ethqos_phy_irq_disable;
	plat_dat->phyad_change = ethqos->phyad_change;
	plat_dat->is_gpio_phy_reset = ethqos->is_gpio_phy_reset;
	plat_dat->handle_mac_err = dwmac_qcom_handle_mac_err;
	plat_dat->rgmii_loopback_cfg = rgmii_loopback_config;
#ifdef CONFIG_DWMAC_QCOM_ETH_AUTOSAR
	plat_dat->handletxcompletion = ethwrapper_handletxcompletion;
	plat_dat->handlericompletion = ethwrapper_handlericompletion;
#endif
	plat_dat->clks_suspended = false;

	/* Get rgmii interface speed for mac2c from device tree */
	if (of_property_read_u32(np, "mac2mac-rgmii-speed",
				&plat_dat->mac2mac_rgmii_speed))
		plat_dat->mac2mac_rgmii_speed = -1;
	else
		ETHQOSINFO("mac2mac rgmii speed = %d\n",
				plat_dat->mac2mac_rgmii_speed);

	if (of_property_read_bool(pdev->dev.of_node,
				  "emac-core-version")) {
		/* Read emac core version value from dtsi */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "emac-core-version",
					   &ethqos->emac_ver);
		if (ret) {
			ETHQOSDBG(":resource emac-hw-ver! not in dtsi\n");
			ethqos->emac_ver = EMAC_HW_NONE;
			WARN_ON(1);
		}
	} else {
		ethqos->emac_ver =
		rgmii_readl(ethqos, EMAC_I0_EMAC_CORE_HW_VERSION_RGOFFADDR);
	}
	ETHQOSDBG(": emac_core_version = %d\n", ethqos->emac_ver);

	/* Update io-macro settings from device tree */
	 rgmii_io_macro_node = of_find_node_by_name(pdev->dev.of_node,
						    "rgmii-io-macro-info");
	if (rgmii_io_macro_node) {
		ETHQOSINFO("rgmii_io_macro_node found in dt\n");
		read_rgmii_io_macro_node_setting(rgmii_io_macro_node, ethqos);
	} else {
		ETHQOSINFO("rgmii_io_macro_node not found in dt\n");
	}

	if (of_property_read_bool(pdev->dev.of_node, "qcom,arm-smmu")) {
		emac_emb_smmu_ctx.pdev_master = pdev;
		ret = of_platform_populate(pdev->dev.of_node,
					   qcom_ethqos_match, NULL, &pdev->dev);
		if (ret)
			ETHQOSERR("Failed to populate EMAC platform\n");
		if (emac_emb_smmu_ctx.ret) {
			ETHQOSERR("smmu probe failed\n");
			of_platform_depopulate(&pdev->dev);
			ret = emac_emb_smmu_ctx.ret;
			emac_emb_smmu_ctx.ret = 0;
		}
	}
	if (!plat_dat->mac2mac_en) {
		if (of_property_read_bool(pdev->dev.of_node,
					  "emac-phy-off-suspend")) {
			/* Read emac core version value from dtsi */
			ret = of_property_read_u32(pdev->dev.of_node,
						   "emac-phy-off-suspend",
						   &ethqos->current_phy_mode);
			if (ret) {
				ETHQOSDBG(":resource emac-phy-off-suspend! ");
				ETHQOSDBG("not in dtsi\n");
				ethqos->current_phy_mode = 0;
			}
		}
	}
	ETHQOSINFO("emac-phy-off-suspend = %d\n",
		   ethqos->current_phy_mode);

	if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY ||
	    ethqos->current_phy_mode == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		plat_dat->is_phy_off = true;
	} else {
		plat_dat->is_phy_off = false;
	}

	plat_dat->mdio_reset = of_property_read_bool(pdev->dev.of_node,
						     "mdio-reset");
	ethqos->skip_ipa_autoresume = of_property_read_bool(pdev->dev.of_node,
							    "skip-ipa-autoresume");
	ethqos->ioaddr = (&stmmac_res)->addr;
	ethqos_update_mdio_drv_strength(ethqos, np);
	ethqos_mac_rec_init(ethqos);
	if (of_device_is_compatible(np, "qcom,qcs404-ethqos"))
		plat_dat->rx_clk_runs_in_lpi = 1;

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_clk;

	if (ethqos->io_macro.rgmii_tx_drv)
		ethqos_update_rgmii_tx_drv_strength(ethqos);

ethqos_emac_mem_base(ethqos);
	pethqos = ethqos;
ethqos_phy_off_sysfs(ethqos);
#ifdef CONFIG_DEBUG_FS
	ethqos_create_debugfs(ethqos);
#endif
	ndev = dev_get_drvdata(&ethqos->pdev->dev);
	priv = netdev_priv(ndev);

#ifdef CONFIG_ETH_IPA_OFFLOAD
	ethqos_ipa_offload_event_handler(ethqos, EV_PROBE_INIT);
#endif

	rgmii_dump(ethqos);

	ethqos->ioaddr = (&stmmac_res)->addr;

	if (ethqos->io_macro.pps_create) {
		ethqos_pps_irq_config(ethqos);
		create_pps_interrupt_device_node(&ethqos->avb_class_a_dev_t,
						 &ethqos->avb_class_a_cdev,
						 &ethqos->avb_class_a_class,
						 AVB_CLASS_A_POLL_DEV_NODE);

		create_pps_interrupt_device_node(&ethqos->avb_class_b_dev_t,
						 &ethqos->avb_class_b_cdev,
						 &ethqos->avb_class_b_class,
						 AVB_CLASS_B_POLL_DEV_NODE);
	}

	priv->en_wol = of_property_read_bool(np, "enable-wol");

	qcom_ethqos_read_iomacro_por_values(ethqos);

	if (pparams.is_valid_mac_addr) {
		priv->dev->addr_assign_type = NET_ADDR_PERM;
		ether_addr_copy(dev_addr, pparams.mac_addr);
		memcpy(priv->dev->dev_addr, dev_addr, ETH_ALEN);
		ETHQOSINFO("using partition device MAC address %pM\n", priv->dev->dev_addr);
	}

	res = platform_get_resource(ethqos->pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ETHQOSERR("get emac-base resource failed\n");
		ret = -ENOMEM;
		goto err_clk;
	}

	ethqos->emac_mem_size = resource_size(res);

	if (ethqos->early_eth_enabled) {
		/* Initialize work*/
		INIT_WORK(&ethqos->early_eth,
			  qcom_ethqos_bringup_iface);
		/* Queue the work*/
		if (open_not_called == 1) {
			ETHQOSINFO("calling driver open\n");
			queue_work(system_wq, &ethqos->early_eth);
		}
		/*Set early eth parameters*/
		ethqos_set_early_eth_param(priv, ethqos);
	}

	if (ethqos->cv2x_mode) {
		for (i = 0; i < plat_dat->rx_queues_to_use; i++) {
			priv->rx_queue[i].en_fep = true;
			if (plat_dat->jumbo_mtu && i == ethqos->cv2x_vlan.rx_queue) {
				priv->rx_queue[i].jumbo_en = true;
				ETHQOSDBG(" Jumbo fram enabled for queue = %d", i);
			}
		}
	}

	if (ethqos->qoe_mode || ethqos->cv2x_mode) {
		ethqos_create_emac_device_node(&ethqos->emac_dev_t,
					       &ethqos->emac_cdev,
					       &ethqos->emac_class,
					       "emac");
	}

	if (priv->plat->mac2mac_en)
		priv->plat->mac2mac_link = 0;

	if (pethqos->cv2x_mode != CV2X_MODE_DISABLE)
		qcom_ethqos_register_listener();

	if (!qcom_ethqos_init_panic_notifier(ethqos))
		atomic_notifier_chain_register(&panic_notifier_list,
					       &qcom_ethqos_panic_blk);

	ethqos_create_emac_rec_device_node(&ethqos->emac_rec_dev_t,
					   &ethqos->emac_rec_cdev,
					   &ethqos->emac_rec_class,
					   "emac_rec");

#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER
	place_marker("M - Ethernet probe end");
#endif

#ifdef CONFIG_NET_L3_MASTER_DEV
	if (ethqos->early_eth_enabled &&
	    ethqos->io_macro.l3_master_dev) {
		ETHQOSINFO("l3mdev_op1 set\n");
		ndev->priv_flags = IFF_L3MDEV_MASTER;
		ndev->l3mdev_ops = &l3mdev_op1;
	}
#endif

	return 0;

err_clk:
	clk_disable_unprepare(ethqos->rgmii_clk);

err_mem:
	ethqos->driver_load_fail = true;
	stmmac_remove_config_dt(pdev, plat_dat);

	return 0;
}

static int qcom_ethqos_probe(struct platform_device *pdev)
{
#ifdef CONFIG_PLATFORM_AUTO
	struct task_struct *ethqos_task = kthread_run(_qcom_ethqos_probe, pdev,
			"ethqos_probe");
	if (PTR_ERR_OR_ZERO(ethqos_task))
		return PTR_ERR(ethqos_task);
	else
		return 0;
#else
	return _qcom_ethqos_probe(pdev);
#endif
}

static int qcom_ethqos_remove(struct platform_device *pdev)
{
	struct qcom_ethqos *ethqos;
	int ret;
	struct stmmac_priv *priv;

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,emac-smmu-embedded")) {
		of_platform_depopulate(&pdev->dev);
		return 0;
	}

	ethqos = get_stmmac_bsp_priv(&pdev->dev);
	if (!ethqos)
		return -ENODEV;

	priv = qcom_ethqos_get_priv(pethqos);

	ret = stmmac_pltfr_remove(pdev);
	clk_disable_unprepare(ethqos->rgmii_clk);

	if (priv->plat->phy_intr_en_extn_stm)
		free_irq(ethqos->phy_intr, ethqos);
	priv->phy_irq_enabled = false;

	if (priv->plat->phy_intr_en_extn_stm)
		cancel_work_sync(&ethqos->emac_phy_work);

	if (ethqos->io_macro.pps_remove)
		ethqos_remove_pps_dev(ethqos);

	ethqos_phy_off_remove_sysfs(ethqos);
#ifdef CONFIG_DEBUG_FS
	ethqos_cleanup_debugfs(ethqos);
#endif
	ethqos_free_gpios(ethqos);
	emac_emb_smmu_exit();
	ethqos_disable_regulators(ethqos);

	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &qcom_ethqos_panic_blk);

	platform_set_drvdata(pdev, NULL);
	of_platform_depopulate(&pdev->dev);

	return ret;
}

static int qcom_ethqos_suspend(struct device *dev)
{
	struct qcom_ethqos *ethqos;
	struct net_device *ndev = NULL;
	int ret;
	struct stmmac_priv *priv;
	struct plat_stmmacenet_data *plat;
	int allow_suspend = 1;

	ETHQOSDBG("Suspend Enter\n");
	if (of_device_is_compatible(dev->of_node, "qcom,emac-smmu-embedded")) {
		ETHQOSDBG("smmu return\n");
		return 0;
	}

	update_marker("M - Ethernet Suspend start");

	ethqos = get_stmmac_bsp_priv(dev);
	if (!ethqos)
		return -ENODEV;

	if (ethqos->driver_load_fail) {
		ETHQOSINFO("driver load failed\n");
		return 0;
	}

	ndev = dev_get_drvdata(dev);

	ethqos_ipa_offload_event_handler(&allow_suspend, EV_DPM_SUSPEND);
	if (!allow_suspend) {
		enable_irq_wake(ndev->irq);
		ETHQOSDBG("Suspend Exit enable IRQ\n");
		return 0;
	}
	if (!ndev)
		return -EINVAL;
	priv = netdev_priv(ndev);
	plat = priv->plat;
	if (priv->phydev)  {
		if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY ||
		    ethqos->current_phy_mode ==
		    DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
			/*Backup phy related data*/
			if (priv->phydev->autoneg == AUTONEG_DISABLE) {
				ethqos->backup_autoneg = priv->phydev->autoneg;
				ethqos->backup_bmcr = ethqos_mdio_read
						      (priv, plat->phy_addr,
						       MII_BMCR);
			} else {
				ethqos->backup_autoneg = AUTONEG_ENABLE;
			}
		}
	}
	ret = stmmac_suspend(dev);
	qcom_ethqos_phy_suspend_clks(ethqos);

	/* Suspend the PHY TXC clock. */
	if (ethqos->rgmii_txc_suspend_state) {
		/* Remove TXC clock source from Phy.*/
		ret = pinctrl_select_state(ethqos->pinctrl,
					   ethqos->rgmii_txc_suspend_state);
	if (ret)
		ETHQOSERR("Unable to set rgmii_txc_suspend_state state, err = %d\n", ret);
	else
		ETHQOSINFO("Set rgmii_txc_suspend_state succeed\n");
	}
	if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY ||
	    ethqos->current_phy_mode == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		ETHQOSINFO("disable phy at suspend\n");
		ethqos_phy_power_off(ethqos);
	}

	update_marker("M - Ethernet Suspend End");
	priv->boot_kpi = false;
	ETHQOSDBG(" ret = %d\n", ret);
	return ret;
}

static int qcom_ethqos_resume(struct device *dev)
{
	struct net_device *ndev = NULL;
	struct qcom_ethqos *ethqos;
	int ret;
	struct stmmac_priv *priv;

	ETHQOSDBG("Resume Enter\n");
	if (of_device_is_compatible(dev->of_node, "qcom,emac-smmu-embedded"))
		return 0;

	place_marker("M - Ethernet Resume start");

	ethqos = get_stmmac_bsp_priv(dev);

	if (!ethqos)
		return -ENODEV;

	if (ethqos->driver_load_fail) {
		ETHQOSINFO("driver load failed\n");
		return 0;
	}

	ndev = dev_get_drvdata(dev);

	if (!ndev) {
		ETHQOSERR(" Resume not possible\n");
		return -EINVAL;
	}

	priv = netdev_priv(ndev);
	/* Resume the PhY TXC clock. */
	if (ethqos->rgmii_txc_resume_state) {
		/* Enable TXC clock source from Phy.*/
		ret = pinctrl_select_state(ethqos->pinctrl,
					   ethqos->rgmii_txc_resume_state);
		if (ret)
			ETHQOSERR("Unable to set rgmii_rxc_resume_state state, err = %d\n", ret);
		else
			ETHQOSINFO("Set rgmii_rxc_resume_state succeed\n");
	}

	if (ethqos->current_phy_mode == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		ETHQOSINFO("enable phy at resume\n");
		ethqos_phy_power_on(ethqos);
	}

	if (!ethqos->clks_suspended) {
		disable_irq_wake(ndev->irq);
		ETHQOSDBG("Resume Exit disable IRQ\n");
		return 0;
	}

	qcom_ethqos_phy_resume_clks(ethqos);

	if (ethqos->current_phy_mode == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		ETHQOSINFO("reset phy after clock\n");
		ethqos_reset_phy_enable_interrupt(ethqos);
		if (ethqos->backup_autoneg == AUTONEG_DISABLE) {
			if (priv->phydev) {
				priv->phydev->autoneg = ethqos->backup_autoneg;
				phy_write(priv->phydev, MII_BMCR, ethqos->backup_bmcr);
			} else {
				ETHQOSINFO("Phy dev is NULL\n");
			}
		}
	}

	if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY) {
		/* Temp Enable LOOPBACK_EN.
		 * TX clock needed for reset As Phy is off
		 */
		rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
			      RGMII_CONFIG_LOOPBACK_EN,
			      RGMII_IO_MACRO_CONFIG);
		ETHQOSINFO("Loopback EN Enabled\n");
	}
	ret = stmmac_resume(dev);
	if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY) {
		//Disable  LOOPBACK_EN
		rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
			      0, RGMII_IO_MACRO_CONFIG);
		ETHQOSINFO("Loopback EN Disabled\n");
	}

	if (!ethqos->skip_ipa_autoresume)
		ethqos_ipa_offload_event_handler(NULL, EV_DPM_RESUME);

	place_marker("M - Ethernet Resume End");

	ETHQOSDBG("<--Resume Exit\n");
	return ret;
}

static int qcom_ethqos_enable_clks(struct qcom_ethqos *ethqos, struct device *dev)
{
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);
	int ret = 0;

	/* clock setup */
	priv->plat->stmmac_clk = devm_clk_get(dev,
					      STMMAC_RESOURCE_NAME);
	if (IS_ERR(priv->plat->stmmac_clk)) {
		dev_warn(dev, "stmmac_clk clock failed\n");
		ret = PTR_ERR(priv->plat->stmmac_clk);
		priv->plat->stmmac_clk = NULL;
	} else {
		ret = clk_prepare_enable(priv->plat->stmmac_clk);
		if (ret)
			ETHQOSINFO("stmmac_clk clk failed\n");
	}

	priv->plat->pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(priv->plat->pclk)) {
		dev_warn(dev, "pclk clock failed\n");
		ret = PTR_ERR(priv->plat->pclk);
		priv->plat->pclk = NULL;
		goto error_pclk_get;
	} else {
		ret = clk_prepare_enable(priv->plat->pclk);
		if (ret) {
			ETHQOSINFO("pclk clk failed\n");
			goto error_pclk_get;
		}
	}

	ethqos->rgmii_clk = devm_clk_get(dev, "rgmii");
	if (IS_ERR(ethqos->rgmii_clk)) {
		dev_warn(dev, "rgmii clock failed\n");
		ret = PTR_ERR(ethqos->rgmii_clk);
		goto error_rgmii_get;
	} else {
		ret = clk_prepare_enable(ethqos->rgmii_clk);
		if (ret) {
			ETHQOSINFO("rgmmi clk failed\n");
			goto error_rgmii_get;
		}
	}
	return 0;

error_rgmii_get:
	clk_disable_unprepare(priv->plat->pclk);
error_pclk_get:
	clk_disable_unprepare(priv->plat->stmmac_clk);
	return ret;
}

static void qcom_ethqos_disable_clks(struct qcom_ethqos *ethqos, struct device *dev)
{
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);

	ETHQOSINFO("Enter\n");

	if (priv->plat->stmmac_clk)
		clk_disable_unprepare(priv->plat->stmmac_clk);

	if (priv->plat->pclk)
		clk_disable_unprepare(priv->plat->pclk);

	if (ethqos->rgmii_clk)
		clk_disable_unprepare(ethqos->rgmii_clk);

	ETHQOSINFO("Exit\n");
}

static int qcom_ethqos_hib_restore(struct device *dev)
{
	struct qcom_ethqos *ethqos;
	struct stmmac_priv *priv;
	struct net_device *ndev = NULL;
	int ret = 0;

	if (of_device_is_compatible(dev->of_node, "qcom,emac-smmu-embedded"))
		return 0;

	ETHQOSINFO(" start\n");
	ethqos = get_stmmac_bsp_priv(dev);
	if (!ethqos)
		return -ENODEV;

	ndev = dev_get_drvdata(dev);

	if (!ndev)
		return -EINVAL;

	priv = netdev_priv(ndev);

	ret = ethqos_init_regulators(ethqos);
	if (ret)
		return ret;

	ret = ethqos_init_gpio(ethqos);
	if (ret)
		return ret;

	ret = qcom_ethqos_enable_clks(ethqos, dev);
	if (ret)
		return ret;

	ethqos_update_rgmii_clk_and_bus_cfg(ethqos, ethqos->speed);

	ethqos_set_func_clk_en(ethqos);

#ifdef DWC_ETH_QOS_CONFIG_PTP
	if (priv->plat->clk_ptp_ref) {
		ret = clk_prepare_enable(priv->plat->clk_ptp_ref);
		if (ret < 0)
			netdev_warn(priv->dev, "failed to enable PTP reference clock: %d\n", ret);
	}
	ret = stmmac_init_ptp(priv);
	if (ret == -EOPNOTSUPP) {
		netdev_warn(priv->dev, "PTP not supported by HW\n");
	} else if (ret) {
		netdev_warn(priv->dev, "PTP init failed\n");
	} else {
		clk_set_rate(priv->plat->clk_ptp_ref,
			     priv->plat->clk_ptp_rate);
	}

	ret = priv->plat->init_pps(priv);
#endif /* end of DWC_ETH_QOS_CONFIG_PTP */

	/* issue software reset to device */
	ret = stmmac_reset(priv, priv->ioaddr);
	if (ret) {
		dev_err(priv->device, "Failed to reset\n");
		return ret;
	}

	if (!(ndev->flags & IFF_UP)) {
		rtnl_lock();
		ndev->netdev_ops->ndo_open(ndev);
		ndev->flags |= IFF_UP;
		rtnl_unlock();
		ETHQOSINFO("calling open\n");
	}

	ETHQOSINFO("end\n");

	return ret;
}

static int qcom_ethqos_hib_freeze(struct device *dev)
{
	struct qcom_ethqos *ethqos;
	struct stmmac_priv *priv;
	int ret = 0;
	struct net_device *ndev = NULL;

	if (of_device_is_compatible(dev->of_node, "qcom,emac-smmu-embedded"))
		return 0;

	ethqos = get_stmmac_bsp_priv(dev);
	if (!ethqos)
		return -ENODEV;

	ndev = dev_get_drvdata(dev);

	if (!ndev)
		return -EINVAL;

	priv = netdev_priv(ndev);

	ETHQOSINFO("start\n");

	if (ndev->flags & IFF_UP) {
		rtnl_lock();
		ndev->flags &= ~IFF_UP;
		ndev->netdev_ops->ndo_stop(ndev);
		rtnl_unlock();
		ETHQOSINFO("calling netdev off\n");
	}

#ifdef DWC_ETH_QOS_CONFIG_PTP
	stmmac_release_ptp(priv);
#endif /* end of DWC_ETH_QOS_CONFIG_PTP */

	qcom_ethqos_disable_clks(ethqos, dev);

	ethqos_disable_regulators(ethqos);

	ethqos_free_gpios(ethqos);

	ETHQOSINFO("end\n");

	return ret;
}

void qcom_ethqos_getcursystime(struct timespec64 *ts)
{
	struct stmmac_priv *priv = NULL;
	u64 systime, ns, sec0, sec1;
	void __iomem *ioaddr;

	priv = qcom_ethqos_get_priv(pethqos);
	if (!priv) {
		pr_err("\nInvalid priv\n");
		return;
	}
	ioaddr = priv->ptpaddr;

	if (!ioaddr) {
		pr_err("\nIncorrect memory address\n");
		return;
	}

	/* Get the TSS value */
	sec1 = readl_relaxed(ioaddr + PTP_STSR);
	do {
		sec0 = sec1;
		/* Get the TSSS value */
		ns = readl_relaxed(ioaddr + PTP_STNSR);
		/* Get the TSS value */
		sec1 = readl_relaxed(ioaddr + PTP_STSR);
	} while (sec0 != sec1);
	/* systime in ns */
	systime = ns + (sec1 * 1000000000ULL);

	*ts = ns_to_timespec64(ns);
}

int qcom_ethqos_enable_hw_timestamp(struct hwtstamp_config *config)
{
	u32 snap_type_sel = 0;
	u32 ptp_over_ipv4_udp = 0;
	u32 ptp_over_ipv6_udp = 0;
	u32 ptp_over_ethernet = 0;
	u32 ts_master_en = 0;
	u32 ts_event_en = 0;
	u32 ptp_v2 = 0;
	u32 av_8021asm_en = 0;
	u32 value = 0;
	u32 tstamp_all = 0;
	bool xmac;
	u32 sec_inc = 0;
	u64 temp = 0;
	struct timespec64 now;
	struct stmmac_priv *priv;

	priv = qcom_ethqos_get_priv(pethqos);
	xmac = priv->plat->has_gmac4 || priv->plat->has_xgmac;

	if (!(priv->dma_cap.time_stamp || priv->adv_ts)) {
		netdev_alert(priv->dev, "No support for HW time stamping\n");
		priv->hwts_tx_en = 0;
		priv->hwts_rx_en = 0;

		return -EOPNOTSUPP;
	}

	if (qcom_ethqos_ipa_enabled() &&
	    config->rx_filter == HWTSTAMP_FILTER_ALL) {
		netdev_alert(priv->dev,
			     "No hw timestamping since ipa is enabled\n");
		return -EOPNOTSUPP;
	}

	/* reserved for future extensions */
	if (config->flags)
		return -EINVAL;

	if (config->tx_type != HWTSTAMP_TX_OFF &&
	    config->tx_type != HWTSTAMP_TX_ON)
		return -ERANGE;

	if (priv->adv_ts) {
		switch (config->rx_filter) {
		case HWTSTAMP_FILTER_NONE:
			/* time stamp no incoming packet at all */
			config->rx_filter = HWTSTAMP_FILTER_NONE;
			break;

		case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
			/* PTP v1, UDP, any kind of event packet */
			config->rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_EVENT;
			/* 'xmac' hardware can support Sync, Pdelay_Req and
			 * Pdelay_resp by setting bit14 and bits17/16 to 01
			 * This leaves Delay_Req timestamps out.
			 * Enable all events *and* general purpose message
			 * timestamping
			 */
			snap_type_sel = PTP_TCR_SNAPTYPSEL_1;
			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
			/* PTP v1, UDP, Sync packet */
			config->rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_SYNC;
			/* take time stamp for SYNC messages only */
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
			/* PTP v1, UDP, Delay_req packet */
			config->rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ;
			/* take time stamp for Delay_Req messages only */
			ts_master_en = PTP_TCR_TSMSTRENA;
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
			/* PTP v2, UDP, any kind of event packet */
			config->rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_EVENT;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			/* take time stamp for all event messages */
			snap_type_sel = PTP_TCR_SNAPTYPSEL_1;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
			/* PTP v2, UDP, Sync packet */
			config->rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_SYNC;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			/* take time stamp for SYNC messages only */
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
			/* PTP v2, UDP, Delay_req packet */
			config->rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			/* take time stamp for Delay_Req messages only */
			ts_master_en = PTP_TCR_TSMSTRENA;
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V2_EVENT:
			/* PTP v2/802.AS1 any layer, any kind of event packet */
			config->rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			snap_type_sel = PTP_TCR_SNAPTYPSEL_1;
			if (priv->synopsys_id < DWMAC_CORE_4_10)
				ts_event_en = PTP_TCR_TSEVNTENA;
			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			ptp_over_ethernet = PTP_TCR_TSIPENA;
			av_8021asm_en = PTP_TCR_AV8021ASMEN;
			break;

		case HWTSTAMP_FILTER_PTP_V2_SYNC:
			/* PTP v2/802.AS1, any layer, Sync packet */
			config->rx_filter = HWTSTAMP_FILTER_PTP_V2_SYNC;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			/* take time stamp for SYNC messages only */
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			ptp_over_ethernet = PTP_TCR_TSIPENA;
			av_8021asm_en = PTP_TCR_AV8021ASMEN;
			break;

		case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
			/* PTP v2/802.AS1, any layer, Delay_req packet */
			config->rx_filter = HWTSTAMP_FILTER_PTP_V2_DELAY_REQ;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			/* take time stamp for Delay_Req messages only */
			ts_master_en = PTP_TCR_TSMSTRENA;
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			ptp_over_ethernet = PTP_TCR_TSIPENA;
			av_8021asm_en = PTP_TCR_AV8021ASMEN;
			break;

		case HWTSTAMP_FILTER_NTP_ALL:
		case HWTSTAMP_FILTER_ALL:
			/* time stamp any incoming packet */
			config->rx_filter = HWTSTAMP_FILTER_ALL;
			tstamp_all = PTP_TCR_TSENALL;
			break;

		default:
			return -ERANGE;
		}
	} else {
		switch (config->rx_filter) {
		case HWTSTAMP_FILTER_NONE:
			config->rx_filter = HWTSTAMP_FILTER_NONE;
			break;
		default:
			/* PTP v1, UDP, any kind of event packet */
			config->rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_EVENT;
			break;
		}
	}

	priv->hwts_rx_en = ((config->rx_filter == HWTSTAMP_FILTER_NONE) ? 0 : 1);
	priv->hwts_tx_en = config->tx_type == HWTSTAMP_TX_ON;

	if (!priv->hwts_tx_en && !priv->hwts_rx_en) {
		stmmac_config_hw_tstamping(priv, priv->ptpaddr, 0);
	} else {
		value = (PTP_TCR_TSENA | PTP_TCR_TSCFUPDT | PTP_TCR_TSCTRLSSR |
			 tstamp_all | ptp_v2 | ptp_over_ethernet |
			 ptp_over_ipv6_udp | ptp_over_ipv4_udp | ts_event_en |
			 ts_master_en | snap_type_sel | av_8021asm_en);
		stmmac_config_hw_tstamping(priv, priv->ptpaddr, value);

		/* program Sub Second Increment reg */
		stmmac_config_sub_second_increment(priv,
						   priv->ptpaddr, priv->plat->clk_ptp_req_rate,
						   xmac, &sec_inc);

		/* Store sub second increment and flags for later use */
		priv->sub_second_inc = sec_inc;
		priv->systime_flags = value;

		/* calculate default added value:
		 * formula is :
		 * addend = (2^32)/freq_div_ratio;
		 * where, freq_div_ratio = 1e9ns/sec_inc
		 */
		temp = (u64)((u64)priv->plat->clk_ptp_req_rate << 32);
		priv->default_addend = div_u64(temp, priv->plat->clk_ptp_rate);
		stmmac_config_addend(priv, priv->ptpaddr, priv->default_addend);

		/* initialize system time */
		ktime_get_real_ts64(&now);

		/* lower 32 bits of tv_sec are safe until y2106 */
		stmmac_init_systime(priv, priv->ptpaddr,
				    (u32)now.tv_sec, now.tv_nsec);
	}

	memcpy(&priv->tstamp_config, &config, sizeof(struct hwtstamp_config));
	return 0;
}
MODULE_DEVICE_TABLE(of, qcom_ethqos_match);

static const struct dev_pm_ops qcom_ethqos_pm_ops = {
	.freeze = qcom_ethqos_hib_freeze,
	.restore = qcom_ethqos_hib_restore,
	.thaw = qcom_ethqos_hib_restore,
	.suspend = qcom_ethqos_suspend,
	.resume = qcom_ethqos_resume,
};

static struct platform_driver qcom_ethqos_driver = {
	.probe  = qcom_ethqos_probe,
	.remove = qcom_ethqos_remove,
	.driver = {
		.name           = DRV_NAME,
		.pm		= &qcom_ethqos_pm_ops,
		.of_match_table = of_match_ptr(qcom_ethqos_match),
	},
};

static int __init qcom_ethqos_init_module(void)
{
	int ret = 0;

	ETHQOSDBG("enter\n");

	ret = platform_driver_register(&qcom_ethqos_driver);
	if (ret < 0) {
		ETHQOSINFO("qcom-ethqos: Driver registration failed");
		return ret;
	}

	ETHQOSDBG("Exit\n");

	return ret;
}

static void __exit qcom_ethqos_exit_module(void)
{
	ETHQOSDBG("Enter\n");

	platform_driver_unregister(&qcom_ethqos_driver);

	ETHQOSDBG("Exit\n");
}

/*!
 * \brief Macro to register the driver registration function.
 *
 * \details A module always begin with either the init_module or the function
 * you specify with module_init call. This is the entry function for modules;
 * it tells the kernel what functionality the module provides and sets up the
 * kernel to run the module's functions when they're needed. Once it does this,
 * entry function returns and the module does nothing until the kernel wants
 * to do something with the code that the module provides.
 */

module_init(qcom_ethqos_init_module)

/*!
 * \brief Macro to register the driver un-registration function.
 *
 * \details All modules end by calling either cleanup_module or the function
 * you specify with the module_exit call. This is the exit function for modules;
 * it undoes whatever entry function did. It unregisters the functionality
 * that the entry function registered.
 */

module_exit(qcom_ethqos_exit_module)

MODULE_DESCRIPTION("Qualcomm ETHQOS driver");
MODULE_LICENSE("GPL v2");
