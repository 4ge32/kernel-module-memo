#include <linux/module.h>

static int __init mem_init(void) {
	return 0;
}

static void __exit mem_exit(void) {
}

module_license("GPL");
module_init(mem_init);
module_exit(mem_exit);
