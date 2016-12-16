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

#ifndef __OEM_CHG_SMEM_H
#define __OEM_CHG_SMEM_H

#define SMEM_CHG_PARAM_SIZE		160

/* 143 is also reserved for use 2 bytes */
#define SMEM_CHG_VBAT_MV		142
#define SMEM_CHG_CDMA_STATUS	145
#define SMEM_CHG_EVDO_STATUS	146
#define SMEM_CHG_LTE_STATUS		147
#define SMEM_CHG_GSM_STATUS		148
#define SMEM_CHG_GPRS_STATUS	149
#define SMEM_CHG_WCDMA_STATUS	150

typedef enum
{
	CDMA_ST_OFF = 0,
	CDMA_ST_TALK,
	CDMA_ST_DATA,
	CDMA_ST_INVALID
} cdma_status;

typedef enum
{
	EVDO_ST_OFF = 0,
	EVDO_ST_ON,
	EVDO_ST_INVALID
} evdo_status;

typedef enum
{
	LTE_ST_OFF = 0,
	LTE_ST_ON,
	LTE_ST_INVALID
} lte_status;

typedef enum
{
	GSM_ST_OFF = 0,
	GSM_ST_ON,
	GSM_ST_INVALID
} gsm_status;

typedef enum
{
	GPRS_ST_OFF = 0,
	GPRS_ST_ON,
	GPRS_ST_INVALID
} gprs_status;

typedef enum
{
	WCDMA_ST_OFF = 0,
	WCDMA_ST_ON,
	WCDMA_ST_INVALID
} wcdma_status;

/* Initialize shared memory access of charge parameters */
extern void oem_chg_smem_init(void);

/* update modem status from shared memory */
extern void oem_chg_update_modem_status(void);

/* get cdma status */
extern cdma_status oem_chg_get_cdma_status(void);

/* get evdo status */
extern evdo_status oem_chg_get_evdo_status(void);

/* get lte status */
extern lte_status oem_chg_get_lte_status(void);

/* get gsm status */
extern gsm_status oem_chg_get_gsm_status(void);

/* get gprs status */
extern gprs_status oem_chg_get_gprs_status(void);

/* get wcdma status */
extern wcdma_status oem_chg_get_wcdma_status(void);

extern void oem_chg_smem_write_vbat_mv(int vbat_mv);
#endif
