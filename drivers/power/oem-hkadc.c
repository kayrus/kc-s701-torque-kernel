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

#include <linux/module.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/power_supply.h>
#include <oem-hkadc.h>
#include <oem-chg_param.h>
#include <oem-chg_control.h>
#include <linux/android_alarm.h>

extern int oem_option_item1_bit0;

DEFINE_SPINLOCK(oem_hkadc_master_lock);
#define MASTERDATA_SPINLOCK_INIT() spin_lock_init(&oem_hkadc_master_lock);

#define MASTERDATA_LOCK() \
	{ \
	unsigned long oem_hkadc_irq_flag; \
	spin_lock_irqsave(&oem_hkadc_master_lock, oem_hkadc_irq_flag);

#define MASTERDATA_UNLOCK() \
	spin_unlock_irqrestore(&oem_hkadc_master_lock, oem_hkadc_irq_flag); \
	}

#define HKADC_DEFAULT_VOLT  3600000
#define HKADC_DEFAULT_TEMP  250

static void *the_chip;

static struct delayed_work oem_hkadc_work;
static int oem_hkadc_master_data[OEM_POWER_SUPPLY_PROP_IDX_NUM];

static int vbat_work[HKADC_ELEMENT_COUNT] = {0};
static int bat_temp_work[HKADC_ELEMENT_COUNT] = {0};
static int pa_therm_work[HKADC_ELEMENT_COUNT] = {0};
static int camera_therm_work[HKADC_ELEMENT_COUNT] = {0};
static int substrate_therm_work[HKADC_ELEMENT_COUNT] = {0};

static unsigned int index_vbat_work = 0;
static unsigned int index_bat_temp_work = 0;
static unsigned int index_pa_therm_work = 0;
static unsigned int index_camera_therm_work = 0;
static unsigned int index_substrate_therm_work = 0;

static struct alarm oem_hkadc_alarm;
static struct wake_lock oem_hkadc_wake_lock;
static bool req_wakeup_monitor = false;

static int therm_monitor_freq = 0;

static atomic_t is_hkadc_initialized = ATOMIC_INIT(0);

static oem_power_supply_property_index oem_hkadc_get_psp_idx(
	enum power_supply_property psp)
{
	oem_power_supply_property_index idx;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		idx = OEM_POWER_SUPPLY_PROP_IDX_BAT_VOLTAGE;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		idx = OEM_POWER_SUPPLY_PROP_IDX_BAT_THERM;
		break;

	case POWER_SUPPLY_PROP_OEM_PA_THERM:
		idx = OEM_POWER_SUPPLY_PROP_IDX_PA_THERM;
		break;

	case POWER_SUPPLY_PROP_OEM_CAMERA_THERM:
		idx = OEM_POWER_SUPPLY_PROP_IDX_CAMERA_THERM;
		break;

	case POWER_SUPPLY_PROP_OEM_SUBSTRATE_THERM:
		idx = OEM_POWER_SUPPLY_PROP_IDX_SUBSTRATE_THERM;
		break;

	default:
		idx = OEM_POWER_SUPPLY_PROP_IDX_NUM;
		break;
	}
	return idx;
}

static void oem_hkadc_master_data_write(
	enum power_supply_property psp, int val)
{
	int* masterdata;
	oem_power_supply_property_index idx;

	idx = oem_hkadc_get_psp_idx(psp);
	if (OEM_POWER_SUPPLY_PROP_IDX_NUM > idx)
		masterdata = &oem_hkadc_master_data[idx];
	else
		masterdata = NULL;

	if (masterdata != NULL) {
		MASTERDATA_LOCK();
		*masterdata = val;
		MASTERDATA_UNLOCK();
	}
}

static int oem_hkadc_master_data_read(
	enum power_supply_property psp, int *intval)
{
	int ret = 0;
	int initialized;
	int* masterdata;
	oem_power_supply_property_index idx;

	initialized = atomic_read(&is_hkadc_initialized);

	if (!initialized) {
		pr_err("called before init\n");
		MASTERDATA_LOCK();
		*intval = 0;
		MASTERDATA_UNLOCK();
		return -EAGAIN;
	}

	idx = oem_hkadc_get_psp_idx(psp);
	if (OEM_POWER_SUPPLY_PROP_IDX_NUM > idx)
		masterdata = &oem_hkadc_master_data[idx];
	else
		masterdata = NULL;

	if (masterdata != NULL) {
		MASTERDATA_LOCK();
		*intval = *masterdata;
		MASTERDATA_UNLOCK();
	} else {
		pr_debug("Out of range psp %d \n", psp);
		ret = -EINVAL;
	}
	return ret;
}

