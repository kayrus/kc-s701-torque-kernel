/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2014 KYOCERA Corporation
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

#include <linux/module.h>
#include <linux/power_supply.h>
#include <oem-chg_control.h>
#include <oem-hkadc.h>
#include <oem-chg_param.h>
#include <mach/oem_fact.h>
#include <linux/reboot.h>
#include <oem-chg_smem.h>
#include <oem-chg_det.h>
#include <mach/board.h>
#include <oem-bms.h>

#define PMA_CHANGE_INPUT_CURRENT_MAX_MA 800000

extern int oem_option_item1_bit0;
extern int oem_option_item1_bit3;
extern int oem_option_item1_bit4;
extern int oem_option_item1_bit6;

static int current_batt_status;
static int current_batt_health;
static int current_batt_voltage;
static int current_capacity = 50;
static int current_batt_temp;
static int current_pa_therm;
static int current_camera_therm;
static int current_board_therm;
static int current_chg_connect;

static struct wake_lock oem_chg_wake_lock;
struct delayed_work oem_chg_battery_removal_work;
struct delayed_work oem_chg_wpc_init_work;
static oem_chg_state_type current_chg_state = OEM_CHG_ST_IDLE;
static oem_chg_state_type new_chg_state = OEM_CHG_ST_IDLE;

static int current_chg_volt_status = OEM_ST_VOLT_LOW;
static int current_chg_temp_status = OEM_ST_TEMP_NORM;

static int battery_limit_volt_status	= OEM_ST_TEMP_NORM;
static int battery_temp_status	= OEM_ST_TEMP_NORM;
static int camera_temp_status	= OEM_ST_TEMP_NORM;
static int board_temp_status	= OEM_ST_TEMP_NORM;
static int battery_limit_soc_status	= OEM_ST_TEMP_NORM;

static int limit_chg_timer = 0;
static bool limit_chg_disable = false;

static uint32_t total_charging_time = 0;

static void oem_chg_wpc_change_iusbmax(void);
static bool oem_chg_wpc_arb_chk(void);

static void oem_chg_wake_lock_control(void)
{
	bool sleep_disable = false;

	switch (current_chg_state) {
		case OEM_CHG_ST_FAST:
		case OEM_CHG_ST_LIMIT:
		case OEM_CHG_ST_ERR_BATT_TEMP:
		case OEM_CHG_ST_ERR_CHARGER:
			sleep_disable = true;
			break;

		default:
			break;
	}

	if (sleep_disable) {
		wake_lock(&oem_chg_wake_lock);
		pr_info("wake_lock set\n");
	} else if (wake_lock_active(&oem_chg_wake_lock)) {
		wake_lock_timeout(&oem_chg_wake_lock, msecs_to_jiffies(CHG_WAKE_LOCK_TIMEOUT_MS));
		pr_info("wake_lock_timeout set:%d[ms]\n", CHG_WAKE_LOCK_TIMEOUT_MS);
	} else {
		pr_info("wake_lock is not active already\n");
	}
}

static void oem_chg_set_usb_property(enum power_supply_property psp, int val)
{
	struct power_supply *usb_psy;
	union power_supply_propval ret = {val,};

	usb_psy = power_supply_get_by_name("usb");
	usb_psy->set_property(usb_psy, psp, &ret);
}

