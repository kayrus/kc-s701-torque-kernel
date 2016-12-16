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

#ifndef __OEM_CHG_API_H
#define __OEM_CHG_API_H

#include <linux/power_supply.h>
/**
 * oem_chg_get_usb_det_status - return usb detect status
 */
int oem_chg_get_usb_det_status(void);

/**
 * oem_chg_get_pma_det_status - return pma detect status
 */
int oem_chg_get_pma_det_status(void);

/**
 * oem_chg_get_hkadc_monitor_time_ms - return hkadc monitor time (msec)
 */
int oem_chg_get_hkadc_monitor_time_ms(void);
/**
 * oem_chg_set_h_chg_on - set output value of H_CHG_ON
 */
void oem_chg_set_h_chg_on(enum power_supply_type psp);
#endif
