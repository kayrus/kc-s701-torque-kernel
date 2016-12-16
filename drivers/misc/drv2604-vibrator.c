/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2014 KYOCERA Corporation
 */
/* include/asm/mach-msm/htc_pwrsink.h
 *
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2011 Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define VIB_USE_REGULATOR
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include "timed_output.h"

enum vib_status
{
	VIB_STANDBY,
	VIB_ON,
	VIB_OFF,
};
enum vib_add_time_flag
{
	VIB_ADD_TIME_FLAG_OFF,
	VIB_ADD_TIME_FLAG_ON,
};

#define DEBUG_VIB_DRV2604				0

#define VIB_DRV_NAME					"DRV2604"
#define VIB_ON_WORK_NUM					(5)
#define I2C_RETRIES_NUM					(5)
#define I2C_WRITE_MSG_NUM				(1)
#define I2C_READ_MSG_NUM				(2)
#define VIB_STANDBY_DELAY_TIME			(1000)
#define VIB_TIME_MIN					(25)
#define VIB_TIME_MAX					(15000)
#define MSMGPIO_LIMTR_EN				(118)
#define MSMGPIO_LIMTR_INTRIG			(117)
#define VIB_I2C_EN_DELAY				(250)	/* us */


struct vib_on_work_data
{
	struct work_struct	work_vib_on;
	int					time;
};
struct drv2604_work_data {
	struct vib_on_work_data vib_on_work_data[VIB_ON_WORK_NUM];
	struct work_struct work_vib_off;
	struct work_struct work_vib_standby;
};
struct drv2604_data_t {
	struct i2c_client *drv2604_i2c_client;
	struct hrtimer vib_off_timer;
	struct hrtimer vib_standby_timer;
	int work_vib_on_pos;
	enum vib_status vib_cur_status;
	enum vib_add_time_flag add_time_flag;
};

#ifdef VIB_USE_REGULATOR
static struct regulator* vib_vdd = 0;
#endif
static struct mutex vib_mutex;
static u8 write_buf[2] = {0x00, 0x00};
static u8 read_buf[4] = {0x00, 0x00, 0x00, 0x00};
struct drv2604_work_data drv2604_work;
struct drv2604_data_t drv2604_data;

#define VIB_LOG(md, fmt, ... ) \
printk(md "[VIB] %s(%d): " fmt, __func__, __LINE__, ## __VA_ARGS__)
#if DEBUG_VIB_DRV2604
#define VIB_DEBUG_LOG(md, fmt, ... ) \
printk(md "[VIB] %s(%d): " fmt, __func__, __LINE__, ## __VA_ARGS__)
#else
#define VIB_DEBUG_LOG(md, fmt, ... )
#endif /* DEBUG_VIB_DRV2604 */


#define VIB_SET_REG(reg, data) { \
	write_buf[0] = reg; \
	write_buf[1] = data; \
	drv2604_i2c_write_data(drv2604_data.drv2604_i2c_client, write_buf, sizeof(write_buf)); }

#define VIB_GET_REG(reg) { \
	drv2604_i2c_read_data(drv2604_data.drv2604_i2c_client, reg, read_buf, 1); }



#define VIB_TEST
#ifdef VIB_TEST
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ioctl.h>

#define VIB_TEST_IOC_MAGIC 'v'
#define IOCTL_VIB_TEST_CTRL _IOWR(VIB_TEST_IOC_MAGIC, 1, vib_test_param)

#define VIB_TEST_SET_VOLTAGE	0x0001
#define VIB_TEST_CALIBRATION	0x0020
#define VIB_TEST_RD_REGISTER	0x0021
#define VIB_TEST_WR_REGISTER	0x0022

#define VIB_TEST_STATUS_SUCCESS	(0)
#define VIB_TEST_STATUS_FAIL	(-1)

typedef struct {
	u16 req_code;
	u8 data[4];
} vib_test_param;

/* for voltage */
typedef struct {
	u16 rated_vol;
	u16 clamp_vol;
} vib_test_set_voltage_req_data;

typedef struct {
	u16 status;
	u8 data[2];
} vib_test_rsp_voltage_data;

/* for register */
typedef struct {
	u16 reserved;
	u8 reg;
	u8 data;
} vib_test_set_rdwr_reg_req_data;

typedef struct {
	u16 reserved;
	u8 data[2];
} vib_test_rsp_rdwr_reg_data;

/* for calibration */
typedef struct {
	u8 data[4];
} vib_test_rsp_calibration_data;

