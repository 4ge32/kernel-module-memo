#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Fumiya Shigemitsu");
MODULE_DESCRIPTION("tiny fuse = tuse");

static struct file_operations tuse_dev_fops = {
	.owner = THIS_MODULE,
};

static struct miscdevice tes_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "tuse",
	.fops  = &tuse_dev_fops,
};

static int __init tuse_dev_init(void)
{
	int ret;

	ret = misc_register(&tes_dev);
	if (ret) {
		printk(KERN_INFO "fail to misc_register (MISC_DYNAMIC_MINOR)\n");
		return ret;
	}

	printk(KERN_INFO "dma test loaded\n");

	return 0;
}

static void __exit tuse_dev_cleanup(void)
{
	misc_deregister(&tes_dev);
	printk(KERN_INFO "Unloaded\n");
}

module_init(tuse_dev_init);
module_exit(tuse_dev_cleanup);
