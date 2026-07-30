// SPDX-License-Identifier: MIT
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "fw_filter.h"
#include "fw_uapi.h"

#define FW_DEVICE_NAME "fw"

static long fw_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fw_ruleset *set;
	int ret;

	switch (cmd) {
	case FW_IOC_SET_RULES:
		set = kmalloc(sizeof(*set), GFP_KERNEL);
		if (!set)
			return -ENOMEM;

		if (copy_from_user(set, (void __user *)arg, sizeof(*set))) {
			kfree(set);
			return -EFAULT;
		}

		ret = fw_rules_set(set);
		kfree(set);
		return ret;
	default:
		return -ENOTTY;
	}
}

static const struct file_operations fw_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= fw_ioctl,
};

static struct miscdevice fw_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= FW_DEVICE_NAME,
	.fops	= &fw_fops,
	.mode	= 0600,
};

static int __init fw_init(void)
{
	int ret;

	ret = misc_register(&fw_misc);
	if (ret) {
		pr_err("fw: misc_register failed: %d\n", ret);
		return ret;
	}

	ret = fw_filter_init();
	if (ret) {
		pr_err("fw: hook registration failed: %d\n", ret);
		misc_deregister(&fw_misc);
		return ret;
	}

	pr_info("fw: loaded, /dev/%s ready\n", FW_DEVICE_NAME);
	return 0;
}

static void __exit fw_exit(void)
{
	fw_filter_exit();
	misc_deregister(&fw_misc);
	pr_info("fw: unloaded\n");
}

module_init(fw_init);
module_exit(fw_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("Simple netfilter firewall controlled via /dev/fw");
MODULE_VERSION("0.1");