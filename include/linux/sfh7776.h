/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2013 KYOCERA Corporation
 */
#ifndef SFH7776_H
#define SFH7776_H

#include <linux/ioctl.h>

typedef struct _t_psals_ioctl_ps_detection
{
    unsigned long ulps_detection;
}T_PSALS_IOCTL_PS_DETECTION;

typedef struct _t_psals_ioctl_als_mean_times
{
    unsigned long ulals_mean_times;
}T_PSALS_IOCTL_ALS_MEAN_TIMES;

typedef struct _t_psals_ioctl_als_lux_ave
{
    unsigned long ulals_lux_ave;
    long lcdata;
    long lirdata;
}T_PSALS_IOCTL_ALS_LUX_AVE;

typedef struct _t_psals_ioctl_ps_data
{
    unsigned long ulps_data;
}T_PSALS_IOCTL_PS_DATA;

#define PSALS_IO             'A'

#define IOCTL_PS_DETECTION_GET        _IOWR(PSALS_IO, 0x01, T_PSALS_IOCTL_PS_DETECTION)
#define IOCTL_ALS_MEAN_TIMES_SET      _IOWR(PSALS_IO, 0x02, T_PSALS_IOCTL_ALS_MEAN_TIMES)
#define IOCTL_ALS_LUX_AVE_GET         _IOWR(PSALS_IO, 0x03, T_PSALS_IOCTL_ALS_LUX_AVE)
#define IOCTL_PSALS_NV_DATA_SET       _IOWR(PSALS_IO, 0x04, T_PSALS_IOCTL_NV)
#define IOCTL_PSALS_NV_DATA_GET       _IOWR(PSALS_IO, 0x05, T_PSALS_IOCTL_NV)
#define IOCTL_PS_DATA_GET             _IOWR(PSALS_IO, 0xA0, T_PSALS_IOCTL_PS_DATA)
#define IOCTL_PS_FACTORY_DATA_SET     _IOWR(PSALS_IO, 0xA1, T_PSALS_IOCTL_PS_DATA)

#endif /* SFH7776_H */
