#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#define MODNAME "mmap_example"
#define N_MINOR 1

static dev_t dev_id;
static struct cdev c_dev;

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Fumiya Shigemitsu");
MODULE_DESCRIPTION("kernel user space mmap");

#ifndef VM_RESERVED
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

struct mmap_info
{
	char *data;
	int reference;
};

static void mmap_open(struct vm_area_struct *vma)
{
	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
	info->reference++;
}

static void mmap_close(struct vm_area_struct *vma)
{
	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
	info->reference--;
}

static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct mmap_info *info;

	info = (struct mmap_info *)vma->vm_private_data;
	if (!info->data)
	{
		printk("No data\n");
		return 0;
	}

	page = virt_to_page(info->data);

	get_page(page);
	vmf->page = page;

	return 0;
}

struct vm_operations_struct mmap_vm_ops =
{
	.open =     mmap_open,
	.close =    mmap_close,
	.fault =    mmap_fault,
};

static int my_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &mmap_vm_ops;
	vma->vm_flags |= VM_RESERVED;
	vma->vm_private_data = filp->private_data;
	mmap_open(vma);
	return 0;
}

static int mmapfop_close(struct inode *inode, struct file *filp)
{
	struct mmap_info *info = filp->private_data;

	free_page((unsigned long)info->data);
	kfree(info);
	filp->private_data = NULL;
	return 0;
}

static int mmapfop_open(struct inode *inode, struct file *fp)
{
	struct mmap_info *info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);

	if (!fp->private_data) {
		info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);
		/* obtain new memory */
		info->data = (char *)get_zeroed_page(GFP_KERNEL);
		memcpy(info->data, "hello from kernel this is file: ", 32);
		memcpy(info->data + 32, fp->f_path.dentry->d_name.name, strlen(fp->f_path.dentry->d_name.name));
		//printk(KERN_INFO "M:hello, %s\n", fp->f_path.dentry->d_name.name);
		fp->private_data = info;
	}

	return 0;
}

static ssize_t t_read(struct file *fp, char __user *buf, size_t len, loff_t *ppos)
{
	struct mmap_info *info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);

	if (!fp->private_data) {
		info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);
		/* obtain new memory */
		info->data = (char *)get_zeroed_page(GFP_KERNEL);
		memcpy(info->data, "hello from kernel this is file: ", 32);
		memcpy(info->data + 32, fp->f_path.dentry->d_name.name, strlen(fp->f_path.dentry->d_name.name));
		//printk(KERN_INFO "R:hello, %s\n", fp->f_path.dentry->d_name.name);
		fp->private_data = info;
	} else
		info = fp->private_data;

	len = strlen(info->data);
	copy_to_user(buf, info->data, len);

	return len;
}

static const struct file_operations mmap_fops = {
	.mmap = my_mmap,
	.open = mmapfop_open,
	.release = mmapfop_close,
	.read = t_read,
};

static int __init mmapexample_module_init(void)
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

	printk(KERN_INFO "modtest is loaded\n");
	printk(KERN_INFO "major = %d\n", MAJOR(dev_id));
	printk(KERN_INFO "minor = %d\n", MINOR(dev_id));

	printk(KERN_INFO "HELLO!\n");

	return 0;

}

static void __exit mmapexample_module_exit(void)
{
	cdev_del(&c_dev);
	unregister_chrdev_region(dev_id, N_MINOR);

	printk(KERN_INFO "BYE!\n");
}

module_init(mmapexample_module_init);
module_exit(mmapexample_module_exit);