#endif /* VIB_TEST */



static int drv2604_i2c_write_data(struct i2c_client *client, u8 *buf, u16 len)
{
	int ret = 0;
	int retry = 0;
	struct i2c_msg msg[I2C_WRITE_MSG_NUM];

	if (client == NULL || buf == NULL)
	{
		VIB_LOG(KERN_ERR, "client=0x%08x,buf=0x%08x\n",
				(unsigned int)client, (unsigned int)buf);
		return 0;
	}

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = len;
	msg[0].buf = buf;

	do
	{
		ret = i2c_transfer(client->adapter, msg, I2C_WRITE_MSG_NUM);
		VIB_DEBUG_LOG(KERN_INFO, "i2c_transfer(write) ret=%d\n", ret);
	} while ((ret != I2C_WRITE_MSG_NUM) && (++retry < I2C_RETRIES_NUM));

	if (ret != I2C_WRITE_MSG_NUM)
	{
		ret = -1;
		VIB_LOG(KERN_ERR, "i2c write error (try:%d)\n", retry);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

static int drv2604_i2c_read_data(struct i2c_client *client, u8 reg, u8 *buf, u16 len)
{
	int ret = 0;
	int retry = 0;
	u8 start_reg = 0;
	struct i2c_msg msg[I2C_READ_MSG_NUM];

	if (client == NULL || buf == NULL)
	{
		VIB_LOG(KERN_ERR, "client=0x%08x\n",
				(unsigned int)client);
		return 0;
	}

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;
	start_reg = reg;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;

	do
	{
		ret = i2c_transfer(client->adapter, msg, I2C_READ_MSG_NUM);
	} while ((ret != I2C_READ_MSG_NUM) && (++retry < I2C_RETRIES_NUM));

	if (ret != I2C_READ_MSG_NUM)
	{
		ret = -1;
		VIB_LOG(KERN_ERR, "i2c read error (try:%d)\n", retry);
	}
	else
	{
		ret = 0;
	}

	return ret;
}


/*
 * Initialization Procedure
 */
static void drv2604_initialization(void)
{
	VIB_DEBUG_LOG(KERN_INFO, "called.\n");

	/* After power up, wait at least 250us before the DRV2604 will accept I2C commands. */
	udelay(VIB_I2C_EN_DELAY);

	/* Assert the EN pin (logic high). The EN pin may be asserted any time during or after the 250us wait period. */
	gpio_set_value(MSMGPIO_LIMTR_EN, 1);
	udelay(VIB_I2C_EN_DELAY);

	VIB_SET_REG(0x16, 0x40);
	VIB_SET_REG(0x17, 0x70);
	VIB_SET_REG(0x18, 0x08);
	VIB_SET_REG(0x19, 0x99);
	VIB_SET_REG(0x1a, 0xbb);
	VIB_SET_REG(0x1b, 0x93);
	VIB_SET_REG(0x1c, 0xf5);
	VIB_SET_REG(0x1d, 0x80);
	VIB_SET_REG(0x1e, 0x20);
	VIB_SET_REG(0x03, 0x00);
	VIB_SET_REG(0x0c, 0x00);
	VIB_SET_REG(0x01, 0x03);

	gpio_set_value(MSMGPIO_LIMTR_EN, 0);

	VIB_DEBUG_LOG(KERN_INFO, "end.\n");
}


static void drv2604_set_vib(enum vib_status status, int time)
{
	enum vib_status cur_status = drv2604_data.vib_cur_status;

	VIB_DEBUG_LOG(KERN_INFO, "called. status=%d,time=%d,cur_status=%d\n",
							  status, time, cur_status);
	mutex_lock(&vib_mutex);

	switch (status) {
		case VIB_ON:
			VIB_DEBUG_LOG(KERN_INFO, "VIB_ON\n");
			/* STANDBY => ON */
			if (cur_status == VIB_STANDBY)
			{
				/* enable */
				gpio_set_value(MSMGPIO_LIMTR_EN, 1);
			}
			else
			{
				VIB_DEBUG_LOG(KERN_INFO, "VIB_ON standby cancel skip.\n");
			}

			/* OFF/STANDBY => ON */
			if (cur_status != VIB_ON)
			{
				/* start vibrator */
				gpio_set_value(MSMGPIO_LIMTR_INTRIG, 1);
			}
			else
			{
				VIB_DEBUG_LOG(KERN_INFO, "VIB_ON skip.\n");
			}
			/* Set vib off timer (ON => OFF) */
			VIB_DEBUG_LOG(KERN_INFO, "hrtimer_start(vib_off_timer). time=%d\n", time);
			hrtimer_start(&drv2604_data.vib_off_timer,
							ktime_set(time / 1000, 
							(time % 1000) * 1000000),
							HRTIMER_MODE_REL);

			drv2604_data.vib_cur_status = status;
			VIB_DEBUG_LOG(KERN_INFO, "set cur_status=%d\n", drv2604_data.vib_cur_status);
			break;
		case VIB_OFF:
			VIB_DEBUG_LOG(KERN_INFO, "VIB_OFF\n");
			/* ON => OFF */
			if (cur_status == VIB_ON)
			{
				/* stop vibrator */
				gpio_set_value(MSMGPIO_LIMTR_INTRIG, 0);
				drv2604_data.vib_cur_status = status;
				VIB_DEBUG_LOG(KERN_INFO, "set cur_status=%d\n", drv2604_data.vib_cur_status);

				/* Set vib standby timer (OFF => STANDBY) */
				VIB_DEBUG_LOG(KERN_INFO, "hrtimer_start(vib_standby_timer).\n");
				hrtimer_start(&drv2604_data.vib_standby_timer,
								ktime_set(VIB_STANDBY_DELAY_TIME / 1000, 
								(VIB_STANDBY_DELAY_TIME % 1000) * 1000000),
								HRTIMER_MODE_REL);
			}
			else
			{
				VIB_DEBUG_LOG(KERN_INFO, "VIB_OFF skip.\n");
			}
			break;
		case VIB_STANDBY:
			VIB_DEBUG_LOG(KERN_INFO, "VIB_STANDBY\n");
			/* OFF => STANDBY */
			if (cur_status == VIB_OFF)
			{
				/* disable */
				gpio_set_value(MSMGPIO_LIMTR_EN, 0);
				drv2604_data.vib_cur_status = status;
				VIB_DEBUG_LOG(KERN_INFO, "set cur_status=%d\n", drv2604_data.vib_cur_status);
			}
			else
			{
				VIB_DEBUG_LOG(KERN_INFO, "VIB_STANDBY skip.\n");
			}
			break;
		default:
			VIB_LOG(KERN_ERR, "parameter error. status=%d\n", status);
			break;
	}
	mutex_unlock(&vib_mutex);
	return;
}

static void drv2604_vib_on(struct work_struct *work)
{
	struct vib_on_work_data *work_data = container_of
										(work, struct vib_on_work_data, work_vib_on);

	VIB_DEBUG_LOG(KERN_INFO, "called. work=0x%08x\n", (unsigned int)work);
	VIB_DEBUG_LOG(KERN_INFO, "work_data=0x%08x,time=%d\n",
					(unsigned int)work_data, work_data->time);
	drv2604_set_vib(VIB_ON, work_data->time);

	return;
}

static void drv2604_vib_off(struct work_struct *work)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. work=0x%08x\n", (unsigned int)work);
	drv2604_set_vib(VIB_OFF, 0);
	return;
}

static void drv2604_vib_standby(struct work_struct *work)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. work=0x%08x\n", (unsigned int)work);

	drv2604_set_vib(VIB_STANDBY, 0);
	return;
}

