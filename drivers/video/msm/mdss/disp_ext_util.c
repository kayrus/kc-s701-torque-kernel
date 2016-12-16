/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2012 KYOCERA Corporation
 * (C) 2013 KYOCERA Corporation
 * (C) 2014 KYOCERA Corporation
 *
 * drivers/video/msm/disp_ext_util.c
 *
 * Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/msm_mdp.h>
#include "disp_ext.h"
#include "mdss_dsi.h"

static struct local_disp_state_type local_state;

#ifdef CONFIG_DISP_EXT_UTIL_VSYNC
#define DISP_EXT_UTIL_TOTAL_LINE  1651
#define DISP_EXT_UTIL_VSYNC_COUNT  290
#define DISP_EXT_UTIL_VSYNC_OFFSET  364
uint32 disp_ext_util_get_total_line( void )
{
	uint32 total_line;
	total_line = DISP_EXT_UTIL_TOTAL_LINE -1;
    DISP_LOCAL_LOG_EMERG("DISP: %s - total line[%d]\n",__func__,total_line);

	return total_line;
}
uint32 disp_ext_util_get_vsync_count( void )
{
	uint32 cnt;
	cnt = DISP_EXT_UTIL_VSYNC_COUNT;
    DISP_LOCAL_LOG_EMERG("DISP: %s - Vsync count[%d]\n",__func__,cnt);

	return cnt;
}
uint32 disp_ext_util_vsync_cal_start( uint32 start_y )
{
	uint32 cal_start_y;
	cal_start_y = start_y + DISP_EXT_UTIL_VSYNC_OFFSET;
    DISP_LOCAL_LOG_EMERG("DISP: %s - Calibration start_y[%d]\n",__func__,cal_start_y);

	return cal_start_y;
}
#endif /*CONFIG_DISP_EXT_UTIL_VSYNC*/

#ifdef CONFIG_DISP_EXT_UTIL_GET_RATE
#define DISP_EXT_UTIL_REFRESH_RATE  57
uint32 disp_ext_util_get_refresh_rate( void )
{
	uint32 rate;
	rate = DISP_EXT_UTIL_REFRESH_RATE;
    DISP_LOCAL_LOG_EMERG("DISP: %s - Refresh Rate[%d]\n",__func__,rate);

	return rate;
}
#endif /*CONFIG_DISP_EXT_UTIL_GET_RATE*/

#ifdef CONFIG_DISP_UTIL_DSI_CLK_OFF
void disp_ext_util_dsi_clk_off( void )
{
	DISP_LOCAL_LOG_EMERG("%s start\n",__func__);

	local_bh_disable();
	mipi_dsi_clk_disable();
	local_bh_enable();

	MIPI_OUTP(MIPI_DSI_BASE + 0x0000, 0);

	mipi_dsi_phy_ctrl(0);

	local_bh_disable();
	mipi_dsi_ahb_ctrl(0);
	local_bh_enable();

	DISP_LOCAL_LOG_EMERG("%s end\n",__func__);

	return;
}
#endif /*CONFIG_DISP_UTIL_DSI_CLK_OFF*/

local_disp_state_enum disp_ext_util_get_disp_state(void)
{
    return local_state.disp_state;
}

void disp_ext_util_set_disp_state(local_disp_state_enum state)
{
    if( state > LOCAL_DISPLAY_TERM ) {
        return;
    }
    local_state.disp_state = state;
}

struct local_disp_state_type *disp_ext_util_get_disp_info(void)
{
    return &local_state;
}

struct semaphore disp_local_mutex;
void disp_ext_util_disp_local_init( void )
{
    sema_init(&disp_local_mutex,1);
    memset((void *)&local_state, 0, sizeof(struct local_disp_state_type));
}

void disp_ext_util_disp_local_lock( void )
{
    down(&disp_local_mutex);
}

void disp_ext_util_disp_local_unlock( void )
{
    up(&disp_local_mutex);
}

DEFINE_SEMAPHORE(disp_ext_util_mipitx_sem);
void disp_ext_util_mipitx_lock( void )
{
    down(&disp_ext_util_mipitx_sem);
}

void disp_ext_util_mipitx_unlock( void )
{
    up(&disp_ext_util_mipitx_sem);
}

