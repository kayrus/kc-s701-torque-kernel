/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2012 KYOCERA Corporation
 * (C) 2013 KYOCERA Corporation
 * (C) 2014 KYOCERA Corporation
 *
 * Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/module.h>
#include <linux/kernel.h>

#include "mdss_dsi.h"
#include "disp_ext.h"

#define DETECT_BOARD_NUM 5

static char reg_read_cmd[1] = {0xa1};
static struct dsi_cmd_desc dsi_cmds = {
	{DTYPE_GEN_READ1, 1, 0, 1, 0, sizeof(reg_read_cmd)},
	reg_read_cmd
};
static char panel_id[4] = {0x01, 0x2A, 0x45, 0x91};

static int panel_detection = 0;       /* -1:not panel 0:Not test 1:panel found */

int disp_ext_board_detect_board(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i;
	int panel_found;
	struct dcs_cmd_req cmdreq;
	char rbuf[10];

	if( panel_detection != 0 ) {
		pr_debug("%s: panel Checked\n", __func__);
		return panel_detection;
	}

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dsi_cmds;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = 10;
	cmdreq.cb = NULL;
	cmdreq.rbuf = rbuf;

	panel_found = 0;
	for (i = 0; i < DETECT_BOARD_NUM; i++) {
		mdss_dsi_cmdlist_put(ctrl, &cmdreq);
		pr_info("%s - panel ID: %02x %02x %02x %02x\n", __func__,
			rbuf[4], rbuf[5], rbuf[6], rbuf[7]);
		if (rbuf[4] == panel_id[0]
		&&  rbuf[5] == panel_id[1]
		&&  rbuf[6] == panel_id[2]
		&&  rbuf[7] == panel_id[3]) {
			panel_found = 1;
			break;
		}
	}
	if (panel_found != 1) {
		pr_err("%s: panel not found\n", __func__);
		panel_detection = -1;
		return panel_detection;
	}
	pr_info("%s: panel found\n", __func__);
	panel_detection = 1;
	return panel_detection;
}

int disp_ext_board_get_panel_detect(void)
{
    return panel_detection;
}
