#include <linux/fs.h>
#include <linux/rwsem.h>
#include <linux/xattr.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/backing-dev-defs.h>
#include <linux/mm_types.h>

#define TUSE_ROOT_ID 1
#define TUSE_REQ_INLINE_PAGES 1

extern const struct file_operations tuse_dev_operations;
extern const struct dentry_operations tuse_root_dentry_operations;
extern const struct dentry_operations tuse_dentry_operations;

enum fuse_opcode {
	FUSE_SETXATTR = 4,
};

struct tuse_in_header {
	uint32_t len;
	uint32_t opcode;
	uint64_t nodeid;
	uint64_t uid;
	uint64_t gid;
	uint64_t pid;
};

struct tuse_out_header {
	uint32_t len;
	int32_t error;
	uint64_t unique;
};

/**
 * Request flags
 *
 * FR_ISREPLY:		set if the request has reply
 * FR_FORCE:		force sending of the request even if interrupted
 * FR_BACKGROUND:	request is sent in the background
 * FR_WAITING:		request is counted as "waiting"
 * FR_ABORTED:		the request was aborted
 * FR_INTERRUPTED:	the request has been interrupted
 * FR_LOCKED:		data is being copied to/from the request
 * FR_PENDING:		request is not yet in userspace
 * FR_SENT:		request is in userspace, waiting for an answer
 * FR_FINISHED:		request is finished
 * FR_PRIVATE:		request is on private list
 */
enum fuse_req_flag {
	FR_ISREPLY,
	FR_FORCE,
	FR_BACKGROUND,
	FR_WAITING,
	FR_ABORTED,
	FR_INTERRUPTED,
	FR_LOCKED,
	FR_PENDING,
	FR_SENT,
	FR_FINISHED,
	FR_PRIVATE,
};

struct tuse_attr {
	uint64_t ino;
	uint64_t size;
	uint32_t mode;
	uint32_t nlink;
};

/*  TUSE page descriptor */
struct tuse_page_desc {
	unsigned int length;
	unsigned int offset;
};

struct tuse_inode {
	struct inode inode;

	/* Unique ID, which identifies the inode between userspace
	 * and kernel */
	u64 nodeid;

	/*
	 * Number of lookups on this inode
	 */
	u64 nlookup;

	/** 64 bit inode number */
	u64 orig_ino;
};

/*  */
struct tuse_in {
	/* The request header */
	struct tuse_in_header h;
};

/* The request output */
struct tuse_out {
	/* The request header */
	struct tuse_out_header h;
};

struct tuse_iqueue {
	/* connection established */
	unsigned connected;

	/* The list of pending requests */
	struct list_head pending;

	/* pending interrupts */
	struct list_head interrupts;

	wait_queue_head_t waitq;

	/** Number of lookups on this inode */
	u64 nlookup;
};

struct tuse_req {
	/** This can be on either pending processing or io lists in
	    fuse_conn */
	struct list_head list;

	/** Entry on the interrupts list  */
	struct list_head intr_entry;

	/* The request input */
	struct tuse_in in;

	/* The request output */
	struct tuse_out out;

	/* page vetor */
	struct page **pages;

	/* page-descriptor vector */
	struct tuse_page_desc *page_descs;

	/** inline page-descriptor vector */
	struct tuse_page_desc inline_page_descs[TUSE_REQ_INLINE_PAGES];

	/** inline page-descriptor vector */
	struct page *inline_pages[1];

	/** Used to wake up the task waiting for completion of request*/
	wait_queue_head_t waitq;

	/** refcount */
	atomic_t count;

	/** size of the 'pages' array */
	unsigned max_pages;

	/** Request completion callback */
	//void (*end)(struct tuse_conn *, struct tuse_req *);

	/* Request flags, updated with test/set/clear_bit() */
	unsigned long flags;
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

	/* Reserved request for the DESTROY message */
	struct tuse_req *destroy_req;

	/* struct backing_dev_info bdi */
	struct backing_dev_info bdi;

	/* superblock for this connection*/
	struct super_block *sb;

};

struct tuse_pqueue {
	/** Connection established */
	unsigned connected;

	/** Lock protecting accessess to  members of this structure */
	spinlock_t lock;

	/** The list of requests being processed */
	struct list_head processing;

	/** The list of requests under I/O */
	struct list_head io;
};

struct tuse_dev {
	struct tuse_conn *tc;

	/** Processing queue */
	struct tuse_pqueue pq;

	struct list_head entry;
};

/*
 * s_fs_info is Filesystem private info
 */
static inline struct tuse_conn *get_tuse_conn_super(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct tuse_inode *get_tuse_inode(struct inode *inode)
{
	/*
	 *  cast a member of a structure out to
	 *  the containing structure
	 */
	return container_of(inode, struct tuse_inode, inode);
}

static inline u64 get_node_id(struct inode *inode)
{
	return get_tuse_inode(inode)->nodeid;
}

/*
 * Initialize the client device
 */
int tuse_dev_init(void);

/*
 * Cleanup the client device
 */
void tuse_dev_cleanup(void);

/*
 * Free a request
 */
void tuse_request_free(struct tuse_req *req);

extern const struct xattr_handler *tuse_xattr_handlers[];

/*
 * Initialize inode operations on regular files and special files
 */
void tuse_init_common(struct inode *inode);

/*
 * Initialize file operations on a regular file
 */
void tuse_init_file_inode(struct inode *inode);

/**
 * Allocate a request
 */
struct tuse_req *tuse_request_alloc(unsigned npages);