#ifdef CONFIG_DISP_EXT_DIAG
uint32_t disp_ext_util_get_crc_error(void)
{
    return local_state.crc_error_count;
}

void disp_ext_util_set_crc_error(uint32_t count)
{
    local_state.crc_error_count = count;
}

void disp_ext_util_crc_countup(void)
{
    local_state.crc_error_count++;
}
#endif /* CONFIG_DISP_EXT_DIAG */

#ifdef CONFIG_DISP_EXT_PROPERTY
static void update_dsi_cmd(struct dsi_panel_cmds *cmds, char cmd_addr, uint8_t *data, int len)
{
	int i;

	for (i = 0; i < cmds->cmd_cnt; i++) {
		if (cmds->cmds[i].payload[0] == cmd_addr) {
			if (cmds->cmds[i].dchdr.dlen == 1 + len) {
				DISP_LOCAL_LOG_EMERG("%s: %02x found at %d\n", __func__, cmd_addr, i);
				memcpy(&cmds->cmds[i].payload[1], data, len);
			}
		}
	}
}

void disp_ext_util_set_kcjprop(struct mdss_panel_data *pdata, struct fb_kcjprop_data* kcjprop_data)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	DISP_LOCAL_LOG_EMERG("DISP disp_ext_util_set_kcjprop S\n");
	disp_ext_util_disp_local_lock();

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (kcjprop_data->rw_display_cabc_valid == 0) {
		update_dsi_cmd(&ctrl->on2_cmds, 0x55, &kcjprop_data->rw_display_cabc, 1);
		DISP_LOCAL_LOG_EMERG("%s:cabc ext set\n", __func__);
	}

	if (kcjprop_data->rw_display_gamma_valid == 0) {
		update_dsi_cmd(&ctrl->on2_cmds, 0xc7,
			kcjprop_data->rw_display_gamma_r, MSMFB_GAMMA_KCJPROP_DATA_NUM);
		update_dsi_cmd(&ctrl->on2_cmds, 0xc8,
			kcjprop_data->rw_display_gamma_g, MSMFB_GAMMA_KCJPROP_DATA_NUM);
		update_dsi_cmd(&ctrl->on2_cmds, 0xc9,
			kcjprop_data->rw_display_gamma_b, MSMFB_GAMMA_KCJPROP_DATA_NUM);
		DISP_LOCAL_LOG_EMERG("%s:gamma ext set\n", __func__);
	}

#ifdef CONFIG_DISP_EXT_REFRESH
    if( kcjprop_data->rw_display_reflesh_valid == 0) {
        if(kcjprop_data->rw_display_reflesh != 0) {
            disp_ext_reflesh_set_enable(0);
            disp_ext_reflesh_set_start(0);
            DISP_LOCAL_LOG_EMERG("%s:display_reflesh_enable=%d\n", __func__,disp_ext_reflesh_get_enable());
        }
        else {
            disp_ext_reflesh_start();
        }
    }
#endif /* CONFIG_DISP_EXT_REFRESH */

	disp_ext_util_disp_local_unlock();
	DISP_LOCAL_LOG_EMERG("DISP disp_ext_util_set_kcjprop E\n");
}
#endif /* CONFIG_DISP_EXT_PROPERTY */

#define MAIN_LCD_START_X                        0
#define MAIN_LCD_START_Y                        0
#define MAIN_LCD_WIDTH                          720
#define MAIN_LCD_HIGHT                          1280
#define MAIN_LCD_END_X                          (MAIN_LCD_START_X+MAIN_LCD_WIDTH)
#define MAIN_LCD_END_Y                          (MAIN_LCD_START_Y+MAIN_LCD_HIGHT)

/*******************************/
/* SR Compression Format 16bit */
/*******************************/
#define SR_ID_POS 0
#define SR_BIT_DEPTH_POS 2
#define SR_TRANS_FLG_POS 3
#define SR_char_PER_DOT_POS 4
#define SR_PACK_FLG_POS 5
#define SR_IMAGE_WIDTH_POS 6
#define SR_IMAGE_HEIGHT_POS 8
#define SR_TP_COLOR_POS 12
#define SR_PALET_NUM_POS 13
#define SR_TRANS_INDEX_POS 14

#define SR_NO_TRANS 0
#define SR_TRANS 1
#define SR_TRANS_ALPHA 2

