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
#include <mach/msm_smsm.h>
#include <oem-chg_smem.h>

#undef CHG_SMEM_DEBUG
#ifdef CHG_SMEM_DEBUG
	#define CHG_SMEM_LOG pr_info
#else
	#define CHG_SMEM_LOG pr_debug
#endif

static uint8_t *smem_ptr = NULL;

static cdma_status current_cdma_status = CDMA_ST_INVALID;
static evdo_status current_evdo_status = EVDO_ST_INVALID;
static lte_status current_lte_status = LTE_ST_INVALID;
static gsm_status current_gsm_status = GSM_ST_INVALID;
static gprs_status current_gprs_status = GPRS_ST_INVALID;
static wcdma_status current_wcdma_status = WCDMA_ST_INVALID;

/* get cdma status */
cdma_status oem_chg_get_cdma_status(void)
{
	return current_cdma_status;
}
EXPORT_SYMBOL(oem_chg_get_cdma_status);

/* get evdo status */
evdo_status oem_chg_get_evdo_status(void)
{
	return current_evdo_status;
}
EXPORT_SYMBOL(oem_chg_get_evdo_status);

/* get lte status */
lte_status oem_chg_get_lte_status(void)
{
	return current_lte_status;
}
EXPORT_SYMBOL(oem_chg_get_lte_status);

/* get gsm status */
gsm_status oem_chg_get_gsm_status(void)
{
	return current_gsm_status;
}
EXPORT_SYMBOL(oem_chg_get_gsm_status);

/* get gprs status */
gprs_status oem_chg_get_gprs_status(void)
{
	return current_gprs_status;
}
EXPORT_SYMBOL(oem_chg_get_gprs_status);

/* get wcdma status */
wcdma_status oem_chg_get_wcdma_status(void)
{
	return current_wcdma_status;
}
EXPORT_SYMBOL(oem_chg_get_wcdma_status);

void oem_chg_smem_write_vbat_mv(int vbat_mv)
{
	uint16_t *pVbat_smem;

	smem_ptr = (uint8_t *)kc_smem_alloc(SMEM_CHG_PARAM, SMEM_CHG_PARAM_SIZE);
	pVbat_smem = (uint16_t *)(smem_ptr + SMEM_CHG_VBAT_MV);
	*pVbat_smem = vbat_mv;
}

void oem_chg_update_modem_status(void)
{
	if (smem_ptr == NULL) {
		pr_err("smem_ptr is NULL\n");
		return;
	}
	current_cdma_status = smem_ptr[SMEM_CHG_CDMA_STATUS];
	current_evdo_status = smem_ptr[SMEM_CHG_EVDO_STATUS];
	current_lte_status = smem_ptr[SMEM_CHG_LTE_STATUS];
	current_gsm_status = smem_ptr[SMEM_CHG_GSM_STATUS];
	current_gprs_status = smem_ptr[SMEM_CHG_GPRS_STATUS];
	current_wcdma_status = smem_ptr[SMEM_CHG_WCDMA_STATUS];

	CHG_SMEM_LOG("cdma:%d evdo:%d lte:%d gsm:%d gprs:%d wcdma:%d\n",
		current_cdma_status, current_evdo_status, current_lte_status,
		current_gsm_status, current_gprs_status, current_wcdma_status);
}
EXPORT_SYMBOL(oem_chg_update_modem_status);

/* Initialize shared memory access of charge parameters */
void oem_chg_smem_init(void)
{
	smem_ptr = (uint8_t *)kc_smem_alloc(SMEM_CHG_PARAM, SMEM_CHG_PARAM_SIZE);
	if (smem_ptr == NULL) {
		pr_err("smem_ptr is NULL\n");
		return;
	}
	smem_ptr[SMEM_CHG_CDMA_STATUS] = CDMA_ST_INVALID;
	smem_ptr[SMEM_CHG_EVDO_STATUS] = EVDO_ST_INVALID;
	smem_ptr[SMEM_CHG_LTE_STATUS] = LTE_ST_INVALID;
	smem_ptr[SMEM_CHG_GSM_STATUS] = GSM_ST_INVALID;
	smem_ptr[SMEM_CHG_GPRS_STATUS] = GPRS_ST_INVALID;
	smem_ptr[SMEM_CHG_WCDMA_STATUS] = WCDMA_ST_INVALID;
	oem_chg_update_modem_status();
	pr_info("initialization is completed!!\n");
}
EXPORT_SYMBOL(oem_chg_smem_init);
