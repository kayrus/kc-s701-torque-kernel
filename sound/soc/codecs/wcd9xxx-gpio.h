/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2012 KYOCERA Corporation
 * (C) 2013 KYOCERA Corporation
 * (C) 2014 KYOCERA Corporation
 */
/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __WCD9XXX_GPIO_H__
#define __WCD9XXX_GPIO_H__

#include <linux/irq.h>
#include <linux/wakelock.h>

enum gpio_hs_det_state {
	HS_DET_STATE_REMOVE,
	HS_DET_STATE_INSERT,
	HS_DET_STATE_INSERT_3PIN,
	HS_DET_STATE_INSERT_4PIN
};

#include "wcd9xxx-mbhc.h"

struct gpio_hs_det_proc_state {
	struct gpio_hs_det_proc_state *next;
	void (*state_enter)(struct wcd9xxx_mbhc *);
	void (*state_exit)(struct wcd9xxx_mbhc *);
	void (*on_gpio_irq)(struct wcd9xxx_mbhc *);
	void (*on_sw_gpio_irq)(struct wcd9xxx_mbhc *);
	void (*on_timeout)(struct wcd9xxx_mbhc *);
};


irqreturn_t gpio_jack_detect_on_gpio_irq(int irq, void *data);
irqreturn_t gpio_btn_detect_on_gpio_irq(int irq, void *data);
void gpio_set_hs_det_proc_state_init(struct wcd9xxx_mbhc *mbhc);
bool gpio_sw_polling_state( void );

#endif /* __WCD9XXX_GPIO_H__ */
