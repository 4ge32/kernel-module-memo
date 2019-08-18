#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/swap.h>

struct hoge {
	struct rcu_head hoge_rcu;
	struct hoge     *down;
	char    name[6];
	struct hoge     *rcu_tmp;
};

static  struct hoge a;
static  struct hoge b;
static  struct hoge c;

static void hoge_rcu_callback(struct rcu_head *head);

static  void    printk_hoge(char* msg, struct hoge* a)
{
	struct hoge *tmp;

	tmp = a->down;
	printk("%s:%s\n", msg, tmp->name);
}

static int __init hoge_rcu_init(void)
{
	strcpy(a.name, "hoge1");
	strcpy(b.name, "hoge2"); 
	a.down= &b;

	rcu_read_lock();
	memcpy((char *)&c, (char *)a.down, sizeof(struct hoge));
	strcpy(c.name, "hoge3");
	a.rcu_tmp = &c;
	rcu_read_unlock();


	printk_hoge("before call rcu", &a);
	call_rcu(&a.hoge_rcu, hoge_rcu_callback);
	schedule_timeout_interruptible(100);
	printk_hoge("after call rcu", &a);

	return -1;
}

static void hoge_rcu_callback(struct rcu_head *head)
{
	struct hoge *hoge =
		container_of(head, struct hoge, hoge_rcu);

	hoge->down = hoge->rcu_tmp;
}
