#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/err.h>
#include <linux/remoteproc.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include <linux/kfifo.h>
#include <linux/delay.h>

#include <plat/ambcache.h>

// #define AMBXC_ACCEPT_DUMMY_BUF

typedef struct xnfs_info_s {

	char		*addr;
	int		size;
	int		count;
	void		*priv;
} xnfs_info_t;

struct xnfs_struct {

	int			id;
	struct kfifo		fifo;
	spinlock_t		lock;
	wait_queue_head_t	wnfs_queue;
	wait_queue_head_t	rnfs_queue;
	struct rpmsg_channel	*rpdev;
};

static struct xnfs_struct g_wnfs;
struct xnfs_struct g_rnfs;

ssize_t xnfs_read(char *buf, size_t len)
{
	struct xnfs_struct *xnfs = &g_wnfs;
	xnfs_info_t info;
	int n = 0;
	char *va;
	struct timespec ts, ts1, ts2;

	// printk(KERN_DEBUG "rpmsg: Linux request: 0x%08x\n", len);
	getnstimeofday(&ts1);
	if (kfifo_out_locked(&xnfs->fifo, &info,
			 sizeof(info), &xnfs->lock) < sizeof(info)) {

		// printk(KERN_DEBUG "rpmsg: Linux is waiting\n");
		wait_event_interruptible(xnfs->wnfs_queue,
					 kfifo_out_locked(&xnfs->fifo, &info,
							  sizeof(info), &xnfs->lock));
	}

	getnstimeofday(&ts2);
	if (!info.size) {
		printk(KERN_DEBUG "rpmsg: got EOS from uItron, and ack it back with NULL buffer\n");
		rpmsg_send(xnfs->rpdev, &n, sizeof(n));
		return 0;
	}

	va = (void *)ambarella_phys_to_virt((unsigned long)info.addr);
	n = info.size * info.count;
	ts = timespec_sub(ts2, ts1);

	printk(KERN_DEBUG "rpmsg: Linux got 0x%08x Bytes for %lu.%09lu Seconds\n",
	       info.count, ts.tv_sec, ts.tv_nsec);

	ambcache_flush_range((void *)va, n);
	memcpy(buf, va, n);

	/* ack to release fread of iTRON side */
	rpmsg_send(xnfs->rpdev, &n, sizeof(n));

	return n;
}

extern int output_callback(char **addr, int *size, int *count, void **priv);
extern void output_callback_done(void *);

void output_buf_queued(void)
{
	wake_up_interruptible(&g_rnfs.rnfs_queue);
}

static void rpmsg_rnfs_cb(struct rpmsg_channel *rpdev, void *data,
			  int len, void *priv)
{
	struct xnfs_struct *xnfs = priv;
	xnfs_info_t *req = data;
	xnfs_info_t avail;
	char *va;
	int n_avail;
	int n_req;
	struct timespec ts, ts1, ts2;

	if (!req->size) {
		printk(KERN_DEBUG "rpmsg: remote file stream closed\n");
		return;
	}

	getnstimeofday(&ts1);
	n_req = req->size * req->count;

	// printk(KERN_DEBUG "rpmsg: uItron reuest: 0x%x\n", n_req);
	va = (void *)ambarella_phys_to_virt((unsigned long)req->addr);

#ifdef AMBXC_ACCEPT_DUMMY_BUF
retry:
#endif
	if (!output_callback(&avail.addr, &avail.size, &avail.count, &avail.priv)) {

		// printk(KERN_DEBUG "rpmsg: uItron is waiting for kfifo\n");
		wait_event_interruptible(xnfs->rnfs_queue,
					 output_callback(&avail.addr,
							 &avail.size,
							 &avail.count,
							 &avail.priv));
	}

	n_avail = avail.size * avail.count;
	if (n_avail == 0) {
		output_callback_done(avail.priv);
#ifdef AMBXC_ACCEPT_DUMMY_BUF
		printk(KERN_DEBUG "rpmsg: got a dummy buffer, goto retry\n");
		goto retry;
#else
		printk(KERN_DEBUG "rpmsg: got a dummy buffer, take it as EOS\n");
#endif
	}
	if (n_avail > 0) {
		if (n_avail > n_req) {
			printk(KERN_ERR "rpmsg: uItron request 0x%08x, but Linux has 0x%08x!\n",
			       n_req, n_avail);

			n_avail = n_req;
		}
		memcpy(va, avail.addr, n_avail);
		output_callback_done(avail.priv);
		ambcache_clean_range((void *)va, n_avail);
	}

	getnstimeofday(&ts2);
	ts = timespec_sub(ts2, ts1);
	printk(KERN_DEBUG "rpmsg: uItron got 0x%08x Bytes for %lu.%09lu\n",
	       n_avail, ts.tv_sec, ts.tv_nsec);

	rpmsg_send(xnfs->rpdev, &n_avail, sizeof(n_avail));
}

