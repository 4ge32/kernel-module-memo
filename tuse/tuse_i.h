#include <linux/fs.h>
#include <linux/rwsem.h>
#include <linux/xattr.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/list.h>

const struct file_operations tuse_dev_operations;

struct tuse_inode {
	struct inode inode;
};

struct tuse_iqueue {
	unsigned connected;

	/* The list of pending requests */
	struct list_head pending;

	/* pending interrupts */
	struct list_head interrupts;

	wait_queue_head_t waitq;
};

/*
 * tuse connection
 * This structure is created when the filesytem is mounted.
 * This structure is destroyed when the filesystem is unmounted.
 */

struct tuse_conn {
	/* Lock protecting accessess to members of this structure */
	spinlock_t lock;

	/* Refcount */
	atomic_t count;

	/* Number of tuse's devs */
	atomic_t dev_count;

	/* Input queue */
	struct tuse_iqueue iq;

	/* Read/Write semaphore to hold when accessing sb */
	struct rw_semaphore killsb;

	/* Entry on the tuse_conn_list */
	struct list_head entry;

	/* Device ID from superblock */
	dev_t dev;

	/* waitq for blocked connection */
	wait_queue_head_t blocked_waitq;

	/* waitq for reserved requests */
	wait_queue_head_t reserved_req_waitq;

	/* called on a final put */
	void (*release)(struct tuse_conn *);

	/* superblock for this connection*/
	struct super_block *sb;

};

struct tuse_dev {
	struct tuse_conn *tc;

	struct list_head entry;
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

extern const struct xattr_handler *tuse_xattr_handlers[];
