/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2014 KYOCERA Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 and
 *  only version 2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>

#include <linux/slab.h>
#include "dnand_access.h"
#include <linux/dnand_cdev_driver.h>
#include <linux/syscore_ops.h>
#include <linux/delay.h>

struct dnand_access_device_t {
	struct miscdevice misc;
};

typedef struct {
	char d1;
	char d2;
} dnand_access_data ;


typedef struct {
	struct work_struct  dnand_access_work;
	int wait_flg;
	dnand_access_data dad;
} dnand_work_struct;


static void local_dnand_read(dnand_access_data *data)
{
	dnand_data_type ddt;

	ddt.cid = DNAND_ID_KERNEL_51;
	ddt.offset = 0;
	ddt.pbuf = (uint8_t *)data;
	ddt.size = sizeof(dnand_access_data);

	if(kdnand_id_read(ddt.cid, ddt.offset, ddt.pbuf, ddt.size))
	{
		data->d1 = -1;
	}
}

static void local_dnand_write(dnand_access_data *data)
{
	dnand_data_type ddt;

	ddt.cid = DNAND_ID_KERNEL_51;
	ddt.offset = 0;
	ddt.pbuf = (uint8_t *)data;
	ddt.size = sizeof(dnand_access_data);

	if(kdnand_id_write(ddt.cid, ddt.offset, ddt.pbuf, ddt.size) == 0)
	{
		data->d1 = 0;
	} else {
		data->d1 = -1;
	}
}


static void dnand_access_read_notify(struct work_struct *work)
{
	dnand_work_struct *dnand_work;

	dnand_work = container_of(work, dnand_work_struct, dnand_access_work);
	local_dnand_read(&dnand_work->dad);

	dnand_work->wait_flg = 0;
}

static void dnand_access_write_notify(struct work_struct *work)
{
	dnand_work_struct *dnand_work;

	dnand_work = container_of(work, dnand_work_struct, dnand_access_work);
	local_dnand_read(&dnand_work->dad);
	if(dnand_work->dad.d1 != -1) 
	{
		dnand_work->dad.d1 = 1;
		local_dnand_write(&dnand_work->dad);
	}

	dnand_work->wait_flg = 0;
}

static long dnand_access_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	unsigned long result = -EFAULT;
	dnand_work_struct *dnand_work;

	switch(cmd)
	{
		case IOCTL_DNAND_ACCESS_READ_CMD:
			dnand_work = kmalloc(sizeof(dnand_work_struct), GFP_KERNEL);
			if(dnand_work != NULL)
			{
				dnand_work->wait_flg = 1;
				INIT_WORK(&dnand_work->dnand_access_work, dnand_access_read_notify);
				schedule_work(&dnand_work->dnand_access_work);
				while(dnand_work->wait_flg)
				{
					usleep(100000);
				}
				result = copy_to_user((dnand_access_data *)arg,&dnand_work->dad,sizeof(dnand_access_data));
				kfree(dnand_work);
			} else {
				dnand_access_data  data = {-1, -1};
				result = copy_to_user((dnand_access_data __user *)arg,&data,sizeof(dnand_access_data));
			}
			break;

		case IOCTL_DNAND_ACCESS_WRITE_CMD:
			dnand_work = kmalloc(sizeof(dnand_work_struct), GFP_KERNEL);
			if(dnand_work != NULL)
			{
				dnand_work->wait_flg = 1;
				INIT_WORK(&dnand_work->dnand_access_work, dnand_access_write_notify);
				schedule_work(&dnand_work->dnand_access_work);
				while(dnand_work->wait_flg)
				{
					usleep(100000);
				}
				result = copy_to_user((dnand_access_data __user *)arg,&dnand_work->dad,sizeof(dnand_access_data));
				kfree(dnand_work);
			} else {
				dnand_access_data  data = {-1, -1};
				result = copy_to_user((dnand_access_data __user *)arg,&data,sizeof(dnand_access_data));
			}
			break;
		default:
			break;
	}
	return 0;
}

static int dnand_access_open(struct inode *ip, struct file *fp)
{
	return nonseekable_open(ip, fp);
}

static int dnand_access_release(struct inode *ip, struct file *fp)
{
	return 0;
}

static const struct file_operations dnand_access_fops = {
	.owner = THIS_MODULE,
	.open = dnand_access_open,
	.unlocked_ioctl = dnand_access_ioctl,
	.release = dnand_access_release,
};

static struct dnand_access_device_t dnand_access_device = {
	.misc = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "dnand_access",
		.fops = &dnand_access_fops,
	}
};

static void __exit dnand_access_exit(void)
{
	misc_deregister(&dnand_access_device.misc);
}

static int __init dnand_access_init(void)
{
	int ret;

	ret = misc_register(&dnand_access_device.misc);
	
	return ret;
}

module_init(dnand_access_init);
module_exit(dnand_access_exit);

