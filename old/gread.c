#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#define MODNAME "gread"
#define N_MINOR 1

static dev_t dev_id;
static struct cdev c_dev;

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Fumiya Shigemitsu");
MODULE_DESCRIPTION("try to use generic_file_read_iter");

#ifndef VM_RESERVED
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

struct this_data
{
	char *data;
};

static int gclose(struct inode *inode, struct file *filp)
{
	struct this_data *info = filp->private_data;

	free_page((unsigned long)info->data);
	kfree(info);
	filp->private_data = NULL;
	return 0;
}

static int gopen(struct inode *inode, struct file *fp)
{
	struct this_data *info = kmalloc(sizeof(struct this_data), GFP_KERNEL);
	printk(KERN_INFO "HELLO OPEN\n");

	if (!fp->private_data) {
		info = kmalloc(sizeof(struct this_data), GFP_KERNEL);
		/* obtain new memory */
		info->data = (char *)get_zeroed_page(GFP_KERNEL);
		memcpy(info->data, "hello from kernel this is file: ", 32);
		memcpy(info->data + 32, fp->f_path.dentry->d_name.name, strlen(fp->f_path.dentry->d_name.name));
		printk(KERN_INFO "O:hello, %s\n", fp->f_path.dentry->d_name.name);
		fp->private_data = info;
	}

	return 0;
}


static const struct file_operations mmap_fops = {
	.open = gopen,
	.release = gclose,
	.read_iter = generic_file_read_iter,
};

static int __init ex_module_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&dev_id, 0, N_MINOR, MODNAME);
	if (ret < 0)
		return ret;

	cdev_init(&c_dev, &mmap_fops);
	c_dev.owner = THIS_MODULE;

	ret = cdev_add(&c_dev, dev_id, N_MINOR);
	if (ret < 0)
		return ret;

	printk(KERN_INFO "gread is loaded\n");
	printk(KERN_INFO "major = %d\n", MAJOR(dev_id));
	printk(KERN_INFO "minor = %d\n", MINOR(dev_id));

	printk(KERN_INFO "HELLO!\n");

	return 0;

}

static void __exit ex_module_exit(void)
{
	cdev_del(&c_dev);
	unregister_chrdev_region(dev_id, N_MINOR);

	printk(KERN_INFO "BYE!\n");
}

module_init(ex_module_init);
module_exit(ex_module_exit);