static void drv2604_timed_vib_on(struct timed_output_dev *dev, int timeout_val)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. dev=0x%08x, timeout_val=%d\n",
					(unsigned int)dev, timeout_val);
	drv2604_work.vib_on_work_data[drv2604_data.work_vib_on_pos].time = timeout_val;

	ret = schedule_work
			(&(drv2604_work.vib_on_work_data[drv2604_data.work_vib_on_pos].work_vib_on));
	if (ret != 0)
	{
		drv2604_data.work_vib_on_pos++;
		if (drv2604_data.work_vib_on_pos >= VIB_ON_WORK_NUM) {
			drv2604_data.work_vib_on_pos = 0;
		}
		VIB_DEBUG_LOG(KERN_INFO, "schedule_work(). work_vib_on_pos=%d\n",
			drv2604_data.work_vib_on_pos);
		VIB_DEBUG_LOG(KERN_INFO, "vib_on_work_data[%d].time=%d\n",
			drv2604_data.work_vib_on_pos,
			drv2604_work.vib_on_work_data[drv2604_data.work_vib_on_pos].time);
	}
	return;
}

static void drv2604_timed_vib_off(struct timed_output_dev *dev)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. dev=0x%08x\n", (unsigned int)dev);
	ret = schedule_work(&drv2604_work.work_vib_off);
	if (ret == 0)
	{
		VIB_LOG(KERN_ERR, "schedule_work error. ret=%d\n",ret);
	}
	return;
}

