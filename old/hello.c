#include <linux/module.h>       /* MODULE macro, THIS_MODULE */
#include <linux/moduleparam.h>
#include <linux/kernel.h>       /* printk() */
#include <linux/fs.h>		/* alloc_chrdev_region(), ... */
#include <linux/types.h>        /* dev_t */
#include <linux/kdev_t.h>       /* MAJOR() */
#include <linux/errno.h>	/* error codes */
#include <linux/cdev.h>         /* cdev_*() */
/* #include <linux/sched.h>        /\* current *\/ */
#include <linux/stat.h>         /* S_IRUGO, S_IWUSR */
#include <linux/slab.h>         /* kmalloc() */
#include <asm/uaccess.h>        /* copy_to_user, copy_from_user */
#include <linux/proc_fs.h>

MODULE_AUTHOR("Masayuki Ito");
MODULE_LICENSE("Dual BSD/GPL");

#ifndef SIMPLE_CHAR_BUFSIZE
#define SIMPLE_CHAR_BUFSIZE 10
#endif

#ifndef SIMPLE_CHAR_NR_DEVS
#define SIMPLE_CHAR_NR_DEVS 4 /* マイナーデバイス番号の総数 */
#endif

#ifndef SIMPLE_CHAR_MAJOR
#define SIMPLE_CHAR_MAJOR 0   /* 0の場合にはデバイス番号を動的に割り当てる */
#endif

#ifndef SIMPLE_CHAR_DEVNAME
#define SIMPLE_CHAR_DEVNAME "simple_char"
#endif

static int simple_char_bufsize = SIMPLE_CHAR_BUFSIZE;
static int simple_char_nr_devs = SIMPLE_CHAR_NR_DEVS;
static int simple_char_major   = SIMPLE_CHAR_MAJOR;
static int simple_char_minor   = 0;

struct simple_char_dev {
	char *buf;
	size_t write_size;
	struct cdev cdev;
};
static struct simple_char_dev *simple_char_devs = NULL;

static int simple_char_open(struct inode *inode, struct file *filep)
{
	struct simple_char_dev *dev;
	unsigned int minor = iminor(inode);
	dev = container_of(inode->i_cdev, struct simple_char_dev, cdev);
	/* readやwriteなどで、デバイス番号を参照出来るようにする */
	filep->private_data = dev;

	printk(KERN_INFO "simple_char: %s", __FUNCTION__);
	printk(KERN_INFO "  &inode->i_cdev = %p\n", &inode->i_cdev);
	printk(KERN_INFO "  dev = %p\n", dev);
	printk(KERN_INFO "  &simple_char_devs[%d] = %p\n",
			minor, simple_char_devs + minor);
	return 0;
}

static int simple_char_release(struct inode *inode, struct file *filep)
{
	printk(KERN_INFO "simple_char: %s", __FUNCTION__);
	return 0;
}

static ssize_t simple_char_read(struct file *filep, char __user *buf, 
		size_t count, loff_t *f_pos)
{
	struct simple_char_dev *dev = filep->private_data;
	size_t write_size = count > dev->write_size ? dev->write_size : count;

	printk(KERN_INFO "simple_char: %s", __FUNCTION__);

	if(copy_to_user(buf, dev->buf, write_size)) {
		return -EFAULT;
	}
	return write_size;
}

static ssize_t simple_char_write(struct file *filep, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct simple_char_dev *dev = filep->private_data;
	size_t write_size = count > simple_char_bufsize ? 
		dev->write_size : simple_char_bufsize;
	if(copy_from_user(dev->buf, buf, write_size)) {
		return -EFAULT;
	}
	dev->write_size = write_size;
	return write_size;
}




struct file_operations simple_char_fops = {
	.open      = simple_char_open,
	.release   = simple_char_release,
	.read      = simple_char_read,
	.write     = simple_char_write,
};

