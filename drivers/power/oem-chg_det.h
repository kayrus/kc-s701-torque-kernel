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

#ifndef __OEM_CHG_DET_H
#define __OEM_CHG_DET_H

/**
 * oem_chg_det_init - charger detect initialize
 */
void oem_chg_det_init(void *chip);

/**
 * oem_chg_pm_batt_power_get_property - get oem charger property
 */
int oem_chg_pm_batt_power_get_property(enum power_supply_property psp, int *intval);
extern void oem_chg_set_usb_path_en(int usb_path_en_f);

void oem_chg_usb_usbin_valid_handler(int);
void oem_chg_h_chg_on_ctrl(int value);
#endif