static void drv2604_timed_vib_standby(struct timed_output_dev *dev)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. dev=0x%08x\n", (unsigned int)dev);
	ret = schedule_work(&drv2604_work.work_vib_standby);
	if (ret == 0)
	{
		VIB_LOG(KERN_ERR, "schedule_work error. ret=%d\n",ret);
	}
	return;
}

static void drv2604_enable(struct timed_output_dev *dev, int value)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. dev=0x%08x,value=%d\n", (unsigned int)dev, value);
	VIB_DEBUG_LOG(KERN_INFO, "add_time_flag=%d\n", drv2604_data.add_time_flag);
	if ((value <= 0) && (drv2604_data.add_time_flag == VIB_ADD_TIME_FLAG_ON))
	{
		VIB_DEBUG_LOG(KERN_INFO, "skip. value=%d,add_time_flag=%d\n",
						value, drv2604_data.add_time_flag);
		return;
	}

	VIB_DEBUG_LOG(KERN_INFO, "hrtimer_cancel(vib_off_timer)\n");
	hrtimer_cancel(&drv2604_data.vib_off_timer);

	if (value <= 0)
	{
		drv2604_timed_vib_off(dev);
	}
	else
	{
		VIB_DEBUG_LOG(KERN_INFO, "hrtimer_cancel(vib_standby_timer)\n");
		hrtimer_cancel(&drv2604_data.vib_standby_timer);
		if (value < VIB_TIME_MIN)
		{
			value = VIB_TIME_MIN;
			drv2604_data.add_time_flag = VIB_ADD_TIME_FLAG_ON;
			VIB_DEBUG_LOG(KERN_INFO, "set add_time_flag=%d\n", drv2604_data.add_time_flag);
		}
		else
		{
			drv2604_data.add_time_flag = VIB_ADD_TIME_FLAG_OFF;
			VIB_DEBUG_LOG(KERN_INFO, "set add_time_flag=%d\n", drv2604_data.add_time_flag);
		}
		drv2604_timed_vib_on(dev, value);
	}
	return;
}

static int drv2604_get_vib_time(struct timed_output_dev *dev)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. dev=0x%08x\n", (unsigned int)dev);
	mutex_lock(&vib_mutex);

	ret = hrtimer_active(&drv2604_data.vib_off_timer);
	if (ret != 0)
	{
		ktime_t r = hrtimer_get_remaining(&drv2604_data.vib_off_timer);
		struct timeval t = ktime_to_timeval(r);
		mutex_unlock(&vib_mutex);

		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	mutex_unlock(&vib_mutex);
	return 0;
}

static enum hrtimer_restart drv2604_off_timer_func(struct hrtimer *timer)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. timer=0x%08x\n", (unsigned int)timer);
	drv2604_data.add_time_flag = VIB_ADD_TIME_FLAG_OFF;
	VIB_DEBUG_LOG(KERN_INFO, "set add_time_flag=%d\n", drv2604_data.add_time_flag);

	drv2604_timed_vib_off(NULL);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart drv2604_standby_timer_func(struct hrtimer *timer)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. timer=0x%08x\n", (unsigned int)timer);
	drv2604_timed_vib_standby(NULL);
	return HRTIMER_NORESTART;
}

static struct timed_output_dev drv2604_output_dev = {
	.name = "vibrator",
	.get_time = drv2604_get_vib_time,
	.enable = drv2604_enable,
};

static int __devinit drv2604_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	int count = 0;
#ifdef VIB_USE_REGULATOR
    uint32_t min_uV=0, max_uV=0, load_uA=0;
#endif