static int oem_hkadc_get_average(int *hkadc_work, int num)
{
	int max = 0;
	int min = 0;
	int sum = 0;
	int average;
	int cnt;

	max = hkadc_work[0];
	min = hkadc_work[0];

	for (cnt=0; cnt<num; cnt++) {
		if (max < hkadc_work[cnt]) {
			max = hkadc_work[cnt];
		} else if (min > hkadc_work[cnt]) {
			min = hkadc_work[cnt];
		}
		sum += hkadc_work[cnt];
	}
	average = (sum - max - min) / (num - 2);

	pr_debug("average=%d sum=%d max=%d min=%d num=%d\n",
		average, sum, max, min, num);

	return average;
}

static int oem_hkadc_init_value(
			enum power_supply_property psp,
			enum qpnp_vadc_channels channel,
			int *hkadc_work)
{
	int rc;
	int work = 0;
	int cnt;
	struct qpnp_vadc_result result;
	oem_power_supply_property_index idx;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	idx = oem_hkadc_get_psp_idx(psp);
	if (OEM_POWER_SUPPLY_PROP_IDX_NUM <= idx)
	{
		pr_err("invalid index = %d \n", idx);
		return -EINVAL;
	}

	rc = oem_chg_vadc_read(channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					channel, rc);
		work = oem_hkadc_master_data[idx];
	} else {
		work = (int)result.physical;
	}
	for (cnt=0; cnt<HKADC_ELEMENT_COUNT; cnt++) {
		hkadc_work[cnt] = work;
	}

	oem_hkadc_master_data[idx] = work;

	pr_err("init hkadc ch=%d value=%d \n",
		channel, oem_hkadc_master_data[idx]);

	return 0;
}

static int oem_hkadc_get_value(
			enum power_supply_property psp,
			enum qpnp_vadc_channels channel,
			int *hkadc_work, int *index_work, int *value)
{
	int rc;
	int average;
	struct qpnp_vadc_result result;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	rc = oem_chg_vadc_read(channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n", channel, rc);
		rc = oem_hkadc_master_data_read(psp, value);
		pr_debug("hkadc get value do not updated. %d %d\n", rc, *value);
		return rc;
	}
	(*index_work)++;
	if (*index_work == HKADC_ELEMENT_COUNT) {
		*index_work = 0;
	}
	hkadc_work[*index_work] = (int)result.physical;
	average = oem_hkadc_get_average(hkadc_work, HKADC_ELEMENT_COUNT);
	oem_hkadc_master_data_write(psp, average);
	*value = average;

	pr_debug("hkadc ch=%d ave=%d 0:%d 1:%d 2:%d 3:%d 4:%d idx=%d \n",
		channel, average, hkadc_work[0], hkadc_work[1], hkadc_work[2],
		hkadc_work[3], hkadc_work[4], *index_work);

	return 0;
}

static int oem_hkadc_get_wakeup_value(
			enum power_supply_property psp,
			enum qpnp_vadc_channels channel,
			int *hkadc_work, int *index_work, int *value)
{
	int rc;
	int cnt;
	int ng_cnt = 0;
	int value_new = 0;
	int average;
	int hkadc_work_tmp[HKADC_ELEMENT_COUNT];
	struct qpnp_vadc_result result;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	for (cnt=0; cnt<HKADC_ELEMENT_COUNT; cnt++) {
		rc = oem_chg_vadc_read(channel, &result);
		if (rc) {
			pr_err("error reading adc channel = %d, rc = %d\n",
						channel, rc);
			ng_cnt++;
		} else {
			value_new = (int)result.physical;
			hkadc_work_tmp[cnt] = (int)result.physical;
		}
	}
	if (ng_cnt == 0) {
		for (cnt=0; cnt<HKADC_ELEMENT_COUNT; cnt++) {
			hkadc_work[cnt] = hkadc_work_tmp[cnt];
			*index_work = cnt;
		}
		average = oem_hkadc_get_average(hkadc_work, HKADC_ELEMENT_COUNT);
		oem_hkadc_master_data_write(psp, average);
		*value = average;

		pr_debug("hkadc ch=%d ave=%d 0:%d 1:%d 2:%d 3:%d 4:%d \n",
			channel, average, hkadc_work[0], hkadc_work[1], hkadc_work[2],
			hkadc_work[3], hkadc_work[4]);
	} else if (ng_cnt < HKADC_ELEMENT_COUNT) {
		for (cnt=0; cnt<HKADC_ELEMENT_COUNT; cnt++) {
			hkadc_work[cnt] = value_new;
		}
		oem_hkadc_master_data_write(psp, value_new);
		*value = value_new;
		pr_debug("hkadc wakeup value value_new=%d\n", value_new);
	} else {
		rc = oem_hkadc_master_data_read(psp, value);
		pr_debug("hkadc wakeup value do not updated. %d %d\n", rc, *value);
	}
	return 0;
}

