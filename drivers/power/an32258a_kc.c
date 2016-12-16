/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2014 KYOCERA Corporation
 * (C) 2015 KYOCERA Corporation
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/log2.h>
#include <linux/bitops.h>
#include <linux/jiffies.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/memory.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>

#define AN32258A_FLAG_ADDR16	0x80	/* address pointer is 16 bit */
#define AN32258A_FLAG_READONLY	0x40	/* sysfs-entry will be read-only */
#define AN32258A_FLAG_IRUGO		0x20	/* sysfs-entry will be world-readable */
#define AN32258A_FLAG_TAKE8ADDR	0x10	/* take always 8 addresses (24c00) */

#define AN32258A_SIZE_BYTELEN 5
#define AN32258A_SIZE_FLAGS 8

#define AN32258A_BITMASK(x) (BIT(x) - 1)

/* create non-zero magic value for given eeprom parameters */
#define AN32258A_DEVICE_MAGIC(_len, _flags) 		\
	((1 << AN32258A_SIZE_FLAGS | (_flags)) 		\
	    << AN32258A_SIZE_BYTELEN | ilog2(_len))

#define WPC_I2C_EN_DEBUG	0
#if WPC_I2C_EN_DEBUG
	#define WPC_I2C_EN_LOG pr_err
#else
	#define WPC_I2C_EN_LOG pr_debug
#endif

#define ADDR_P1		0xB6
#define VALUE_P1	0x5A
#define ADDR_P2		0xB6
#define VALUE_P2	0xA5
#define ADDR_P3		0x94
#define VALUE_P3	0x06
#define ADDR_P4		0x43
#define VALUE_P4	0x0F
#define ADDR_P5		0x44
#define VALUE_P5	0x06
#define ADDR_P6		0x44
#define VALUE_P6	0x09
#define ADDR_P7		0x94
#define VALUE_P7	0x13

#define GPIO_USB_DET_N		35
#define GPIO_WPC_I2C_EN		33
#define IS_USB_DET()		(0 == gpio_get_value(GPIO_USB_DET_N))
#define IS_WPC_I2C_EN()		(1 == gpio_get_value(GPIO_WPC_I2C_EN))
#define WPC_REG_STEP1_WAIT_MS  400
#define WPC_REG_STEP2_WAIT_MS  5900
#define CHECK_CYCLE_MS   50
#define WPC_I2C_EN_CHECK_COUNT	(WPC_REG_STEP1_WAIT_MS / CHECK_CYCLE_MS)

typedef enum {
	WPC_REG_ST_UNKOWN,
	WPC_REG_ST_STEP1,
	WPC_REG_ST_STEP2,
	WPC_REG_ST_COMP
} wpc_reg_status;

static struct delayed_work oem_chg_wpc_i2c_en_work;
static struct wake_lock wpc_i2c_en_wake_lock;
static int current_wpc_i2c_en_status = 0;
static bool init_flg = false;
static wpc_reg_status wpc_reg_st = WPC_REG_ST_UNKOWN;

struct an32258a_platform_data {
	u32		byte_len;		/* size (sum of all addr) */
	u16		page_size;		/* for writes */
	u8		flags;
	void		(*setup)(struct memory_accessor *, void *context);
	void		*context;
};

struct delayed_work wpc_i2c_reg_work;

struct an32258a_data {
	struct an32258a_platform_data chip;

	/*
	 * Lock protects against activities from other Linux tasks,
	 * but not from changes by other I2C masters.
	 */
	struct mutex lock;

	u8 *writebuf;
	unsigned write_max;
	unsigned num_addresses;

	/*
	 * Some chips tie up multiple I2C addresses; dummy devices reserve
	 * them for us, and we'll use them with SMBus calls.
	 */
	struct i2c_client *client[];
};

static unsigned write_timeout = 25;
module_param(write_timeout, uint, 0);
MODULE_PARM_DESC(write_timeout, "Time (in ms) to try writes (default 25)");


