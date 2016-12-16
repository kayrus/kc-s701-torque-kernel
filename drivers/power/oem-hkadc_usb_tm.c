/* 
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2014 KYOCERA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"OEM_HKADC_USB_TM %s: " fmt, __func__

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/leds.h>

#define HKADC_USB_TM_ERR		pr_err
//#define FEATURE_HKADC_USB_TM_DEBUG
#ifdef FEATURE_HKADC_USB_TM_DEBUG
#define HKADC_USB_TM_DEBUG	pr_err
#else
#define HKADC_USB_TM_DEBUG	pr_debug
#endif

#define USB_THERM_N		50

#define USB_THERM_NORMAL	25
#define USB_THERM_OVERHEAT	95

#define USB_THERM_CHECK_COUNT   2
#define CHECK_CYCLE_MS   100

#define LOW_BATT_LED_ON_TIME        250
#define LOW_BATT_LED_OFF_TIME       250

struct oem_hkadc_usb_tm_device {
	int gpio;
	int irq;
	struct work_struct irq_work;
};

static struct oem_hkadc_usb_tm_device *oem_hkadc_usb_tm_dev;

static int usb_therm_value = USB_THERM_NORMAL;

static int usb_therm_status = 1;

static int oem_hkadc_usb_therm_error_flag = 0;

static struct power_supply *batt_psy = NULL;

void oem_hkadc_usb_tm_level_set(struct power_supply *);

struct delayed_work oem_hkadc_usb_tm_work;

static void oem_hkadc_usb_tm_irq(void);
static irqreturn_t oem_hkadc_usb_tm_isr(int irq, void *dev);

static struct led_control_ex_data led_ctrl;

static void oem_set_led_usb_therm_error(void)
{
	led_ctrl.priority   = LED_PRIORITY_BT_INCOMING;
	led_ctrl.color      = LED_COLOR_RED;
	led_ctrl.mode       = LED_BLINK_ON;
	led_ctrl.pattern[0] = LOW_BATT_LED_ON_TIME;
	led_ctrl.pattern[1] = LOW_BATT_LED_OFF_TIME;
	led_ctrl.pattern[2] = 0;
	
	qpnp_set_leddata_ex(&led_ctrl);
}

static void oem_set_led_off(void)
{
	led_ctrl.priority   = LED_PRIORITY_BT_INCOMING;
	led_ctrl.color      = LED_COLOR_OFF;
	led_ctrl.mode       = LED_BLINK_OFF;
	led_ctrl.pattern[0] = 0;
	
	qpnp_set_leddata_ex(&led_ctrl);
}

static void oem_hkadc_set_usb_therm_state(int therm_status)
{
	if(!therm_status){
		HKADC_USB_TM_DEBUG("USB_THERM_N:High ¨ Low\n");
		oem_hkadc_usb_therm_error_flag = 1;
		
		oem_set_led_usb_therm_error();
		
		usb_therm_value = USB_THERM_OVERHEAT;
	}else{
		HKADC_USB_TM_DEBUG("USB_THERM_N:Low ¨ High\n");
		oem_hkadc_usb_therm_error_flag = 0;
		
		oem_set_led_off();
		
		usb_therm_value = USB_THERM_NORMAL;
	}
	
	oem_hkadc_usb_tm_level_set(batt_psy);
	
	power_supply_changed(batt_psy);

	return;
}

static void oem_hkadc_usb_therm_error_detection(struct work_struct *work)
{
	static int check_cnt = 0;
	int new_usb_therm_status = gpio_get_value(oem_hkadc_usb_tm_dev->gpio);
	
	if (usb_therm_status == new_usb_therm_status) {
		check_cnt = 0;
		HKADC_USB_TM_DEBUG("equ usb_therm_status=%d\n", usb_therm_status);
		goto set_irq;
	}
	check_cnt++;
	
	if (USB_THERM_CHECK_COUNT <= check_cnt) {
		HKADC_USB_TM_DEBUG("usb_therm_status old=%d new=%d\n",
			usb_therm_status, new_usb_therm_status);
		usb_therm_status = new_usb_therm_status;
		oem_hkadc_set_usb_therm_state(usb_therm_status);
		check_cnt = 0;
	}

	if (check_cnt) {
		schedule_delayed_work(&oem_hkadc_usb_tm_work, msecs_to_jiffies(CHECK_CYCLE_MS));
		HKADC_USB_TM_DEBUG("set oem_hkadc_usb_tm_work=%dms cnt=%d\n", CHECK_CYCLE_MS, check_cnt);
		return;
	}

set_irq:
	oem_hkadc_usb_tm_irq();
	return;
}

static void oem_hkadc_usb_tm_irq(void)
{
	int rc = 0;

	free_irq(oem_hkadc_usb_tm_dev->irq, 0);
	if (!usb_therm_status) {
		rc = request_irq(oem_hkadc_usb_tm_dev->irq, oem_hkadc_usb_tm_isr, IRQF_TRIGGER_HIGH,"oem_hkadc_usb_tm-irq", 0);
	} else {
		rc = request_irq(oem_hkadc_usb_tm_dev->irq, oem_hkadc_usb_tm_isr, IRQF_TRIGGER_LOW,"oem_hkadc_usb_tm-irq", 0);
	}
	if (rc) {
		HKADC_USB_TM_ERR("couldn't register interrupts rc=%d\n", rc);
	}
}

static irqreturn_t oem_hkadc_usb_tm_isr(int irq, void *dev)
{
	disable_irq_nosync(oem_hkadc_usb_tm_dev->irq);
	
	HKADC_USB_TM_DEBUG("USB_THERM_N irq detected!! GPIO=%d\n", gpio_get_value(oem_hkadc_usb_tm_dev->gpio));
	
	schedule_delayed_work(&oem_hkadc_usb_tm_work, msecs_to_jiffies(CHECK_CYCLE_MS));
	
	return IRQ_HANDLED;
}

void oem_hkadc_usb_tm_init(struct power_supply *the_batt_psy)
{
	int rc = 0;
	
	INIT_DELAYED_WORK(&oem_hkadc_usb_tm_work, oem_hkadc_usb_therm_error_detection);
	
	batt_psy = the_batt_psy;
	if (!batt_psy) {
		HKADC_USB_TM_ERR("battery power supply not found deferring probe\n");
		rc = -EPROBE_DEFER;
		goto fail_hkadc_enable;
	}

	oem_hkadc_usb_tm_dev = kzalloc(sizeof(struct oem_hkadc_usb_tm_device), GFP_KERNEL);
	if (!oem_hkadc_usb_tm_dev) {
		HKADC_USB_TM_ERR("kzalloc fail\n");
		rc = -ENOMEM;
		goto err_alloc;
	}
	
	oem_hkadc_usb_tm_dev->gpio = USB_THERM_N;
	if (oem_hkadc_usb_tm_dev->gpio < 0) {
		HKADC_USB_TM_ERR("of_get_named_gpio failed.\n");
		rc = -EINVAL;
		goto err_gpio;
	}

	rc = gpio_request(oem_hkadc_usb_tm_dev->gpio, "oem_hkadc_usb_tm-gpio");
	if (rc) {
		HKADC_USB_TM_ERR("gpio_request failed.\n");
		goto err_gpio;
	}
	
	usb_therm_status = 1;
	
	oem_hkadc_usb_tm_dev->irq = gpio_to_irq(oem_hkadc_usb_tm_dev->gpio);

	rc = request_irq(oem_hkadc_usb_tm_dev->irq, oem_hkadc_usb_tm_isr, IRQF_TRIGGER_LOW,
							"oem_hkadc_usb_tm-irq", 0);
	if (rc) {
		HKADC_USB_TM_ERR("failed request_irq.\n");
		goto err_irq;
	}

	HKADC_USB_TM_ERR("successful. GPIO(%d)=%d\n", oem_hkadc_usb_tm_dev->gpio, gpio_get_value(oem_hkadc_usb_tm_dev->gpio));
	enable_irq_wake(oem_hkadc_usb_tm_dev->irq);
	
	return;


err_irq:
	gpio_free(oem_hkadc_usb_tm_dev->gpio);

err_gpio:
	kfree(oem_hkadc_usb_tm_dev);

err_alloc:
fail_hkadc_enable:
	
	return;
}
EXPORT_SYMBOL(oem_hkadc_usb_tm_init);

int oem_hkadc_usb_therm_value(void)
{
	return usb_therm_value;
}
EXPORT_SYMBOL(oem_hkadc_usb_therm_value);

int oem_hkadc_usb_therm_error_state(void)
{
	return oem_hkadc_usb_therm_error_flag;
}
EXPORT_SYMBOL(oem_hkadc_usb_therm_error_state);

MODULE_AUTHOR("KYOCERA Corporation");
MODULE_DESCRIPTION("OEM HKADC USB TM Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("oem_hkadc_usb_tm");