#ifdef VIB_USE_REGULATOR
    of_property_read_u32(client->dev.of_node, "vib-vdd-min-voltage", &min_uV);
    of_property_read_u32(client->dev.of_node, "vib-vdd-max-voltage", &max_uV);
    of_property_read_u32(client->dev.of_node, "vib-vdd-load-current", &load_uA);
    printk(KERN_NOTICE "[VIB]%s regulator min_uV = %d, max_uV = %d, load_uA = %d\n",
        __func__, min_uV, max_uV, load_uA);

    vib_vdd = regulator_get(&client->dev, "vib-vdd");
    if( IS_ERR(vib_vdd) ) {
        printk(KERN_ERR "[VIB]%s regulator_get fail.\n", __func__);
        ret = PTR_ERR(vib_vdd);
        goto ERR;
    }

    ret = regulator_set_voltage(vib_vdd, min_uV, max_uV);
    if( ret ) {
        printk(KERN_ERR "[VIB]%s regulator_set_voltage fail. ret=%d\n", __func__, ret);
        goto ERR0;
    }

    ret = regulator_set_optimum_mode(vib_vdd, load_uA);
    if( ret < 0 ) {
        printk(KERN_ERR "[VIB]%s regulator_set_optimum_mode fail. ret=%d\n", __func__, ret);
        goto ERR0;
    }

    ret = regulator_enable(vib_vdd);
    if( ret ) {
        printk(KERN_ERR "[VIB]%s regulator_enable fail. ret=%d\n", __func__, ret);
        goto ERR0;
    }
    usleep_range(3000,3000);
#endif

	VIB_DEBUG_LOG(KERN_INFO, "called. id=0x%08x\n", (unsigned int)id);
	drv2604_data.drv2604_i2c_client = client;
	drv2604_data.vib_cur_status = VIB_STANDBY;
	drv2604_data.add_time_flag = VIB_ADD_TIME_FLAG_OFF;

	mutex_init(&vib_mutex);

	for (count = 0; count < VIB_ON_WORK_NUM; count++)
	{
		INIT_WORK(&(drv2604_work.vib_on_work_data[count].work_vib_on),
					drv2604_vib_on);
		drv2604_work.vib_on_work_data[count].time = 0;
	}
	drv2604_data.work_vib_on_pos = 0;
	INIT_WORK(&drv2604_work.work_vib_off, drv2604_vib_off);
	INIT_WORK(&drv2604_work.work_vib_standby, drv2604_vib_standby);

	hrtimer_init(&drv2604_data.vib_off_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	drv2604_data.vib_off_timer.function = drv2604_off_timer_func;
	hrtimer_init(&drv2604_data.vib_standby_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	drv2604_data.vib_standby_timer.function = drv2604_standby_timer_func;

	ret = timed_output_dev_register(&drv2604_output_dev);
	VIB_DEBUG_LOG(KERN_INFO, "timed_output_dev_register() ret=%d\n", ret);
	if (ret != 0)
	{
		VIB_LOG(KERN_ERR, "timed_output_dev_register() ERROR ret=%d\n", ret);
		goto probe_dev_register_error;
	}

	VIB_DEBUG_LOG(KERN_INFO, "timed_output_dev_register() ok\n");

	drv2604_initialization();

	VIB_DEBUG_LOG(KERN_INFO, "timed_output_dev_register() end\n");

	return 0;

probe_dev_register_error:
	mutex_destroy(&vib_mutex);

#ifdef VIB_USE_REGULATOR
ERR0:
    if( vib_vdd ) {
        regulator_set_optimum_mode(vib_vdd, 0);
        regulator_put(vib_vdd);
        vib_vdd = 0;
    }
ERR:
#endif
	return ret;
}

static int32_t __devexit drv2604_remove(struct i2c_client *pst_client)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called. pst_client=0x%08x\n", (unsigned int)pst_client);

	timed_output_dev_unregister(&drv2604_output_dev);

	mutex_destroy(&vib_mutex);
	return ret;
}

static int32_t drv2604_suspend(struct i2c_client *pst_client, pm_message_t mesg)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. pst_client=0x%08x,mesg=%d\n",
					(unsigned int)pst_client, mesg.event);
	VIB_DEBUG_LOG(KERN_INFO, "end.\n");
	return 0;
}

static int32_t drv2604_resume(struct i2c_client *pst_client)
{
	VIB_DEBUG_LOG(KERN_INFO, "called. pst_client=0x%08x\n", (unsigned int)pst_client);
	VIB_DEBUG_LOG(KERN_INFO, "end.\n");
	return 0;
}


#ifdef VIB_TEST
static int vibrator_test_open(struct inode *ip, struct file *fp)
{
	VIB_DEBUG_LOG(KERN_INFO, "called.\n");
	VIB_DEBUG_LOG(KERN_INFO, "end.\n");
	return 0;
}

static int vibrator_test_release(struct inode *ip, struct file *fp)
{
	VIB_DEBUG_LOG(KERN_INFO, "called.\n");
	VIB_DEBUG_LOG(KERN_INFO, "end.\n");
	return 0;
}