static const struct i2c_device_id an32258a_ids[] = {
	/* needs 8 addresses as A0-A2 are ignored */
	{ "an32258a_kc", AN32258A_DEVICE_MAGIC(2048 / 8, 0) },
	{ /* END OF LIST */ }
};
MODULE_DEVICE_TABLE(i2c, an32258a_ids);

static struct an32258a_data *the_an32258a;
#define IADC_ADDR		0x08
/*-------------------------------------------------------------------------*/

static struct i2c_client *an32258a_translate_offset(struct an32258a_data *an32258a,
		unsigned *offset)
{
	unsigned i;

		i = *offset >> 8;
		*offset &= 0xff;

	return an32258a->client[i];
}

static ssize_t an32258a_eeprom_read(struct an32258a_data *an32258a, char *buf,
		unsigned offset, size_t count)
{
	struct i2c_msg msg[2];
	u8 msgbuf[2];
	struct i2c_client *client;
	unsigned long timeout, read_time;
	int status, i;

	memset(msg, 0, sizeof(msg));


	client = an32258a_translate_offset(an32258a, &offset);

		i = 0;
		msgbuf[i++] = offset;

		msg[0].addr = client->addr;
		msg[0].buf = msgbuf;
		msg[0].len = i;

		msg[1].addr = client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].buf = buf;
		msg[1].len = count;

	timeout = jiffies + msecs_to_jiffies(write_timeout);
	do {
		read_time = jiffies;
			status = i2c_transfer(client->adapter, msg, 2);
			if (status == 2)
				status = count;
		dev_dbg(&client->dev, "read %zu@%d --> %d (%ld)\n",
				count, offset, status, jiffies);

		if (status == count)
			return count;

		/* REVISIT: at HZ=100, this is sloooow */
		msleep(1);
	} while (time_before(read_time, timeout));

	return -ETIMEDOUT;
}

static ssize_t an32258a_read(struct an32258a_data *an32258a,
		char *buf, loff_t off, size_t count)
{
	ssize_t retval = 0;

	if (unlikely(!count))
		return count;

	/*
	 * Read data from chip, protecting against concurrent updates
	 * from this host, but not from other I2C masters.
	 */
	mutex_lock(&an32258a->lock);

	while (count) {
		ssize_t	status;

		status = an32258a_eeprom_read(an32258a, buf, off, count);
		if (status <= 0) {
			if (retval == 0)
				retval = status;
			break;
		}
		buf += status;
		off += status;
		count -= status;
		retval += status;
	}

	mutex_unlock(&an32258a->lock);

	return retval;
}



static ssize_t an32258a_eeprom_write(struct an32258a_data *an32258a, const char *buf,
		unsigned offset, size_t count)
{
	struct i2c_client *client;
	struct i2c_msg msg;
	ssize_t status;
	unsigned long timeout, write_time;
	unsigned next_page;
	int i;

	/* Get corresponding I2C address and adjust offset */
	client = an32258a_translate_offset(an32258a, &offset);

	/* write_max is at most a page */
	if (count > an32258a->write_max) {
		count = an32258a->write_max;
	}

	/* Never roll over backwards, to the start of this page */
	next_page = roundup(offset + 1, an32258a->chip.page_size);
	if (offset + count > next_page) {
		count = next_page - offset;
	}

	/* If we'll use I2C calls for I/O, set up the message */
		i = 0;
		msg.addr = client->addr;
		msg.flags = 0;

		/* msg.buf is u8 and casts will mask the values */
		msg.buf = an32258a->writebuf;

		msg.buf[i++] = offset;
		memcpy(&msg.buf[i], buf, count);
		msg.len = i + count;

	/*
	 * Writes fail if the previous one didn't complete yet. We may
	 * loop a few times until this one succeeds, waiting at least
	 * long enough for one entire page write to work.
	 */
	timeout = jiffies + msecs_to_jiffies(write_timeout);
	do {
		write_time = jiffies;
			status = i2c_transfer(client->adapter, &msg, 1);
			if (status == 1)
				status = count;
		dev_dbg(&client->dev, "write %zu@%d --> %zd (%ld)\n",
				count, offset, status, jiffies);

		if (status == count)
			return count;

		/* REVISIT: at HZ=100, this is sloooow */
		msleep(1);
	} while (time_before(write_time, timeout));

	return -ETIMEDOUT;
}

