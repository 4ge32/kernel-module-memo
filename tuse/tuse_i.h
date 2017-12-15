#include <linux/fs.h>
#include <linux/rwsem.h>

struct tuse_inode {
	struct inode inode;
};

/*
 * tuse connection
 * This structure is created when the filesytem is mounted.
 * This structure is destroyed when the filesystem is unmounted.
 */

struct tuse_conn {
	struct rw_semaphore kill_sb;

	/* Device ID from superblock */
	dev_t dev;

	/* superblock for this connection*/
	struct super_block *sb;
};

/*
 * s_fs_info is Filesystem private info
 */
static inline struct tuse_conn *get_tuse_conn_super(struct super_block *sb)
{
	return sb->s_fs_info;
}

/*
 * Initialize the client device
 */
int tuse_dev_init(void);

/*
 * Cleanup the client device
 */
int tuse_dev_cleanup(void);