#define DEFAULT_VOLTAGE	(3800*1000)
int oem_hkadc_pm_batt_power_get_property(enum power_supply_property psp, int *intval)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (oem_option_item1_bit0) {
			*intval = DEFAULT_VOLTAGE;
			ret = 0;
		} else {
			ret = oem_hkadc_master_data_read(psp, intval);
		}
		break;
	case POWER_SUPPLY_PROP_OEM_PA_THERM:
	case POWER_SUPPLY_PROP_OEM_CAMERA_THERM:
	case POWER_SUPPLY_PROP_OEM_SUBSTRATE_THERM:
		if (oem_param_share.cool_down_mode_disable) {
			*intval = HKADC_DEFAULT_TEMP;
		} else {
			ret = oem_hkadc_master_data_read(psp, intval);
		}
		break;

	case POWER_SUPPLY_PROP_TEMP:
		ret = oem_hkadc_master_data_read(psp, intval);
		break;

	default:
		ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(oem_hkadc_pm_batt_power_get_property);

static void oem_hkadc_monitor(struct work_struct *work)
{
	struct timespec ts;
	static struct timespec last_ts;
	int vbat  = 0;
	int temp  = 0;
	int pa_therm  = 0;
	int camera_therm  = 0;
	int substrate_therm = 0;
	int rc = 0;
	int req_ps_changed_batt = false;

	alarm_cancel(&oem_hkadc_alarm);
	ts = ktime_to_timespec(alarm_get_elapsed_realtime());
	ts.tv_sec += HKADC_SUSPEND_MONITOR_FREQ;
	ts.tv_nsec = 0;
	alarm_start_range(&oem_hkadc_alarm,
		timespec_to_ktime(ts),
		timespec_to_ktime(ts));
	if (req_wakeup_monitor) {
		oem_hkadc_get_wakeup_value(POWER_SUPPLY_PROP_VOLTAGE_NOW, VBAT_SNS,
			vbat_work, &index_vbat_work, &vbat);
		oem_hkadc_get_wakeup_value(POWER_SUPPLY_PROP_TEMP, LR_MUX1_BATT_THERM,
			bat_temp_work, &index_bat_temp_work, &temp);
		oem_hkadc_get_wakeup_value(POWER_SUPPLY_PROP_OEM_PA_THERM, P_MUX5_1_1,
			pa_therm_work, &index_pa_therm_work, &pa_therm);
		oem_hkadc_get_wakeup_value(POWER_SUPPLY_PROP_OEM_CAMERA_THERM, P_MUX8_1_1,
			camera_therm_work, &index_camera_therm_work, &camera_therm);
		oem_hkadc_get_wakeup_value(POWER_SUPPLY_PROP_OEM_SUBSTRATE_THERM, P_MUX7_1_1,
			substrate_therm_work, &index_substrate_therm_work, &substrate_therm);

		therm_monitor_freq = 0;
		req_wakeup_monitor = false;
		req_ps_changed_batt |= true;
		pr_info("wakeup tv_sec=%d diff:%d vbat:%d temp:%d pa:%d cam:%d sub:%d\n",
			(int)ts.tv_sec, (int)(ts.tv_sec - last_ts.tv_sec),
			vbat, temp, pa_therm, camera_therm, substrate_therm);
		last_ts = ts;
	} else {
		rc = oem_hkadc_get_value(POWER_SUPPLY_PROP_VOLTAGE_NOW, VBAT_SNS,
			vbat_work, &index_vbat_work, &vbat);
		rc |= oem_hkadc_get_value(POWER_SUPPLY_PROP_TEMP, LR_MUX1_BATT_THERM,
			bat_temp_work, &index_bat_temp_work, &temp);

		therm_monitor_freq++;

		if (therm_monitor_freq >= HKADC_THERM_MONITOR_FREQ) {
			therm_monitor_freq = 0;
			oem_hkadc_get_value(POWER_SUPPLY_PROP_OEM_PA_THERM, P_MUX5_1_1,
				pa_therm_work, &index_pa_therm_work, &pa_therm);
			oem_hkadc_get_value(POWER_SUPPLY_PROP_OEM_CAMERA_THERM, P_MUX8_1_1,
				camera_therm_work, &index_camera_therm_work, &camera_therm);
			oem_hkadc_get_value(POWER_SUPPLY_PROP_OEM_SUBSTRATE_THERM, P_MUX7_1_1,
				substrate_therm_work, &index_substrate_therm_work, &substrate_therm);
			req_ps_changed_batt |= true;
		}
	}

	req_ps_changed_batt |= oem_chg_control();

	if (req_ps_changed_batt) {
		oem_chg_ps_changed_batt();
	}

	oem_chg_uim_status_check();

	oem_bms_reset_cc();

	schedule_delayed_work(&oem_hkadc_work,
			round_jiffies_relative(msecs_to_jiffies(OEM_HKADC_MONITOR_TIME_MS)));
	if (wake_lock_active(&oem_hkadc_wake_lock)) {
		wake_unlock(&oem_hkadc_wake_lock);
	}
}

