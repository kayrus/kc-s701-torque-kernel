/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C)2013 KYOCERA Corporation
 * (C)2014 KYOCERA Corporation
 */


#include "msm_sensor.h"
#include "msm_cci.h"
#include "msm.h"
#undef FEATURE_KYOCERA_MCAM_BB_ADJ
#ifdef FEATURE_KYOCERA_MCAM_BB_ADJ
#include <linux/vmalloc.h> 
#endif /* FEATURE_KYOCERA_MCAM_BB_ADJ */

#undef CDBG
#define TEST_CAMERA_DEBUG_LOG
#undef TEST_CAMERA_DEBUG_LOG
#ifdef TEST_CAMERA_DEBUG_LOG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#define T4K28_SENSOR_NAME "t4k28"
DEFINE_MSM_MUTEX(t4k28_mut);

static struct msm_sensor_ctrl_t t4k28_s_ctrl;

static struct msm_sensor_power_setting t4k28_power_setting[] = {
    /* AVDD(L19) ON -> 1ms wait */
    {
        .seq_type = SENSOR_VREG,
        .seq_val = CAM_VANA,
        .config_val = 0,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_CLK,
        .seq_val = SENSOR_CAM_MCLK,
        .config_val = 24000000,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_GPIO,
        .seq_val = SENSOR_GPIO_RESET,
        .config_val = GPIO_OUT_HIGH,
        .delay = 5,
    },
};

static struct msm_camera_i2c_reg_conf t4k28_keep_exposure_value[] = {
    {0x355F, 0x00},
    {0x3560, 0x00},
    {0x3561, 0x00},
    {0x3562, 0x00},
    {0x3563, 0x00},
    {0x3564, 0x00},
};

static struct msm_camera_i2c_reg_conf t4k28_write_exposure_value[] = {
    {0x3506, 0x00},
    {0x3507, 0x00},
    {0x350A, 0x00},
    {0x350B, 0x00},
    {0x350C, 0x00},
};

static struct msm_camera_i2c_reg_conf t4k28_write_exposure_value_2[] = {
    {0x3017, 0x00},
    {0x3018, 0x00},
};

static bool wb_read = false;
static enum msm_sensor_resolution_t res_mode = MSM_SENSOR_INVALID_RES;


static struct msm_camera_i2c_reg_conf t4k28_stop_settings[] = {
    {0x3040, 0x81},
};


static struct msm_camera_i2c_reg_conf t4k28_start_settings[] = {
    {0x3010, 0x01},
    {0x3040, 0x80},
};

static struct msm_camera_i2c_reg_conf t4k28_preview_settings[] = {
    {0x3012, 0x03},
    {0x3015, 0x05},
    {0x3016, 0x16},
    {0x3017, 0x01},
    {0x3018, 0x80},
    {0x3019, 0x00},
    {0x301A, 0x20},
    {0x301B, 0x10},
    {0x301C, 0x01},
    {0x3020, 0x03},
    {0x3021, 0x20},
    {0x3022, 0x02},
    {0x3023, 0x58},
    {0x334D, 0x50},
    {0x334E, 0x01},
    {0x334F, 0x40},
    {0x3047, 0x64}, //PLL_MULTI[7:0]
    {0x3048, 0x01}, //-/-/-/-/VT_SYS_CNTL[3:0]
    {0x3049, 0x01}, //-/-/-/-/OP_SYS_CNTL[3:0]
    {0x304A, 0x0A},
    {0x304B, 0x0A},
    {0x3089, 0x02},
    {0x308A, 0x58},
    {0x351C, 0x6C},
    {0x351D, 0x78},
    {0x3012, 0x02}, //-/-/-/-/-/-/VLAT_ON/GROUP_HOLD
};

static struct msm_camera_i2c_reg_conf t4k28_720p_settings[] = {
    {0x3012, 0x03}, //-/-/-/-/-/-/VLAT_ON/GROUP_HOLD
    {0x3015, 0x05}, //-/-/-/H_COUNT[12:8]
    {0x3016, 0x22}, //H_COUNT[7:0]
    {0x3017, 0x02}, //-/-/-/V_COUNT[12:8]
    {0x3018, 0x6F}, //V_COUNT[7:0]
    {0x3019, 0x00}, //-/-/-/-/-/-/-/SCALE_M[8]
    {0x301A, 0x10}, //SCALE_M[7:0]
    {0x301B, 0x00}, //-/-/-/V_ANABIN/-/-/-/-
    {0x301C, 0x01}, //-/-/-/-/-/-/-/SCALING_MODE
    {0x3020, 0x05}, //-/-/-/-/-/HOUTPIX[10:8]
    {0x3021, 0x00}, //HOUTPIX[7:0]
    {0x3022, 0x02}, //-/-/-/-/-/VOUTPIX[10:8]
    {0x3023, 0xD0}, //VOUTPIX[7:0]
    {0x334D, 0x50},
    {0x334E, 0x01},
    {0x334F, 0x40},
    {0x3047, 0x82}, //PLL_MULTI[7:0]
    {0x3048, 0x01}, //-/-/-/-/VT_SYS_CNTL[3:0]
    {0x3049, 0x01}, //-/-/-/-/OP_SYS_CNTL[3:0]
    {0x304A, 0x08}, //-/-/-/-/VT_PIX_CNTL[3:0]
    {0x304B, 0x0A}, //-/-/-/-/ST_CLK_CNTL[3:0]
    {0x3089, 0x03}, //MSB_LBRATE[15:8]
    {0x308A, 0x20}, //MSB_LBRATE[7:0]
    {0x351C, 0x6F}, //FLLONGON/FRMSPD[1:0]/FL600S[12:8]
    {0x351D, 0x75}, //FL600S[7:0]
    {0x3012, 0x02}, //-/-/-/-/-/-/VLAT_ON/GROUP_HOLD
};

static struct msm_camera_i2c_reg_conf t4k28_fullsize_settings[] = {
    {0x3012, 0x03},
    {0x3015, 0x05},
    {0x3016, 0x16},
    {0x3017, 0x03},
    {0x3018, 0x00},
    {0x3019, 0x00},
    {0x301A, 0x10},
    {0x301B, 0x00},
    {0x301C, 0x01},
    {0x3020, 0x06},
    {0x3021, 0x40},
    {0x3022, 0x04},
    {0x3023, 0xB0},
    {0x334D, 0x50},
    {0x334E, 0x00},
    {0x334F, 0xA0},
    {0x3047, 0x64}, //PLL_MULTI[7:0]
    {0x3048, 0x01}, //-/-/-/-/VT_SYS_CNTL[3:0]
    {0x3049, 0x01}, //-/-/-/-/OP_SYS_CNTL[3:0]
    {0x304A, 0x0A},
    {0x304B, 0x0A},
    {0x3089, 0x02},
    {0x308A, 0x58},
    {0x351C, 0x6C},
    {0x351D, 0x78},
    {0x3012, 0x02}, //-/-/-/-/-/-/VLAT_ON/GROUP_HOLD
};