static ssize_t an32258a_write(struct an32258a_data *an32258a, const char *buf, loff_t off,
			  size_t count)
{
	ssize_t retval = 0;

	if (unlikely(!count))
		return count;

	/*
	 * Write data to chip, protecting against concurrent updates
	 * from this host, but not from other I2C masters.
	 */
	mutex_lock(&an32258a->lock);

	while (count) {
		ssize_t	status;

		status = an32258a_eeprom_write(an32258a, buf, off, count);
		if (status <= 0) {
			if (retval == 0)
				retval = status;
			break;
		}
		buf += status;
		off += status;
		count -= status;
		retval += status;
	}

	mutex_unlock(&an32258a->lock);

	return retval;
}


static void wpc_reg_init(void)
{
	if (!init_flg) {
		return;
	}
	cancel_delayed_work_sync(&wpc_i2c_reg_work);
	wpc_reg_st = WPC_REG_ST_STEP1;
	schedule_delayed_work(&wpc_i2c_reg_work, 0);
	pr_info("set wpc_i2c_reg_work\n");
}

static void wpc_set_i2c_reg(struct work_struct *work)
{
	char read_buf;
	char write_buf;
	ssize_t retval = 0;

	switch (wpc_reg_st) {
		case WPC_REG_ST_STEP1:
			write_buf = VALUE_P1;
			retval = an32258a_write(the_an32258a, &write_buf, ADDR_P1, 1);
			pr_info("W ADDR_P1:0x%x DATA:0x%x status:%d\n", ADDR_P1, write_buf, retval);
			retval = an32258a_read(the_an32258a, &read_buf, ADDR_P1, 1);
			pr_info("R ADDR_P1:0x%x DATA:0x%x status:%d\n", ADDR_P1, read_buf, retval);

			write_buf = VALUE_P2;
			retval = an32258a_write(the_an32258a, &write_buf, ADDR_P2, 1);
			pr_info("W ADDR_P2:0x%x DATA:0x%x status:%d\n", ADDR_P2, write_buf, retval);
			retval = an32258a_read(the_an32258a, &read_buf, ADDR_P2, 1);
			pr_info("R ADDR_P2:0x%x DATA:0x%x status:%d\n", ADDR_P2, read_buf, retval);

			write_buf = VALUE_P3;
			retval = an32258a_write(the_an32258a, &write_buf, ADDR_P3, 1);
			pr_info("W ADDR_P3:0x%x DATA:0x%x status:%d\n", ADDR_P3, write_buf, retval);
			retval = an32258a_read(the_an32258a, &read_buf, ADDR_P3, 1);
			pr_info("R ADDR_P3:0x%x DATA:0x%x status:%d\n", ADDR_P3, read_buf, retval);

			write_buf = VALUE_P4;
			retval = an32258a_write(the_an32258a, &write_buf, ADDR_P4, 1);
			pr_info("W ADDR_P4:0x%x DATA:0x%x status:%d\n", ADDR_P4, write_buf, retval);
			retval = an32258a_read(the_an32258a, &read_buf, ADDR_P4, 1);
			pr_info("R ADDR_P4:0x%x DATA:0x%x status:%d\n", ADDR_P4, read_buf, retval);

			write_buf = VALUE_P5;
			retval = an32258a_write(the_an32258a, &write_buf, ADDR_P5, 1);
			pr_info("W ADDR_P5:0x%x DATA:0x%x status:%d\n", ADDR_P5, write_buf, retval);
			retval = an32258a_read(the_an32258a, &read_buf, ADDR_P5, 1);
			pr_info("R ADDR_P5:0x%x DATA:0x%x status:%d\n", ADDR_P5, read_buf, retval);
			wpc_reg_st = WPC_REG_ST_STEP2;
			schedule_delayed_work(&wpc_i2c_reg_work, msecs_to_jiffies(WPC_REG_STEP2_WAIT_MS));
			break;

		case WPC_REG_ST_STEP2:
			write_buf = VALUE_P6;
			retval = an32258a_write(the_an32258a, &write_buf, ADDR_P6, 1);
			pr_info("W ADDR_P6:0x%x DATA:0x%x status:%d\n", ADDR_P6, write_buf, retval);
			retval = an32258a_read(the_an32258a, &read_buf, ADDR_P6, 1);
			pr_info("R ADDR_P6:0x%x DATA:0x%x status:%d\n", ADDR_P6, read_buf, retval);

			write_buf = VALUE_P7;
			retval = an32258a_write(the_an32258a, &write_buf, ADDR_P7, 1);
			pr_info("W ADDR_P7:0x%x DATA:0x%x status:%d\n", ADDR_P7, write_buf, retval);
			retval = an32258a_read(the_an32258a, &read_buf, ADDR_P7, 1);
			pr_info("R ADDR_P7:0x%x DATA:0x%x status:%d\n", ADDR_P7, read_buf, retval);

			wpc_reg_st = WPC_REG_ST_COMP;
			break;

		default:
			break;
	}
}