static int vibrator_test_set(u16 rated_mv, u16 clamp_mv)
{
	unsigned long	vreg;

	VIB_DEBUG_LOG(KERN_INFO, "called. rated=%u(mV) clamp=%u(mV)\n", rated_mv, clamp_mv);

	if (rated_mv)
	{
		if (rated_mv > clamp_mv)
			clamp_mv = rated_mv;

		gpio_set_value(MSMGPIO_LIMTR_EN, 1);
		udelay(VIB_I2C_EN_DELAY);

		/* V_AVG_ABS = Rated Voltage[7:0] / 255 * 5.28V */
		vreg = rated_mv;
		vreg *= 25500;
		vreg /= 528;
		vreg /= 100;
		VIB_DEBUG_LOG(KERN_INFO, "Rated=%d => %lu\n", rated_mv, vreg);
		/* Rounded */
		vreg += 5;
		vreg /= 10;
		VIB_DEBUG_LOG(KERN_INFO, "set reg [%lu=%lx]\n", vreg, vreg);
		VIB_SET_REG(0x16, (u8)vreg);


		/* V_OD = ODClamp[7:0] / 255 * 5.6V */
		vreg = clamp_mv;
		vreg *= 2550;
		vreg /= 56;
		vreg /= 100;
		VIB_DEBUG_LOG(KERN_INFO, "ODClamp=%d => %lu\n", clamp_mv, vreg);
		/* Rounded */
		vreg += 5;
		vreg /= 10;
		VIB_DEBUG_LOG(KERN_INFO, "set reg [%lu=%lx]\n", vreg, vreg);
		VIB_SET_REG(0x17, (u8)vreg);

		gpio_set_value(MSMGPIO_LIMTR_INTRIG, 1);
	}
	else
	{
		gpio_set_value(MSMGPIO_LIMTR_INTRIG, 0);
		gpio_set_value(MSMGPIO_LIMTR_EN, 0);
	}

	VIB_DEBUG_LOG(KERN_INFO, "end.\n");

	return 0;
}

static int vibrator_test_set_voltage(u8 *data)
{
	int ret = 0;
	vib_test_set_voltage_req_data *req_data =
		(vib_test_set_voltage_req_data *)data;
	vib_test_rsp_voltage_data *rsp_data = (vib_test_rsp_voltage_data *)data;
	s16 status = VIB_TEST_STATUS_SUCCESS;

	VIB_DEBUG_LOG(KERN_INFO, "called. rated_vol=%u clamp_vol=%u\n", req_data->rated_vol, req_data->clamp_vol);

	if (status == VIB_TEST_STATUS_SUCCESS)
	{
		ret = vibrator_test_set(req_data->rated_vol, req_data->clamp_vol);
	}

	if (ret < 0) {
		VIB_LOG(KERN_ERR, "vibrator_config error. ret=%d\n", ret);
		status = VIB_TEST_STATUS_FAIL;
	}

	memset(rsp_data, 0x00, sizeof(vib_test_rsp_voltage_data));
	rsp_data->status = (u32)status;

	VIB_DEBUG_LOG(KERN_INFO, "end. ret=%d\n", ret);
	return ret;
}

static int vibrator_test_read_register(u8 *data)
{
	vib_test_set_rdwr_reg_req_data *req_data = (vib_test_set_rdwr_reg_req_data *)data;
	vib_test_rsp_rdwr_reg_data *rsp_data = (vib_test_rsp_rdwr_reg_data *)data;

	gpio_set_value(MSMGPIO_LIMTR_EN, 1);
	udelay(VIB_I2C_EN_DELAY);
	VIB_GET_REG(req_data->reg);
	gpio_set_value(MSMGPIO_LIMTR_EN, 0);

	VIB_DEBUG_LOG(KERN_INFO, "read. reg=%X data=%x\n", req_data->reg, read_buf[0]);

	rsp_data->reserved = 0;
	rsp_data->data[0] = req_data->reg;
	rsp_data->data[1] = read_buf[0];

	return 0;
}

static int vibrator_test_write_register(u8 *data)
{
	vib_test_set_rdwr_reg_req_data *req_data = (vib_test_set_rdwr_reg_req_data *)data;
	vib_test_rsp_rdwr_reg_data *rsp_data = (vib_test_rsp_rdwr_reg_data *)data;

	VIB_DEBUG_LOG(KERN_INFO, "write. reg=%X data=%x\n", req_data->reg, req_data->data);

	gpio_set_value(MSMGPIO_LIMTR_EN, 1);
	udelay(VIB_I2C_EN_DELAY);
	VIB_SET_REG(req_data->reg, req_data->data);
	gpio_set_value(MSMGPIO_LIMTR_EN, 0);

	rsp_data->reserved = 0;
	return 0;
}