#define SR_char_ORDER_MASK 0x01
#define SR_LITTLE_ENDIAN 0x00
#define SR_BIG_ENDIAN 0x01

#define SR_PACK_DIRECTION_MASK 0x02
#define SR_RIGHT_PACK 0x00
#define SR_LEFT_PACK 0x02

#define SR_PACK_LENGTH_MASK 0x04

#define SR_HEADER_SIZE 16

#define BOOT_XSTA_LIMIT(x,left)  (((x) < (left))? (left-x) : (0))
#define BOOT_YPOS_LIMIT(y,top)   (((top) > (y))?  (top-y)  : (0))
#define BOOT_XEND_LIMIT(x,width,right)   (((x+width-1) > (right))?   (right-x)  : (width-1))
#define BOOT_YEND_LIMIT(y,height,bottom) (((y+height-1) > (bottom))? (bottom-y) : (height-1))
#define BOOT_YPOS_ADD(ypos,y,column_size,x) (((ypos) + (y)) * (column_size) + (x))

short work_buf[MAIN_LCD_WIDTH];

void rgb565_to_rgb888(short rgb565, char *out_buf)
{
  char r,g,b;
  r = (rgb565 & 0xF800) >> 8;
  g = (rgb565 & 0x07E0) >> 3;
  b = (rgb565 & 0x001F) << 3;
  out_buf[0] = b;
  out_buf[1] = g;
  out_buf[2] = r;
}

void  kernel_disp_raw_bitmap_SR(int x,
                                int y,
                                int column_size,
                                int line_size,
                                const unsigned char *imageData,
                                char *out_dbuf)
{
  int width,height;
  int cnt;
  int copy_size=0;
  int xsta;
  int xend;
  int xpos;
  int ypos;
  int yend;
  int top,bottom,left,right;
  int ypos_add;
  int image_ix,work_buf_ix,ix;
  int temp_width1, temp_width2;
  char   *output_dbuf = (char *)out_dbuf;

  if(!imageData)return;

  top    = 0;
  left   = 0;
  bottom = line_size   - 1;
  right  = column_size - 1;

  if( !memcmp(imageData,"SR",2) )
  {
    short image_work;
    const char *img_data_Bp = (const char *)imageData;
    short dotParchar;

    dotParchar = imageData[SR_char_PER_DOT_POS];
    width  = (unsigned int)imageData[SR_IMAGE_WIDTH_POS]  + (unsigned int)imageData[SR_IMAGE_WIDTH_POS  + 1] * 256;
    height = (unsigned int)imageData[SR_IMAGE_HEIGHT_POS] + (unsigned int)imageData[SR_IMAGE_HEIGHT_POS + 1] * 256;

    image_ix  = SR_HEADER_SIZE;
    xsta = BOOT_XSTA_LIMIT(x,left);
    xend = BOOT_XEND_LIMIT(x,width,right);
    ypos = BOOT_YPOS_LIMIT(y,top);
    yend = BOOT_YEND_LIMIT(y,height,bottom);
    ypos_add = BOOT_YPOS_ADD(ypos,y,column_size,x);
    copy_size = (xend-xsta+1)*2;
  
    temp_width1 = (width * 2) + 1;
    temp_width2 = width * 2;

    if(dotParchar == 2)
    {
      for( ;ypos <=yend; ypos++)
      {
        if(imageData[image_ix])
        {
            image_ix++;
            work_buf_ix = 0;
            while(1)
            {
              cnt = imageData[image_ix];
              image_ix++;
              image_work = (short)*(img_data_Bp+image_ix)+ (short)(*(img_data_Bp+image_ix+1) << 8 );
              for(ix = 0;ix < cnt;ix++)
              {
                work_buf[work_buf_ix + ix] = image_work;
              }
              image_ix += 2;
              work_buf_ix += cnt;
              if(work_buf_ix >= width)
              {
                break;
              }
            }
        }
        else
        {
            memcpy((char *)work_buf,&imageData[image_ix + 1], temp_width2 );
            image_ix = image_ix + temp_width1;
        }

        for( xpos=xsta; xpos<=xend; xpos++ )
        {
          rgb565_to_rgb888(work_buf[xpos], (char *)&output_dbuf[(ypos_add + xpos) * 3]);
        }
        ypos_add += column_size;
      }
    }
  }
}
