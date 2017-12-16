#include "tuse_i.h"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/string.h>
#include <linux/parser.h>
#include <linux/spinlock.h>
#include <linux/list.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Fumiya Shigemitsu");
MODULE_DESCRIPTION("kernel user space mmap");

#define TUSE_SUPER_MAGIC 0xEFF10
#define TUSE_DEFAULT_BLKSIZE 512

static struct kmem_cache *tuse_inode_cachep;

struct tuse_mount_data {
	int fd;
	kuid_t user_id;
	kgid_t group_id;
	unsigned fd_present:1;
	unsigned user_id_present:1;
	unsigned group_id_present:1;
	unsigned max_read;
	unsigned blksize;
};

enum {
	OPT_FD,
	OPT_USER_ID,
	OPT_GROUP_ID,
	OPT_ERR,
};

static const match_table_t tokens = {
	{OPT_FD,     	"fd=%u"},
	{OPT_USER_ID, 	"user_id=%u"},
	{OPT_GROUP_ID,  "group_id=%u"},
	{OPT_ERR,       NULL}
};

static int tuse_match_uint(substring_t *s, unsigned int *res)
{
	int err = -ENOMEM;
	char *buf = match_strdup(s);
	if (buf) {
		/* convert string to an unsigned int */
		err = kstrtouint(buf, 10, res);
		kfree(buf);
	}
	return err;
}

static int parse_tuse_opt(char *opt, struct tuse_mount_data *d, int is_dev)
{
	char *p;
	memset(d, 0, sizeof(struct tuse_mount_data));
	d->max_read = ~0;
	d->blksize = TUSE_DEFAULT_BLKSIZE;

	/* split a string int token */
	while ((p = strsep(&opt, ",")) != NULL) {
		int token;
		int value;
		unsigned uv;
		substring_t args[MAX_OPT_ARGS];
		if (!*p)
			continue;
		token = match_token(p, tokens, args);
		switch (token) {
		case OPT_FD:
			if (match_int(&args[0], &value))
				return 0;
			d->fd = value;
			d->fd_present = 1;
			break;
		case OPT_USER_ID:
			if (tuse_match_uint(&args[0], &uv))
				return 0;
			d->user_id = make_kuid(current_user_ns(), uv);
			if (!uid_valid(d->user_id))
				return 0;
			d->user_id_present = 1;
			break;
		case OPT_GROUP_ID:
			if (tuse_match_uint(&args[0], &uv))
				return 0;
			d->group_id = make_kgid(current_user_ns(), uv);
			if (!gid_valid(d->group_id))
				return 0;
			d->group_id_present = 1;
			break;
		default:
			return 0;
		}
	}

	if (!d->fd_present || !d->user_id_present || !d->group_id_present)
		return 0;

	return 1;
}

static const struct super_operations tuse_super_operations = {
	.alloc_inode  = tuse_alloc_inode,
};

static void tuse_iqueue_init(struct tuse_iqueue *tiq)
{
	memset(tiq, 0, sizeof(struct tuse_iqueue));
	init_waitqueue_head(&tiq->waitq);
	INIT_LIST_HEAD(&tiq->pending);
	INIT_LIST_HEAD(&tiq->interrupts);
	tiq->connected = 1;
}

void tuse_conn_init(struct tuse_conn *tc)
{
	memset(tc, 0, sizeof(*tc));
	spin_lock_init(&tc->lock);
	init_rwsem(&tc->killsb);
	/* Initialize atomic valuable */
	atomic_set(&tc->count, 1);
	atomic_set(&tc->dev_count, 1);
	init_waitqueue_head(&tc->blocked_waitq);
	init_waitqueue_head(&tc->reserved_req_waitq);
	tuse_iqueue_init(&tc->iq);
	INIT_LIST_HEAD(&tc->entry);
}

static void tuse_free_conn(struct tuse_conn *tc)
{
	//WARN_ON(!list_empty(&tc->devices));
	//kfree_rcu(tc, rcu);
}

static struct tuse_conn *tuse_conn_get(struct tuse_conn *tc)
{
	atomic_inc(&tc->count);
	return tc;
}

struct tuse_dev *tuse_dev_alloc(struct tuse_conn *tc)
{
	struct tuse_dev *tud;

	tud = kzalloc(sizeof(struct tuse_dev), GFP_KERNEL);
	if (tud) {
		tud->tc = tuse_conn_get(tc);
	}

	return tud;
}

