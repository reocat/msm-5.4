#include <linux/rpmsg.h>

#include <linux/module.h>
#include <linux/platform_device.h>

#include <plat/rpmsg_compat.h>

extern struct platform_device ambveth_device;
extern void ambveth_enqueue(void *priv, void *data, int len);

//static struct rpmsg_channel *g_rpdev;
static struct rpmsg_device *g_rpdev;

int ambveth_do_send(void *data, int len)
{
	return rpmsg_trysend(g_rpdev->ept, data, len);
}

//static void rpmsg_veth_server_cb(struct rpmsg_device *rpdev, void *data, int len,
static int rpmsg_veth_server_cb(struct rpmsg_device *rpdev, void *data, int len,
			void *priv, u32 src)
{
	ambveth_enqueue(priv, data, len);
	return 0;
}

static int rpmsg_veth_server_probe(struct rpmsg_device *rpdev)
{
	int ret = 0;

	struct rpmsg_ns_msg nsm;
	////struct rpmsg_channel_info chinfo;

	platform_device_register(&ambveth_device);
	

	rpdev->ept->priv = &ambveth_device;

	////strncpy(chinfo.name, rpdev->id.name, sizeof(chinfo.name));
	////chinfo.dst = rpdev->dst;

	nsm.addr = rpdev->dst;
	memcpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
	nsm.flags = 0;

	g_rpdev = rpdev;
	rpmsg_send(rpdev->ept, &nsm, sizeof(nsm));
	////rpmsg_send(rpdev->ept, &chinfo, sizeof(chinfo));

	return ret;
}

static void rpmsg_veth_server_remove(struct rpmsg_device *rpdev)
{
}

static struct rpmsg_device_id rpmsg_veth_server_id_table[] = {
	{ .name	= "veth_ca9_b", },
	{ .name	= "veth_arm11", },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_veth_server_id_table);

struct rpmsg_driver rpmsg_veth_server_driver = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= rpmsg_veth_server_id_table,
	.probe		= rpmsg_veth_server_probe,
	.callback	= rpmsg_veth_server_cb,
	.remove		= rpmsg_veth_server_remove,
};

static int __init rpmsg_veth_server_init(void)
{
	return register_rpmsg_driver(&rpmsg_veth_server_driver);
}

static void __exit rpmsg_veth_server_fini(void)
{
	unregister_rpmsg_driver(&rpmsg_veth_server_driver);
}

module_init(rpmsg_veth_server_init);
module_exit(rpmsg_veth_server_fini);

MODULE_DESCRIPTION("RPMSG VETH");
