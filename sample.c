#include <linux/module.h>

static int __init init_sample_(void)
{
	return 0;
}

static void __exit exit_sample_(void)
{
}

MODULE_AUTHOR("Fumiya Shigemitsu");
MODULE_DESCRIPTION("sample: -");
MODULE_LICENSE("GPL");

module_init(init_sample_)
module_exit(exit_sample_)