static int tuse_fill_super(struct super_block *sb, void *data, int silent)
{
	struct tuse_dev *tud;
	struct tuse_conn *tc;
	struct tuse_mount_data d;
	struct file *file;
	int err;
	/* struct block_device *s_bdev */
	int is_bdev = sb->s_bdev != NULL;

	err = -EINVAL;
	/*
	 * Allow mandatory locks on an FS
	 */
	if (sb->s_flags & MS_MANDLOCK)
		goto err;

	sb->s_flags &= ~(MS_NOSEC | MS_I_VERSION);

	if (!parse_tuse_opt(data, &d, is_bdev))
		goto err;

	if (!is_bdev) {
		sb->s_blocksize = PAGE_SIZE;
		sb->s_blocksize_bits = PAGE_SHIFT;
	}
	sb->s_magic = TUSE_SUPER_MAGIC;
	sb->s_op = &tuse_super_operations;
	sb->s_xattr = tuse_xattr_handlers;
	/* Max file size */
	sb->s_maxbytes = MAX_LFS_FILESIZE;
	/* Granularity of c/m/atime in ns */
	sb->s_time_gran = 1;

	file = fget(d.fd);
	err = -EINVAL;
	if (!file)
		goto err;

	if ((file->f_op != &tuse_dev_operations) ||
			(file->f_cred->user_ns != &init_user_ns))
		goto err_fput;

	tc = kmalloc(sizeof(*tc), GFP_KERNEL);
	err = -ENOMEM;
	if (!tc)
		goto err_fput;

	tuse_conn_init(tc);
	tc->release = tuse_free_conn;

	tud = tuse_dev_alloc(tc);
err_fput:
	fput(file);
err:
	return 0;
}

static struct dentry *tuse_mount(struct file_system_type *fs_type,
			         int flags, const char *dev_name,
				 void *raw_data)
{
	return mount_nodev(fs_type, flags, raw_data, tuse_fill_super);
}

static void tuse_kill_sb_anon(struct super_block *sb)
{
	struct tuse_conn *fc = get_tuse_conn_super(sb);

	if (fc) {
		down_write(&fc->killsb);
		fc->sb = NULL;
		up_write(&fc->killsb);
	}

	kill_anon_super(sb);
}

static struct file_system_type tuse_fs_type = {
	.owner    = THIS_MODULE,
	.name     = "tuse",
	.fs_flags = FS_HAS_SUBTYPE,
	.mount    = tuse_mount,
	.kill_sb     = tuse_kill_sb_anon,
};

static struct dentry *tuse_mount_blk(struct file_system_type *fs_type,
				     int flags, const char *dev_name,
				     void *raw_data)
{
	return mount_bdev(fs_type, flags, dev_name, raw_data, tuse_fill_super);
}

static void tuse_kill_sb_blk(struct super_block *sb)
{
	struct tuse_conn *fc = get_tuse_conn_super(sb);

	if (fc) {
		/* lock for writing */
		down_write(&fc->killsb);
		fc->sb = NULL;
		/* release a write lock */
		up_write(&fc->killsb);
	}

	kill_block_super(sb);
}

static struct file_system_type tuseblk_fs_type = {
	.owner    = THIS_MODULE,
	.name     = "tuseblk",
	.mount    = tuse_mount_blk,
	.kill_sb  = tuse_kill_sb_blk,
	.fs_flags = FS_REQUIRES_DEV | FS_HAS_SUBTYPE,
};

static inline int register_tuseblk(void)
{
	return register_filesystem(&tuseblk_fs_type);
}

static inline int unregister_tuseblk(void)
{
	return unregister_filesystem(&tuseblk_fs_type);
}

static void tuse_inode_init_once(void *foo)
{
	struct inode *inode = foo;

	inode_init_once(inode);
}

static int __init tuse_fs_init(void)
{
	int err;

	tuse_inode_cachep = kmem_cache_create("tuse_inode",
					      sizeof(struct tuse_inode), 0,
					      SLAB_HWCACHE_ALIGN|SLAB_ACCOUNT,
					      tuse_inode_init_once);

	err = -ENOMEM;
	if (!tuse_inode_cachep)
		goto out;

	err = register_tuseblk();
	if (err)
		goto err_register_blk;

	return 0;
err_register_blk:
	kmem_cache_destroy(tuse_inode_cachep);
out:
	return err;
}

static void tuse_fs_cleanup(void)
{
	unregister_filesystem(&tuse_fs_type);
	unregister_tuseblk();

	/*
	 * Wait until all the in-flight RCUs are complete.
	 */
	rcu_barrier();
	kmem_cache_destroy(tuse_inode_cachep);
}

static int __init tuse_init(void)
{
	int res;

	res = tuse_fs_init();
	if (res)
		goto err;

	res = tuse_dev_init();
	if (res)
		goto err_fs_cleanup;

	return 0;

err_fs_cleanup:
	tuse_fs_cleanup();
err:
	return res;
}

static void __exit tuse_exit(void)
{
	printk(KERN_INFO "tuse exit\n");
	tuse_fs_cleanup();
	tuse_dev_cleanup();
}

module_init(tuse_init);
module_exit(tuse_exit);
