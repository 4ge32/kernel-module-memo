#include <linux/module.h>
#include <linux/miscdevice.h>

MODULE_ALIAS_MISCDEV(MISC_DYNAMIC_MINOR);
MODULE_ALIAS("devname:tuse");

const struct file_operations tuse_dev_operations = {
	.owner = THIS_MODULE,
};

static struct miscdevice tuse_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "tuse",
	.fops  = &tuse_dev_operations,
};

int __init tuse_dev_init(void)
{
	int ret;

	ret = misc_register(&tuse_miscdev);

	return ret;
}

void tuse_dev_cleanup(void)
{
	misc_deregister(&tuse_miscdev);
}
