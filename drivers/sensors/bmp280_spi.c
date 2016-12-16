/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2014 KYOCERA Corporation
 */
/*!
 * @section LICENSE
 *
 * (C) Copyright 2013 Bosch Sensortec GmbH All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *------------------------------------------------------------------------------
 * Disclaimer
 *
 * Common: Bosch Sensortec products are developed for the consumer goods
 * industry. They may only be used within the parameters of the respective valid
 * product data sheet.  Bosch Sensortec products are provided with the express
 * understanding that there is no warranty of fitness for a particular purpose.
 * They are not fit for use in life-sustaining, safety or security sensitive
 * systems or any system or device that may lead to bodily harm or property
 * damage if the system or device malfunctions. In addition, Bosch Sensortec
 * products are not fit for use in products which interact with motor vehicle
 * systems.  The resale and/or use of products are at the purchaser's own risk
 * and his own responsibility. The examination of fitness for the intended use
 * is the sole responsibility of the Purchaser.
 *
 * The purchaser shall indemnify Bosch Sensortec from all third party claims,
 * including any claims for incidental, or consequential damages, arising from
 * any product use not covered by the parameters of the respective valid product
 * data sheet or not approved by Bosch Sensortec and reimburse Bosch Sensortec
 * for all costs in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products,
 * particularly with regard to product safety and inform Bosch Sensortec without
 * delay of all security relevant incidents.
 *
 * Engineering Samples are marked with an asterisk (*) or (e). Samples may vary
 * from the valid technical specifications of the product series. They are
 * therefore not intended or fit for resale to third parties or for use in end
 * products. Their sole purpose is internal client testing. The testing of an
 * engineering sample may in no way replace the testing of a product series.
 * Bosch Sensortec assumes no liability for the use of engineering samples. By
 * accepting the engineering samples, the Purchaser agrees to indemnify Bosch
 * Sensortec from all claims arising from the use of engineering samples.
 *
 * Special: This software module (hereinafter called "Software") and any
 * information on application-sheets (hereinafter called "Information") is
 * provided free of charge for the sole purpose to support your application
 * work. The Software and Information is subject to the following terms and
 * conditions:
 *
 * The Software is specifically designed for the exclusive use for Bosch
 * Sensortec products by personnel who have special experience and training. Do
 * not use this Software if you do not have the proper experience or training.
 *
 * This Software package is provided `` as is `` and without any expressed or
 * implied warranties, including without limitation, the implied warranties of
 * merchantability and fitness for a particular purpose.
 *
 * Bosch Sensortec and their representatives and agents deny any liability for
 * the functional impairment of this Software in terms of fitness, performance
 * and safety. Bosch Sensortec and their representatives and agents shall not be
 * liable for any direct or indirect damages or injury, except as otherwise
 * stipulated in mandatory applicable law.
 *
 * The Information provided is believed to be accurate and reliable. Bosch
 * Sensortec assumes no responsibility for the consequences of use of such
 * Information nor for any infringement of patents or other rights of third
 * parties which may result from its use.
 *
 * @filename bmp280_spi.c
 * @date	 "Mon Jun 3 16:16:29 2013 +0800"
 * @id		 "be67fe4"
 *
 * @brief
 * This file implements moudle function, which add
 * the driver to SPI core.
*/

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include "bmp280_core.h"
#include "bs_log.h"

/*! @defgroup bmp280_spi_src
 *  @brief bmp280 spi driver module
 @{*/
/*! the maximum of retry during SPI transfer */
#define BMP_MAX_RETRY_SPI_XFER      10
/*! delay time between two error transfers */
#define BMP_SPI_WRITE_DELAY_TIME    1

static struct spi_device *bmp_spi_client;

