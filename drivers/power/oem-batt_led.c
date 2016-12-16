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
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/leds.h>
#include <oem-chg_control.h>
#include <oem-bms.h>
#include <mach/board.h>


#define LOW_BATT_CAPACITY           15
#define LOW_BATT_LED_ON_TIME        250
#define LOW_BATT_LED_OFF_TIME       4750
#define TRICKLE_CHG_LED_BLINK_TIME  2000

enum oem_batt_led_status
{
	OEM_BATT_LED_ST_INVALID,
	OEM_BATT_LED_ST_OFF,
	OEM_BATT_LED_ST_LOW_BATTERY,
	OEM_BATT_LED_ST_CHG_TRICKLE,
	OEM_BATT_LED_ST_CHG_FAST,
	OEM_BATT_LED_ST_CHG_FULL,
	OEM_BATT_LED_ST_TEMP_ANOMALY
};

static struct delayed_work oem_batt_led_work;

static void set_led_param_off(struct led_control_ex_data *led_ctrl,
				enum oem_batt_led_status *led_st)
{
	led_ctrl->priority   = LED_PRIORITY_BATTERY;
	led_ctrl->color      = LED_COLOR_OFF;
	led_ctrl->mode       = LED_BLINK_OFF;
	led_ctrl->pattern[0] = 0;
	*led_st = OEM_BATT_LED_ST_OFF;
}

static void set_led_param_chg_fast(struct led_control_ex_data *led_ctrl,
				enum oem_batt_led_status *led_st)
{
	led_ctrl->priority   = LED_PRIORITY_BATTERY;
	led_ctrl->color      = LED_COLOR_RED;
	led_ctrl->mode       = LED_BLINK_OFF;
	led_ctrl->pattern[0] = 0;
	*led_st = OEM_BATT_LED_ST_CHG_FAST;
}

static void set_led_param_chg_full(struct led_control_ex_data *led_ctrl,
				enum oem_batt_led_status *led_st)
{
	led_ctrl->priority   = LED_PRIORITY_BATTERY;
//oem original setting	led_ctrl->color      = LED_COLOR_GREEN;	
	led_ctrl->color      = LED_COLOR_OFF;
	led_ctrl->mode       = LED_BLINK_OFF;
	led_ctrl->pattern[0] = 0;
	*led_st = OEM_BATT_LED_ST_CHG_FULL;
}

static void set_led_param_chg_trickle(struct led_control_ex_data *led_ctrl,
				enum oem_batt_led_status *led_st)
{
		led_ctrl->priority   = LED_PRIORITY_BATTERY;
//oem original setting		led_ctrl->color      = LED_COLOR_RED;
//oem original setting		led_ctrl->mode       = LED_BLINK_ON;
//oem original setting		led_ctrl->pattern[0] = TRICKLE_CHG_LED_BLINK_TIME;
//oem original setting		led_ctrl->pattern[1] = TRICKLE_CHG_LED_BLINK_TIME;
//oem original setting		led_ctrl->pattern[2] = 0;

	led_ctrl->color      = LED_COLOR_RED;
	led_ctrl->mode       = LED_BLINK_OFF;
	led_ctrl->pattern[0] = 0;

	*led_st = OEM_BATT_LED_ST_CHG_TRICKLE;
}

static void set_led_param_low_batt(struct led_control_ex_data *led_ctrl,
				enum oem_batt_led_status *led_st)
{
	led_ctrl->priority   = LED_PRIORITY_BATTERY;
	led_ctrl->color      = LED_COLOR_RED;
	led_ctrl->mode       = LED_BLINK_ON;
	led_ctrl->pattern[0] = LOW_BATT_LED_ON_TIME;
	led_ctrl->pattern[1] = LOW_BATT_LED_OFF_TIME;
	led_ctrl->pattern[2] = 0;
	*led_st = OEM_BATT_LED_ST_LOW_BATTERY;
}

static void set_led_param_temp_anomaly(struct led_control_ex_data *led_ctrl,
				enum oem_batt_led_status *led_st)
{
	led_ctrl->priority   = LED_PRIORITY_BATTERY;
	led_ctrl->color      = LED_COLOR_RED;
	led_ctrl->mode       = LED_BLINK_ON;
	led_ctrl->pattern[0] = LOW_BATT_LED_ON_TIME;
	led_ctrl->pattern[1] = LOW_BATT_LED_ON_TIME;
	led_ctrl->pattern[2] = 0;
	*led_st = OEM_BATT_LED_ST_TEMP_ANOMALY;
}


