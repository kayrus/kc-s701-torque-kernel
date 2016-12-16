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
#ifndef OEM_HKADC_H
#define OEM_HKADC_H

#define OEM_HKADC_MONITOR_TIME_MS		3000 //1000

#define HKADC_ELEMENT_COUNT				5

#define HKADC_SUSPEND_MONITOR_FREQ		(60 * 10)

#define HKADC_THERM_MONITOR_FREQ		2 //5

typedef enum{
	OEM_POWER_SUPPLY_PROP_IDX_BAT_VOLTAGE=0,
	OEM_POWER_SUPPLY_PROP_IDX_PA_THERM,
	OEM_POWER_SUPPLY_PROP_IDX_BAT_THERM,
	OEM_POWER_SUPPLY_PROP_IDX_CAMERA_THERM,
	OEM_POWER_SUPPLY_PROP_IDX_SUBSTRATE_THERM,
	OEM_POWER_SUPPLY_PROP_IDX_NUM
}oem_power_supply_property_index;

/**
 * oem_hkadc_init - oem hkadc initialize
 *
 */
void oem_hkadc_init(void *chip);

/**
 * oem_hkadc_exit - oem hkadc exit
 *
 */
void oem_hkadc_exit(void);

/**
 * oem_hkadc_pm_batt_power_get_property - get oem_hkadc property
 *
 */
int oem_hkadc_pm_batt_power_get_property(enum power_supply_property psp, int *intval);

/**
 * oem_charger_pm_batt_power_get_property - get oem_charger property
 *
 */
int oem_charger_pm_batt_power_get_property(enum power_supply_property psp, int *intval);

void oem_hkadc_set_wakeup_monitor(void);

extern int oem_hkadc_get_vbat_latest(void);

extern void oem_bms_reset_cc(void);

#endif
