#include "tuse_i.h"

#include <linux/file.h>
#include <linux/xattr.h>

union tuse_dentry {
	u64 time;
	struct rcu_head rcu;
};

static int tuse_dentry_init(struct dentry *dentry)
{
	dentry->d_fsdata = kzalloc(sizeof(union tuse_dentry), GFP_KERNEL);

	return dentry->d_fsdata ? 0 : -ENOMEM;
}
static void tuse_dentry_release(struct dentry *dentry)
{
	union tuse_dentry *td = dentry->d_fsdata;

	kfree_rcu(td, rcu);
}

const struct dentry_operations tuse_dentry_operations = {
	//.d_revalidate	= tuse_dentry_revalidate,
	.d_init		= tuse_dentry_init,
	.d_release	= tuse_dentry_release,
};

const struct dentry_operations tuse_root_dentry_operations = {
	.d_init    = tuse_dentry_init,
	.d_release = tuse_dentry_release,
};

static int tuse_setattr(struct dentry *entry, struct iattr *attr)
{
	return 0;
}


static const struct inode_operations tuse_common_inode_operations = {
	.setattr	= tuse_setattr,
};

void tuse_init_common(struct inode *inode)
{
	inode->i_op = &tuse_common_inode_operations;
}
