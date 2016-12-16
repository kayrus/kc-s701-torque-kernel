# This software is contributed or developed by KYOCERA Corporation.
# (C) 2014 KYOCERA Corporation
#
#
# Copyright (c) 2010 Yamaha Corporation
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
# USA.
#
ccflags-y := -mfloat-abi=softfp -mfpu=vfp

ccflags-y += -Iinclude/linux

ccflags-y += -DCONFIG_INPUT_YAS_ACCELEROMETER_POSITION=5
ccflags-y += -DCONFIG_INPUT_YAS_MAGNETOMETER_POSITION=7

obj-$(CONFIG_INPUT_YAS_ACCELEROMETER) += yas_acc_kernel_driver.o
obj-$(CONFIG_INPUT_YAS_MAGNETOMETER) += yas_mag_kernel_driver.o
obj-$(CONFIG_INPUT_YAS_ORIENTATION) += yas_ori_kernel_driver.o
obj-$(CONFIG_INPUT_YAS_PRESSURE) += yas_pres_kernel_driver.o
obj-$(CONFIG_INPUT_SENSOR_PROXIMITY) += sfh7776.o
obj-$(CONFIG_INPUT_SENSOR_PRESSURE) += bmp280_core.o
obj-$(CONFIG_INPUT_SENSOR_PRESSURE) += bmp280_i2c.o
obj-$(CONFIG_INPUT_SENSOR_PRESSURE) += bmp280_spi.o
obj-$(CONFIG_INPUT_SENSOR_PRESSURE) += BMP280.o

obj-$(CONFIG_INPUT_YAS_FUSION_SENSOR) += yas_gravity_kernel_driver.o
obj-$(CONFIG_INPUT_YAS_FUSION_SENSOR) += yas_linear_acceleration_kernel_driver.o
obj-$(CONFIG_INPUT_YAS_FUSION_SENSOR) += yas_rotation_vector_kernel_driver.o

obj-$(CONFIG_SENSORS)		+= sensors_class.o
