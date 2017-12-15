#include <linux/module.h>
#include <linux/miscdevice.h>

MODULE_ALIAS_MISCDEV(MISC_DYNAMIC_MINOR);
MODULE_ALIAS("devname:tuse");