int oem_chg_get_hkadc_monitor_time_ms(void)
{
	return OEM_HKADC_MONITOR_TIME_MS;
}
EXPORT_SYMBOL(oem_chg_get_hkadc_monitor_time_ms);

static void oem_hkadc_alarm_handler(struct alarm *alarm)
{
	return;
}

void oem_hkadc_set_wakeup_monitor(void)
{
	wake_lock(&oem_hkadc_wake_lock);
	req_wakeup_monitor = true;
}
EXPORT_SYMBOL(oem_hkadc_set_wakeup_monitor);

int oem_hkadc_get_vbat_latest(void)
{
	return vbat_work[index_vbat_work];
}
EXPORT_SYMBOL(oem_hkadc_get_vbat_latest);

void oem_hkadc_init(void *chip)
{
	the_chip = chip;

	memset(&oem_hkadc_master_data, 0x0, sizeof(oem_hkadc_master_data));

	oem_hkadc_master_data[0] = HKADC_DEFAULT_VOLT;  /* POWER_SUPPLY_PROP_VOLTAGE_NOW */
	oem_hkadc_master_data[1] = HKADC_DEFAULT_TEMP;  /* POWER_SUPPLY_PROP_TEMP */
	oem_hkadc_master_data[2] = HKADC_DEFAULT_TEMP;  /* POWER_SUPPLY_PROP_OEM_PA_THERM */
	oem_hkadc_master_data[3] = HKADC_DEFAULT_TEMP;  /* POWER_SUPPLY_PROP_OEM_CAMERA_THERM */
	oem_hkadc_master_data[4] = HKADC_DEFAULT_TEMP;  /* POWER_SUPPLY_PROP_OEM_SUBSTRATE_THERM */

	oem_hkadc_init_value(POWER_SUPPLY_PROP_VOLTAGE_NOW, VBAT_SNS,
		vbat_work);
	oem_hkadc_init_value(POWER_SUPPLY_PROP_TEMP, LR_MUX1_BATT_THERM,
		bat_temp_work);
	oem_hkadc_init_value(POWER_SUPPLY_PROP_OEM_PA_THERM, P_MUX5_1_1,
		pa_therm_work);
	oem_hkadc_init_value(POWER_SUPPLY_PROP_OEM_CAMERA_THERM, P_MUX8_1_1,
		camera_therm_work);
	oem_hkadc_init_value(POWER_SUPPLY_PROP_OEM_SUBSTRATE_THERM, P_MUX7_1_1,
		substrate_therm_work);

	atomic_set(&is_hkadc_initialized, 1);

	MASTERDATA_SPINLOCK_INIT();
	wake_lock_init(&oem_hkadc_wake_lock, WAKE_LOCK_SUSPEND, "oem_hkadc");

	alarm_init(&oem_hkadc_alarm,
		ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
		oem_hkadc_alarm_handler);

	INIT_DELAYED_WORK(&oem_hkadc_work, oem_hkadc_monitor);
	schedule_delayed_work(&oem_hkadc_work,
			round_jiffies_relative(msecs_to_jiffies(0)));
}
EXPORT_SYMBOL(oem_hkadc_init);

void oem_hkadc_exit(void)
{
	int rc;
	rc = alarm_cancel(&oem_hkadc_alarm);
	pr_debug("alarm_cancel result=%d\n", rc);
	atomic_set(&is_hkadc_initialized, 0);
}
EXPORT_SYMBOL(oem_hkadc_exit);
