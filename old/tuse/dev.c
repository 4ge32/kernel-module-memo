#include "tuse_i.h"

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uio.h>
#include <linux/pipe_fs_i.h>
#include <linux/pagemap.h>

MODULE_ALIAS_MISCDEV(MISC_DYNAMIC_MINOR);
MODULE_ALIAS("devname:tuse");

static struct kmem_cache *tuse_req_cachep;

static struct tuse_dev *tuse_get_dev(struct file *file)
{
	/*
	 * Lockless access is OK, because file->private data is set
	 * once during mount and is valid until the file is released.
	 */
	return ACCESS_ONCE(file->private_data);
}

struct tuse_copy_state {
	int write;
	struct tuse_req *req;
	struct iov_iter *iter;
	struct pipe_buffer *pipebufs;
	struct pipe_buffer *currbuf;
	struct pipe_inode_info *pipe;
	unsigned long nr_segs;
	struct page *pg;
	unsigned len;
	unsigned offset;
	unsigned move_pages:1;
};

static void tuse_copy_init(struct tuse_copy_state *cs, int write,
			   struct iov_iter *iter)
{
	memset(cs, 0, sizeof(*cs));
	cs->write = write;
	cs->iter = iter;
}

/* Unmap and put previous page of userspace buffer */
static void tuse_copy_finish(struct tuse_copy_state *cs)
{
	if (cs->currbuf) {
		struct pipe_buffer *buf = cs->currbuf;

		if (cs->write)
			buf->len = PAGE_SIZE - cs->len;
		cs->currbuf = NULL;
	} else if (cs->pg) {
		if (cs->write) {
			flush_dcache_page(cs->pg);
			set_page_dirty_lock(cs->pg);
		}
		put_page(cs->pg);
	}
	cs->pg = NULL;
}

/* Do as much copy to/from userspace buffer as we can */
static int tuse_copy_do(struct tuse_copy_state *cs, void **val, unsigned *size)
{
	unsigned ncpy = min(*size, cs->len);
	if (val) {
		/* temporary kernel mapping */
		void *pgaddr = kmap_atomic(cs->pg);
		void *buf = pgaddr + cs->offset;

		if (cs->write)
			memcpy(buf, *val, ncpy);
		else
			memcpy(*val, buf, ncpy);

		kunmap_atomic(pgaddr);
		*val += ncpy;
	}
	*size -= ncpy;
	cs->len -= ncpy;
	cs->offest += ncpy;
	return ncpy;
}

static int tuse_copy_one(struct tuse_copy_state *cs, void *val, unsigned size)
{
	while (size) {
		if (!cs->len) {
			int err = tuse_copy_fill(cs);
			if (err)
				return err;
		}
		tuse_copy_do(cs, &val, &size);
	}
	return 0;
}

static int request_pending(struct tuse_iqueue *tiq)
{
	return !list_empty(&tiq->pending) || !list_empty(&tiq->interrupts);
}

/*
 * This function is called when a request is finished.  Either a reply
 * has arrived or it was aborted (and not yet sent) or some error
 * occurred during communication with userspace, or the device file
 * was closed.  The requester thread is woken up (if still waiting),
 * the 'end' callback is called if given, else the reference to the
 * request is released
 */
static void request_end(struct tuse_conn *tc, struct tuse_req *req)
{
}

/*
 * Read a single request into the userspace filesystem's buffer.  This
 * function waits until a request is available, then removes it from
 * the pending list and copies request data to userspace buffer.  If
 * no reply is needed (FORGET) or request has been aborted or there
 * was an error during the copying then it's finished by calling
 * request_end().  Otherwise add it to the processing list, and set
 * the 'sent' flag.
 */
static ssize_t tuse_dev_do_read(struct tuse_dev *tud, struct file *file,
				struct tuse_copy_state *cs, size_t nbytes)
{
	ssize_t err;
	struct tuse_conn *tc = tud->tc;
	struct tuse_iqueue *tiq = &tc->iq;
	struct tuse_pqueue *tpq = &tud->pq;
	struct tuse_req *req;
	struct tuse_in *in;
	unsigned reqsize;

restart:
	spin_lock(&tiq->waitq.lock);
	err = -EAGAIN;
	if ((file->f_flags & O_NONBLOCK) && &tiq->connected && tiq->connected &&
			!request_pending(tiq))
		goto err_unlock;

	/* sllep exclusively until a condition gets true */
	err = wait_event_interruptible_exclusive_locked(tiq->waitq,
				!tiq->connected || request_pending(tiq));
	if (err)
		goto err_unlock;

	err = -ENODEV;
	if (!tiq->connected)
		goto err_unlock;

	req = list_entry(tiq->pending.next, struct tuse_req, list);
	clear_bit(FR_PENDING, &req->flags);
	list_del_init(&req->list);
	spin_unlock(&tiq->waitq.lock);

	in = &req->in;
	reqsize = in->h.len;
	/* If request is too large, reply with an error and restart the read */
	if (nbytes < reqsize) {
		req->out.h.error = -EIO;
		/* SETXATTR is special, since it may contain too large data */
		if (in->h.opcode == FUSE_SETXATTR)
			req->out.h.error = -E2BIG;
		request_end(tc, req);
		goto restart;
	}
	spin_lock(&tpq->lock);
	list_add(&req->list, &tpq->io);
	spin_unlock(&tpq->lock);
	cs->req = req;
	err = tuse_copy_one(cs, &in->h, sizeof(in->h));
	tuse_copy_finish(cs);
	clear_bit(FR_LOCKED, &req->flags);
	if (!tpq->connected) {
		err = -ENODEV;
		goto out_end;
	}
	if (err) {
		req->out.h.error = -EIO;
		goto out_end;
	}
	if (!test_bit(FR_ISREPLY, &req->flags)) {
		err = reqsize;
		goto out_end;
	}
	list_move_tail(&req->list, &tpq->processing);
	spin_unlock(&tpq->lock);
	set_bit(FR_SENT, &req->flags);

	return reqsize;

out_end:
	if (!test_bit(FR_PRIVATE, &req->flags))
		list_del_init(&req->list);
	spin_unlock(&tpq->lock);
	request_end(tc, req);
	return err;
err_unlock:
	spin_unlock(&tiq->waitq.lock);
	return err;
}

static int tuse_dev_open(struct inode *inode, struct file *file)
{
	/*
	 * The fuse device's file's private_data is used to hold
	 * the fuse_conn(ection) when it is mounted, and is used to
	 * keep track of whether the file has been mounted already.
	 */
	file->private_data = NULL;
	return 0;
}

static ssize_t tuse_dev_read(struct kiocb *iocb, struct iov_iter *to)
{
	struct tuse_copy_state cs;
	struct file *file = iocb->ki_filp;
	struct tuse_dev *tud = tuse_get_dev(file);

	if (!tud)
		return -EPERM;

	if (!iter_is_iovec(to))
		return -EINVAL;

	tuse_copy_init(&cs, 1, to);

	return tuse_dev_do_read(tud, file, &cs, iov_iter_count(to));
}

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
	.owner     = THIS_MODULE,
	.open      = tuse_dev_open,
	.read_iter = tuse_dev_read,
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