/*!
 * @brief define spi wirte function
 *
 * @param dev_addr sensor device address
 * @param reg_addr register address
 * @param data the pointer of data buffer
 * @param len block size need to write
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static char bmp_spi_write_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	struct spi_device *client = bmp_spi_client;
	u8 buffer[2];
	u8 retry;

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < BMP_MAX_RETRY_SPI_XFER; retry++) {
			if (spi_write(client, buffer, 2) >= 0)
				break;
			else
				mdelay(BMP_SPI_WRITE_DELAY_TIME);
		}
		if (BMP_MAX_RETRY_SPI_XFER <= retry) {
			PERR("SPI xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	return 0;
}

/*!
 * @brief define spi read function
 *
 * @param dev_addr sensor device address
 * @param reg_addr register address
 * @param data the pointer of data buffer
 * @param len block size need to read
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static char bmp_spi_read_block(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	struct spi_device *client = bmp_spi_client;
	u8 buffer[2] = {reg_addr, 0};
	int status = spi_write(client, buffer, 2);
	if (status < 0)
		return status;

	return spi_read(client, data, len);
}

/*!
 * @brief spi bus operation
*/
static const struct bmp_bus_ops bmp_spi_bus_ops = {
	/**< spi block write pointer */
	.bus_write	= bmp_spi_write_block,
	/**< spi block read pointer */
	.bus_read	= bmp_spi_read_block
};

/*!
 * @brief BMP probe function via spi bus
 *
 * @param client the pointer of spi client
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int __devinit bmp_spi_probe(struct spi_device *client)
{
	int status;
	struct bmp_data_bus data_bus = {
		.bops = &bmp_spi_bus_ops,
		.client = client
	};

	if (NULL == bmp_spi_client)
		bmp_spi_client = client;
	else{
		PERR("This driver does not support multiple clients!\n");
		return -EINVAL;
	}

	client->bits_per_word = 8;
	status = spi_setup(client);
	if (status < 0) {
		PERR("spi_setup failed!\n");
		return status;
	}

	return bmp_probe(&client->dev, &data_bus);
}

/*!
 * @brief shutdown bmp device in spi driver
 *
 * @param client the pointer of spi client
 *
 * @return no return value
*/
static void bmp_spi_shutdown(struct spi_device *client)
{
#ifdef CONFIG_PM
	bmp_disable(&client->dev);
#endif
}

/*!
 * @brief remove bmp spi client
 *
 * @param client the pointer of spi client
 *
 * @return zero
 * @retval zero
*/
static int bmp_spi_remove(struct spi_device *client)
{
	return bmp_remove(&client->dev);
}

#ifdef CONFIG_PM
/*!
 * @brief suspend bmp device in spi driver
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
static int bmp_spi_suspend(struct device *dev)
{
	return bmp_disable(dev);
}

/*!
 * @brief resume bmp device in spi driver
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
static int bmp_spi_resume(struct device *dev)
{
	return bmp_enable(dev);
}

/*!
 * @brief register spi device power manager hooks
*/
static const struct dev_pm_ops bmp_spi_pm_ops = {
	/**< device suspend */
	.suspend = bmp_spi_suspend,
	/**< device resume */
	.resume  = bmp_spi_resume
};
#endif

/*!
 * @brief register spi device id
*/
static const struct spi_device_id bmp_id[] = {
	{ BMP_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, bmp_id);

static struct of_device_id bmp_match_table[] = {
	{ .compatible = BMP_NAME,},
	{ },
};

/*!
 * @brief register spi driver hooks
*/
static struct spi_driver bmp_spi_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = BMP_NAME,
#ifdef CONFIG_PM
		.pm = &bmp_spi_pm_ops,
#endif
		.of_match_table = bmp_match_table,
	},
	.id_table = bmp_id,
	.probe    = bmp_spi_probe,
	.shutdown = bmp_spi_shutdown,
	.remove   = __devexit_p(bmp_spi_remove)
};

/*!
 * @brief initialize bmp spi module
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int __init bmp_spi_init(void)
{
	return spi_register_driver(&bmp_spi_driver);
}

/*!
 * @brief remove bmp spi module
 *
 * @return no return value
*/
static void __exit bmp_spi_exit(void)
{
	spi_unregister_driver(&bmp_spi_driver);
}


MODULE_DESCRIPTION("BMP280 SPI BUS DRIVER");
MODULE_LICENSE("GPL");

module_init(bmp_spi_init);
module_exit(bmp_spi_exit);
/*@}*/