static irqreturn_t oem_chg_wpc_i2c_en_isr(int irq, void *ptr)
{
	disable_irq_nosync(gpio_to_irq(GPIO_WPC_I2C_EN));

	pr_info("WPC_I2C_EN irq detected!! GPIO:%d current:%d\n", IS_WPC_I2C_EN(), current_wpc_i2c_en_status);

	wake_lock(&wpc_i2c_en_wake_lock);
	schedule_delayed_work(&oem_chg_wpc_i2c_en_work,
			msecs_to_jiffies(CHECK_CYCLE_MS));
	pr_info("oem_chg_wpc_i2c_en_work:%dms\n", CHECK_CYCLE_MS);

	return IRQ_HANDLED;
}

static void oem_chg_set_wpc_i2c_en_irq(void)
{
	int rc = 0;

	free_irq(gpio_to_irq(GPIO_WPC_I2C_EN), 0);
	if (current_wpc_i2c_en_status) {
		rc = request_irq(gpio_to_irq(GPIO_WPC_I2C_EN), oem_chg_wpc_i2c_en_isr, IRQF_TRIGGER_LOW, "WPC_I2C_EN", 0);
	} else {
		rc = request_irq(gpio_to_irq(GPIO_WPC_I2C_EN), oem_chg_wpc_i2c_en_isr, IRQF_TRIGGER_HIGH, "WPC_I2C_EN", 0);
	}
	if (rc) {
		pr_err("couldn't register interrupts rc=%d\n", rc);
	}
}

static void oem_chg_wpc_i2c_en_detection(struct work_struct *work)
{
	static int check_cnt = 0;
	int new_wpc_i2c_en_status = IS_WPC_I2C_EN();

	if (current_wpc_i2c_en_status == new_wpc_i2c_en_status) {
		check_cnt = 0;
		WPC_I2C_EN_LOG("equ wpc_i2c_en_status=%d\n", new_wpc_i2c_en_status);
		if ((new_wpc_i2c_en_status) && !IS_USB_DET()) {
			wpc_reg_init();
			pr_info("bounce call wpc_reg_init\n");
		}
		goto set_irq;
	}
	check_cnt++;

	if (WPC_I2C_EN_CHECK_COUNT <= check_cnt) {
		pr_info("change wpc_i2c_en_status old=%d new=%d\n",
			current_wpc_i2c_en_status, new_wpc_i2c_en_status);
		current_wpc_i2c_en_status = new_wpc_i2c_en_status;
		if ((current_wpc_i2c_en_status) && !IS_USB_DET()) {
			wpc_reg_init();
		}
		check_cnt = 0;
	}

	if (check_cnt) {
		schedule_delayed_work(&oem_chg_wpc_i2c_en_work,
				msecs_to_jiffies(CHECK_CYCLE_MS));
		WPC_I2C_EN_LOG("set oem_chg_wpc_i2c_en_work:%dms cnt=%d\n", CHECK_CYCLE_MS, check_cnt);
		return;
	}

set_irq:
	oem_chg_set_wpc_i2c_en_irq();
	wake_unlock(&wpc_i2c_en_wake_lock);
	pr_info("set irq current_status:%d\n", current_wpc_i2c_en_status);
	return;
}