static void rpmsg_wnfs_cb(struct rpmsg_channel *rpdev, void *data,
			  int len, void *priv)
{
	int ret = 0;
	struct xnfs_struct *xnfs = priv;

	BUG_ON(len != sizeof(xnfs_info_t));
	ret = kfifo_in_locked(&xnfs->fifo, (char *)data, len, &xnfs->lock);

	// printk("wnfs: uItron write 0x%08x to Linux\n", len);
	/*
	 * Currently, the following should never happen, since Itron only
	 * synchronously sends one buffer at a time.
	 *
	 * So there is at most 1 buffer at a time in the kfifo.
	 */
	while (ret < len) {

		xnfs_info_t dummy;

		printk("wnfs: kfifo full, the oldest buffer gets overwritten\n");

		ret = kfifo_out_locked(&xnfs->fifo, &dummy, sizeof(dummy), &xnfs->lock);
		/* ack to iTRON's fread that we have consumed (discard) it */
		rpmsg_send(xnfs->rpdev, &len, sizeof(len));

		ret = kfifo_in_locked(&xnfs->fifo, (char *)data, len, &xnfs->lock);
	}

	wake_up_interruptible(&xnfs->wnfs_queue);
}

static void rpmsg_xnfs_cb(struct rpmsg_channel *rpdev, void *data,
			  int len, void *priv, u32 src)
{
	struct xnfs_struct *xnfs = priv;

	if (xnfs->id == 0)
		return rpmsg_rnfs_cb(rpdev, data, len, priv);
	else if (xnfs->id == 1)
		return rpmsg_wnfs_cb(rpdev, data, len, priv);
}

static int xnfs_init(struct xnfs_struct *xnfs)
{
	int ret;

	spin_lock_init(&xnfs->lock);
	init_waitqueue_head(&xnfs->wnfs_queue);
	init_waitqueue_head(&xnfs->rnfs_queue);

	ret = kfifo_alloc(&xnfs->fifo, 4096 * 16, GFP_KERNEL);
	if (ret)
		return ret;

	return 0;
}

static struct rpmsg_device_id rpmsg_xnfs_id_table[] = {
	{ .name = "rnfs_arm11", },
	{ .name	= "wnfs_arm11", },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_xnfs_id_table);

static int rpmsg_xnfs_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;
	struct rpmsg_ns_msg nsm;
	struct xnfs_struct *xnfs = NULL;

	nsm.addr = rpdev->dst;
	memcpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
	nsm.flags = 0;

	if (!strcmp(rpdev->id.name, rpmsg_xnfs_id_table[0].name)) {
		xnfs = &g_rnfs;
		xnfs->id = 0;
		printk("RPMSG Ready from NFS Server [ARM11] is ready\n");

	} else if (!strcmp(rpdev->id.name, rpmsg_xnfs_id_table[1].name)) {
		xnfs = &g_wnfs;
		xnfs->id = 1;
		printk("RPMSG Write to NFS Server [ARM11] is ready\n");
	}

	xnfs_init(xnfs);
	xnfs->rpdev = rpdev;

	rpdev->ept->priv = xnfs;

	rpmsg_send(rpdev, &nsm, sizeof(nsm));

	return ret;
}

static void rpmsg_xnfs_remove(struct rpmsg_channel *rpdev)
{
}

struct rpmsg_driver rpmsg_xnfs_driver = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= rpmsg_xnfs_id_table,
	.probe		= rpmsg_xnfs_probe,
	.callback	= rpmsg_xnfs_cb,
	.remove		= rpmsg_xnfs_remove,
};
