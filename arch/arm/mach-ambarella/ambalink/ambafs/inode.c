#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/delay.h>
#include <asm/page.h>
#include <plat/ambalink_cfg.h>
#include "ambafs.h"

#define MAX_NR_PAGES 32

/*
 * invalidate the cache for a page
 */
static void invalidate_page(struct page *page)
{
	void *addr = page_address(page);

	__asm__ __volatile__ (
		"add r1, %0, #4096\n\t"
	"1:\n\t"
		"mcr p15, 0, %0, c7, c6, 1\n\t"
		"add %0, %0, #32\n\t"
		"cmp %0, r1\n\t"
		"blo 1b\n\t"
		: "+r" (addr)
		);
	dsb();
}

/*
 * RPMSG callback when read data is ready
 */
static void readpage_cb(void *priv, struct ambafs_msg *msg, int len)
{
	struct ambafs_io *io = (struct ambafs_io*) msg->parameter;
	int page_idx, nr_pages = io->total;

	//AMBAFS_DMSG("readpage_cb with count %d\n", nr_pages);
	for (page_idx = 0; page_idx < nr_pages; page_idx++) {
		struct page *page;
		page = ambalink_phys_to_page((phys_addr_t)io->bh[page_idx].addr);
		invalidate_page(page);
		SetPageUptodate(page);
		unlock_page(page);
	}
}

/*
 * prepare a RPMSG for read
 */
static void inline prepare_read_msg(struct ambafs_msg *msg, struct file *file)
{
	struct ambafs_io  *io  = (struct ambafs_io*)  msg->parameter;

	msg->cmd = AMBAFS_CMD_READ;
	io->fp = file->private_data;
	io->total = 0;
}

/*
 * insert a page info into READ message
 */
static int insert_page_info(struct ambafs_msg *msg, struct page *page)
{
	struct ambafs_io  *io  = (struct ambafs_io*)  msg->parameter;
	int idx = io->total++;

	io->bh[idx].addr = (void*)ambalink_page_to_phys(page);
	io->bh[idx].len = 4096;
	io->bh[idx].offset = page_offset(page);

	return sizeof(struct ambafs_io) + io->total * sizeof(struct ambafs_bh);
}

/*
 * read a page [async]
 */
static int ambafs_readpage(struct file *file, struct page *page)
{
	int buf[16], msg_len;
	struct ambafs_msg *msg = (struct ambafs_msg*) buf;

	//AMBAFS_DMSG("ambafs_readpage %d\n", (int)page_offset(page));
	prepare_read_msg(msg, file);
	msg_len = insert_page_info(msg, page);
	ambafs_rpmsg_send(msg, msg_len, readpage_cb, NULL);

	return 0;
}

/*
 * read a list of pages [async]
 *      @nr_pages is broken into chunks of size MAX_NR_PAGES which is the
 *      maximum number of pages ThreadX can handle in one message.
 */
static int ambafs_readpages(struct file *filp, struct address_space *mapping,
			struct list_head *pages, unsigned nr_pages)
{
	int buf[192], msg_len = 0;
	struct ambafs_msg *msg = (struct ambafs_msg*) buf;
	unsigned page_idx, io_pages;

	//AMBAFS_DMSG("ambafs_readpages %d\n", nr_pages);
	while (nr_pages) {
		struct page *page;
		loff_t offset;

		io_pages = min_t(unsigned, nr_pages, MAX_NR_PAGES);
		prepare_read_msg(msg, filp);

		// add first page
		page = list_entry(pages->prev, struct page, lru);
		list_del(&page->lru);
		offset = page_offset(page);
		if (!add_to_page_cache_lru(page, mapping,
						page->index, GFP_KERNEL)) {
			msg_len = insert_page_info(msg, page);
		} else {
			AMBAFS_EMSG("ambafs_readpages lru error\n");
		}
		page_cache_release(page);

		// add pages one by one if continuous
		for (page_idx = 1; page_idx < io_pages; page_idx++) {
			page = list_entry(pages->prev, struct page, lru);
			if (offset + PAGE_SIZE != page_offset(page)) {
				AMBAFS_DMSG("ambafs_readpages with holes\n");
				break;
			}
			offset += PAGE_SIZE;
			list_del(&page->lru);

			if (!add_to_page_cache_lru(page, mapping,
				      page->index, GFP_KERNEL)) {
				msg_len = insert_page_info(msg, page);
			} else {
				AMBAFS_EMSG("ambafs_readpages lru error\n");
			}
			page_cache_release(page);

		}

		nr_pages -= page_idx;
		if (msg_len)
			ambafs_rpmsg_send(msg, msg_len, readpage_cb, NULL);
	}

	return 0;
}

/*
 * RPMSG callback when write is done
 */
static void writepage_cb(void *priv, struct ambafs_msg *msg, int len)
{
	struct ambafs_io *io = (struct ambafs_io*) msg->parameter;
	int page_idx, nr_pages = io->total;

	//AMBAFS_DMSG("writepage_cb %d\t", nr_pages);
	for (page_idx = 0; page_idx < nr_pages; page_idx++) {
		struct page *page;
		page = ambalink_phys_to_page((phys_addr_t)io->bh[page_idx].addr);
		unlock_page(page);
		end_page_writeback(page);
	}
}