static struct msm_camera_i2c_reg_conf t4k28_recommend_settings[] = {
    {0x3010, 0x00},
    {0x3000, 0x08},
    {0x3001, 0x40},
    {0x3002, 0x00},
    {0x3003, 0x00},
    {0x3004, 0x00},
    {0x3005, 0x66},
    {0x3011, 0x00},
    {0x3012, 0x02},
    {0x3014, 0x00},
    {0x3015, 0x05},
    {0x3016, 0x16},
    {0x3017, 0x03},
    {0x3018, 0x00},
    {0x3019, 0x00},
    {0x301A, 0x10},
    {0x301B, 0x00},
    {0x301C, 0x01},
    {0x3020, 0x06},
    {0x3021, 0x40},
    {0x3022, 0x04},
    {0x3023, 0xB0},
    {0x3025, 0x00},
    {0x3026, 0x00},
    {0x3027, 0x00},
    {0x302C, 0x00},
    {0x302D, 0x00},
    {0x302E, 0x00},
    {0x302F, 0x00},
    {0x3030, 0x00},
    {0x3031, 0x02},
    {0x3032, 0x00},
    {0x3033, 0x83},
    {0x3034, 0x01},
    {0x3037, 0x00},
    {0x303C, 0x80},
    {0x303E, 0x01},
    {0x303F, 0x00},
    {0x3040, 0x80},
    {0x3044, 0x02},
    {0x3045, 0x04},
    {0x3046, 0x00},
    {0x3047, 0x64},
    {0x3048, 0x01},
    {0x3049, 0x01},
    {0x304A, 0x0A},
    {0x304B, 0x0A},
    {0x304C, 0x00},
    {0x304E, 0x01},
    {0x3050, 0x60},
    {0x3051, 0x82},
    {0x3052, 0x10},
    {0x3053, 0x00},
    {0x3055, 0x84},
    {0x3056, 0x02},
    {0x3059, 0x18},
    {0x305A, 0x00},
    {0x3068, 0xF0},
    {0x3069, 0xF0},
    {0x306C, 0x06},
    {0x306D, 0x40},
    {0x306E, 0x00},
    {0x306F, 0x04},
    {0x3070, 0x06},
    {0x3071, 0x43},
    {0x3072, 0x04},
    {0x3073, 0xB0},
    {0x3074, 0x00},
    {0x3075, 0x04},
    {0x3076, 0x04},
    {0x3077, 0xB3},
    {0x307F, 0x03},
    {0x3080, 0x70},
    {0x3081, 0x28},
    {0x3082, 0x60},
    {0x3083, 0x48},
    {0x3084, 0x40},
    {0x3085, 0x28},
    {0x3086, 0xF8},
    {0x3087, 0x38},
    {0x3088, 0x03},
    {0x3089, 0x02},
    {0x308A, 0x58},
    {0x3091, 0x00},
    {0x3092, 0x18},
    {0x3093, 0xA1},
    {0x3095, 0x78},
    {0x3097, 0x00},
    {0x3098, 0x40},
    {0x309A, 0x00},
    {0x309B, 0x00},
    {0x309D, 0x00},
    {0x309E, 0x00},
    {0x309F, 0x00},
    {0x30A0, 0x00},
    {0x30A1, 0x00},
    {0x30A2, 0xA7},
    {0x30A3, 0x00},
    {0x30A4, 0xFF},
    {0x30A5, 0x80},
    {0x30A6, 0xFF},
    {0x30A7, 0x00},
    {0x30A8, 0x01},
    {0x30F1, 0x00},
    {0x30F2, 0x00},
    {0x30FE, 0x80},
    {0x3100, 0xD2},
    {0x3101, 0xD3},
    {0x3102, 0x95},
    {0x3103, 0x80},
    {0x3104, 0x31},
    {0x3105, 0x04},
    {0x3106, 0x23},
    {0x3107, 0x20},
    {0x3108, 0x7B},
    {0x3109, 0x80},
    {0x310A, 0x00},
    {0x310B, 0x00},
    {0x3110, 0x11},
    {0x3111, 0x11},
    {0x3112, 0x00},
    {0x3113, 0x00},
    {0x3114, 0x10},
    {0x3115, 0x22},
    {0x3120, 0x08},
    {0x3121, 0x13},
    {0x3122, 0x33},
    {0x3123, 0x0E},
    {0x3124, 0x26},
    {0x3125, 0x00},
    {0x3126, 0x0C},
    {0x3127, 0x08},
    {0x3128, 0x80},
    {0x3129, 0x65},
    {0x312A, 0x27},
    {0x312B, 0x77},
    {0x312C, 0x77},
    {0x312D, 0x1A},
    {0x312E, 0xB8},
    {0x312F, 0x38},
    {0x3130, 0x80},
    {0x3131, 0x33},
    {0x3132, 0x63},
    {0x3133, 0x00},
    {0x3134, 0xDD},
    {0x3135, 0x07},
    {0x3136, 0xB7},
    {0x3137, 0x11},
    {0x3138, 0x0B},
    {0x313B, 0x0A},
    {0x313C, 0x05},
    {0x313D, 0x01},
    {0x313E, 0x62},
    {0x313F, 0x85},
    {0x3140, 0x01},
    {0x3141, 0x40},
    {0x3142, 0x80},
    {0x3143, 0x22},
    {0x3144, 0x3E},
    {0x3145, 0x32},
    {0x3146, 0x2E},
    {0x3147, 0x23},
    {0x3148, 0x22},
    {0x3149, 0x11},
    {0x314A, 0x6B},
    {0x314B, 0x30},
    {0x314C, 0x69},
    {0x314D, 0x80},
    {0x314E, 0x31},
    {0x314F, 0x32},
    {0x3150, 0x32},
    {0x3151, 0x03},
    {0x3152, 0x0C},
    {0x3153, 0xB3},
    {0x3154, 0x20},
    {0x3155, 0x13},
    {0x3156, 0x66},
    {0x3157, 0x02},
    {0x3158, 0x03},
    {0x3159, 0x01},
    {0x315A, 0x16},
    {0x315B, 0x10},
    {0x315C, 0x00},
    {0x315D, 0x44},
    {0x315E, 0x1B},
    {0x315F, 0x52},
    {0x3160, 0x00},
    {0x3161, 0x03},
    {0x3162, 0x00},
    {0x3163, 0xFF},
    {0x3164, 0x00},
    {0x3165, 0x01},
    {0x3166, 0x00},
    {0x3167, 0xFF},
    {0x3168, 0x01},
    {0x3169, 0x00},
    {0x3180, 0x00},
    {0x3181, 0x20},
    {0x3182, 0x40},
    {0x3183, 0x96},
    {0x3184, 0x30},
    {0x3185, 0x8F},
    {0x3186, 0x31},
    {0x3187, 0x06},
    {0x3188, 0x0C},
    {0x3189, 0x44},
    {0x318A, 0x42},
    {0x318B, 0x0B},
    {0x318C, 0x11},
    {0x318D, 0xAA},
    {0x318E, 0x40},
    {0x318F, 0x30},
    {0x3190, 0x03},
    {0x3191, 0x01},
    {0x3192, 0x00},
    {0x3193, 0x00},
    {0x3194, 0x00},
    {0x3195, 0x00},
    {0x3196, 0x00},
    {0x3197, 0xDE},
    {0x3198, 0x00},
    {0x3199, 0x00},
    {0x319A, 0x00},
    {0x319B, 0x00},
    {0x319C, 0x16},
    {0x319D, 0x0A},
    {0x31A0, 0xBF},
    {0x31A1, 0xFF},
    {0x31A2, 0x11},
    {0x31B0, 0x00},
    {0x31B1, 0x41},
    {0x31B2, 0x1A},
    {0x31B3, 0x61},
    {0x31B4, 0x03},
    {0x31B5, 0x2D},
    {0x31B6, 0x1A},
    {0x31B7, 0x83},
    {0x31B8, 0x00},
    {0x31B9, 0x03},
    {0x31BA, 0x3F},
    {0x31BB, 0xFF},
    {0x3300, 0xFF},
    {0x3301, 0x35},
    {0x3303, 0x40},
    {0x3304, 0x00},
    {0x3305, 0x00},
    {0x3306, 0x30},
    {0x3307, 0x00},
    {0x3308, 0x87},
    {0x330A, 0x60},
    {0x330B, 0x56},
    {0x330D, 0x79},
    {0x330E, 0xFF},
    {0x330F, 0xFF},
    {0x3310, 0xFF},
    {0x3311, 0x7F},
    {0x3312, 0x0F},
    {0x3313, 0x0F},
    {0x3314, 0x02},
    {0x3315, 0xC0},
    {0x3316, 0x18},
    {0x3317, 0x08},
    {0x3318, 0x60},
    {0x3319, 0x90},
    {0x331B, 0x00},
    {0x331C, 0x00},
    {0x331D, 0x00},
    {0x331E, 0x00},
    {0x3322, 0x23},
    {0x3323, 0x23},
    {0x3324, 0x0E},
    {0x3325, 0x32},
    {0x3327, 0x00},
    {0x3328, 0x00},
    {0x3329, 0x80},
    {0x332A, 0x80},
    {0x332B, 0x80},
    {0x332C, 0x80},
    {0x332D, 0x80},
    {0x332E, 0x80},
    {0x332F, 0x08},
    {0x3330, 0x18},
    {0x3331, 0x10},
    {0x3332, 0x0C},
    {0x3333, 0x18},
    {0x3334, 0x18},
    {0x3335, 0x08},
    {0x3336, 0x1C},
    {0x3337, 0x10},
    {0x3338, 0x14},
    {0x3339, 0x1C},
    {0x333A, 0x18},
    {0x333B, 0x44},
    {0x333C, 0x4C},
    {0x333D, 0x30},
    {0x333E, 0x44},
    {0x333F, 0x4C},
    {0x3340, 0x30},
    {0x3341, 0x2F},
    {0x3342, 0x38},
    {0x3343, 0x20},
    {0x3344, 0x34},
    {0x3345, 0x38},
    {0x3346, 0x20},
    {0x3347, 0x00},
    {0x3348, 0x00},
    {0x3349, 0x00},
    {0x334A, 0x00},
    {0x334B, 0x00},
    {0x334C, 0x00},
    {0x334D, 0x50},
    {0x334E, 0x00},
    {0x334F, 0xA0},
    {0x3350, 0x03},
    {0x335F, 0x00},
    {0x3360, 0x00},
    {0x3400, 0xA4},
    {0x3401, 0x7F},
    {0x3402, 0x00},
    {0x3403, 0x00},
    {0x3404, 0x3A},
    {0x3405, 0xE3},
    {0x3406, 0x22},
    {0x3407, 0x25},
    {0x3408, 0x17},
    {0x3409, 0x5C},
    {0x340A, 0x20},
    {0x340B, 0x20},
    {0x340C, 0x3B},
    {0x340D, 0x2E},
    {0x340E, 0x26},
    {0x340F, 0x3F},
    {0x3410, 0x34},
    {0x3411, 0x2D},
    {0x3412, 0x28},
    {0x3413, 0x47},
    {0x3414, 0x3E},
    {0x3415, 0x6A},
    {0x3416, 0x5A},
    {0x3417, 0x50},
    {0x3418, 0x48},
    {0x3419, 0x42},
    {0x341B, 0x30},
    {0x341C, 0x40},
    {0x341D, 0x70},
    {0x341E, 0xA0},
    {0x341F, 0x88},
    {0x3420, 0x70},
    {0x3421, 0xA0},
    {0x3422, 0x18},
    {0x3423, 0x0F},
    {0x3424, 0x0F},
    {0x3425, 0x0F},
    {0x3426, 0x0F},
    {0x342B, 0x10},
    {0x342C, 0x10},
    {0x342D, 0x30},
    {0x342E, 0x90},
    {0x342F, 0x30},
    {0x3430, 0x90},
    {0x3431, 0x20},
    {0x3432, 0x20},
    {0x3433, 0x00},
    {0x3434, 0x00},
    {0x3435, 0x00},
    {0x3436, 0x00},
    {0x343F, 0xE0},
    {0x3440, 0xE0},
    {0x3441, 0x90},
    {0x3442, 0x00},
    {0x3443, 0x00},
    {0x3444, 0x00},
    {0x3446, 0x00},
    {0x3447, 0x00},
    {0x3448, 0x00},
    {0x3449, 0x00},
    {0x344A, 0x00},
    {0x344B, 0x00},
    {0x344C, 0x20},
    {0x344D, 0xFF},
    {0x344E, 0x0F},
    {0x344F, 0x20},
    {0x3450, 0x80},
    {0x3451, 0x0F},
    {0x3452, 0x55},
    {0x3453, 0x49},
    {0x3454, 0x6A},
    {0x3455, 0x93},
    {0x345C, 0x00},
    {0x345D, 0x00},
    {0x345E, 0x00},
    {0x3500, 0xC1},
    {0x3501, 0x01},
    {0x3502, 0x30},
    {0x3503, 0x1A},
    {0x3504, 0x00},
    {0x3505, 0xFF},
    {0x3506, 0x04},
    {0x3507, 0xD0},
    {0x3508, 0x00},
    {0x3509, 0xBD},
    {0x350A, 0x00},
    {0x350B, 0x20},
    {0x350C, 0x00},
    {0x350D, 0x15},
    {0x350E, 0x15},
    {0x350F, 0x51},
    {0x3510, 0x50},
    {0x3511, 0x90},
    {0x3512, 0x10},
    {0x3513, 0x00},
    {0x3514, 0x00},
    {0x3515, 0x10},
    {0x3516, 0x10},
    {0x3517, 0x00},
    {0x3518, 0x00},
    {0x3519, 0xFF},
    {0x351A, 0xC0},
    {0x351B, 0x98}, 
    {0x351C, 0x6C},
    {0x351D, 0x78},
    {0x351E, 0x96}, 
    {0x351F, 0x80},
    {0x3520, 0x26},
    {0x3521, 0x02},
    {0x3522, 0x08},
    {0x3523, 0x0C},
    {0x3524, 0x01},
    {0x3525, 0x5A},
    {0x3526, 0x3C},
    {0x3527, 0xE0},
    {0x3528, 0x55},
    {0x3529, 0x72},
    {0x352A, 0x5E},
    {0x352B, 0x28},
    {0x352C, 0x65},
    {0x352D, 0x22},
    {0x352E, 0x43},
    {0x352F, 0xD6},
    {0x3530, 0x60},
    {0x3531, 0x24},
    {0x3532, 0x40},
    {0x3533, 0x42},
    {0x3534, 0x00},
    {0x3535, 0x00},
    {0x3536, 0x00},
    {0x3537, 0x00},
    {0x3538, 0x00},
    {0x3539, 0x00},
    {0x353A, 0x00},
    {0x353B, 0x00},
    {0x353C, 0x00},
    {0x353D, 0x00},
    {0x353E, 0x00},
    {0x353F, 0x00},
    {0x3540, 0x00},
    {0x3541, 0x00},
    {0x3542, 0x00},
    {0x3543, 0x00},
    {0x3544, 0x00},
    {0x3545, 0x00},
    {0x3546, 0xE0},
    {0x3547, 0x10},
    {0x3550, 0x00},
    {0x3551, 0x03},
    {0x3552, 0x28},
    {0x3553, 0x20},
    {0x3554, 0x60},
    {0x3555, 0xF0},
    {0x355D, 0x01},
    {0x355E, 0x1C},
    {0x355F, 0x03},
    {0x3560, 0x00},
    {0x3561, 0x00},
    {0x3562, 0x2C},
    {0x3563, 0x00},
    {0x3564, 0x50},
    {0x3565, 0x00},
    {0x3566, 0xD2},
    {0x3567, 0x2E},
    {0x3568, 0x00},
    {0x3569, 0x00},
    {0x356A, 0x00},
    {0x356B, 0x00},
    {0x356C, 0xFF},
    {0x356D, 0xFF},
    {0x356E, 0x01},
    {0x356F, 0x72},
    {0x3570, 0x01},
    {0x3571, 0x00},
    {0x3572, 0x01},
    {0x3573, 0x4D},
    {0x3574, 0x01},
    {0x3575, 0x20},
    {0x3576, 0x45},
    {0x3577, 0x10},
    {0x3578, 0xEE},
    {0x3579, 0x09},
    {0x357A, 0x99},
    {0x357B, 0x00},
    {0x357C, 0xE0},
    {0x357D, 0x00},
    {0x3900, 0x00},
    {0x3901, 0x07},
    {0x3902, 0x00},
    {0x3903, 0x00},
    {0x3904, 0x00},
    {0x3905, 0x00},
    {0x3906, 0x00},
    {0x3907, 0x00},
    {0x3908, 0x00},
    {0x3909, 0x00},
    {0x390A, 0x00},
    {0x390B, 0x00},
    {0x390C, 0x00},
    {0x30F0, 0x00},
};