static int simple_char_setup_devnr(void)
{
	int result;
	dev_t dev;

	if(simple_char_major) {
		/* ユーザ指定のデバイス番号を登録する*/
		dev = MKDEV(simple_char_major, 
				simple_char_minor);
		result = register_chrdev_region(dev,
				simple_char_nr_devs,
				SIMPLE_CHAR_DEVNAME);
	} else {
		/* デバイス番号を動的に確保する */
		result = alloc_chrdev_region(&dev, 
				simple_char_minor,
				simple_char_nr_devs,
				SIMPLE_CHAR_DEVNAME);
		simple_char_major = MAJOR(dev);
	}
	if(result < 0) {
		printk(KERN_WARNING "simple_char: fail to get major %d\n",
				simple_char_major);
		return result;
	}

	return 0;
}

static int simple_char_clear_devnr(void)
{
	dev_t dev = MKDEV(simple_char_major, simple_char_minor);
	unregister_chrdev_region(dev, simple_char_nr_devs);
	return 0;
}

static void simple_char_cdev_add(struct cdev *cdev, int i)
{
	int devno = MKDEV(simple_char_major,
			simple_char_minor + i);
	int err;
	cdev_init(cdev, &simple_char_fops);
	cdev->owner = THIS_MODULE;
	err = cdev_add(cdev, devno, 1);
	if(err) {
		printk(KERN_WARNING
				"simple_char: fail to add cdev %d\n", i);
	}
}

static int simple_char_setup_cdev(void)
{
	int i;
	for(i = 0; i < simple_char_nr_devs; ++i) {
		simple_char_cdev_add(&simple_char_devs[i].cdev, i);
	}

	return 0;
}

static void simple_char_clear_cdev(void)
{
	int i;
	for(i = 0; i < simple_char_nr_devs; ++i) {
		cdev_del(&simple_char_devs[i].cdev);
	}
}

static int simple_char_setup_buf(void)
{
	int i;
	for(i = 0; i < simple_char_nr_devs; ++i) {
		simple_char_devs[i].write_size = 0;
		simple_char_devs[i].buf = (char*)kmalloc(simple_char_bufsize,
				GFP_KERNEL);
		simple_char_devs[i].buf[0] = '\0';
	}
	return 0;
}

static void simple_char_clear_buf(void)
{
	int i;
	for(i = 0; i < simple_char_nr_devs; ++i) {
		kfree(simple_char_devs[i].buf);
	}
}

//static int simple_char_read_proc(char *page, char **start, off_t offset, 
//		int count, int *eof, void *data)
//{
//	int i, len = 0;
//	for(i = 0; i < simple_char_nr_devs; ++i) {
//		struct simple_char_dev *dev = simple_char_devs + i;
//		len += sprintf(page + len, "%d: %s \n",
//				i, dev->buf);
//	}
//	*eof = 1;
//	return len;
//}
//
//static void simple_char_setup_proc(void)
//{
//	/* struct proc_dir_entry *entry; */
//	create_proc_read_entry("simple_char", 0 /* default mode */,
//			NULL /* 親ディレクトリ */,
//			simple_char_read_proc,
//			NULL);
//}
//
//static void simple_char_clear_proc(void)
//{
//	remove_proc_entry("simple_char", NULL /* 親ディレクトリ */);
//}


static int simple_char_init(void)
{
	int result;
	size_t size = sizeof(struct simple_char_dev) * simple_char_nr_devs;

	printk(KERN_INFO "simple_char: %s\n", __FUNCTION__);
	printk(KERN_INFO "  simple_char_bufsize = %d\n",
			simple_char_bufsize);

	simple_char_devs = (struct simple_char_dev*)kmalloc(size, GFP_KERNEL);

	result = simple_char_setup_buf();
	if(result) {
		goto fail;
	}

	result = simple_char_setup_devnr();
	if(result) {
		goto fail;
	}

	result = simple_char_setup_cdev();
	if(result) {
		goto fail;
	}

	//simple_char_setup_proc();

	return 0;

fail:
	printk(KERN_WARNING "simple_char: init fail\n");
	return result;
}

static void simple_char_exit(void)
{
	simple_char_clear_buf();
	simple_char_clear_cdev();
	simple_char_clear_devnr();
	kfree(simple_char_devs);
	//simple_char_clear_proc();
	return ;
}

module_param(simple_char_bufsize, int, S_IRUGO| S_IWUSR);
module_param(simple_char_nr_devs, int, S_IRUGO| S_IWUSR);
module_param(simple_char_major, int, S_IRUGO| S_IWUSR);
module_init(simple_char_init);
module_exit(simple_char_exit);


