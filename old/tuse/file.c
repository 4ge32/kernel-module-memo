#include "tuse_i.h"

#include <linux/file.h>
#include <linux/xattr.h>

static const struct file_operations tuse_file_operations = {
	.read_iter = generic_file_read_iter,
};

void tuse_init_file_inode(struct inode *inode)
{
	inode->i_fop = &tuse_file_operations;
}