static struct msm_camera_i2c_reg_conf t4k28_wb_auto_settings[] = {
    {0x3524, 0x01},
    {0x3525, 0x5A},
    {0x3526, 0x3C},
    {0x3527, 0xE0},
    {0x3528, 0x55},
    {0x3529, 0x72},
    {0x3322, 0x25},
    {0x3323, 0x25},
    {0x3324, 0x00},
    {0x3325, 0x63},
    {0x352A, 0x5E},
    {0x352B, 0x28},
    {0x352C, 0x65},
    {0x352D, 0x22},
};

static struct msm_camera_i2c_reg_conf t4k28_wb_fluorescent_settings[] = {
    {0x3524, 0x01},
    {0x3525, 0x00},
    {0x3526, 0x3D},
    {0x3527, 0x00},
    {0x3528, 0x55},
    {0x3529, 0x00},
    {0x3322, 0x10},
    {0x3323, 0x10},
    {0x3324, 0x22},
    {0x3325, 0xA8},
    {0x352A, 0x05},
    {0x352B, 0x05},
    {0x352C, 0x05},
    {0x352D, 0x05},
};

static struct msm_camera_i2c_reg_conf t4k28_wb_daylight_settings[] = {
    {0x3524, 0x01},
    {0x3525, 0x00},
    {0x3526, 0x3D},
    {0x3527, 0x00},
    {0x3528, 0x55},
    {0x3529, 0x00},
    {0x3322, 0x10},
    {0x3323, 0x10},
    {0x3324, 0x29},
    {0x3325, 0x9C},
    {0x352A, 0x05},
    {0x352B, 0x05},
    {0x352C, 0x05},
    {0x352D, 0x05},
};

static struct msm_camera_i2c_reg_conf t4k28_wb_cloudy_settings[] = {
    {0x3524, 0x01},
    {0x3525, 0x00},
    {0x3526, 0x3D},
    {0x3527, 0x00},
    {0x3528, 0x55},
    {0x3529, 0x00},
    {0x3322, 0x10},
    {0x3323, 0x10},
    {0x3324, 0x38},
    {0x3325, 0x85},
    {0x352A, 0x05},
    {0x352B, 0x05},
    {0x352C, 0x05},
    {0x352D, 0x05},
};

static struct msm_camera_i2c_reg_conf t4k28_wb_incandescent_settings[] = {
    {0x3524, 0x01},
    {0x3525, 0x00},
    {0x3526, 0x3D},
    {0x3527, 0x00},
    {0x3528, 0x55},
    {0x3529, 0x00},
    {0x3322, 0x10},
    {0x3323, 0x10},
    {0x3324, 0x00},
    {0x3325, 0xDC},
    {0x352A, 0x08},
    {0x352B, 0x08},
    {0x352C, 0x08},
    {0x352D, 0x08},
};

static struct msm_camera_i2c_reg_conf t4k28_brightness_1_settings[] = {
    {0x343F, 0xA0},
    {0x3440, 0xA0},
};

static struct msm_camera_i2c_reg_conf t4k28_brightness_2_settings[] = {
    {0x343F, 0xB0},
    {0x3440, 0xB0},
};

static struct msm_camera_i2c_reg_conf t4k28_brightness_3_settings[] = {
    {0x343F, 0xC0},
    {0x3440, 0xC0},
};

static struct msm_camera_i2c_reg_conf t4k28_brightness_4_settings[] = {
    {0x343F, 0xD0},
    {0x3440, 0xD0},
};

static struct msm_camera_i2c_reg_conf t4k28_brightness_5_settings[] = {
    {0x343F, 0xE0},
    {0x3440, 0xE0},
};

static struct msm_camera_i2c_reg_conf t4k28_brightness_6_settings[] = {
    {0x343F, 0xF0},
    {0x3440, 0xF0},
};

static struct msm_camera_i2c_reg_conf t4k28_brightness_7_settings[] = {
    {0x343F, 0x00},
    {0x3440, 0x00},
};

static struct msm_camera_i2c_reg_conf t4k28_brightness_8_settings[] = {
    {0x343F, 0x10},
    {0x3440, 0x10},
};

static struct msm_camera_i2c_reg_conf t4k28_brightness_9_settings[] = {
    {0x343F, 0x20},
    {0x3440, 0x20},
};

static struct msm_camera_i2c_reg_conf t4k28_iso_auto_settings[] = {
    {0x3503, 0x1A},
    {0x3504, 0x00},
    {0x3505, 0x9C},
};

static struct msm_camera_i2c_reg_conf t4k28_iso_100_settings[] = {
    {0x3503, 0x1F},
    {0x3504, 0x00},
    {0x3505, 0x1F},
};

static struct msm_camera_i2c_reg_conf t4k28_iso_200_settings[] = {
    {0x3503, 0x3E},
    {0x3504, 0x00},
    {0x3505, 0x3E},
};

static struct msm_camera_i2c_reg_conf t4k28_iso_400_settings[] = {
    {0x3503, 0x7D},
    {0x3504, 0x00},
    {0x3505, 0x7D},
};

static struct msm_camera_i2c_reg_conf t4k28_weight_frame_average_settings[] = {
    {0x350D, 0x55},
    {0x350E, 0x55},
    {0x350F, 0x55},
    {0x3510, 0x54},
};

static struct msm_camera_i2c_reg_conf t4k28_weight_center_settings[] = {
    {0x350D, 0x55},
    {0x350E, 0x57},
    {0x350F, 0x55},
    {0x3510, 0x54},
};

static struct msm_camera_i2c_reg_conf t4k28_weight_spot_settings[] = {
    {0x350D, 0x00},
    {0x350E, 0x03},
    {0x350F, 0x00},
    {0x3510, 0x00},
};
#ifdef FEATURE_KYOCERA_MCAM_BB_ADJ
/* ---------------------------------- */
/*  Adjust Log...                  */
/* ---------------------------------- */
#define ADJ_LOG_SWITCH
//#undef ADJ_LOG_SWITCH
#ifdef ADJ_LOG_SWITCH
#define ADJ_LOG(fmt, args...) printk(KERN_NOTICE fmt, ##args)
#else
#define ADJ_LOG(fmt, args...) do{ } while(0)
#endif

/* ---------------------------------- */
/*  Adjust Define...               */
/* ---------------------------------- */
#define CAMADJ_FLAG_NONE              0x00000000
#define CAMADJ_FLAG_INIT              0x00000001
#define CAMADJ_FLAG_ISO_Auto          0x00000010
#define CAMADJ_FLAG_ISO_100           0x00000020
#define CAMADJ_FLAG_ISO_200           0x00000040
#define CAMADJ_FLAG_ISO_400           0x00000080
#define CAMADJ_FLAG_WB_Auto           0x00000100
#define CAMADJ_FLAG_WB_Fluorescent    0x00000200
#define CAMADJ_FLAG_WB_DayLight       0x00000400
#define CAMADJ_FLAG_WB_Cloudy         0x00000800
#define CAMADJ_FLAG_WB_Incandescent   0x00001000
#define CAMADJ_FLAG_Brightness_1      0x00010000
#define CAMADJ_FLAG_Brightness_2      0x00020000
#define CAMADJ_FLAG_Brightness_3      0x00040000
#define CAMADJ_FLAG_Brightness_4      0x00080000
#define CAMADJ_FLAG_Brightness_5      0x00100000
#define CAMADJ_FLAG_Brightness_6      0x00200000
#define CAMADJ_FLAG_Brightness_7      0x00400000
#define CAMADJ_FLAG_Brightness_8      0x00800000
#define CAMADJ_FLAG_Brightness_9      0x01000000
#define CAMADJ_FLAG_AE_Weight_Fr_Ave  0x10000000
#define CAMADJ_FLAG_AE_Weight_Center  0x20000000
#define CAMADJ_FLAG_AE_Weight_Spot    0x40000000
#define CAMADJ_FLAG_FILE_END          0xFFFFFFFF
#define CAMADJ_FLAG_ISO_MASK          0x000000F0
#define CAMADJ_FLAG_WB_MASK           0x00001F00
#define CAMADJ_FLAG_BR_MASK           0x01FF0000
#define CAMADJ_FLAG_AE_MASK           0x70000000

