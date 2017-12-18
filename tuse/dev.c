#include "tuse_i.h"

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

MODULE_ALIAS_MISCDEV(MISC_DYNAMIC_MINOR);
MODULE_ALIAS("devname:tuse");

static struct kmem_cache *tuse_req_cachep;


static void tuse_request_init(struct tuse_req *req, struct page **pages,
			      struct tuse_page_desc *page_descs,
			      unsigned npages)
{
	memset(req, 0, sizeof(*req));
	memset(pages, 0, sizeof(*pages) * npages);
	memset(page_descs, 0, sizeof(*page_descs) * npages);
	INIT_LIST_HEAD(&req->list);
	INIT_LIST_HEAD(&req->intr_entry);
	init_waitqueue_head(&req->waitq);
	atomic_set(&req->count, 1);
	req->pages = pages;
	req->page_descs = page_descs;
	req->max_pages = npages;
	__set_bit(FR_PENDING, &req->flags);
}

static struct tuse_req *__tuse_request_alloc(unsigned npages, gfp_t flags)
{
	struct tuse_req *req = kmem_cache_alloc(tuse_req_cachep, flags);
	if (req) {
		struct page **pages;
		struct tuse_page_desc *page_descs;

		if (npages <= TUSE_REQ_INLINE_PAGES) {
			pages = req->inline_pages;
			page_descs = req->inline_page_descs;
		} else {
			pages = kmalloc(sizeof(struct page *) * npages, flags);
			page_descs = kmalloc(sizeof(struct tuse_page_desc) *
					     npages, flags);
		}

		if (!pages || !page_descs) {
			kfree(pages);
			kfree(page_descs);
			kmem_cache_free(tuse_req_cachep, req);
			return NULL;
		}

		tuse_request_init(req, pages, page_descs, npages);
	}
	return req;
}

struct tuse_req *tuse_request_alloc(unsigned npages)
{
	return __tuse_request_alloc(npages, GFP_KERNEL);
}

void tuse_request_free(struct tuse_req *req)
{
	if (req->pages != req->inline_pages) {
		kfree(req->pages);
		kfree(req->page_descs);
	}
	kmem_cache_free(tuse_req_cachep, req);
}

const struct file_operations tuse_dev_operations = {
	.owner = THIS_MODULE,
};

static struct miscdevice tuse_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "tuse",
	.fops  = &tuse_dev_operations,
};

int __init tuse_dev_init(void)
{
	int err = -ENOMEM;
	tuse_req_cachep = kmem_cache_create("tuse_request",
			                    sizeof(struct tuse_req),
					    0, 0, NULL);

	if (!tuse_req_cachep)
		return err;

	err = misc_register(&tuse_miscdev);
	if (err) {
		kmem_cache_destroy(tuse_req_cachep);
		return err;
	}

	return 0;
}

void tuse_dev_cleanup(void)
{
	misc_deregister(&tuse_miscdev);
}
