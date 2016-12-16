/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2011 KYOCERA Corporation
 * (C) 2012 KYOCERA Corporation
 *
 * drivers/input/touchscreen/ts_ctrl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/

#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/input.h>

#include <linux/namei.h>
#include <linux/compiler.h>
#include <asm/thread_info.h>
#include <asm/string.h>
#include <asm/io.h>
#include <linux/vmalloc.h>

#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include "ts_ctrl.h"

int device_major;
struct class *device_class;
int ts_diag_start_flag = 0;
EXPORT_SYMBOL_GPL(ts_diag_start_flag);
struct ts_diag_type *diag_data;
EXPORT_SYMBOL_GPL(diag_data);
struct mutex diag_lock;
EXPORT_SYMBOL_GPL(diag_lock);
struct mutex file_lock;
EXPORT_SYMBOL_GPL(file_lock);
char ts_event_control;
EXPORT_SYMBOL_GPL(ts_event_control);
char ts_log_level = 0;
EXPORT_SYMBOL_GPL(ts_log_level);
char ts_log_file_enable = 0;
EXPORT_SYMBOL_GPL(ts_log_file_enable);
char ts_esd_recovery = 1;
EXPORT_SYMBOL_GPL(ts_esd_recovery);
char ts_config_switching = 1;
EXPORT_SYMBOL_GPL(ts_config_switching);

int ts_ctrl_init(struct cdev *device_cdev, const struct file_operations *fops)
{
	dev_t device_t = MKDEV(0, 0);
	struct device *class_dev_t = NULL;
	int ret;

	ret = alloc_chrdev_region(&device_t, 0, 1, "ts_ctrl");
	if (ret)
		goto error;

	device_major = MAJOR(device_t);

	cdev_init(device_cdev, fops);
	device_cdev->owner = THIS_MODULE;
	device_cdev->ops = fops;
	ret = cdev_add(device_cdev, MKDEV(device_major, 0), 1);
	if (ret)
		goto err_unregister_chrdev;

	device_class = class_create(THIS_MODULE, "ts_ctrl");
	if (IS_ERR(device_class)) {
		ret = -1;
		goto err_cleanup_cdev;
	};

	class_dev_t = device_create(device_class, NULL,
		MKDEV(device_major, 0), NULL, "ts_ctrl");
	if (IS_ERR(class_dev_t)) {
		ret = -1;
		goto err_destroy_class;
	}

	return 0;

err_destroy_class:
	class_destroy(device_class);
err_cleanup_cdev:
	cdev_del(device_cdev);
err_unregister_chrdev:
	unregister_chrdev_region(device_t, 1);
error:
	return ret;
}
EXPORT_SYMBOL_GPL(ts_ctrl_init);

int ts_ctrl_exit(struct cdev *device_cdev)
{
	dev_t device_t = MKDEV(device_major, 0);
	if (device_class) {
		device_destroy(device_class, MKDEV(device_major, 0));
		class_destroy(device_class);
	}
	if (device_cdev) {
		cdev_del(device_cdev);
		unregister_chrdev_region(device_t, 1);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ts_ctrl_exit);

MODULE_AUTHOR("KYOCERA Corporation");
MODULE_DESCRIPTION("Touchscreen Control");
MODULE_LICENSE("GPL v2");