/* ---------------------------------- */
/*  Adjust Module...               */
/* ---------------------------------- */
static uint32_t camadjust_flag = CAMADJ_FLAG_NONE;
static struct msm_camera_i2c_conf_array camadjust_init_conf[]             = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_ISO_Auto_conf[]         = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_ISO_100_conf[]          = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_ISO_200_conf[]          = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_ISO_400_conf[]          = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_WB_Auto_conf[]          = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_WB_Fluorescent_conf[]   = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_WB_DayLight_conf[]      = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_WB_Cloudy_conf[]        = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_WB_Incandescent_conf[]  = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_Brightness_1_conf[]     = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_Brightness_2_conf[]     = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_Brightness_3_conf[]     = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_Brightness_4_conf[]     = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_Brightness_5_conf[]     = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_Brightness_6_conf[]     = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_Brightness_7_conf[]     = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_Brightness_8_conf[]     = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_Brightness_9_conf[]     = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_AE_Weight_Fr_Ave_conf[] = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_AE_Weight_Center_conf[] = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static struct msm_camera_i2c_conf_array camadjust_AE_Weight_Spot_conf[]   = { {NULL,0, 0, MSM_CAMERA_I2C_BYTE_DATA} };
static void t4k28_main_read_adjust_file( void );
#endif/* FEATURE_KYOCERA_MCAM_BB_ADJ */

static struct msm_camera_i2c_reg_conf t4k28_framerate_auto_settings[] = {
    {0x351B, 0x98},
};

static struct msm_camera_i2c_reg_conf t4k28_framerate_fix_settings[] = {
    {0x351B, 0x08},
};

static struct v4l2_subdev_info t4k28_subdev_info[] = {
    {
        .code   = V4L2_MBUS_FMT_SBGGR10_1X10,
        .colorspace = V4L2_COLORSPACE_JPEG,
        .fmt    = 1,
        .order    = 0,
    },
};

static const struct i2c_device_id t4k28_i2c_id[] = {
    {T4K28_SENSOR_NAME, (kernel_ulong_t)&t4k28_s_ctrl},
    { }
};

