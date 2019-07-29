#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#define NODE_NAME "tgw"
#define SUCCESS 0

MODULE_LICENSE("GPL");

static int major_num = 0;
static unsigned char k_buf[1024];
static int data_size;
static int all_read = 0;

static int tgw_open(struct inode *inode, struct file *file){
	printk("tgw_open\n");
	return SUCCESS;
}

static int tgw_release(struct inode *inode, struct file *file){
	printk("tgw_release\n");
	return SUCCESS;
}

static ssize_t tgw_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos){
	int len;

	if (all_read == data_size) {
		all_read = 0;
		printk("read_end\n");
		return 0;
	}

	printk("continue\n");
	printk("tgw_read, count: %lu, data_size: %d\n", count, data_size);

	if (count > data_size){
		len = data_size;
	} else {
		len = count;
	}
	printk("len: %d\n", len);
	copy_to_user(buf, k_buf, len);
	all_read += len;

	return len;
}

static ssize_t tgw_write(struct file *file, const char __user *buf, size_t count, loff_t *f_pos) {
	printk("tgw_write, count = %lu\n", count);
	copy_from_user(k_buf, buf, count);
	data_size = count;

	return count;
}

struct file_operations kure_fops = {
	.owner = THIS_MODULE,
	.read = tgw_read,
	.open = tgw_open,
	.write = tgw_write,
	.release = tgw_release
};

static int tgw_init(void){
	major_num = register_chrdev(major_num, NODE_NAME, &kure_fops);
	printk("tgw_init\n");
	return SUCCESS;
}

static void tgw_exit(void){
	unregister_chrdev(major_num, NODE_NAME);
	printk("tgw_exit\n");
}

module_init(tgw_init);
module_exit(tgw_exit);