/*
 * send WRITE msg to remote
 */
static void perform_writepages(struct address_space *mapping,
				struct page **pages, int nr_pages, void *fp)
{
	int buf[192], page_idx, msg_len;
	struct ambafs_msg *msg = (struct ambafs_msg*) buf;
	struct ambafs_io  *io  = (struct ambafs_io*)  msg->parameter;

	for (page_idx = 0; page_idx < nr_pages; page_idx++) {
		struct page *page = pages[page_idx];

		lock_page(page);
		if (PageWriteback(page)) {
			BUG();
		}

		clear_page_dirty_for_io(page);
		set_page_writeback(page);
	        flush_dcache_page(page);

		io->bh[page_idx].addr = (void*)ambalink_page_to_phys(page);
		io->bh[page_idx].offset = page_offset(page);
		io->bh[page_idx].len = page->mapping->host->i_size - page_offset(page);
		io->bh[page_idx].len = (io->bh[page_idx].len < 4096) ? (io->bh[page_idx].len) : 4096;
	}

	msg->cmd = AMBAFS_CMD_WRITE;
	io->fp = fp;
	io->total = nr_pages;
	msg_len = sizeof(struct ambafs_io) + nr_pages * sizeof(struct ambafs_bh);
	ambafs_rpmsg_send(msg, msg_len, writepage_cb, NULL);
}

/*
 * write to multiple pages
 */
static int ambafs_writepages(struct address_space *mapping,
			struct writeback_control *wbc)
{
	struct page *pages[MAX_NR_PAGES];
	pgoff_t index, end;
	int tag, done = 0;
	void *fp;

	index = wbc->range_start >> PAGE_CACHE_SHIFT;
	end = wbc->range_end >> PAGE_CACHE_SHIFT;
	//AMBAFS_DMSG("%s %d %d\n", __FUNCTION__, (int)index, (int)end);

	if (mapping->private_data)
		fp = mapping->private_data;
	else
		fp = ambafs_remote_open(
			(struct dentry*)mapping->host->i_private, 0x1F);

	if (!fp)
		return -EPERM;

	if (wbc->sync_mode == WB_SYNC_ALL || wbc->tagged_writepages)
		tag = PAGECACHE_TAG_TOWRITE;
	else
		tag = PAGECACHE_TAG_DIRTY;

	if (wbc->sync_mode == WB_SYNC_ALL || wbc->tagged_writepages)
		tag_pages_for_writeback(mapping, index, end);
	while (!done && (index <= end)) {
		int i, nr_pages;

		nr_pages = find_get_pages_tag(mapping, &index, tag,
				MAX_NR_PAGES, pages);
		if (nr_pages == 0)
			break;

		for (i = 0; i < nr_pages; i++) {
			struct page *page = pages[i];

			if (page->index > end) {
				done = 1;
				break;
			}

			//AMBAFS_DMSG("write page %d\n", (int)page_index(page));
		        perform_writepages(mapping, &page, 1, fp);

			if (--wbc->nr_to_write <= 0 &&
				wbc->sync_mode == WB_SYNC_NONE) {
				done = 1;
				break;
			}
		}
		release_pages(pages, nr_pages, 0);
		cond_resched();
	}

	if (fp != mapping->private_data)
		ambafs_remote_close(fp);
	return 0;
}

/*
 * prepare a page for write
 */
static int ambafs_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
	struct page *page;
	pgoff_t index;

	index = pos >> PAGE_CACHE_SHIFT;
	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page) {
		AMBAFS_DMSG("%s: oom\n", __func__);
		return -ENOMEM;
	}

	*pagep = page;

	if (!PageUptodate(page) && len != PAGE_CACHE_SIZE) {
		int buf[16], msg_len;
		struct ambafs_msg *msg = (struct ambafs_msg*) buf;

		//AMBAFS_DMSG("Read-for-Write, pos=%d, len=%u\n", (int)pos, len);
		zero_user_segment(page, pos, PAGE_CACHE_SIZE);

		prepare_read_msg(msg, file);
		msg_len = insert_page_info(msg, page);
		ambafs_rpmsg_exec(msg, msg_len);
		invalidate_page(page);
	}

	return 0;
}

/*
 * send multiple page for write
 */
static int ambafs_write_end(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *page, void *fsdata)
{
	struct page *pages[MAX_NR_PAGES];
	pgoff_t index = 0;
	int nr_pages;

	simple_write_end(file, mapping, pos, len, copied, page, fsdata);

	/* check if we have enough pages to warrant a write */
	nr_pages = find_get_pages_tag(mapping, &index, PAGECACHE_TAG_DIRTY,
				MAX_NR_PAGES, pages);

        if (nr_pages == MAX_NR_PAGES) {
		//AMBAFS_DMSG("%s: write %d page 0x%08x\n", __func__, nr_pages, (int)page_index(pages));
		perform_writepages(mapping, pages, nr_pages, file->private_data);
	}

	release_pages(pages, nr_pages, 0);

	return copied;
}

const struct address_space_operations ambafs_aops = {
	.readpage    = ambafs_readpage,
	.readpages   = ambafs_readpages,
	.writepages  = ambafs_writepages,
	.write_begin = ambafs_write_begin,
	.write_end   = ambafs_write_end,
};

