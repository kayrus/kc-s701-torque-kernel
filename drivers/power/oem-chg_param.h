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
#ifndef __OEM_CHG_PARAM_H
#define __OEM_CHG_PARAM_H

#include <linux/debugfs.h>
#include <mach/msm_smsm.h>

#define CHG_PARAM_SIZE			160
#define SMEM_CHG_PARAM_SHARE	8
#define CHG_CYCLE_SIZE			32

#define SET_CHG_PARAM_INIT(val, type, member)	\
	if (0 == val) {								\
		oem_param_##type.member = 				\
			ptr->oem_chg_param_##type.member;	\
		pr_info("chg_param_%s.%s : %d"			\
			, #type, #member, oem_param_##type.member);\
	}

#define SET_CHG_PARAM(val, type, member)			\
	if (val != ptr->oem_chg_param_##type.member) {	\
		oem_param_##type.member = 					\
			ptr->oem_chg_param_##type.member;		\
		pr_info("chg_param_%s.%s : %d"				\
			, #type, #member, oem_param_##type.member);\
	} else {											\
		pr_info("chg_param_%s.%s use default:%d bad param:%d"	\
			, #type, #member, oem_param_##type.member	\
			, ptr->oem_chg_param_##type.member);		\
	}

#define SET_CHG_PARAM2(val1, val2, type, member)		\
	if ((val1 != ptr->oem_chg_param_##type.member) &&	\
		(val2 != ptr->oem_chg_param_##type.member)) {	\
		oem_param_##type.member = 						\
			ptr->oem_chg_param_##type.member;			\
		pr_info("chg_param_%s.%s : %d"					\
			, #type, #member, oem_param_##type.member);	\
	} else {											\
		pr_info("chg_param_%s.%s use default:%d bad param:%d"	\
			, #type, #member, oem_param_##type.member	\
			, ptr->oem_chg_param_##type.member);		\
	}

#define SET_CHG_PARAM_MINMAX(val, min, max, type, member)	\
	if (val != ptr->oem_chg_param_##type.member) {			\
		if (min > ptr->oem_chg_param_##type.member) {		\
			oem_param_##type.member = min;					\
			pr_err("min > chg_param_%s.%s : %d"				\
				, #type, #member, ptr->oem_chg_param_##type.member);\
		} else if (max < ptr->oem_chg_param_##type.member) {\
			oem_param_##type.member = max;					\
			pr_err("max < chg_param_%s.%s : %d"				\
				, #type, #member, ptr->oem_chg_param_##type.member);\
		} else {											\
			oem_param_##type.member = 						\
				ptr->oem_chg_param_##type.member;			\
		}													\
		pr_info("chg_param_%s.%s : %d"						\
			, #type, #member, oem_param_##type.member);		\
	}

typedef struct {
	uint32_t	cal_vbat1;
	uint32_t	cal_vbat2;
} oem_chg_param_hkadc_type;		/* 8 */

typedef struct {
	uint8_t		critical_batt;	/* offset 8 */
	uint8_t		rbat_data_idx;	/* offset 9 */
	uint16_t	rbat_data[10];	/* offset 10-29 */
	uint16_t	reserve2_4;
	uint16_t	reserve2_5;
	uint16_t	reserve2_6;
	uint16_t	reserve2_7;
	uint16_t	reserve2_8;
	uint16_t	reserve3_1;
	uint16_t	uim_undete_chg;
	uint16_t	i_chg_nouim;
	uint16_t	reserve3_4;
	uint16_t	reserve3_5;
	uint16_t	reserve3_6;
	uint16_t	reserve3_7;
	uint16_t	reserve3_8;
	uint16_t	reserve4_1;
	uint16_t	reserve4_2;
	uint16_t	reserve4_3;
	uint16_t	reserve4_4;
	uint16_t	reserve4_5;
	uint16_t	reserve4_6;
	uint16_t	reserve4_7;
	uint16_t	reserve4_8;
	uint16_t	reserve5_1;
	uint16_t	reserve5_2;
	uint16_t	reserve5_3;
	uint16_t	reserve5_4;
	uint16_t	reserve5_5;
	uint16_t	reserve5_6;
	uint16_t	reserve5_7;
	uint16_t	reserve5_8;
	uint16_t	reserve6_1;
	uint16_t	reserve6_2;
	uint16_t	reserve6_3;
	uint16_t	reserve6_4;
	uint16_t	reserve6_5;
	uint16_t	reserve6_6;
	uint16_t	reserve6_7;
	uint16_t	reserve6_8;
	uint16_t	reserve7_1;
	uint16_t	reserve7_2;
	uint16_t	reserve7_3;
	uint16_t	reserve7_4;
	uint16_t	reserve7_5;
	uint16_t	reserve7_6;
	uint16_t	reserve7_7;
	uint16_t	reserve7_8;
	uint16_t	reserve8_1;
	uint16_t	reserve8_2;
	uint16_t	reserve8_3;
	uint16_t	reserve8_4;
	uint16_t	reserve8_5;
	uint16_t	reserve8_6;
	uint16_t	reserve8_7;
	uint16_t	reserve8_8;
	uint16_t	reserve9_1;
	uint16_t	reserve9_2;
	uint16_t	reserve9_3;
	uint16_t	reserve9_4;
	uint16_t	reserve9_5;
	uint16_t	reserve9_6;
	uint16_t	reserve9_7;
	uint16_t	reserve9_8;
} oem_chg_param_charger_type;	/* 144(152) */

typedef struct {
	uint8_t		factory_mode_1;
	uint8_t		test_mode_chg_disable;
	uint8_t		cool_down_mode_disable;
	uint8_t		reserve1_4;
	uint8_t		reserve1_5;
	uint8_t		reserve1_6;
	uint8_t		reserve1_7;
	uint8_t		reserve1_8;
} oem_chg_param_share_type;		/* 8(160) */

typedef struct {
	uint16_t	last_chargecycles;
	u8			last_charge_increase;
	u8			reserved;
	uint32_t	batt_deteriorationstatus;
	u8			fcc[5];	/* [min_fcc_learning_samples] */
	u8			chgcyl[5];	/* [min_fcc_learning_samples] */
	u8			reserved2[14];
}oem_chg_param_cycles_type;

extern oem_chg_param_hkadc_type		oem_param_hkadc;
extern oem_chg_param_charger_type	oem_param_charger;
extern oem_chg_param_share_type		oem_param_share;
extern oem_chg_param_cycles_type	oem_param_cycles;

void oem_chg_param_init(void);
void oem_param_cycles_backup(uint16_t, u8);
void oem_param_fcc_backup(u8 , u8 , u8);
void oem_param_deterioration_status_backup(int);

#endif