static struct msm_camera_i2c_conf_array t4k28_confs[] = {
  {&t4k28_fullsize_settings[0], ARRAY_SIZE(t4k28_fullsize_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
  {&t4k28_720p_settings[0], ARRAY_SIZE(t4k28_720p_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
  {&t4k28_preview_settings[0], ARRAY_SIZE(t4k28_preview_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};


static int32_t msm_t4k28_i2c_probe(struct i2c_client *client,
    const struct i2c_device_id *id)
{
    return msm_sensor_i2c_probe(client, id, &t4k28_s_ctrl);
}

static struct i2c_driver t4k28_i2c_driver = {
    .id_table = t4k28_i2c_id,
    .probe  = msm_t4k28_i2c_probe,
    .driver = {
        .name = T4K28_SENSOR_NAME,
    },
};

static struct msm_camera_i2c_client t4k28_sensor_i2c_client = {
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id t4k28_dt_match[] = {
    {.compatible = "qcom,t4k28", .data = &t4k28_s_ctrl},
    {}
};

MODULE_DEVICE_TABLE(of, t4k28_dt_match);

static struct platform_driver t4k28_platform_driver = {
    .driver = {
        .name = "qcom,t4k28",
        .owner = THIS_MODULE,
        .of_match_table = t4k28_dt_match,
    },
};

static int32_t t4k28_platform_probe(struct platform_device *pdev)
{
    int32_t rc = 0;
    const struct of_device_id *match;
    match = of_match_device(t4k28_dt_match, &pdev->dev);
    rc = msm_sensor_platform_probe(pdev, match->data);
    return rc;
}

static int __init t4k28_init_module(void)
{
    int32_t rc = 0;
    pr_info("%s:%d\n", __func__, __LINE__);
    rc = platform_driver_probe(&t4k28_platform_driver,
        t4k28_platform_probe);
    if (!rc)
        return rc;
    pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
    return i2c_add_driver(&t4k28_i2c_driver);
}

static void __exit t4k28_exit_module(void)
{
    pr_info("%s:%d\n", __func__, __LINE__);
#ifdef FEATURE_KYOCERA_MCAM_BB_ADJ
    if( camadjust_init_conf[0].conf )
    {
        vfree( camadjust_init_conf[0].conf );
        camadjust_init_conf[0].conf = NULL;
    }
    if( camadjust_ISO_Auto_conf[0].conf )
    {
        vfree( camadjust_ISO_Auto_conf[0].conf );
        camadjust_ISO_Auto_conf[0].conf = NULL;
    }
    if( camadjust_ISO_100_conf[0].conf )
    {
        vfree( camadjust_ISO_100_conf[0].conf );
        camadjust_ISO_100_conf[0].conf = NULL;
    }
    if( camadjust_ISO_200_conf[0].conf )
    {
        vfree( camadjust_ISO_200_conf[0].conf );
        camadjust_ISO_200_conf[0].conf = NULL;
    }
    if( camadjust_ISO_400_conf[0].conf )
    {
        vfree( camadjust_ISO_400_conf[0].conf );
        camadjust_ISO_400_conf[0].conf = NULL;
    }
    if( camadjust_WB_Auto_conf[0].conf )
    {
        vfree( camadjust_WB_Auto_conf[0].conf );
        camadjust_WB_Auto_conf[0].conf = NULL;
    }
    if( camadjust_WB_Fluorescent_conf[0].conf )
    {
        vfree( camadjust_WB_Fluorescent_conf[0].conf );
        camadjust_WB_Fluorescent_conf[0].conf = NULL;
    }
    if( camadjust_WB_DayLight_conf[0].conf )
    {
        vfree( camadjust_WB_DayLight_conf[0].conf );
        camadjust_WB_DayLight_conf[0].conf = NULL;
    }
    if( camadjust_WB_Cloudy_conf[0].conf )
    {
        vfree( camadjust_WB_Cloudy_conf[0].conf );
        camadjust_WB_Cloudy_conf[0].conf = NULL;
    }
    if( camadjust_WB_Incandescent_conf[0].conf )
    {
        vfree( camadjust_WB_Incandescent_conf[0].conf );
        camadjust_WB_Incandescent_conf[0].conf = NULL;
    }
    if( camadjust_Brightness_1_conf[0].conf )
    {
        vfree( camadjust_Brightness_1_conf[0].conf );
        camadjust_Brightness_1_conf[0].conf = NULL;
    }
    if( camadjust_Brightness_2_conf[0].conf )
    {
        vfree( camadjust_Brightness_2_conf[0].conf );
        camadjust_Brightness_2_conf[0].conf = NULL;
    }
    if( camadjust_Brightness_3_conf[0].conf )
    {
        vfree( camadjust_Brightness_3_conf[0].conf );
        camadjust_Brightness_3_conf[0].conf = NULL;
    }
    if( camadjust_Brightness_4_conf[0].conf )
    {
        vfree( camadjust_Brightness_4_conf[0].conf );
        camadjust_Brightness_4_conf[0].conf = NULL;
    }
    if( camadjust_Brightness_5_conf[0].conf )
    {
        vfree( camadjust_Brightness_5_conf[0].conf );
        camadjust_Brightness_5_conf[0].conf = NULL;
    }
    if( camadjust_Brightness_6_conf[0].conf )
    {
        vfree( camadjust_Brightness_6_conf[0].conf );
        camadjust_Brightness_6_conf[0].conf = NULL;
    }
    if( camadjust_Brightness_7_conf[0].conf )
    {
        vfree( camadjust_Brightness_7_conf[0].conf );
        camadjust_Brightness_7_conf[0].conf = NULL;
    }
    if( camadjust_Brightness_8_conf[0].conf )
    {
        vfree( camadjust_Brightness_8_conf[0].conf );
        camadjust_Brightness_8_conf[0].conf = NULL;
    }
    if( camadjust_Brightness_9_conf[0].conf )
    {
        vfree( camadjust_Brightness_9_conf[0].conf );
        camadjust_Brightness_9_conf[0].conf = NULL;
    }
    if( camadjust_AE_Weight_Fr_Ave_conf[0].conf )
    {
        vfree( camadjust_AE_Weight_Fr_Ave_conf[0].conf );
        camadjust_AE_Weight_Fr_Ave_conf[0].conf = NULL;
    }
    if( camadjust_AE_Weight_Center_conf[0].conf )
    {
        vfree( camadjust_AE_Weight_Center_conf[0].conf );
        camadjust_AE_Weight_Center_conf[0].conf = NULL;
    }
    if( camadjust_AE_Weight_Spot_conf[0].conf )
    {
        vfree( camadjust_AE_Weight_Spot_conf[0].conf );
        camadjust_AE_Weight_Spot_conf[0].conf = NULL;
    }
#endif /* FEATURE_KYOCERA_MCAM_BB_ADJ */
    if (t4k28_s_ctrl.pdev) {
        msm_sensor_free_sensor_data(&t4k28_s_ctrl);
        platform_driver_unregister(&t4k28_platform_driver);
    } else
        i2c_del_driver(&t4k28_i2c_driver);
    return;
}

static long t4k28_set_wb_mode(struct msm_sensor_ctrl_t *s_ctrl, int32_t mode)
{
    long rc = 0;
    struct msm_camera_i2c_reg_conf *data = NULL;
    int size = 0;

    CDBG("%s %d", __func__, mode);

    switch (mode) {
    case MSM_CAMERA_WB_MODE_AUTO:
        data = &t4k28_wb_auto_settings[0];
        size = ARRAY_SIZE(t4k28_wb_auto_settings);
        break;
    case MSM_CAMERA_WB_MODE_INCANDESCENT:
        data = &t4k28_wb_incandescent_settings[0];
        size = ARRAY_SIZE(t4k28_wb_incandescent_settings);
        break;
    case MSM_CAMERA_WB_MODE_FLUORESCENT:
        data = &t4k28_wb_fluorescent_settings[0];
        size = ARRAY_SIZE(t4k28_wb_fluorescent_settings);
        break;
    case MSM_CAMERA_WB_MODE_DAYLIGHT:
        data = &t4k28_wb_daylight_settings[0];
        size = ARRAY_SIZE(t4k28_wb_daylight_settings);
        break;
    case MSM_CAMERA_WB_MODE_CLOUDY_DAYLIGHT:
        data = &t4k28_wb_cloudy_settings[0];
        size = ARRAY_SIZE(t4k28_wb_cloudy_settings);
        break;
    default:
        break;
    }

    if (data) {
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl(
                 s_ctrl->sensor_i2c_client,
                 data, size,
                 MSM_CAMERA_I2C_BYTE_DATA);
    }
    if (rc < 0) {
        pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
        msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
        return rc;
    }

    return rc;
}

static long t4k28_set_brightness_mode(struct msm_sensor_ctrl_t *s_ctrl, int32_t mode)
{
    long rc = 0;
    struct msm_camera_i2c_reg_conf *data = NULL;
    int size = 0;

    CDBG("%s %d", __func__, mode);

    switch (mode) {
    case 0:
        data = &t4k28_brightness_1_settings[0];
        size = ARRAY_SIZE(t4k28_brightness_1_settings);
        break;
    case 1:
        data = &t4k28_brightness_2_settings[0];
        size = ARRAY_SIZE(t4k28_brightness_2_settings);
        break;
    case 2:
        data = &t4k28_brightness_3_settings[0];
        size = ARRAY_SIZE(t4k28_brightness_3_settings);
        break;
    case 3:
        data = &t4k28_brightness_4_settings[0];
        size = ARRAY_SIZE(t4k28_brightness_4_settings);
        break;
    case 4:
        data = &t4k28_brightness_5_settings[0];
        size = ARRAY_SIZE(t4k28_brightness_5_settings);
        break;
    case 5:
        data = &t4k28_brightness_6_settings[0];
        size = ARRAY_SIZE(t4k28_brightness_6_settings);
        break;
    case 6:
        data = &t4k28_brightness_7_settings[0];
        size = ARRAY_SIZE(t4k28_brightness_7_settings);
        break;
    case 7:
        data = &t4k28_brightness_8_settings[0];
        size = ARRAY_SIZE(t4k28_brightness_8_settings);
        break;
    case 8:
        data = &t4k28_brightness_9_settings[0];
        size = ARRAY_SIZE(t4k28_brightness_9_settings);
        break;
    default:
        break;
    }

    if (data) {
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl(
                 s_ctrl->sensor_i2c_client,
                 data, size,
                 MSM_CAMERA_I2C_BYTE_DATA);
    }
    if (rc < 0) {
        pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
        msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
        return rc;
    }

    return rc;
}

static long t4k28_set_iso_mode(struct msm_sensor_ctrl_t *s_ctrl, int32_t mode)
{
    long rc = 0;
    struct msm_camera_i2c_reg_conf *data = NULL;
    int size = 0;

    CDBG("%s %d", __func__, mode);

    switch (mode) {
    case MSM_CAMERA_ISO_MODE_AUTO:
        data = &t4k28_iso_auto_settings[0];
        size = ARRAY_SIZE(t4k28_iso_auto_settings);
        break;
    case MSM_CAMERA_ISO_MODE_100:
        data = &t4k28_iso_100_settings[0];
        size = ARRAY_SIZE(t4k28_iso_100_settings);
        break;
    case MSM_CAMERA_ISO_MODE_200:
        data = &t4k28_iso_200_settings[0];
        size = ARRAY_SIZE(t4k28_iso_200_settings);
        break;
    case MSM_CAMERA_ISO_MODE_400:
        data = &t4k28_iso_400_settings[0];
        size = ARRAY_SIZE(t4k28_iso_400_settings);
        break;
    default:
        break;
    }

    if (data) {
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl(
                 s_ctrl->sensor_i2c_client,
                 data, size,
                 MSM_CAMERA_I2C_BYTE_DATA);
    }
    if (rc < 0) {
        pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
        msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
        return rc;
    }

    return rc;
}

static long t4k28_set_aec_mode(struct msm_sensor_ctrl_t *s_ctrl, int32_t mode)
{
    long rc = 0;
    struct msm_camera_i2c_reg_conf *data = NULL;
    int size = 0;

    CDBG("%s %d", __func__, mode);

    switch (mode) {
    case MSM_CAMERA_AEC_MODE_FRAME_AVERAGE:
        data = &t4k28_weight_frame_average_settings[0];
        size = ARRAY_SIZE(t4k28_weight_frame_average_settings);
        break;
    case MSM_CAMERA_AEC_MODE_CENTER_WEIGHTED:
        data = &t4k28_weight_center_settings[0];
        size = ARRAY_SIZE(t4k28_weight_center_settings);
        break;
    case MSM_CAMERA_AEC_MODE_SPOT_METERING:
        data = &t4k28_weight_spot_settings[0];
        size = ARRAY_SIZE(t4k28_weight_spot_settings);
        break;
    default:
        break;
    }

    if (data) {
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl(
                 s_ctrl->sensor_i2c_client,
                 data, size,
                 MSM_CAMERA_I2C_BYTE_DATA);
    }
    if (rc < 0) {
        pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
        msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
        return rc;
    }

    return rc;
}

#ifdef FEATURE_KYOCERA_MCAM_BB_ADJ
static long t4k28_set_wb_mode_tool_data(struct msm_sensor_ctrl_t *s_ctrl, int32_t mode)
{
    long rc = 0;
    struct msm_camera_i2c_reg_conf *data = NULL;
    int size = 0;

    CDBG("%s %d", __func__, mode);

    switch (mode) {
    case MSM_CAMERA_WB_MODE_AUTO:
        data = camadjust_WB_Auto_conf[0].conf;
        size = camadjust_WB_Auto_conf[0].size;
        break;
    case MSM_CAMERA_WB_MODE_INCANDESCENT:
        data = camadjust_WB_Incandescent_conf[0].conf;
        size = camadjust_WB_Incandescent_conf[0].size;
        break;
    case MSM_CAMERA_WB_MODE_FLUORESCENT:
        data = camadjust_WB_Fluorescent_conf[0].conf;
        size = camadjust_WB_Fluorescent_conf[0].size;
        break;
    case MSM_CAMERA_WB_MODE_DAYLIGHT:
        data = camadjust_WB_DayLight_conf[0].conf;
        size = camadjust_WB_DayLight_conf[0].size;
        break;
    case MSM_CAMERA_WB_MODE_CLOUDY_DAYLIGHT:
        data = camadjust_WB_Cloudy_conf[0].conf;
        size = camadjust_WB_Cloudy_conf[0].size;
        break;
    default:
        break;
    }

    if (data) {
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl(
                 s_ctrl->sensor_i2c_client,
                 data, size,
                 MSM_CAMERA_I2C_BYTE_DATA);
    }
    if (rc < 0) {
        pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
        msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
        return rc;
    }

    return rc;
}

static long t4k28_set_brightness_mode_tool_data(struct msm_sensor_ctrl_t *s_ctrl, int32_t mode)
{
    long rc = 0;
    struct msm_camera_i2c_reg_conf *data = NULL;
    int size = 0;

    CDBG("%s %d", __func__, mode);

    switch (mode) {
    case 0:
        data = camadjust_Brightness_1_conf[0].conf;
        size = camadjust_Brightness_1_conf[0].size;
        break;
    case 1:
        data = camadjust_Brightness_2_conf[0].conf;
        size = camadjust_Brightness_2_conf[0].size;
        break;
    case 2:
        data = camadjust_Brightness_3_conf[0].conf;
        size = camadjust_Brightness_3_conf[0].size;
        break;
    case 3:
        data = camadjust_Brightness_4_conf[0].conf;
        size = camadjust_Brightness_4_conf[0].size;
        break;
    case 4:
        data = camadjust_Brightness_5_conf[0].conf;
        size = camadjust_Brightness_5_conf[0].size;
        break;
    case 5:
        data = camadjust_Brightness_6_conf[0].conf;
        size = camadjust_Brightness_6_conf[0].size;
        break;
    case 6:
        data = camadjust_Brightness_7_conf[0].conf;
        size = camadjust_Brightness_7_conf[0].size;
        break;
    case 7:
        data = camadjust_Brightness_8_conf[0].conf;
        size = camadjust_Brightness_8_conf[0].size;
        break;
    case 8:
        data = camadjust_Brightness_9_conf[0].conf;
        size = camadjust_Brightness_9_conf[0].size;
        break;
    default:
        break;
    }

    if (data) {
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl(
                 s_ctrl->sensor_i2c_client,
                 data, size,
                 MSM_CAMERA_I2C_BYTE_DATA);
    }
    if (rc < 0) {
        pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
        msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
        return rc;
    }

    return rc;
}

static long t4k28_set_iso_mode_tool_data(struct msm_sensor_ctrl_t *s_ctrl, int32_t mode)
{
    long rc = 0;
    struct msm_camera_i2c_reg_conf *data = NULL;
    int size = 0;

    CDBG("%s %d", __func__, mode);

    switch (mode) {
    case MSM_CAMERA_ISO_MODE_AUTO:
        data = camadjust_ISO_Auto_conf[0].conf;
        size = camadjust_ISO_Auto_conf[0].size;
        break;
    case MSM_CAMERA_ISO_MODE_100:
        data = camadjust_ISO_100_conf[0].conf;
        size = camadjust_ISO_100_conf[0].size;
        break;
    case MSM_CAMERA_ISO_MODE_200:
        data = camadjust_ISO_200_conf[0].conf;
        size = camadjust_ISO_200_conf[0].size;
        break;
    case MSM_CAMERA_ISO_MODE_400:
        data = camadjust_ISO_400_conf[0].conf;
        size = camadjust_ISO_400_conf[0].size;
        break;
    default:
        break;
    }

    if (data) {
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl(
                 s_ctrl->sensor_i2c_client,
                 data, size,
                 MSM_CAMERA_I2C_BYTE_DATA);
    }
    if (rc < 0) {
        pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
        msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
        return rc;
    }

    return rc;
}

static long t4k28_set_aec_mode_tool_data(struct msm_sensor_ctrl_t *s_ctrl, int32_t mode)
{
    long rc = 0;
    struct msm_camera_i2c_reg_conf *data = NULL;
    int size = 0;

    CDBG("%s %d", __func__, mode);

    switch (mode) {
    case MSM_CAMERA_AEC_MODE_FRAME_AVERAGE:
        data = camadjust_AE_Weight_Fr_Ave_conf[0].conf;
        size = camadjust_AE_Weight_Fr_Ave_conf[0].size;
        break;
    case MSM_CAMERA_AEC_MODE_CENTER_WEIGHTED:
        data = camadjust_AE_Weight_Center_conf[0].conf;
        size = camadjust_AE_Weight_Center_conf[0].size;
        break;
    case MSM_CAMERA_AEC_MODE_SPOT_METERING:
        data = camadjust_AE_Weight_Spot_conf[0].conf;
        size = camadjust_AE_Weight_Spot_conf[0].size;
        break;
    default:
        break;
    }

    if (data) {
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl(
                 s_ctrl->sensor_i2c_client,
                 data, size,
                 MSM_CAMERA_I2C_BYTE_DATA);
    }
    if (rc < 0) {
        pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
        msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
        return rc;
    }

    return rc;
}
#endif /* FEATURE_KYOCERA_MCAM_BB_ADJ */
static long t4k28_set_framerate_mode(struct msm_sensor_ctrl_t *s_ctrl, int32_t mode)
{
    long rc = 0;
    struct msm_camera_i2c_reg_conf *data = NULL;
    int size = 0;

	CDBG("%s %d", __func__, mode);

    switch (mode) {
    case MSM_CAMERA_FRAMERATE_MODE_AUTO:
        data = &t4k28_framerate_auto_settings[0];
        size = ARRAY_SIZE(t4k28_framerate_auto_settings);
        break;
    case MSM_CAMERA_FRAMERATE_MODE_FIX:
        data = &t4k28_framerate_fix_settings[0];
        size = ARRAY_SIZE(t4k28_framerate_fix_settings);
        break;
    default:
        break;
    }

    if (data) {
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl(
                 s_ctrl->sensor_i2c_client,
                 data, size,
                 MSM_CAMERA_I2C_BYTE_DATA);
    }
    if (rc < 0) {
        pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
        msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
        return rc;
    }

    return rc;
}

int32_t t4k28_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
    struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
    long rc = 0;
    int32_t i = 0;
    enum msm_sensor_resolution_t res;
    mutex_lock(s_ctrl->msm_sensor_mutex);
    CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
        s_ctrl->sensordata->sensor_name, cdata->cfgtype);
    switch (cdata->cfgtype) {
    case CFG_GET_SENSOR_INFO:
        memcpy(cdata->cfg.sensor_info.sensor_name,
            s_ctrl->sensordata->sensor_name,
            sizeof(cdata->cfg.sensor_info.sensor_name));
        cdata->cfg.sensor_info.session_id =
            s_ctrl->sensordata->sensor_info->session_id;
        for (i = 0; i < SUB_MODULE_MAX; i++)
            cdata->cfg.sensor_info.subdev_id[i] =
                s_ctrl->sensordata->sensor_info->subdev_id[i];
        CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
            cdata->cfg.sensor_info.sensor_name);
        CDBG("%s:%d session id %d\n", __func__, __LINE__,
            cdata->cfg.sensor_info.session_id);
        for (i = 0; i < SUB_MODULE_MAX; i++)
            CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
                cdata->cfg.sensor_info.subdev_id[i]);

        break;
    case CFG_SET_INIT_SETTING:
#ifdef FEATURE_KYOCERA_MCAM_BB_ADJ
        camadjust_flag = CAMADJ_FLAG_NONE;
        t4k28_main_read_adjust_file();
        if (camadjust_flag & CAMADJ_FLAG_INIT)
        {
            /* 1. Write Recommend settings */
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                i2c_write_conf_tbl(
                s_ctrl->sensor_i2c_client,
                camadjust_init_conf[0].conf,
                camadjust_init_conf[0].size,
                MSM_CAMERA_I2C_BYTE_DATA);
            break;
        }
#endif /* FEATURE_KYOCERA_MCAM_BB_ADJ */
        /* 1. Write Recommend settings */
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
            i2c_write_conf_tbl(
            s_ctrl->sensor_i2c_client, t4k28_recommend_settings,
            ARRAY_SIZE(t4k28_recommend_settings),
            MSM_CAMERA_I2C_BYTE_DATA);
{
        wb_read = false;
        res_mode = MSM_SENSOR_INVALID_RES;
}
        break;
    case CFG_SET_RESOLUTION:
        res = *(enum msm_sensor_resolution_t *)cdata->cfg.setting;
        if( res >= ARRAY_SIZE( t4k28_confs ))
        {
            res = MSM_SENSOR_RES_FULL;
        }
        
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
            i2c_write_conf_tbl(
            s_ctrl->sensor_i2c_client, t4k28_confs[res].conf,
            t4k28_confs[res].size,
            MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0) {
            pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
            msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
            mutex_unlock(s_ctrl->msm_sensor_mutex);
            return rc;
        }
{
        CDBG("###TEST### SET RES=%d", res );
        res_mode = res;
}
        break;
    case CFG_SET_STOP_STREAM:
{
        /* 1-1.Preview mode */
        if ( res_mode == MSM_SENSOR_RES_2 && !wb_read ) {
            int i = 0;
{
            uint16_t read_data = 0;
            uint16_t write_data = 0;

            /* 2.Read AEC LOCK */
            CDBG("###TEST### 2.Read AEC LOCK" );
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                 i2c_read(
                 s_ctrl->sensor_i2c_client,
                 0x3500,
                 &read_data,
                 MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0) {
                pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
                msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
                mutex_unlock(s_ctrl->msm_sensor_mutex);
                return rc;
            }
            
            /* 3.Write AEC LOCK */
            CDBG("###TEST### 3.Write AEC LOCK" );
            write_data = read_data | 0x20;
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                 i2c_write(
                 s_ctrl->sensor_i2c_client,
                 0x3500,
                 write_data,
                 MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0) {
                pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
                msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
                mutex_unlock(s_ctrl->msm_sensor_mutex);
                return rc;
            }
            
            /* 4.Read exposure value Keep */
            CDBG("###TEST### 4.Read exposure value Keep" );
            /* Before write capture setting, read below register first */
            for ( i = 0; i < (ARRAY_SIZE(t4k28_keep_exposure_value)); i++ ) {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                    i2c_read(
                    s_ctrl->sensor_i2c_client,
                    t4k28_keep_exposure_value[i].reg_addr,
                    &t4k28_keep_exposure_value[i].reg_data,
                    MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0) {
                    pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
                    msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
                    mutex_unlock(s_ctrl->msm_sensor_mutex);
                    return rc;
                }
            }
            /* 5.add one more step */
            CDBG("###TEST### 5.add one more step" );
            msleep(5);
            write_data = write_data & ~0x80;
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                 i2c_write(
                 s_ctrl->sensor_i2c_client,
                 0x3500,
                 write_data,
                 MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0) {
                pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
                msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
                mutex_unlock(s_ctrl->msm_sensor_mutex);
                return rc;
            }
}
            wb_read = true;
        }
}
        CDBG("###TEST### 6.Write Stop Stream" );
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
            i2c_write_conf_tbl(
            s_ctrl->sensor_i2c_client,
            t4k28_stop_settings,
            ARRAY_SIZE(t4k28_stop_settings),
            MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0) {
            pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
            msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
            mutex_unlock(s_ctrl->msm_sensor_mutex);
            return rc;
        }
        break;
    case CFG_SET_START_STREAM:
        CDBG("###TEST### 9.Write Start Stream" );
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
            i2c_write_conf_tbl(
            s_ctrl->sensor_i2c_client,
            t4k28_start_settings,
            ARRAY_SIZE(t4k28_start_settings),
            MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0) {
            pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
            msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
            mutex_unlock(s_ctrl->msm_sensor_mutex);
            return rc;
        }
{
        if ( res_mode == MSM_SENSOR_RES_QTR || res_mode == MSM_SENSOR_RES_2 )
        {
            uint16_t read_data = 0;
            uint16_t write_data = 0;
            CDBG("###TEST### WRITE EXP ALC UNLOCK" );
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                i2c_read(
                s_ctrl->sensor_i2c_client,
                0x3500,
                &read_data,
                MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0) {
                pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
                msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
                mutex_unlock(s_ctrl->msm_sensor_mutex);
                return rc;
            }
            
            write_data = read_data | 0x80;
            write_data = write_data & ~0x20;
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                i2c_write(
                s_ctrl->sensor_i2c_client,
                0x3500,
                write_data,
                MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0) {
                pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
                msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
                mutex_unlock(s_ctrl->msm_sensor_mutex);
                return rc;
            }

            wb_read = false;
        }
        if ( wb_read ) {
            /* 4-2.Write white balance vaule when it memorized at preview mode */
{
            uint16_t tmp_data = 0;

            /* 11.Write exposure value */
            CDBG("###TEST### 11.Write exposure value" );

            /* ********* Then write the above value into below register  ********** */
            tmp_data = (( t4k28_keep_exposure_value[0].reg_data << 8) | t4k28_keep_exposure_value[1].reg_data );
            CDBG("### 0x355F:0x%x 0x3560:0x%x \n", t4k28_keep_exposure_value[0].reg_data, t4k28_keep_exposure_value[1].reg_data);
            CDBG("### tmp_data:0x%x :%d(dec)\n", tmp_data, tmp_data);
            t4k28_write_exposure_value[0].reg_data = (tmp_data >> 8);                              /* 0x3506 MES[15:8] */
            t4k28_write_exposure_value[1].reg_data = tmp_data & 0xFF;                              /* 0x3507 MES[7:0] */
            t4k28_write_exposure_value[2].reg_data = t4k28_keep_exposure_value[2].reg_data & 0x0F; /* 0x350A MAG[11:8] */
            t4k28_write_exposure_value[3].reg_data = t4k28_keep_exposure_value[3].reg_data;        /* 0x350B MAG[7:0] */
            t4k28_write_exposure_value[4].reg_data = (( t4k28_keep_exposure_value[4].reg_data & 0x03) | t4k28_keep_exposure_value[5].reg_data ) / 4;
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                i2c_write_conf_tbl(
                s_ctrl->sensor_i2c_client,
                t4k28_write_exposure_value,
                ARRAY_SIZE(t4k28_write_exposure_value),
                MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0) {
                pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
                msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
                mutex_unlock(s_ctrl->msm_sensor_mutex);
                return rc;
            }

            //******** After write the Capture setting, check Vcount and Shutter *** //
            if((( t4k28_write_exposure_value[0].reg_data << 8 ) | (t4k28_write_exposure_value[1].reg_data)) > 1240) {
                uint16_t v_count;
                v_count = (((t4k28_write_exposure_value[0].reg_data << 8 ) | t4k28_write_exposure_value[1].reg_data) / 2) + 6;
                t4k28_write_exposure_value_2[0].reg_data = (v_count >> 8) & 0x1F;
                t4k28_write_exposure_value_2[1].reg_data = v_count & 0xFF;
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                    i2c_write_conf_tbl(
                    s_ctrl->sensor_i2c_client,
                    t4k28_write_exposure_value_2,
                    ARRAY_SIZE(t4k28_write_exposure_value_2),
                    MSM_CAMERA_I2C_BYTE_DATA);
            }
            if (rc < 0) {
                pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
                msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
                mutex_unlock(s_ctrl->msm_sensor_mutex);
                return rc;
            }
}
            if ( res_mode == MSM_SENSOR_RES_2 ) {
{
                uint16_t read_data = 0;
                uint16_t write_data = 0;
                CDBG("###TEST### WRITE EXP ALC UNLOCK" );
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                    i2c_read(
                    s_ctrl->sensor_i2c_client,
                    0x3500,
                    &read_data,
                    MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0) {
                    pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
                    msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
                    mutex_unlock(s_ctrl->msm_sensor_mutex);
                    return rc;
                }
                
                write_data = read_data | 0x80;
                write_data = write_data & ~0x20;
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                    i2c_write(
                    s_ctrl->sensor_i2c_client,
                    0x3500,
                    write_data,
                    MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0) {
                    pr_err("%s:%d: i2c_write failed\n", __func__, __LINE__);
                    msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_I2C_ERROR);
                    mutex_unlock(s_ctrl->msm_sensor_mutex);
                    return rc;
                }
}

                wb_read = false;
            }
        }
}
        break;
    case CFG_GET_SENSOR_INIT_PARAMS:
        cdata->cfg.sensor_init_params =
            *s_ctrl->sensordata->sensor_init_params;
        CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
            __LINE__,
            cdata->cfg.sensor_init_params.modes_supported,
            cdata->cfg.sensor_init_params.position,
            cdata->cfg.sensor_init_params.sensor_mount_angle);
        break;
    case CFG_SET_SLAVE_INFO: {
        struct msm_camera_sensor_slave_info sensor_slave_info;
        struct msm_sensor_power_setting_array *power_setting_array;
        int slave_index = 0;
        if (copy_from_user(&sensor_slave_info,
            (void *)cdata->cfg.setting,
            sizeof(struct msm_camera_sensor_slave_info))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }
        /* Update sensor slave address */
//        if (sensor_slave_info.slave_addr) {
//            s_ctrl->sensor_i2c_client->cci_client->sid =
//                sensor_slave_info.slave_addr >> 1;
//        }

        /* Update sensor address type */
        s_ctrl->sensor_i2c_client->addr_type =
            sensor_slave_info.addr_type;

        /* Update power up / down sequence */
        s_ctrl->power_setting_array =
            sensor_slave_info.power_setting_array;
        power_setting_array = &s_ctrl->power_setting_array;
        power_setting_array->power_setting = kzalloc(
            power_setting_array->size *
            sizeof(struct msm_sensor_power_setting), GFP_KERNEL);
        if (!power_setting_array->power_setting) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -ENOMEM;
            break;
        }
        if (copy_from_user(power_setting_array->power_setting,
            (void *)sensor_slave_info.power_setting_array.power_setting,
            power_setting_array->size *
            sizeof(struct msm_sensor_power_setting))) {
            kfree(power_setting_array->power_setting);
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }
        s_ctrl->free_power_setting = true;
        CDBG("%s sensor id %x\n", __func__,
            sensor_slave_info.slave_addr);
        CDBG("%s sensor addr type %d\n", __func__,
            sensor_slave_info.addr_type);
        CDBG("%s sensor reg %x\n", __func__,
            sensor_slave_info.sensor_id_info.sensor_id_reg_addr);
        CDBG("%s sensor id %x\n", __func__,
            sensor_slave_info.sensor_id_info.sensor_id);
        for (slave_index = 0; slave_index <
            power_setting_array->size; slave_index++) {
            CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
                slave_index,
                power_setting_array->power_setting[slave_index].
                seq_type,
                power_setting_array->power_setting[slave_index].
                seq_val,
                power_setting_array->power_setting[slave_index].
                config_val,
                power_setting_array->power_setting[slave_index].
                delay);
        }
        kfree(power_setting_array->power_setting);
        break;
    }
    case CFG_WRITE_I2C_ARRAY: {
        struct msm_camera_i2c_reg_setting conf_array;
        struct msm_camera_i2c_reg_array *reg_setting = NULL;

        if (copy_from_user(&conf_array,
            (void *)cdata->cfg.setting,
            sizeof(struct msm_camera_i2c_reg_setting))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }

        reg_setting = kzalloc(conf_array.size *
            (sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
        if (!reg_setting) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -ENOMEM;
            break;
        }
        if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
            conf_array.size *
            sizeof(struct msm_camera_i2c_reg_array))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            kfree(reg_setting);
            rc = -EFAULT;
            break;
        }