static void oem_batt_led_control(void)
{
	struct led_control_ex_data led_ctrl;
	static enum oem_batt_led_status current_led_st = OEM_BATT_LED_ST_INVALID;
	enum oem_batt_led_status new_led_st = OEM_BATT_LED_ST_INVALID;
	int batt_status;
	int batt_capacity;
	int batt_volt_status;

	batt_status = oem_chg_get_batt_property(POWER_SUPPLY_PROP_STATUS);
	batt_capacity = oem_chg_get_current_capacity();
	batt_volt_status = oem_chg_get_current_volt_status();

	switch (batt_status)
	{
		case POWER_SUPPLY_STATUS_DISCHARGING:
			if (oem_is_off_charge())
			{
				if (OEM_ST_VOLT_HIGH == batt_volt_status) {
					set_led_param_temp_anomaly(&led_ctrl, &new_led_st);
				} else if (OEM_CHG_ST_ERR_CHARGER == oem_get_current_chg_state()) {
					set_led_param_temp_anomaly(&led_ctrl, &new_led_st);
				} else {
					set_led_param_off(&led_ctrl, &new_led_st);
				}
			}
			else
			{
				if (OEM_ST_VOLT_HIGH == batt_volt_status) {
					set_led_param_temp_anomaly(&led_ctrl, &new_led_st);
				} else if (OEM_CHG_ST_ERR_CHARGER == oem_get_current_chg_state()) {
					set_led_param_temp_anomaly(&led_ctrl, &new_led_st);
				} else if (LOW_BATT_CAPACITY > batt_capacity) {
					set_led_param_low_batt(&led_ctrl, &new_led_st);
				} else {
					set_led_param_off(&led_ctrl, &new_led_st);
				}
			}
			break;

		case POWER_SUPPLY_STATUS_CHARGING:
			if (oem_is_off_charge())
			{
				if (OEM_ST_VOLT_LOW == batt_volt_status)
				{
					set_led_param_chg_trickle(&led_ctrl, &new_led_st);
				}
				else
				{
					set_led_param_chg_fast(&led_ctrl, &new_led_st);
				}
			}
			else
			{
				set_led_param_chg_fast(&led_ctrl, &new_led_st);
			}
			break;

		case POWER_SUPPLY_STATUS_FULL:
			if (oem_get_status_oem_after_warmcool_recharge_flag() == OEM_AFTER_WARMCOOL_RECHARGING) {
				if (OEM_ST_VOLT_HIGH == batt_volt_status) {
					set_led_param_temp_anomaly(&led_ctrl, &new_led_st);
				} else if (OEM_CHG_ST_ERR_CHARGER == oem_get_current_chg_state()) {
					set_led_param_temp_anomaly(&led_ctrl, &new_led_st);
				} else {
					set_led_param_chg_full(&led_ctrl, &new_led_st);
				}
			}else {
				set_led_param_chg_full(&led_ctrl, &new_led_st);
			}
			break;

		case POWER_SUPPLY_STATUS_NOT_CHARGING:
			set_led_param_off(&led_ctrl, &new_led_st);
			break;

		default:
			new_led_st = current_led_st;
			pr_err("batt_status = %d\n" ,batt_status);
			break;
	}

	if (current_led_st != new_led_st)
	{
		qpnp_set_leddata_ex(&led_ctrl);
		current_led_st = new_led_st;
		pr_info("led_st=%d batt_st=%d cap=%d vbatt_st=%d\n",
			current_led_st, batt_status, batt_capacity, batt_volt_status);
	}
}

static void oem_batt_led_monitor(struct work_struct *work)
{
	oem_batt_led_control();
	schedule_delayed_work(&oem_batt_led_work,
			round_jiffies_relative(msecs_to_jiffies(1000)));
}

void oem_batt_led_init(void)
{
	INIT_DELAYED_WORK(&oem_batt_led_work, oem_batt_led_monitor);
	schedule_delayed_work(&oem_batt_led_work,
			round_jiffies_relative(msecs_to_jiffies(0)));
}
EXPORT_SYMBOL(oem_batt_led_init);