/*
 * AUTO CALIBRATION PROCEDURE
 */
static void vibrator_test_auto_calibration(u8 *data)
{
	vib_test_rsp_calibration_data *rsp_data = (vib_test_rsp_calibration_data *)data;
	int cnt;

	VIB_DEBUG_LOG(KERN_INFO, "auto calibration start\n");

	gpio_set_value(MSMGPIO_LIMTR_EN, 1);

	VIB_SET_REG(0x01, 0x00);
	VIB_SET_REG(0x16, 0x40);
	VIB_SET_REG(0x17, 0x70);
	VIB_SET_REG(0x1a, 0xbb);
	VIB_SET_REG(0x1b, 0x93);
	VIB_SET_REG(0x1c, 0xf5);
	VIB_SET_REG(0x1d, 0x80);
	VIB_SET_REG(0x01, 0x07);
	VIB_SET_REG(0x1e, 0x00);
	VIB_SET_REG(0x03, 0x00);
	VIB_SET_REG(0x0c, 0x01);

	for (cnt = 0; cnt < 50; ++cnt)
	{
		VIB_GET_REG(0x0c);
		if (!(read_buf[0] & 0x01))
			break;
		mdelay(10);
	}

	VIB_DEBUG_LOG(KERN_INFO, "auto calibration end\n");

	if (read_buf[0] & 0x01)
	{
		VIB_LOG(KERN_ERR, "auto calibration invalid\n");
	}
	else
	{
		VIB_DEBUG_LOG(KERN_INFO, "auto calibration normal end\n");

		VIB_GET_REG(0x18);
		VIB_DEBUG_LOG(KERN_INFO, "ACalComp 0x18 [7:0] = %02x\n", (unsigned)read_buf[0]);
		VIB_GET_REG(0x19);
		VIB_DEBUG_LOG(KERN_INFO, "ACalBEMF 0x19 [7:0] = %02x\n", (unsigned)read_buf[0]);
		VIB_GET_REG(0x1a);
		VIB_DEBUG_LOG(KERN_INFO, "BEMFGain 0x1a [1:0] = %02x\n", (unsigned)read_buf[0]);
	}

	VIB_GET_REG(0x18);
	rsp_data->data[0] = read_buf[0];
	VIB_GET_REG(0x19);
	rsp_data->data[1] = read_buf[0];
	VIB_GET_REG(0x1a);
	rsp_data->data[2] = read_buf[0];
	rsp_data->data[3] = 0;


	gpio_set_value(MSMGPIO_LIMTR_EN, 0);
}