static int oem_chg_get_iusbmax(void)
{
	struct power_supply *usb_psy;
	union power_supply_propval ret = {0,};

	usb_psy = power_supply_get_by_name("usb");
	usb_psy->get_property(usb_psy, POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
	return (ret.intval / 1000);
}

int oem_chg_get_batt_property(enum power_supply_property psp)
{
	struct power_supply *batt_psy;
	union power_supply_propval ret = {0,};

	batt_psy = power_supply_get_by_name("battery");
	batt_psy->get_property(batt_psy, psp, &ret);

	return ret.intval;
}
EXPORT_SYMBOL(oem_chg_get_batt_property);

static bool is_current_property_init = false;
static void oem_chg_update_batt_property(void)
{
	current_batt_status		= oem_chg_get_batt_property(POWER_SUPPLY_PROP_STATUS);
	current_batt_health		= oem_chg_get_batt_property(POWER_SUPPLY_PROP_HEALTH);
	current_batt_voltage	= oem_chg_get_batt_property(POWER_SUPPLY_PROP_VOLTAGE_NOW);
	current_capacity		= oem_chg_get_batt_property(POWER_SUPPLY_PROP_CAPACITY);
	current_batt_temp		= oem_chg_get_batt_property(POWER_SUPPLY_PROP_TEMP);
	current_pa_therm		= oem_chg_get_batt_property(POWER_SUPPLY_PROP_OEM_PA_THERM);
	current_camera_therm	= oem_chg_get_batt_property(POWER_SUPPLY_PROP_OEM_CAMERA_THERM);
	current_board_therm		= oem_chg_get_batt_property(POWER_SUPPLY_PROP_OEM_SUBSTRATE_THERM);
	current_chg_connect		= oem_chg_get_batt_property(POWER_SUPPLY_PROP_OEM_CHARGE_CONNECT_STATE);
	is_current_property_init = true;

	pr_debug("st:%d h:%d bv:%d cap:%d bt:%d pa:%d cam:%d bo:%d con:%d\n",
		current_batt_status, current_batt_health, current_batt_voltage,
		current_capacity, current_batt_temp, current_pa_therm,
		current_camera_therm, current_board_therm, current_chg_connect);

	oem_chg_smem_write_vbat_mv(current_batt_voltage / 1000);
}

static void oem_chg_check_battery_removal(void)
{
	if (oem_option_item1_bit0) {
		return;
	}

	if (oem_chg_get_current_chg_connect() == POWER_SUPPLY_OEM_CONNECT_NONE) {
		return;
	}

	if (oem_chg_get_batt_property(POWER_SUPPLY_PROP_PRESENT)) {
		return;
	}
	schedule_delayed_work(&oem_chg_battery_removal_work,
			msecs_to_jiffies(BATT_REMOVAL_CHECK_TIME));
	pr_info("set oem_chg_battery_removal_work:%dms\n",BATT_REMOVAL_CHECK_TIME);
}

static void oem_chg_battery_removal(struct work_struct *work)
{
	if (oem_chg_get_batt_property(POWER_SUPPLY_PROP_PRESENT)) {
		return;
	}
	pr_err("power off, detected a battery removal\n");
	kernel_power_off();
}

static int oem_chg_check_timer(void)
{
	bool ret = false;

	if (total_charging_time < CHG_ERR_TIME_SEC) {
		if (!(total_charging_time % 60)) {
			pr_info("total:%d[min] timer:%d[min]\n",
				total_charging_time/60, CHG_ERR_TIME_SEC/60);
		}
		total_charging_time += OEM_HKADC_MONITOR_TIME_MS / 1000;
	} else {
		pr_info("expire total:%d[min] timer:%d[min]\n",
			total_charging_time/60, CHG_ERR_TIME_SEC/60);
		ret = true;
	}
	return ret;
}

static void oem_chg_reset_timer(void)
{
	total_charging_time = 0;
}

/*
static bool oem_chg_is_voice_call(void)
{
	bool ret = false;

	if (CDMA_ST_TALK == oem_chg_get_cdma_status()) {
		ret = true;
	}
	return ret;
}

static bool oem_chg_is_data_communication(void)
{
	bool ret = false;

	if ((CDMA_ST_DATA == oem_chg_get_cdma_status()) ||
		(EVDO_ST_ON == oem_chg_get_evdo_status()) ||
		(LTE_ST_ON == oem_chg_get_lte_status())) {
		ret = true;
	}
	return ret;
}
*/

static int oem_chg_check_volt_status(void)
{
	int ret = false;
	int new_chg_volt_status = OEM_ST_VOLT_NORM;
	int batt_voltage = oem_chg_get_current_batt_voltage();

	switch (oem_chg_get_current_volt_status()) {

	case OEM_ST_VOLT_LOW:
		if (batt_voltage >= BATT_VOLT_ERR_CHG_OFF) {
			new_chg_volt_status = OEM_ST_VOLT_HIGH;
		} else if (batt_voltage >= BATT_VOLT_POWER_ON_OK) {
			new_chg_volt_status = OEM_ST_VOLT_NORM;
		} else {
			new_chg_volt_status = OEM_ST_VOLT_LOW;
		}
		break;

	case OEM_ST_VOLT_NORM:
		if (batt_voltage >= BATT_VOLT_ERR_CHG_OFF) {
			new_chg_volt_status = OEM_ST_VOLT_HIGH;
		} else if (batt_voltage <= BATT_VOLT_POWER_ON_NG) {
			new_chg_volt_status = OEM_ST_VOLT_LOW;
		} else {
			new_chg_volt_status = OEM_ST_VOLT_NORM;
		}
		break;

	case OEM_ST_VOLT_HIGH:
		new_chg_volt_status = OEM_ST_VOLT_HIGH;
		break;

	default:
		break;
	}

	if (oem_chg_get_current_volt_status() != new_chg_volt_status) {
		pr_info("chg_volt_status current:%d new:%d\n",
			current_chg_volt_status, new_chg_volt_status);
		current_chg_volt_status = new_chg_volt_status;
		ret = true;
	}
	return ret;
}

static void oem_chg_battery_temp_check(void)
{
	int batt_temp_hot_chg_off	= BATT_TEMP_HOT_CHG_OFF;
	int batt_temp_hot_chg_on	= BATT_TEMP_HOT_CHG_ON;
	int batt_temp_cold_chg_off	= BATT_TEMP_COLD_CHG_OFF;
	int batt_temp_cold_chg_on	= BATT_TEMP_COLD_CHG_ON;
	int batt_temp_limit_chg_on	= BATT_TEMP_LIMIT_ON;
	int batt_temp_limit_chg_off	= BATT_TEMP_LIMIT_OFF;
	int temp = oem_chg_get_current_batt_temp();

	if (oem_is_off_charge()) {
		batt_temp_limit_chg_on =  FACTORY_TEMP_LIMIT_CHG_ON;
		batt_temp_limit_chg_off = FACTORY_TEMP_LIMIT_CHG_ON;
	}

	if (oem_option_item1_bit3) {
		batt_temp_hot_chg_off = FACTORY_TEMP_CHG_OFF;
		batt_temp_limit_chg_on = FACTORY_TEMP_LIMIT_CHG_ON;
	}

	switch (oem_chg_get_current_temp_status()) {

	case OEM_ST_TEMP_NORM:
		if (batt_temp_cold_chg_off >= temp) {
			battery_temp_status = OEM_ST_TEMP_STOP;
		} else if (batt_temp_hot_chg_off <= temp) {
			battery_temp_status = OEM_ST_TEMP_STOP;
		} else if (batt_temp_limit_chg_on <= temp) {
			battery_temp_status = OEM_ST_TEMP_LIMIT;
		} else {
			battery_temp_status = OEM_ST_TEMP_NORM;
		}
		break;

	case OEM_ST_TEMP_LIMIT:
		if (batt_temp_hot_chg_off <= temp) {
			battery_temp_status = OEM_ST_TEMP_STOP;
		} else if (batt_temp_limit_chg_off < temp) {
			battery_temp_status = OEM_ST_TEMP_LIMIT;
		} else {
			battery_temp_status = OEM_ST_TEMP_NORM;
		}
		break;

	case OEM_ST_TEMP_STOP:
		if (batt_temp_cold_chg_on > temp) {
			battery_temp_status = OEM_ST_TEMP_STOP;
		} else if (batt_temp_hot_chg_on < temp) {
			battery_temp_status = OEM_ST_TEMP_STOP;
		} else if (batt_temp_limit_chg_off < temp) {
			battery_temp_status = OEM_ST_TEMP_LIMIT;
		} else {
			battery_temp_status = OEM_ST_TEMP_NORM;
		}
		break;

	default:
		break;
	}
}

static void oem_chg_camera_temp_check(void)
{
	int cam_temp_chg_off		= FACTORY_TEMP_CHG_OFF;
	int cam_temp_chg_on			= FACTORY_TEMP_CHG_OFF;
	int cam_temp_limit_chg_on	= FACTORY_TEMP_LIMIT_CHG_ON;
	int cam_temp_limit_chg_off	= FACTORY_TEMP_LIMIT_CHG_ON;
	int temp = oem_chg_get_current_camera_therm();

	if (oem_is_off_charge()) {
		cam_temp_chg_off		= CAM_TEMP_HOT_CHG_OFF;
		cam_temp_chg_on			= CAM_TEMP_HOT_CHG_ON;
	}
	if (oem_option_item1_bit3) {
		cam_temp_chg_off		= FACTORY_TEMP_CHG_OFF;
		cam_temp_limit_chg_on	= FACTORY_TEMP_LIMIT_CHG_ON;
	}

	switch (oem_chg_get_current_temp_status()) {

	case OEM_ST_TEMP_NORM:
		if (cam_temp_chg_off <= temp) {
			camera_temp_status = OEM_ST_TEMP_STOP;
		} else if (cam_temp_limit_chg_on <= temp) {
			camera_temp_status = OEM_ST_TEMP_LIMIT;
		} else {
			camera_temp_status = OEM_ST_TEMP_NORM;
		}
		break;

	case OEM_ST_TEMP_LIMIT:
		if (cam_temp_chg_off <= temp) {
			camera_temp_status = OEM_ST_TEMP_STOP;
		} else if (cam_temp_limit_chg_off < temp) {
			camera_temp_status = OEM_ST_TEMP_LIMIT;
		} else {
			camera_temp_status = OEM_ST_TEMP_NORM;
		}
		break;

	case OEM_ST_TEMP_STOP:
		if (cam_temp_chg_on < temp) {
			camera_temp_status = OEM_ST_TEMP_STOP;
		} else if (cam_temp_limit_chg_off < temp) {
			camera_temp_status = OEM_ST_TEMP_LIMIT;
		} else {
			camera_temp_status = OEM_ST_TEMP_NORM;
		}
		break;

	default:
		break;
	}
}

static void oem_chg_board_temp_check(void)
{
	int board_temp_chg_off			= FACTORY_TEMP_CHG_OFF;
	int board_temp_chg_on			= FACTORY_TEMP_CHG_OFF;
	int board_temp_limit_chg_on		= FACTORY_TEMP_LIMIT_CHG_ON;
	int board_temp_limit_chg_off	= FACTORY_TEMP_LIMIT_CHG_ON;
	int temp = oem_chg_get_current_board_therm();

	if (oem_is_off_charge()) {
		board_temp_chg_off			= BOARD_TEMP_HOT_CHG_OFF;
		board_temp_chg_on			= BOARD_TEMP_HOT_CHG_ON;
	}
	if (oem_option_item1_bit3) {
		board_temp_chg_off			= FACTORY_TEMP_CHG_OFF;
		board_temp_limit_chg_on		= FACTORY_TEMP_LIMIT_CHG_ON;
	} else if (oem_chg_get_current_chg_connect() > POWER_SUPPLY_OEM_CONNECT_USB) {
		board_temp_limit_chg_on		= BOARD_TEMP_W_CHG_LIMIT_ON;
		board_temp_limit_chg_off	= BOARD_TEMP_W_CHG_LIMIT_OFF;
	} else {
		board_temp_limit_chg_on		= BOARD_TEMP_LIMIT_ON;
		board_temp_limit_chg_off	= BOARD_TEMP_LIMIT_OFF;
	}

	switch (oem_chg_get_current_temp_status()) {

	case OEM_ST_TEMP_NORM:
		if (board_temp_chg_off <= temp) {
			board_temp_status = OEM_ST_TEMP_STOP;
		} else if (board_temp_limit_chg_on <= temp) {
			board_temp_status = OEM_ST_TEMP_LIMIT;
		} else {
			board_temp_status = OEM_ST_TEMP_NORM;
		}
		break;

	case OEM_ST_TEMP_LIMIT:
		if (board_temp_chg_off <= temp) {
			board_temp_status = OEM_ST_TEMP_STOP;
		} else if (board_temp_limit_chg_off < temp) {
			board_temp_status = OEM_ST_TEMP_LIMIT;
		} else {
			board_temp_status = OEM_ST_TEMP_NORM;
		}
		break;

	case OEM_ST_TEMP_STOP:
		if (board_temp_chg_on < temp) {
			board_temp_status = OEM_ST_TEMP_STOP;
		} else if (board_temp_limit_chg_off < temp) {
			board_temp_status = OEM_ST_TEMP_LIMIT;
		} else {
			board_temp_status = OEM_ST_TEMP_NORM;
		}
		break;

	default:
		break;
	}
}

static void oem_chg_battery_volt_limit_check(void)
{
	int batt_volt_chg_limit_on = BATT_VOLT_LIMIT_CHG_ON;
	int batt_volt_chg_limit_off = BATT_VOLT_LIMIT_CHG_OFF;
	int batt_volt = oem_chg_get_current_batt_voltage();

	switch (oem_chg_get_current_temp_status()) {

	case OEM_ST_TEMP_NORM:
		if (batt_volt_chg_limit_on <= batt_volt) {
			battery_limit_volt_status = OEM_ST_TEMP_LIMIT;
		} else {
			battery_limit_volt_status = OEM_ST_TEMP_NORM;
		}
		break;

	case OEM_ST_TEMP_LIMIT:
		if (batt_volt_chg_limit_off >= batt_volt) {
			battery_limit_volt_status = OEM_ST_TEMP_NORM;
		} else {
			battery_limit_volt_status = OEM_ST_TEMP_LIMIT;
		}
		break;

	case OEM_ST_TEMP_STOP:
		break;

	default:
		break;
	}
}

static void oem_chg_battery_soc_limit_check(void)
{
	int batt_soc = oem_chg_get_current_capacity();

	if (batt_soc >= BATT_SOC_LIMIT_OFF) {
		battery_limit_soc_status = OEM_ST_TEMP_NORM;
	} else {
		battery_limit_soc_status = OEM_ST_TEMP_LIMIT;
	}
	pr_debug("SOC = %d, battery_limit_soc_status = %d\n" ,batt_soc ,battery_limit_soc_status);
}

static int oem_chg_check_temp_status(void)
{
	int ret = false;
	int new_chg_temp_status = OEM_ST_TEMP_NORM;
	oem_chg_update_modem_status();
	oem_chg_battery_volt_limit_check();
	oem_chg_battery_temp_check();
	oem_chg_camera_temp_check();
	oem_chg_board_temp_check();
	oem_chg_battery_soc_limit_check();

	if ((POWER_SUPPLY_HEALTH_OVERHEAT == oem_chg_get_current_batt_health()) ||
		(POWER_SUPPLY_HEALTH_COLD == oem_chg_get_current_batt_health()) ||
		(OEM_ST_TEMP_STOP == battery_temp_status) ||
		(OEM_ST_TEMP_STOP == camera_temp_status) ||
		(OEM_ST_TEMP_STOP == board_temp_status)) {
		new_chg_temp_status = OEM_ST_TEMP_STOP;
	} else if ((OEM_ST_TEMP_LIMIT == battery_limit_volt_status) &&
		(OEM_ST_TEMP_LIMIT == battery_limit_soc_status) &&
		((OEM_ST_TEMP_LIMIT == battery_temp_status) ||
		 (OEM_ST_TEMP_LIMIT <= camera_temp_status) ||
		 (OEM_ST_TEMP_LIMIT <= board_temp_status))) {
		new_chg_temp_status = OEM_ST_TEMP_LIMIT;
	}

	if (oem_chg_get_current_temp_status() != new_chg_temp_status) {
		pr_info("chg_temp_status current:%d new:%d\n",current_chg_temp_status, new_chg_temp_status);
		current_chg_temp_status = new_chg_temp_status;
		pr_info("volt:%d batt:%d cam:%d board:%d 1x:%d evdo:%d lte:%d\n",
			oem_chg_get_current_batt_voltage(),
			oem_chg_get_current_batt_temp(),
			oem_chg_get_current_camera_therm(),
			oem_chg_get_current_board_therm(),
			oem_chg_get_cdma_status(),
			oem_chg_get_evdo_status(),
			oem_chg_get_lte_status());
		ret = true;
	}
	return ret;
}

static void oem_chg_limit_control(void)
{
	int oem_chg_limit_check_time = LIMIT_CHG_ON_TIME;

	limit_chg_timer += OEM_HKADC_MONITOR_TIME_MS;

	if (limit_chg_disable) {
		oem_chg_limit_check_time = LIMIT_CHG_OFF_TIME;
	}

	if (limit_chg_timer >= oem_chg_limit_check_time ) {
		limit_chg_timer = 0;
		limit_chg_disable ^= 1;
		oem_chg_buck_ctrl(limit_chg_disable);
		pr_info("limit_chg_disable=%d time_out=%d\n",
			limit_chg_disable, oem_chg_limit_check_time);
	}
}

static void oem_chg_init_limit_control(void)
{
	oem_chg_buck_ctrl(1);
	limit_chg_timer = 0;
	limit_chg_disable = true;
}

void oem_chg_set_charging(void)
{
	oem_chg_batt_fet_ctrl(1);
	oem_chg_buck_ctrl(0);
	current_chg_state = OEM_CHG_ST_FAST;
	new_chg_state = OEM_CHG_ST_FAST;
}
EXPORT_SYMBOL(oem_chg_set_charging);

void oem_chg_set_charger_power_supply(void)
{
	oem_chg_batt_fet_ctrl(0);
	oem_chg_buck_ctrl(0);
}
EXPORT_SYMBOL(oem_chg_set_charger_power_supply);

void oem_chg_set_battery_power_supply(void)
{
	oem_chg_batt_fet_ctrl(0);
	oem_chg_buck_ctrl(1);
}
EXPORT_SYMBOL(oem_chg_set_battery_power_supply);

void oem_chg_stop_charging(void)
{
	oem_clear_oem_chg_vbatdet_change_flag();
	oem_chg_batt_fet_ctrl(1);

	oem_chg_buck_ctrl(0);
	current_chg_state = OEM_CHG_ST_IDLE;
	new_chg_state = OEM_CHG_ST_IDLE;
	current_chg_volt_status = OEM_ST_TEMP_NORM;
	current_chg_temp_status = OEM_ST_TEMP_NORM;
	
	oem_clear_oem_after_warmcool_recharge_flag();

	oem_chg_reset_timer();
	oem_chg_wake_lock_control();
}
EXPORT_SYMBOL(oem_chg_stop_charging);

int oem_chg_get_current_batt_status(void)
{
	int ret = current_batt_status;
	return ret;
}
EXPORT_SYMBOL(oem_chg_get_current_batt_status);

int oem_chg_get_current_batt_health(void)
{
	int ret = current_batt_health;
	return ret;
};
EXPORT_SYMBOL(oem_chg_get_current_batt_health);

int oem_chg_get_current_batt_voltage(void)
{
	int ret = current_batt_voltage;
	return ret;
}
EXPORT_SYMBOL(oem_chg_get_current_batt_voltage);

int oem_chg_get_current_capacity(void)
{
	int ret = current_capacity;
	return ret;
}
EXPORT_SYMBOL(oem_chg_get_current_capacity);

int oem_chg_get_current_batt_temp(void)
{
	if (!is_current_property_init) {
		current_batt_temp = oem_chg_get_batt_property(POWER_SUPPLY_PROP_TEMP);
		pr_info("is_current_property_init:false %s%dC\n",
			current_batt_temp < 0 ? "-" : "+", (int)abs(current_batt_temp));
	}
	return current_batt_temp;
}
EXPORT_SYMBOL(oem_chg_get_current_batt_temp);

int oem_chg_get_current_pa_therm(void)
{
	int ret = current_pa_therm;
	return ret;
}
EXPORT_SYMBOL(oem_chg_get_current_pa_therm);

int oem_chg_get_current_camera_therm(void)
{
	int ret = current_camera_therm;
	return ret;
}
EXPORT_SYMBOL(oem_chg_get_current_camera_therm);

int oem_chg_get_current_board_therm(void)
{
	int ret = current_board_therm;
	return ret;
}
EXPORT_SYMBOL(oem_chg_get_current_board_therm);

int oem_chg_get_current_chg_connect(void)
{
	int ret = current_chg_connect;
	return ret;
}
EXPORT_SYMBOL(oem_chg_get_current_chg_connect);

int oem_chg_get_current_temp_status(void)
{
	int ret = current_chg_temp_status;
	return ret;
}
EXPORT_SYMBOL(oem_chg_get_current_temp_status);

int oem_chg_get_current_volt_status(void)
{
	int ret = current_chg_volt_status;
	return ret;
}
EXPORT_SYMBOL(oem_chg_get_current_volt_status);

extern bool oem_is_usb_overvolutage_bit;


#define BATT_TEMP_RECHERGE_HOT		(35 * 10)
#define BATT_TEMP_RECHERGE_LOW		(15 * 10)

int oem_chg_control(void)
{
	int req_ps_changed_batt = false;

	int temp = oem_chg_get_current_batt_temp();

	oem_chg_update_batt_property();

	oem_chg_wpc_change_iusbmax();

	oem_chg_check_battery_removal();

	if (1 == oem_param_share.test_mode_chg_disable) {
		return false;
	}

	switch (current_chg_state) {
		case OEM_CHG_ST_IDLE:
			if (oem_chg_get_current_chg_connect() == POWER_SUPPLY_OEM_CONNECT_NONE) {
				break;
			}
			req_ps_changed_batt = oem_chg_check_volt_status();
			req_ps_changed_batt |= oem_chg_check_temp_status();
			if (oem_chg_get_current_volt_status() == OEM_ST_VOLT_HIGH) {
				new_chg_state = OEM_CHG_ST_ERR_BATT_VOLT;
			} else if (oem_is_usb_overvolutage_bit) {
					new_chg_state = OEM_CHG_ST_ERR_CHARGER;
			} else if (oem_chg_is_thermal_limitation()) {
				new_chg_state = OEM_CHG_ST_ERR_THERMAL_CTRL;
			} else if (oem_chg_get_current_temp_status() == OEM_ST_TEMP_STOP) {
				new_chg_state = OEM_CHG_ST_ERR_BATT_TEMP;
			} else if (oem_chg_get_current_temp_status() == OEM_ST_TEMP_LIMIT) {
				new_chg_state = OEM_CHG_ST_LIMIT;
			} else if (oem_chg_get_current_batt_status() == POWER_SUPPLY_STATUS_FULL) {
				new_chg_state = OEM_CHG_ST_FULL;
			} else if (oem_chg_get_current_batt_status() == POWER_SUPPLY_STATUS_CHARGING) {
				new_chg_state = OEM_CHG_ST_FAST;
			}
			break;

		case OEM_CHG_ST_FAST:
			req_ps_changed_batt = oem_chg_check_volt_status();
			req_ps_changed_batt |= oem_chg_check_temp_status();
			if (oem_chg_get_current_volt_status() == OEM_ST_VOLT_HIGH) {
				new_chg_state = OEM_CHG_ST_ERR_BATT_VOLT;
			} else	if (oem_is_usb_overvolutage_bit) {
					new_chg_state = OEM_CHG_ST_ERR_CHARGER;
			} else if (oem_chg_is_thermal_limitation()) {
				new_chg_state = OEM_CHG_ST_ERR_THERMAL_CTRL;
			} else if (oem_chg_get_current_temp_status() == OEM_ST_TEMP_STOP) {
				new_chg_state = OEM_CHG_ST_ERR_BATT_TEMP;
			} else if (oem_chg_wpc_arb_chk() == true) {
				new_chg_state = OEM_CHG_ST_ERR_OEM_ARB;
			} else if (oem_chg_get_current_temp_status() == OEM_ST_TEMP_LIMIT) {
				new_chg_state = OEM_CHG_ST_LIMIT;
			} else if (oem_chg_get_current_batt_status() == POWER_SUPPLY_STATUS_FULL) {
				new_chg_state = OEM_CHG_ST_FULL;
			} else if (oem_chg_check_timer()) {
				new_chg_state = OEM_CHG_ST_ERR_TIME_OVER;
			}
			break;

		case OEM_CHG_ST_LIMIT:
			req_ps_changed_batt = oem_chg_check_volt_status();
			req_ps_changed_batt |= oem_chg_check_temp_status();
			if (oem_chg_get_current_volt_status() == OEM_ST_VOLT_HIGH) {
				new_chg_state = OEM_CHG_ST_ERR_BATT_VOLT;
			} else if (oem_chg_is_thermal_limitation()) {
				new_chg_state = OEM_CHG_ST_ERR_THERMAL_CTRL;
			} else if (oem_chg_get_current_temp_status() == OEM_ST_TEMP_STOP) {
				new_chg_state = OEM_CHG_ST_ERR_BATT_TEMP;
			} else if (oem_chg_get_current_temp_status() == OEM_ST_TEMP_NORM) {
				new_chg_state = OEM_CHG_ST_FAST;
			} else if (oem_chg_get_current_batt_status() == POWER_SUPPLY_STATUS_FULL) {
				new_chg_state = OEM_CHG_ST_FULL;
			} else {
				oem_chg_limit_control();
			}
			break;

		case OEM_CHG_ST_FULL:
			if (oem_is_usb_overvolutage_bit) {
					new_chg_state = OEM_CHG_ST_ERR_CHARGER;
			} else if (oem_chg_get_current_batt_status() == POWER_SUPPLY_STATUS_CHARGING) {
				new_chg_state = OEM_CHG_ST_FAST;
			} else if (oem_chg_is_thermal_limitation()) {
				oem_clear_oem_after_warmcool_recharge_flag();
				new_chg_state = OEM_CHG_ST_ERR_THERMAL_CTRL;
			} else if (oem_get_status_oem_after_warmcool_recharge_flag() == OEM_AFTER_WARMCOOL_CHARGE_DONE_ON_NOT_NORMAL) {
				if( (BATT_TEMP_RECHERGE_LOW <= temp ) && ( temp <= BATT_TEMP_RECHERGE_HOT) ){
					pr_debug("Recharge temp:%d", temp);
					oem_recharge_request();
				}
			} else if (oem_get_status_oem_after_warmcool_recharge_flag() == OEM_AFTER_WARMCOOL_RECHARGING) {
				req_ps_changed_batt = oem_chg_check_volt_status();
				req_ps_changed_batt |= oem_chg_check_temp_status();
				if (oem_chg_get_current_volt_status() == OEM_ST_VOLT_HIGH) {
					oem_clear_oem_after_warmcool_recharge_flag();
					new_chg_state = OEM_CHG_ST_ERR_BATT_VOLT;
				} else if (oem_chg_get_current_temp_status() == OEM_ST_TEMP_STOP) {
					oem_clear_oem_after_warmcool_recharge_flag();
					new_chg_state = OEM_CHG_ST_ERR_BATT_TEMP;
				}
			} else if (oem_chg_wpc_arb_chk() == true) {
				new_chg_state = OEM_CHG_ST_ERR_OEM_ARB;
			}
			break;

		case OEM_CHG_ST_ERR_BATT_TEMP:
			req_ps_changed_batt = oem_chg_check_temp_status();
			if (oem_chg_get_current_temp_status() != OEM_ST_TEMP_STOP) {
				new_chg_state = OEM_CHG_ST_FAST;
			} else if (oem_chg_is_thermal_limitation()) {
				new_chg_state = OEM_CHG_ST_ERR_THERMAL_CTRL;
			}
			break;

		case OEM_CHG_ST_ERR_BATT_VOLT:
            /* no processing */
			break;

		case OEM_CHG_ST_ERR_TIME_OVER:
            /* no processing */
			break;

		case OEM_CHG_ST_ERR_CHARGER:
            /* no processing */
			break;

		case OEM_CHG_ST_ERR_THERMAL_CTRL:
			if (!oem_chg_is_thermal_limitation()) {
				new_chg_state = OEM_CHG_ST_IDLE;
			}
			break;

		case OEM_CHG_ST_ERR_OEM_ARB:
			new_chg_state = OEM_CHG_ST_IDLE;
			break;
		default:
			break;
	}

	if (new_chg_state != current_chg_state) {
		switch (new_chg_state) {
			case OEM_CHG_ST_IDLE:
				oem_chg_stop_charging();
				break;

			case OEM_CHG_ST_FAST:
				oem_chg_set_charging();
				break;

			case OEM_CHG_ST_LIMIT:
				oem_chg_init_limit_control();
				break;

			case OEM_CHG_ST_FULL:
				/* no processing */
				break;

			case OEM_CHG_ST_ERR_BATT_TEMP:
				oem_chg_set_charger_power_supply();
				break;

			case OEM_CHG_ST_ERR_BATT_VOLT:
				oem_chg_set_battery_power_supply();
				break;

			case OEM_CHG_ST_ERR_TIME_OVER:
				oem_chg_set_charger_power_supply();
				break;

			case OEM_CHG_ST_ERR_CHARGER:
				oem_chg_set_battery_power_supply();
				break;

			case OEM_CHG_ST_ERR_THERMAL_CTRL:
				oem_chg_set_battery_power_supply();
				break;

			case OEM_CHG_ST_ERR_OEM_ARB:
					oem_chg_set_battery_power_supply();
				break;

			default:
				break;
		}
		pr_info("chg_state current:%d new:%d\n",current_chg_state, new_chg_state);
		current_chg_state = new_chg_state;
		req_ps_changed_batt |= true;
		oem_chg_reset_timer();
		oem_chg_wake_lock_control();
	}
	return req_ps_changed_batt;
}
EXPORT_SYMBOL(oem_chg_control);

static int wpc_init_st = -1;
void oem_chg_wpc_init_start(void)
{
	int batt_temp = oem_chg_get_current_batt_temp();

	if ((batt_temp >= BATT_TEMP_HOT_CHG_OFF) ||
		(batt_temp <= BATT_TEMP_COLD_CHG_OFF)) {
		pr_info("temp error:%dC can't wpc init start\n", batt_temp);
		return;
	}
	oem_chg_set_battery_power_supply();
	wpc_init_st = 0;
	schedule_delayed_work(&oem_chg_wpc_init_work,
		msecs_to_jiffies(WPC_INIT_STEP1_TIME_MS));
	pr_info("oem_chg_wpc_init_work:%d\n", WPC_INIT_STEP1_TIME_MS);
}
EXPORT_SYMBOL(oem_chg_wpc_init_start);

void oem_chg_wpc_init_stop(void)
{
	if (wpc_init_st != -1) {
		cancel_delayed_work_sync(&oem_chg_wpc_init_work);
		pr_info("cancel oem_chg_wpc_init_work\n");
		wpc_init_st = -1;
	}
}
EXPORT_SYMBOL(oem_chg_wpc_init_stop);

static void oem_chg_wpc_init(struct work_struct *work)
{
	switch (wpc_init_st) {
		case 0:
			oem_chg_set_charging();
			wpc_init_st = 1;
			schedule_delayed_work(&oem_chg_wpc_init_work,
				msecs_to_jiffies(WPC_INIT_STEP2_TIME_MS));
			pr_info("oem_chg_set_charging oem_chg_wpc_init_work:%d\n",
				WPC_INIT_STEP2_TIME_MS);
			break;

		case 1:
			oem_chg_set_usb_property(POWER_SUPPLY_PROP_CURRENT_MAX,
				WPC_INIT_STEP3_CURRENT_UA);
			wpc_init_st = 2;
			pr_info("POWER_SUPPLY_PROP_CURRENT_MAX:%dmA\n",
				WPC_INIT_STEP3_CURRENT_UA / 1000);
			break;

		default:
			break;
	}
}

void oem_chg_control_init(void)
{
	wake_lock_init(&oem_chg_wake_lock, WAKE_LOCK_SUSPEND, "oem_chg_control");
	INIT_DELAYED_WORK(&oem_chg_battery_removal_work,
		oem_chg_battery_removal);
	INIT_DELAYED_WORK(&oem_chg_wpc_init_work, oem_chg_wpc_init);
	pr_info("CHG_ERR_TIME_SEC:%d[min]\n", CHG_ERR_TIME_SEC/60);
}
EXPORT_SYMBOL(oem_chg_control_init);

oem_chg_state_type oem_get_current_chg_state(void)
{
	return current_chg_state;
}
EXPORT_SYMBOL(oem_get_current_chg_state);


static uint32_t *uim_status_smem_ptr = NULL;
void oem_uim_smem_init(void)
{
	uim_status_smem_ptr = (uint32_t *)kc_smem_alloc(SMEM_UICC_INFO, 1);
	if (NULL == uim_status_smem_ptr) {
		pr_err("smem uicc read error.\n");
		return;
	}
}
EXPORT_SYMBOL(oem_uim_smem_init);

static bool is_oem_uim_status(void)
{
	if (NULL == uim_status_smem_ptr)
	{
		pr_err("uim is detatched\n");
		return false;
	}

	if ((0 == *uim_status_smem_ptr) || (1 == *uim_status_smem_ptr))
	{
		return true;
	}
	return false;
}

bool oem_is_uim_dete(void)
{
	if (oem_option_item1_bit4) {
		return true;
	}
	
	return is_oem_uim_status();
}
EXPORT_SYMBOL(oem_is_uim_dete);

void oem_chg_uim_status_check(void)
{
	static int initialized = 0;

	if(initialized)
		return;

	if (NULL == uim_status_smem_ptr)
	{
		pr_err("uim is detatched\n");
		return;
	}

	if(*uim_status_smem_ptr != 0) {
		oem_chg_set_uim_status_fix_parm();
		oem_bms_uim_status_notify();
		initialized = 1;
	}
}
EXPORT_SYMBOL(oem_chg_uim_status_check);

#define WLC_IUSB_MAX_MIDDLE	400
#define WLC_IUSB_MAX_HIGH	700
#define WLC_IUSB_MAX_DEFAULT	WLC_IUSB_MAX_MIDDLE
#define WLC_IUSB_MAX_STEP	100

#define DELTA_VDD_MAX_FULL	20

static void oem_chg_wpc_change_iusbmax(void)
{
	int voltage_now_mv = 0;
	unsigned int vdd_max_mv = 0;
	unsigned int resume_delta_mv = 0;

	int current_iusbmax_mA = 0;
	int iusbmax_mA = 0;
	static int target_iusbmax_mA = WLC_IUSB_MAX_DEFAULT;

	if((wpc_init_st == 2) &&
		(oem_chg_get_current_chg_connect()
			== POWER_SUPPLY_OEM_CONNECT_CHARGE_STAND_1)) {

		current_iusbmax_mA = oem_chg_get_iusbmax();

		voltage_now_mv = oem_chg_get_current_batt_voltage() / 1000;
		vdd_max_mv = oem_vddmax_get();
		resume_delta_mv = oem_resume_delta_mv();

		switch(target_iusbmax_mA) {
		case WLC_IUSB_MAX_MIDDLE:
			if(voltage_now_mv < (vdd_max_mv - resume_delta_mv)) {
				target_iusbmax_mA = WLC_IUSB_MAX_HIGH;
			}
			break;

		case WLC_IUSB_MAX_HIGH:
			if((vdd_max_mv - DELTA_VDD_MAX_FULL) <= voltage_now_mv) {
				target_iusbmax_mA = WLC_IUSB_MAX_MIDDLE;
			}
			break;
		default:
			target_iusbmax_mA = WLC_IUSB_MAX_DEFAULT;
			break;
		}

		if(target_iusbmax_mA < current_iusbmax_mA) {
			iusbmax_mA = current_iusbmax_mA - WLC_IUSB_MAX_STEP;
		}
		else if(current_iusbmax_mA < target_iusbmax_mA) {
			iusbmax_mA = current_iusbmax_mA + WLC_IUSB_MAX_STEP;
		}
		else {
			iusbmax_mA = current_iusbmax_mA;
		}
		if(current_iusbmax_mA != iusbmax_mA) {
			pr_debug("voltage_now[%d], vdd_max[%d], delta[%d], target_iusbmax[%d]\n"
							, voltage_now_mv, vdd_max_mv, resume_delta_mv, target_iusbmax_mA);
			pr_debug("old_iusbmax_mA[%d], new_iusbmax_mA[%d]\n"
							, current_iusbmax_mA, iusbmax_mA);
			oem_chg_set_usb_property(POWER_SUPPLY_PROP_CURRENT_MAX,
				iusbmax_mA *1000);
		}
	}
	else {
		target_iusbmax_mA = WLC_IUSB_MAX_DEFAULT;
	}
	return;
}

int oem_chg_wpc_get_init_status(void)
{
	return wpc_init_st;
}

#define OEM_CHG_WPC_IADC_THRESHOLD   0x0A
#define OEM_CHG_VUSB_CHK_CNT         3
#define OEM_CHG_VUSB_LOW_THRESHOLD   4000000
#define OEM_CHG_VUSB_HIGH_THRESHOLD  4500000
static bool oem_chg_wpc_arb_chk(void)
{
	int ret = 0;
	char iadc_val = 0;
	bool arb_status = false;
	int vusb_uv = 0;

	if((wpc_init_st >= 1) &&
		(oem_chg_get_current_chg_connect()
			== POWER_SUPPLY_OEM_CONNECT_CHARGE_STAND_1)) {

		ret = oem_chg_wpc_get_iadc_reg(&iadc_val);
		pr_debug("oem_arb_chk iadc = 0x%02X ret = %d \n", iadc_val, ret);
		if( ret == 1 && iadc_val <= OEM_CHG_WPC_IADC_THRESHOLD) {
			vusb_uv = oem_chg_get_vusb_averaged(OEM_CHG_VUSB_CHK_CNT);
			pr_debug("oem_arb_chk VUSB = %d uv sample = %d \n", vusb_uv, OEM_CHG_VUSB_CHK_CNT);
			if( OEM_CHG_VUSB_LOW_THRESHOLD <= vusb_uv && vusb_uv <= OEM_CHG_VUSB_HIGH_THRESHOLD) {
				arb_status = true;
			}
		}
	}
	return arb_status;
}
