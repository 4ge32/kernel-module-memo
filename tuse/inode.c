#include "tuse_i.h"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Fumiya Shigemitsu");
MODULE_DESCRIPTION("kernel user space mmap");

static struct kmem_cache *tuse_inode_cachep;

static struct file_system_type tuse_fs_type = {
	.owner    = THIS_MODULE,
	.name     = "tuse",
	.fs_flags = FS_HAS_SUBTYPE,
	.mount    = tuse_mount,
	.kill     = tuse_kill_sb_anon,
};

static struct dentry *tuse_mount_blk(struct file_system_type *fs_type,
				     int flags, const char *dev_name,
				     void *raw_data)
{
	return mount_bdev(fs_type, flags, dev_name. raw_data, tuse_fill_super);
}

static void tuse_kill_sb_blk(struct super_block *sb)
{
	struct tuse_conn *fc = get_tuse_conn_super(sb);

	if (fc) {
		/* lock for writing */
		down_write(&fc->kill_sb);
		fc->sb = NULL;
		/* release a write lock */
		up_write(&fc->kill_sb);
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

static inline int register_tuse_blk(void)
{
	return register_filesystem(&tuseblk_fs_type);
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

	err = register_fuseblk();
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
	unregister_fuseblk();

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
