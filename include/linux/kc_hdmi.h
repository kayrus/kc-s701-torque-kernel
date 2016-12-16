/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2013 KYOCERA Corporation
 *
 * drivers/video/msm/mdss/kc_hdmi.h
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/

#ifndef __LINUX_KC_HDMI_H
#define __LINUX_KC_HDMI_H

#include <linux/ioctl.h>

struct kc_hdmi_aksv {
	unsigned long lsb;
	unsigned long msb;
	int result;
};

#define IOC_MAGIC 'h'
#define IOCTL_GET_HDCP _IOR(IOC_MAGIC, 1, unsigned long)
#define IOCTL_GET_AKSV _IOR(IOC_MAGIC, 2, struct kc_hdmi_aksv)

#endif /* __LINUX_KC_HDMI_H */