int oem_chg_wpc_get_iadc_reg(char *iadc_val)
{
	ssize_t retval = 0;

	retval = an32258a_read(the_an32258a, iadc_val, IADC_ADDR, 1);
	pr_debug("R IADC_ADDR:0x%02x DATA:0x%02x status:%d\n", IADC_ADDR, *iadc_val, retval);
	return retval;
}

static int an32258a_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct an32258a_platform_data chip;
	bool writable;
	struct an32258a_data *an32258a;
	int err;
	unsigned i, num_addresses;
	kernel_ulong_t magic;
	int gpio_trigger_wpc_i2c_en;
	int rc;

	if (client->dev.platform_data) {
		chip = *(struct an32258a_platform_data *)client->dev.platform_data;
	} else {
		if (!id->driver_data) {
			err = -ENODEV;
			goto err_out;
		}
		magic = id->driver_data;
		chip.byte_len = BIT(magic & AN32258A_BITMASK(AN32258A_SIZE_BYTELEN));
		magic >>= AN32258A_SIZE_BYTELEN;
		chip.flags = magic & AN32258A_BITMASK(AN32258A_SIZE_FLAGS);
		/*
		 * This is slow, but we can't know all eeproms, so we better
		 * play safe. Specifying custom eeprom-types via platform_data
		 * is recommended anyhow.
		 */
		chip.page_size = 1;

		chip.setup = NULL;
		chip.context = NULL;
	}

	if (!is_power_of_2(chip.byte_len))
		dev_err(&client->dev,
			"byte_len looks suspicious (no power of 2)!\n");
	if (!chip.page_size) {
		dev_err(&client->dev, "page_size must not be 0!\n");
		err = -EINVAL;
		goto err_out;
	}
	if (!is_power_of_2(chip.page_size))
		dev_err(&client->dev,
			"page_size looks suspicious (no power of 2)!\n");

	/* Use I2C operations unless we're stuck with SMBus extensions. */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
			err = -EPFNOSUPPORT;
			goto err_out;
	}

	if (chip.flags & AN32258A_FLAG_TAKE8ADDR)
	{
		num_addresses = 8;
	}
	else
	{
		num_addresses =	DIV_ROUND_UP(chip.byte_len,
			(chip.flags & AN32258A_FLAG_ADDR16) ? 65536 : 256);
	}

	an32258a = kzalloc(sizeof(struct an32258a_data) +
		num_addresses * sizeof(struct i2c_client *), GFP_KERNEL);
	if (!an32258a) {
		err = -ENOMEM;
		goto err_out;
	}
	the_an32258a = an32258a;

	mutex_init(&an32258a->lock);
	an32258a->chip = chip;
	an32258a->num_addresses = num_addresses;


	writable = !(chip.flags & AN32258A_FLAG_READONLY);
	if (writable) {
		if (i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)) {

			unsigned write_max = chip.page_size;


			an32258a->write_max = write_max;

			/* buffer (data + address at the beginning) */
			an32258a->writebuf = kmalloc(write_max + 2, GFP_KERNEL);
			if (!an32258a->writebuf) {
				err = -ENOMEM;
				goto err_struct;
			}
		} else {
			dev_err(&client->dev,
				"cannot write due to controller restrictions.");
		}
	}

	an32258a->client[0] = client;

	/* use dummy devices for multiple-address chips */
	for (i = 1; i < num_addresses; i++) {
		an32258a->client[i] = i2c_new_dummy(client->adapter,
					client->addr + i);
		if (!an32258a->client[i]) {
			dev_err(&client->dev, "address 0x%02x unavailable\n",
					client->addr + i);
			err = -EADDRINUSE;
			goto err_clients;
		}
	}


	i2c_set_clientdata(client, an32258a);

	wake_lock_init(&wpc_i2c_en_wake_lock, WAKE_LOCK_SUSPEND, "wpc_i2c_en");
	INIT_DELAYED_WORK(&oem_chg_wpc_i2c_en_work, oem_chg_wpc_i2c_en_detection);
	INIT_DELAYED_WORK(&wpc_i2c_reg_work, wpc_set_i2c_reg);
	gpio_tlmm_config(GPIO_CFG(GPIO_USB_DET_N, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_WPC_I2C_EN, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	if (IS_WPC_I2C_EN()) {
		gpio_trigger_wpc_i2c_en = IRQF_TRIGGER_LOW;
		current_wpc_i2c_en_status = 1;
		pr_info("WPC_I2C_EN:HI\n");
	} else {
		gpio_trigger_wpc_i2c_en = IRQF_TRIGGER_HIGH;
		current_wpc_i2c_en_status = 0;
		pr_info("WPC_I2C_EN:LO\n");
	}

	if ((current_wpc_i2c_en_status) && !IS_USB_DET()) {
		wpc_reg_init();
		pr_info("call wpc_reg_init\n");
	}

	rc = request_irq(gpio_to_irq(GPIO_WPC_I2C_EN), oem_chg_wpc_i2c_en_isr, gpio_trigger_wpc_i2c_en, "WPC_I2C_EN", 0);
	if (rc) {
		pr_err("wpc couldn't register interrupts rc=%d\n", rc);
		return -EIO;
	}
	enable_irq_wake(gpio_to_irq(GPIO_WPC_I2C_EN));

	init_flg = true;

	dev_info(&client->dev, "%s, %s, %u bytes/write\n",
		client->name,
		writable ? "writable" : "read-only", an32258a->write_max);

	return 0;

err_clients:
	for (i = 1; i < num_addresses; i++)
		if (an32258a->client[i])
			i2c_unregister_device(an32258a->client[i]);

	kfree(an32258a->writebuf);
err_struct:
	kfree(an32258a);
err_out:
	dev_dbg(&client->dev, "probe error %d\n", err);
	return err;
}

static int __devexit an32258a_remove(struct i2c_client *client)
{
	struct an32258a_data *an32258a;
	int i;

	an32258a = i2c_get_clientdata(client);

	for (i = 1; i < an32258a->num_addresses; i++)
		i2c_unregister_device(an32258a->client[i]);

	kfree(an32258a->writebuf);
	kfree(an32258a);
	return 0;
}

/*-------------------------------------------------------------------------*/

static struct of_device_id an32258a_match_table[] = {
	{ .compatible = "at,an32258a_kc",},
	{ },
};

static struct i2c_driver an32258a_driver = {
	.driver = {
		.name = "an32258a_kc",
		.owner = THIS_MODULE,
		.of_match_table = an32258a_match_table,
	},
	.probe = an32258a_probe,
	.remove = __devexit_p(an32258a_remove),
	.id_table = an32258a_ids,
};

static int __init an32258a_init(void)
{
	return i2c_add_driver(&an32258a_driver);
}
module_init(an32258a_init);

static void __exit an32258a_exit(void)
{
	i2c_del_driver(&an32258a_driver);
}
module_exit(an32258a_exit);

/* Module information */
MODULE_AUTHOR("KYOCERA Corporation");
MODULE_DESCRIPTION("Wireless Power Consortium driver");
MODULE_LICENSE("GPL");