        conf_array.reg_setting = reg_setting;
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
            s_ctrl->sensor_i2c_client, &conf_array);
        kfree(reg_setting);
        break;
    }
    case CFG_SLAVE_READ_I2C: {
        struct msm_camera_i2c_read_config read_config;
        uint16_t local_data = 0;
        uint16_t orig_slave_addr = 0, read_slave_addr = 0;
        if (copy_from_user(&read_config,
            (void *)cdata->cfg.setting,
            sizeof(struct msm_camera_i2c_read_config))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }
        read_slave_addr = read_config.slave_addr;
        CDBG("%s:CFG_SLAVE_READ_I2C:", __func__);
        CDBG("%s:slave_addr=0x%x reg_addr=0x%x, data_type=%d\n",
            __func__, read_config.slave_addr,
            read_config.reg_addr, read_config.data_type);
        if (s_ctrl->sensor_i2c_client->cci_client) {
            orig_slave_addr =
                s_ctrl->sensor_i2c_client->cci_client->sid;
            s_ctrl->sensor_i2c_client->cci_client->sid =
                read_slave_addr >> 1;
        } else if (s_ctrl->sensor_i2c_client->client) {
            orig_slave_addr =
                s_ctrl->sensor_i2c_client->client->addr;
            s_ctrl->sensor_i2c_client->client->addr =
                read_slave_addr >> 1;
        } else {
            pr_err("%s: error: no i2c/cci client found.", __func__);
            rc = -EFAULT;
            break;
        }
        CDBG("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x",
                __func__, orig_slave_addr,
                read_slave_addr >> 1);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
                s_ctrl->sensor_i2c_client,
                read_config.reg_addr,
                &local_data, read_config.data_type);
        if (rc < 0) {
            pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
            break;
        }
        if (copy_to_user((void __user *)read_config.data,
            (void *)&local_data, sizeof(uint16_t))) {
            pr_err("%s:%d copy failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }
    }
        break;
    case CFG_WRITE_I2C_SEQ_ARRAY: {
        struct msm_camera_i2c_seq_reg_setting conf_array;
        struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

        if (copy_from_user(&conf_array,
            (void *)cdata->cfg.setting,
            sizeof(struct msm_camera_i2c_seq_reg_setting))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }

        reg_setting = kzalloc(conf_array.size *
            (sizeof(struct msm_camera_i2c_seq_reg_array)),
            GFP_KERNEL);
        if (!reg_setting) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -ENOMEM;
            break;
        }
        if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
            conf_array.size *
            sizeof(struct msm_camera_i2c_seq_reg_array))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            kfree(reg_setting);
            rc = -EFAULT;
            break;
        }

        conf_array.reg_setting = reg_setting;
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
            i2c_write_seq_table(s_ctrl->sensor_i2c_client,
            &conf_array);
        kfree(reg_setting);
        break;
    }

    case CFG_POWER_UP:
        if (s_ctrl->func_tbl->sensor_power_up)
            rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
        else
            rc = -EFAULT;
        break;

    case CFG_POWER_DOWN:
        if (s_ctrl->func_tbl->sensor_power_down)
            rc = s_ctrl->func_tbl->sensor_power_down(
                s_ctrl);
        else
            rc = -EFAULT;
        break;

    case CFG_SET_STOP_STREAM_SETTING: {
        struct msm_camera_i2c_reg_setting *stop_setting =
            &s_ctrl->stop_setting;
        struct msm_camera_i2c_reg_array *reg_setting = NULL;
        if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
            sizeof(struct msm_camera_i2c_reg_setting))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }

        reg_setting = stop_setting->reg_setting;
        stop_setting->reg_setting = kzalloc(stop_setting->size *
            (sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
        if (!stop_setting->reg_setting) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -ENOMEM;
            break;
        }
        if (copy_from_user(stop_setting->reg_setting,
            (void *)reg_setting, stop_setting->size *
            sizeof(struct msm_camera_i2c_reg_array))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            kfree(stop_setting->reg_setting);
            stop_setting->reg_setting = NULL;
            stop_setting->size = 0;
            rc = -EFAULT;
            break;
        }
        break;
    }
    case CFG_SET_WHITE_BALANCE: {
        int32_t mode;
        if (copy_from_user(&mode, (void *)cdata->cfg.setting, sizeof(int32_t))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }
#ifdef FEATURE_KYOCERA_MCAM_BB_ADJ
        if (camadjust_flag & CAMADJ_FLAG_WB_MASK) {
            rc = t4k28_set_wb_mode_tool_data(s_ctrl, mode);
            break;
        }
#endif /* EATURE_KYOCERA_MCAM_BB_ADJ */
        rc = t4k28_set_wb_mode(s_ctrl, mode);
        break;
    }

    case CFG_SET_BRIGHTNESS: {
        int32_t mode;
        if (copy_from_user(&mode, (void *)cdata->cfg.setting, sizeof(int32_t))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }
#ifdef FEATURE_KYOCERA_MCAM_BB_ADJ
        if (camadjust_flag & CAMADJ_FLAG_BR_MASK) {
            rc = t4k28_set_brightness_mode_tool_data(s_ctrl, mode);
            break;
        }
#endif /* EATURE_KYOCERA_MCAM_BB_ADJ */
        rc = t4k28_set_brightness_mode(s_ctrl, mode);
        break;
    }

    case CFG_SET_ISO: {
        int32_t mode;
        if (copy_from_user(&mode, (void *)cdata->cfg.setting, sizeof(int32_t))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }
#ifdef FEATURE_KYOCERA_MCAM_BB_ADJ
        if (camadjust_flag & CAMADJ_FLAG_ISO_MASK) {
            rc = t4k28_set_iso_mode_tool_data(s_ctrl, mode);
            break;
        }
#endif /* EATURE_KYOCERA_MCAM_BB_ADJ */
        rc = t4k28_set_iso_mode(s_ctrl, mode);
        break;
    }

    case CFG_SET_AEC_ALGO: {
        int32_t mode;
        if (copy_from_user(&mode, (void *)cdata->cfg.setting, sizeof(int32_t))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }
#ifdef FEATURE_KYOCERA_MCAM_BB_ADJ
        if (camadjust_flag & CAMADJ_FLAG_AE_MASK) {
            rc = t4k28_set_aec_mode_tool_data(s_ctrl, mode);
            break;
        }
#endif /* EATURE_KYOCERA_MCAM_BB_ADJ */
        rc = t4k28_set_aec_mode(s_ctrl, mode);
        break;
    }
    case CFG_GET_GAIN: {
        uint16_t addr[] = {0x3562};
        uint16_t data[] = {0x0000};
        uint16_t ret = 0x0000;
        int i;

        for( i = 0 ; i < ARRAY_SIZE(addr);i++){
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
                     s_ctrl->sensor_i2c_client, 
                     addr[i], &data[i], MSM_CAMERA_I2C_BYTE_DATA);
            pr_info("Get sub camera gain addr = [0x%04X]\n", addr[i]);
            pr_info("Get sub camera gain data = [0x%04X]\n", data[i]);
        }
        ret = data[0];
        pr_info("Get sub camera gain ret = [0x%04X]\n", ret);
        if (copy_to_user((void *)cdata->cfg.setting, (void *)&ret, sizeof(uint16_t)))
        {
            rc = -EFAULT;
        }
        break;
    }
    case CFG_GET_EXPOSURE: {
        uint16_t addr[] = {0x355F,0x3560};
        uint16_t data[] = {0x0000,0x0000};
        uint32_t ret = 0x0000;
        int i;

        for( i = 0 ; i < ARRAY_SIZE(addr);i++){
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
                     s_ctrl->sensor_i2c_client,
                     addr[i], &data[i], MSM_CAMERA_I2C_BYTE_DATA);
            pr_info("Get sub camera exposure addr = [0x%04X]\n", addr[i]);
            pr_info("Get sub camera exposure data = [0x%04X]\n", data[i]);
        }
        /* Exposure time = 0x355F ALC_ES[15:8], 0x3560 ALC_ES[7:0] read * 0.0434ms */
        ret = ((data[0] & 0x00FF) << 8) | data[1];
        ret = ret * 434;
        pr_info("Get sub camera exposure ret = [0x%04X]\n", ret);
        if (copy_to_user((void *)cdata->cfg.setting, (void *)&ret, sizeof(uint32_t)))
        {
            rc = -EFAULT;
        }
        break;
    }
    case CFG_SET_FRAMERATE_MODE: {
        int32_t mode;
        if (copy_from_user(&mode, (void *)cdata->cfg.setting, sizeof(int32_t))) {
            pr_err("%s:%d failed\n", __func__, __LINE__);
            rc = -EFAULT;
            break;
        }
        rc = t4k28_set_framerate_mode(s_ctrl, mode);
        break;
    }
    case CFG_SET_TIMEOUT_ERROR: {
        msm_error_notify((unsigned int)s_ctrl->sensordata->sensor_info->session_id, MSM_CAMERA_PRIV_TIMEOUT_ERROR);
        break;
    }
    default:
        rc = 0;
        break;
    }

    mutex_unlock(s_ctrl->msm_sensor_mutex);
    
    return rc;
}