static long vibrator_test_ioctl
(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	int ret = 0;
	u64 ret2 = 0;
	vib_test_param test_param;
	VIB_DEBUG_LOG(KERN_INFO, "called. cmd=0x%08X\n", cmd);

	switch (cmd) {
		case IOCTL_VIB_TEST_CTRL:
			VIB_DEBUG_LOG(KERN_INFO, "cmd=IOCTL_VIB_TEST_CTRL\n");
			ret2 = copy_from_user(&test_param, (void *)arg, sizeof(test_param));
			VIB_DEBUG_LOG(KERN_INFO, "copy_from_user() called. ret2=%lu\n",
							(long unsigned int)ret2);
			VIB_DEBUG_LOG(KERN_INFO,
							"copy_from_user() req_code=0x%04X,data=0x%02X%02X%02X%02X\n",
							test_param.req_code, test_param.data[0], test_param.data[1],
							test_param.data[2], test_param.data[3]);
			if (ret2) {
				VIB_LOG(KERN_ERR, "copy_from_user() error. ret2=%lu\n",
						(long unsigned int)ret2);
				rc = -EINVAL;
				break;
			}
			VIB_DEBUG_LOG(KERN_INFO, "req_code=0x%04X\n", test_param.req_code);
			switch (test_param.req_code) {
				case VIB_TEST_SET_VOLTAGE:
					VIB_LOG(KERN_ERR, "VIB_TEST_SET_VOLTAGE\n");
					ret = vibrator_test_set_voltage(&test_param.data[0]);
					if (ret < 0) VIB_LOG(KERN_ERR,
										"vibrator_test_set_voltage() error. ret=%d\n", ret);
					ret2 = copy_to_user((void *)arg, &test_param, sizeof(vib_test_param));
					VIB_DEBUG_LOG(KERN_INFO, "copy_to_user() called. ret2=%lu\n",
									(long unsigned int)ret2);
					VIB_DEBUG_LOG(KERN_INFO,
									"copy_to_user() req_code=0x%04X,data=0x%02X%02X%02X%02X\n",
									test_param.req_code, test_param.data[0], test_param.data[1],
									test_param.data[2], test_param.data[3]);
					if (ret2) {
						VIB_LOG(KERN_ERR, "copy_to_user() error. ret2=%lu\n",
								(long unsigned int)ret2);
						rc = -EINVAL;
					}
					break;

				case VIB_TEST_CALIBRATION:
					VIB_LOG(KERN_ERR, "VIB_TEST_CALIBRATION\n");
					vibrator_test_auto_calibration(&test_param.data[0]);
					ret2 = copy_to_user((void *)arg, &test_param, sizeof(vib_test_param));
					if (ret2) {
						VIB_LOG(KERN_ERR, "VIB_TEST_CALIBRATION error ret2=%llu\n", ret2);
						rc = -EINVAL;
					}
					break;
				case VIB_TEST_RD_REGISTER:
					VIB_LOG(KERN_ERR, "VIB_TEST_RD_REGISTER\n");
					vibrator_test_read_register(&test_param.data[0]);
					ret2 = copy_to_user((void *)arg, &test_param, sizeof(vib_test_param));
					if (ret2) {
						VIB_LOG(KERN_ERR, "VIB_TEST_RD_REGISTER error ret2=%llu\n", ret2);
						rc = -EINVAL;
					}
					break;
				case VIB_TEST_WR_REGISTER:
					VIB_LOG(KERN_ERR, "VIB_TEST_WR_REGISTER\n");
					vibrator_test_write_register(&test_param.data[0]);
					ret2 = copy_to_user((void *)arg, &test_param, sizeof(vib_test_param));
					if (ret2) {
						VIB_LOG(KERN_ERR, "VIB_TEST_RD_REGISTER error ret2=%llu\n", ret2);
						rc = -EINVAL;
					}
					break;
				default:
					VIB_LOG(KERN_ERR, "req_code error. req_code=0x%04X\n",
								test_param.req_code);
					rc = -EINVAL;
					break;
			}
			break;
		default:
			VIB_LOG(KERN_ERR, "cmd error. cmd=0x%08X\n", cmd);
			rc = -EINVAL;
			break;
	}

	VIB_DEBUG_LOG(KERN_INFO, "end. rc=%d\n", rc);
	return rc;
}

static const struct file_operations vibrator_test_fops = {
	.owner			= THIS_MODULE,
	.open			= vibrator_test_open,
	.release		= vibrator_test_release,
	.unlocked_ioctl	= vibrator_test_ioctl,
};

static struct miscdevice vibrator_test_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "kc_vibrator_test",
	.fops = &vibrator_test_fops,
};

void vibrator_test_init(void)
{
	misc_register(&vibrator_test_dev);
}
#endif /* VIB_TEST */


static struct i2c_device_id drv2604_idtable[] = {
	{VIB_DRV_NAME, 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, drv2604_idtable);

static struct i2c_driver drv2604_driver = {
	.driver		= {
		.name	= VIB_DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= drv2604_probe,
	.remove		= __devexit_p(drv2604_remove),
	.suspend	= drv2604_suspend,
	.resume		= drv2604_resume,
	.id_table	= drv2604_idtable,
};

static int __init drv2604_init(void)
{
	int ret = 0;

	VIB_DEBUG_LOG(KERN_INFO, "called.\n");
#ifdef VIB_TEST
	vibrator_test_init();
#endif /* VIB_TEST */
	ret = i2c_add_driver(&drv2604_driver);
	VIB_DEBUG_LOG(KERN_INFO, "i2c_add_driver() ret=%d\n", ret);
	if (ret != 0)
	{
		VIB_LOG(KERN_ERR, "i2c_add_driver() ret=%d\n", ret);
	}

	return ret;
}

static void __exit drv2604_exit(void)
{
	VIB_DEBUG_LOG(KERN_INFO, "called.\n");
	i2c_del_driver(&drv2604_driver);
	VIB_DEBUG_LOG(KERN_INFO, "i2c_del_driver()\n");

	return;
}

module_init(drv2604_init);
module_exit(drv2604_exit);
MODULE_DESCRIPTION("timed output DRV2604 vibrator device");
MODULE_LICENSE("GPL");
