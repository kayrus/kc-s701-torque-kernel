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

#ifndef __OEM_BMS_H
#define __OEM_BMS_H

enum batt_deterioration_type {
	BATT_DETERIORATION_GOOD,
	BATT_DETERIORATION_NORMAL,
	BATT_DETERIORATION_DEAD,
};

/**
 * oem_bms_correct_soc - readjustment oem soc
 */
int oem_bms_correct_soc(int in_soc);
int oem_bms_get_deteriorationstatus(int *, int, int);
extern void oem_bms_uim_status_notify(void);

enum oem_after_warmcool_recharge_flag_type {
	OEM_AFTER_WARMCOOL_INIT = 0,
	OEM_AFTER_WARMCOOL_CHARGE_DONE_ON_NOT_NORMAL,
	OEM_AFTER_WARMCOOL_RECHARGING,
	OEM_AFTER_WARMCOOL_CHARGE_DONE,
};

/**
 * oem_get_status_oem_after_warmcool_recharge_flag - get status of oem_after_warmcool_recharge_flag
 */
int oem_get_status_oem_after_warmcool_recharge_flag(void);
void oem_clear_oem_after_warmcool_recharge_flag(void);
void oem_recharge_request(void);

#endif