#ifdef FEATURE_KYOCERA_MCAM_BB_ADJ
static void t4k28_main_read_adjust_file( void )
{
    uint32_t data_code = 0;
    uint32_t data_cnt  = 0;
    unsigned char temp_buf[32];
    struct file *file_p;
    int read_cnt;
    uint32_t i  = 0;
    struct msm_camera_i2c_conf_array *temp_p = NULL;
    mm_segment_t old_fs = get_fs();

    ADJ_LOG("[CAMADJ_LOG]Camera AdjustFile ReadStart\n");

    /* ---------------------------------------------------- *
     *  File Open
     * ---------------------------------------------------- */
    memset(&temp_buf[0],0x00,sizeof(temp_buf));
    sprintf( temp_buf, "/data/CAM_ADJ.bin" );
    set_fs(KERNEL_DS);
    file_p = filp_open( temp_buf, O_RDONLY, 0);
    if( IS_ERR(file_p) )
    {
        ADJ_LOG("[CAMADJ_LOG]AdjustFile Open Failed!\n");
        set_fs(old_fs);
        return;
    }

    while( 1 )
    {
        /* ---------------------------------------------------- *
         *  Data Code Read
         * ---------------------------------------------------- */
        read_cnt = file_p->f_op->read( file_p, temp_buf, 4, &(file_p->f_pos) );
        data_code = (uint32_t)temp_buf[0] | ((uint32_t)temp_buf[1] << 8) | ((uint32_t)temp_buf[2] << 16) | ((uint32_t)temp_buf[3] << 24);
        ADJ_LOG("[CAMADJ_LOG] Data Code[%08x]\n", data_code);
        if( data_code == CAMADJ_FLAG_FILE_END )
        {
            break;
        }
        else if( data_code == CAMADJ_FLAG_INIT )
        {
            temp_p = &camadjust_init_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_ISO_Auto )
        {
            temp_p = &camadjust_ISO_Auto_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_ISO_100 )
        {
            temp_p = &camadjust_ISO_100_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_ISO_200 )
        {
            temp_p = &camadjust_ISO_200_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_ISO_400 )
        {
            temp_p = &camadjust_ISO_400_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_WB_Auto )
        {
            temp_p = &camadjust_WB_Auto_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_WB_Fluorescent )
        {
            temp_p = &camadjust_WB_Fluorescent_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_WB_DayLight )
        {
            temp_p = &camadjust_WB_DayLight_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_WB_Cloudy )
        {
            temp_p = &camadjust_WB_Cloudy_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_WB_Incandescent )
        {
            temp_p = &camadjust_WB_Incandescent_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_Brightness_1 )
        {
            temp_p = &camadjust_Brightness_1_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_Brightness_2 )
        {
            temp_p = &camadjust_Brightness_2_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_Brightness_3 )
        {
            temp_p = &camadjust_Brightness_3_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_Brightness_4 )
        {
            temp_p = &camadjust_Brightness_4_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_Brightness_5 )
        {
            temp_p = &camadjust_Brightness_5_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_Brightness_6 )
        {
            temp_p = &camadjust_Brightness_6_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_Brightness_7 )
        {
            temp_p = &camadjust_Brightness_7_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_Brightness_8 )
        {
            temp_p = &camadjust_Brightness_8_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_Brightness_9 )
        {
            temp_p = &camadjust_Brightness_9_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_AE_Weight_Fr_Ave )
        {
            temp_p = &camadjust_AE_Weight_Fr_Ave_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_AE_Weight_Center )
        {
            temp_p = &camadjust_AE_Weight_Center_conf[0];
        }
        else if( data_code == CAMADJ_FLAG_AE_Weight_Spot )
        {
            temp_p = &camadjust_AE_Weight_Spot_conf[0];
        }

        /* ---------------------------------------------------- *
         *  Data Count Read
         * ---------------------------------------------------- */
        read_cnt = file_p->f_op->read( file_p, temp_buf, 4, &(file_p->f_pos) );
        data_cnt = (uint32_t)temp_buf[0] | ((uint32_t)temp_buf[1] << 8) | ((uint32_t)temp_buf[2] << 16) | ((uint32_t)temp_buf[3] << 24);
        ADJ_LOG("[CAMADJ_LOG] Data Count[%d]\n", data_cnt);

        /* ---------------------------------------------------- *
         *  Data Read
         * ---------------------------------------------------- */
        temp_p->conf = (struct msm_camera_i2c_reg_conf *)vmalloc(( data_cnt * sizeof(struct msm_camera_i2c_reg_conf)) + 1 );
        temp_p->size = (uint16_t)data_cnt;
        for( i = 0; i < data_cnt; i++ )
        {
            read_cnt = file_p->f_op->read(file_p, temp_buf, 4, &(file_p->f_pos) );
            temp_p->conf[i].reg_addr = (uint16_t)temp_buf[0] | ((uint16_t)temp_buf[1] << 8);
            temp_p->conf[i].reg_data = (uint16_t)temp_buf[2] | ((uint16_t)temp_buf[3] << 8);
            temp_p->conf[i].dt = MSM_CAMERA_I2C_BYTE_DATA;
            ADJ_LOG("[CAMADJ_LOG] Read Addr[0x%04X]", temp_p->conf[i].reg_addr);
            ADJ_LOG("[CAMADJ_LOG] Read Data[0x%04X]", temp_p->conf[i].reg_data);
        }
        camadjust_flag |= data_code;
    }

    /* ---------------------------------------------------- *
     *  File Close
     * ---------------------------------------------------- */
    filp_close(file_p, NULL);
    set_fs(old_fs);

    ADJ_LOG("[CAMADJ_LOG]Camera AdjustFile ReadEnd\n");
    return;
}
#endif /* FEATURE_KYOCERA_MCAM_BB_ADJ */

static struct msm_sensor_fn_t t4k28_sensor_func_tbl = {
    .sensor_config = t4k28_sensor_config,
    .sensor_power_up = msm_sensor_power_up,
    .sensor_power_down = msm_sensor_power_down,
    .sensor_match_id = msm_sensor_match_id,
};

static struct msm_sensor_ctrl_t t4k28_s_ctrl = {
    .sensor_i2c_client = &t4k28_sensor_i2c_client,
    .power_setting_array.power_setting = t4k28_power_setting,
    .power_setting_array.size = ARRAY_SIZE(t4k28_power_setting),
    .msm_sensor_mutex = &t4k28_mut,
    .sensor_v4l2_subdev_info = t4k28_subdev_info,
    .sensor_v4l2_subdev_info_size = ARRAY_SIZE(t4k28_subdev_info),
    .func_tbl = &t4k28_sensor_func_tbl,
};

module_init(t4k28_init_module);
module_exit(t4k28_exit_module);
MODULE_DESCRIPTION("t4k28");
MODULE_LICENSE("GPL v2");
