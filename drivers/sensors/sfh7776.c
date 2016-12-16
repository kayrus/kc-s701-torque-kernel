/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2014 KYOCERA Corporation
 */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/wakelock.h>

#include <linux/gpio.h>
#include <mach/vreg.h>
#include <linux/regulator/consumer.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/sfh7776.h>

#define SFH7776_DRV_NAME   "SFH7776"
#define DRIVER_VERSION      "1.0.0"

#define PSALS_DEVICE_NAME "psals_sensor"

#define SFH7776_PROXIMITY_SENSOR_GPIO        (65)

//#define TUNE_DEBUG


//#define SFH7776_ESD_RECOVERY


#define SFH7776_DEBUG        0

#if SFH7776_DEBUG
#define SFH7776_DEBUG_LOG( arg... )   printk(KERN_NOTICE "SFH7776:" arg )
#else
#define SFH7776_DEBUG_LOG( arg... )
#endif
#define SFH7776_NOTICE_LOG( arg... )  printk(KERN_NOTICE "SFH7776:" arg )

#define SFH7776_SYSTEM_CONTROL_REG    0x40
#define SFH7776_MODE_CONTROL_REG      0x41
#define SFH7776_ALS_PS_CONTROL_REG    0x42
#define SFH7776_ALS_PS_STATUS_REG     0x43
#define SFH7776_PS_DATA_LSB_REG       0x44
#define SFH7776_PS_DATA_MSB_REG       0x45
#define SFH7776_ALS_VIS_DATA_LSB_REG  0x46
#define SFH7776_ALS_VIS_DATA_MSB_REG  0x47
#define SFH7776_ALS_IR_DATA_LSB_REG   0x48
#define SFH7776_ALS_IR_DATA_MSB_REG   0x49
#define SFH7776_INTERRUPT_REG         0x4A
#define SFH7776_PS_TH_LSB_REG         0x4B
#define SFH7776_PS_TH_MSB_REG         0x4C
#define SFH7776_PS_TL_LSB_REG         0x4D
#define SFH7776_PS_TL_MSB_REG         0x4E
#define SFH7776_ALS_VIS_TH_LSB_REG    0x4F
#define SFH7776_ALS_VIS_TH_MSB_REG    0x50
#define SFH7776_ALS_VIS_TL_LSB_REG    0x51
#define SFH7776_ALS_VIS_TL_MSB_REG    0x52

#define SFH7776_PS_ENABLE             0x3
#define SFH7776_ALS_ENABLE            0x5
#define SFH7776_ALS_PS_ENABLE         0x6
#define SFH7776_ALS_PS_DISABLE        0x0

#define SFH7776_LUXVALUE_MAX       50000
#define SFH7776_LUXVALUE_TABLE_MAX 50
#define SFH7776_LUX_DELAY_MIN         20

#define SFH7776_DEV_STATUS_INIT            0x00000000
#define SFH7776_DEV_STATUS_SUSPEND         0x00000001
#define SFH7776_DEV_STATUS_SUSPEND_INT     0x00000002

#define SFH7776_INPUT_DUMMY_VALUE   -1

struct sfh7776_data {
    struct i2c_client *client;
    struct mutex update_lock;
    struct delayed_work dwork;
    struct delayed_work    als_dwork;
    struct input_dev *input_dev_als;
    struct input_dev *input_dev_ps;

    struct wake_lock proximity_wake_lock;

    unsigned int system_control;
    unsigned int mode_control;
    unsigned int als_ps_control;
    unsigned int als_ps_status;
    unsigned int interrupt;
    unsigned int ps_th;
    unsigned int ps_tl;
    unsigned int als_vis_th;
    unsigned int als_vis_tl;
    unsigned int ps_data;
    unsigned int als_vis_data;
    unsigned int als_ir_data;

    unsigned int ps_lv;

    unsigned int als_poll_delay;

    int enable_ps_sensor;
    int enable_als_sensor;

    int32_t ps_irq;
};

struct proximity_threshold{
    int lv;
    int drive;
    int hi;
    int lo;
};

static struct i2c_client *client_sfh7776 = NULL;

static atomic_t g_dev_status;
static atomic_t g_ct_val = ATOMIC_INIT(0);

static struct proximity_threshold proximity_thres_tbl[] = {
    {0, 2, 0xffff,20 },
    {1, 2, 22,    17 },
    {2, 2, 19,     7 },
    {3, 2,  8,     3 },
    {4, 2,  4,     0 },
};


const static struct proximity_threshold proximity_thres_tbl_bk[] = {
    {0, 2, 0xffff,20 },
    {1, 2, 22,    17 },
    {2, 2, 19,     7 },
    {3, 2,  8,     3 },
    {4, 2,  4,     0 },
};



static struct proximity_threshold *proximity_thres;
static int proximity_max_level;

static struct regulator* sensor_vdd = 0;

static void sfh7776_reg_init_config(struct i2c_client *client);

static int sfh7776_i2c_read(struct i2c_client *client,u8 addr,u8 *buf,int size)
{
  u8 addr_buf;
  struct i2c_msg msg[2];

  addr_buf = addr;
  
  msg[0].addr  = client->addr;
  msg[0].flags = 0;
  msg[0].len   = 1;
  msg[0].buf   = &addr_buf;
  
  msg[1].addr  = client->addr;
  msg[1].flags = I2C_M_RD;
  msg[1].len   = size;
  msg[1].buf   = buf;

  return i2c_transfer(client->adapter,msg,2);
}
#define SFH7776_I2C_WBUF_SIZE (2)
static int sfh7776_i2c_write(struct i2c_client *client,u8 addr,u8 *buf,int size)
{
  u8 w_buf[SFH7776_I2C_WBUF_SIZE+1];
  struct i2c_msg msg;

  if(size>SFH7776_I2C_WBUF_SIZE)
  {
    printk("%s: data size[%d] exceeds max size[%d]\n", __func__, size,SFH7776_I2C_WBUF_SIZE);
    return -1;
  }
  
  w_buf[0] = addr;
  
  memcpy(&w_buf[1],buf,size);
  
  msg.addr  = client->addr;
  msg.flags = 0;
  msg.len   = 1+size;
  msg.buf   = w_buf;
  
  return i2c_transfer(client->adapter,&msg,1);
}

static int sfh7776_set_system_control(struct sfh7776_data *data, unsigned int v)
{
    u8 buf;
    int ret = 0;
    
    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    data->system_control = v;
    buf = (u8)data->system_control;
    ret = sfh7776_i2c_write(data->client, SFH7776_SYSTEM_CONTROL_REG, &buf, 1);
    return ret;
}

static int sfh7776_set_mode_control(struct sfh7776_data *data, unsigned int v)
{
    u8 buf;
    int ret = 0;
    
    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    data->mode_control = v;
    buf = (u8)data->mode_control;
    ret = sfh7776_i2c_write(data->client, SFH7776_MODE_CONTROL_REG, &buf, 1);
    return ret;
}

static int sfh7776_set_als_ps_control(struct sfh7776_data *data, unsigned int v)
{
    u8 buf;
    int ret = 0;
    
    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    data->als_ps_control = v;
    buf = (u8)data->als_ps_control;
    ret = sfh7776_i2c_write(data->client, SFH7776_ALS_PS_CONTROL_REG, &buf, 1);
    return ret;
}

static int sfh7776_set_als_ps_status(struct sfh7776_data *data, unsigned int v)
{
    u8 buf;
    int ret = 0;
    
    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    data->als_ps_status = v;
    buf = (u8)data->als_ps_status;
    ret = sfh7776_i2c_write(data->client, SFH7776_ALS_PS_STATUS_REG, &buf, 1);
    return ret;
}

static int sfh7776_set_interrupt(struct sfh7776_data *data, unsigned int v)
{
    u8 buf;
    int ret = 0;
    
    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    data->interrupt = v;
    buf = (u8)data->interrupt;
    ret = sfh7776_i2c_write(data->client, SFH7776_INTERRUPT_REG, &buf, 1);
    return ret;
}

static int sfh7776_set_ps_th(struct sfh7776_data *data, unsigned int v)
{
    u8 buf;
    int ret = 0;
    
    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    data->ps_th = v;
    buf = (u8)(data->ps_th & 0x00FF);
    ret = sfh7776_i2c_write(data->client, SFH7776_PS_TH_LSB_REG, &buf, 1);
    if(0 > ret) return ret;
    buf = (u8)(data->ps_th >> 8);
    ret = sfh7776_i2c_write(data->client, SFH7776_PS_TH_MSB_REG, &buf, 1);
    return ret;
}

static int sfh7776_set_ps_tl(struct sfh7776_data *data, unsigned int v)
{
    u8 buf;
    int ret = 0;
    
    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    data->ps_tl = v;
    buf = (u8)(data->ps_tl & 0x00FF);
    ret = sfh7776_i2c_write(data->client, SFH7776_PS_TL_LSB_REG, &buf, 1);
    if(0 > ret) return ret;
    buf = (u8)(data->ps_tl >> 8);
    ret = sfh7776_i2c_write(data->client, SFH7776_PS_TL_MSB_REG, &buf, 1);
    return ret;
}

static int sfh7776_set_als_vis_th(struct sfh7776_data *data, unsigned int v)
{
    u8 buf;
    int ret = 0;
    
    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    data->als_vis_th = v;
    buf = (u8)(data->als_vis_th & 0x00FF);
    ret = sfh7776_i2c_write(data->client, SFH7776_ALS_VIS_TH_LSB_REG, &buf, 1);
    if(0 > ret) return ret;
    buf = (u8)(data->als_vis_th >> 8);
    ret = sfh7776_i2c_write(data->client, SFH7776_ALS_VIS_TH_MSB_REG, &buf, 1);
    return ret;
}

static int sfh7776_set_als_vis_tl(struct sfh7776_data *data, unsigned int v)
{
    u8 buf;
    int ret = 0;
    
    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    data->als_vis_tl = v;
    buf = (u8)(data->als_vis_tl & 0x00FF);
    ret = sfh7776_i2c_write(data->client, SFH7776_ALS_VIS_TL_LSB_REG, &buf, 1);
    if(0 > ret) return ret;
    buf = (u8)(data->als_vis_tl >> 8);
    ret = sfh7776_i2c_write(data->client, SFH7776_ALS_VIS_TL_MSB_REG, &buf, 1);
    return ret;
}

static int sfh7776_get_ps_data(struct sfh7776_data *data, unsigned int *v)
{
    u8 buf[2];
    int ret = 0;

    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    ret = sfh7776_i2c_read(data->client, SFH7776_PS_DATA_LSB_REG, &buf[0], 1);
    if(0 > ret) return ret;
    ret = sfh7776_i2c_read(data->client, SFH7776_PS_DATA_MSB_REG, &buf[1], 1);
    if(0 > ret) return ret;

    *v = (((unsigned int)buf[1]) << 8 | (unsigned int)buf[0]);

    return ret;
}
static int sfh7776_get_als_vis_data(struct sfh7776_data *data, unsigned int *v)
{
    u8 buf[2];
    int ret = 0;

    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    ret = sfh7776_i2c_read(data->client, SFH7776_ALS_VIS_DATA_LSB_REG, &buf[0], 1);
    if(0 > ret) return ret;
    ret = sfh7776_i2c_read(data->client, SFH7776_ALS_VIS_DATA_MSB_REG, &buf[1], 1);
    if(0 > ret) return ret;

    *v = (((unsigned int)buf[1]) << 8 | (unsigned int)buf[0]);

    return ret;
}
static int sfh7776_get_als_ir_data(struct sfh7776_data *data, unsigned int *v)
{
    u8 buf[2];
    int ret = 0;

    SFH7776_DEBUG_LOG("%s(): start\n",__func__);
    ret = sfh7776_i2c_read(data->client, SFH7776_ALS_IR_DATA_LSB_REG, &buf[0], 1);
    if(0 > ret) return ret;
    ret = sfh7776_i2c_read(data->client, SFH7776_ALS_IR_DATA_MSB_REG, &buf[1], 1);
    if(0 > ret) return ret;

    *v = (((unsigned int)buf[1]) << 8 | (unsigned int)buf[0]);

    return ret;
}

static u16 sfh7776_select_control(struct sfh7776_data *data)
{
    u16 command = SFH7776_ALS_PS_DISABLE;
    
    if (data->enable_ps_sensor > 0 && data->enable_als_sensor > 0) {
        command = (data->mode_control & ~0xf) | (SFH7776_ALS_PS_ENABLE & 0xf);
    }
    else if (data->enable_ps_sensor > 0 && data->enable_als_sensor <= 0) {
        command = (data->mode_control & ~0xf) | (SFH7776_PS_ENABLE & 0xf);
    }
    else if (data->enable_ps_sensor <= 0 && data->enable_als_sensor > 0) {
        command = (data->mode_control & ~0xf) | (SFH7776_ALS_ENABLE & 0xf);
    }
    else {
        command = (data->mode_control & ~0xf) | (SFH7776_ALS_PS_DISABLE & 0xf);
    }

    sfh7776_set_mode_control(data, command);
    SFH7776_DEBUG_LOG("%s: command (%d)\n",__func__,command);
    return command;
}

static void sfh7776_proximity_change_data(struct sfh7776_data *data)
{
    int i;

    SFH7776_DEBUG_LOG("START:%s ps_data:%d\n", __func__, data->ps_data);
    
    if(data->ps_data > 0xFFFF || data->ps_data < 0)
    {
      SFH7776_DEBUG_LOG("[prox] %s: invalid param (%d)\n",__func__,data->ps_data);
      return;
    }
    
    for(i=data->ps_lv;i > 0;i--)
    {
      if(data->ps_data >= proximity_thres[i].hi){
        data->ps_lv = i - 1;
        if(proximity_thres[i].drive != proximity_thres[data->ps_lv].drive  ){
          break;
        }
      }
      else {
        break;
      }
    }
    
    for(i=data->ps_lv;i < proximity_max_level;i++)
    {
      if(data->ps_data <= proximity_thres[i].lo){
        data->ps_lv = i + 1;
        if(proximity_thres[i].drive != proximity_thres[data->ps_lv].drive  ){
          break;
        }
      }
      else {
        break;
      }
    }
    
    SFH7776_DEBUG_LOG("END:%s data->ps_lv:%d\n", __func__, data->ps_lv);
}

static void sfh7776_check_error(void)
{
    memcpy(proximity_thres, proximity_thres_tbl_bk, sizeof(proximity_thres_tbl_bk));
    SFH7776_NOTICE_LOG("%s: Set Default ThresholdTable\n",__func__);
}


static void sfh7776_check_ps_thresh(void)
{
    int i;

    SFH7776_DEBUG_LOG("%s: [IN]\n",__func__);
    if(proximity_thres[2].lo <= atomic_read(&g_ct_val))
    {
        SFH7776_NOTICE_LOG("%s: Error thresh[2].lo val:%d, crosstalk val:%d\n",__func__, proximity_thres[2].lo, atomic_read(&g_ct_val));
        sfh7776_check_error();
        SFH7776_DEBUG_LOG("%s: [OUT]\n",__func__);
        return;
    }

    for(i = 0; i <= proximity_max_level; ++i){
        if(proximity_thres[i].hi <= proximity_thres[i].lo){
            SFH7776_NOTICE_LOG("%s: Error thresh[%d] lo val:%d hi val:%d\n",__func__, i, proximity_thres[i].hi, proximity_thres[i].lo);
            sfh7776_check_error();
            SFH7776_DEBUG_LOG("%s: [OUT]\n",__func__);
            return;
        }
        if(i != 0){
            if(proximity_thres[i].hi <= proximity_thres[i-1].lo){
                SFH7776_NOTICE_LOG("%s: Error thresh[%d].hi val:%d thresh[%d].hi val:%d\n",__func__, i, proximity_thres[i].hi, i-1, proximity_thres[i-1].lo);
                sfh7776_check_error();
                SFH7776_DEBUG_LOG("%s: [OUT]\n",__func__);
                return;
            }
        }
    }

    SFH7776_DEBUG_LOG("%s: Check No Error\n",__func__);
    SFH7776_DEBUG_LOG("%s: [OUT]\n",__func__);
}
static void sfh7776_proximity_change_setting(struct sfh7776_data *data)
{
#ifdef TUNE_DEBUG
    sfh7776_set_ps_th(data, 0xFFFF);
    sfh7776_set_ps_tl(data, 0x0000);
    sfh7776_set_als_ps_control(data, data->als_ps_control);
#else
    sfh7776_set_ps_tl(data, proximity_thres[data->ps_lv].lo);
    sfh7776_set_ps_th(data, proximity_thres[data->ps_lv].hi);

    if(data->ps_lv == proximity_max_level) {
        sfh7776_set_interrupt(data, (data->interrupt & ~(0x3<<4)) | (0x1<<4));
    }
    else {
        sfh7776_set_interrupt(data, (data->interrupt & ~(0x3<<4)) | (0x2<<4));
    }

    if((data->als_ps_control & 0x3) != proximity_thres[data->ps_lv].drive) {
        sfh7776_set_als_ps_control(data, 
             (data->als_ps_control & ~0x3) | (proximity_thres[data->ps_lv].drive & 0x3));
    }
#endif
    SFH7776_DEBUG_LOG(":%s int:0x%x ctl:0x%x data:%d lv:%d hi:%d lo:%d\n",
                      __func__,
                      data->interrupt,
                      data->als_ps_control,
                      data->ps_data,
                      data->ps_lv,
                      data->ps_th,
                      data->ps_tl);
}

static void sfh7776_reschedule_work(struct sfh7776_data *data, 
                               struct delayed_work *work, long delay)
{
    unsigned long flags;
    
    spin_lock_irqsave(&data->update_lock.wait_lock, flags);

    __cancel_delayed_work(work);
    if( delay >= 0 ){
        schedule_delayed_work(work, delay);
    }

    spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);
}

static void sfh7776_ps_reschedule_work(struct sfh7776_data *data, long delay)
{
    sfh7776_reschedule_work(data, &data->dwork, delay);
}

static void sfh7776_als_reschedule_work(struct sfh7776_data *data, long delay)
{
    sfh7776_reschedule_work(data, &data->als_dwork, delay);
}

static unsigned long sfh7776_als_lux_conversion(unsigned int vis, unsigned int ir)
{
    long GAIN_VIS = 64;
    long GAIN_IR = 64;
    long GA_A = 218;
    long GA_B = 154;
    long GA_C = 50;
    long GA_D = 700;
    long GA_E = 1540;
    long GA_F = 1000;

    long ratio = (!ir || !vis) ? 0 : 1000 * ir / vis * GAIN_VIS / GAIN_IR;
    long lux = 0;

    if(350 > ratio)
    {
        lux = 11466*vis/GAIN_VIS - 22895*ir/GAIN_IR;
        lux = lux / 1000 * GA_A / 100;
    }
    else if(657 > ratio)
    {
        lux = 5695*vis/GAIN_VIS - 6416*ir/GAIN_IR;
        lux = lux / 1000 * GA_D / 100;
    }
    else if(1015 > ratio)
    {
        lux = 2864*vis/GAIN_VIS - 2104*ir/GAIN_IR;
        lux = lux / 1000 * GA_E / 100;
    }
    else if(1717 > ratio)
    {
        lux = 1782*vis/GAIN_VIS - 1038*ir/GAIN_IR;
        if(10000 > vis || 10000 > ir){
            lux = lux / 1000 * GA_B / 100;
        }
        else {
            lux = lux / 1000 * GA_F / 100;
        }
    }
    else if(2576 > ratio)
    {
        lux = 3564*vis/GAIN_VIS - 1225*ir/GAIN_IR;
        lux = lux / 1000 * GA_C / 100;
    }
    else if(4293 > ratio)
    {
        lux = 7129*vis/GAIN_VIS - 1380*ir/GAIN_IR;
        lux = lux / 1000 * GA_C / 100;
    }
    else
    {
        lux = 14257*vis/GAIN_VIS;
        lux = lux / 1000 * GA_C / 100;
    }

    if(50000 < vis && 50000 < ir)
    {
        lux = 20000;
    }

    if(0 > lux)
    {
        lux = 0;
    }

    SFH7776_DEBUG_LOG("%s: lux=%lu, vis=%d, ir=%d, ratio=%lu\n", 
                      __func__, lux, vis, ir, ratio);
    SFH7776_DEBUG_LOG("%s: GA_A=%lu, GA_B=%lu, GA_C=%lu, GA_D=%lu, GA_E=%lu, GA_F=%lu\n", 
                      __func__, GA_A, GA_B, GA_C, GA_D, GA_E, GA_F);

    return lux;
}

static void sfh7776_als_work_handler(struct work_struct *work)
{
    struct sfh7776_data *data = container_of(work, struct sfh7776_data, als_dwork.work);
    unsigned long luxValue=0;
    
    sfh7776_get_als_vis_data(data, &data->als_vis_data);
    sfh7776_get_als_ir_data(data, &data->als_ir_data);
    
    luxValue = sfh7776_als_lux_conversion(data->als_vis_data, data->als_ir_data);
    
    luxValue = luxValue>0 ? luxValue : 0;
    luxValue = luxValue<SFH7776_LUXVALUE_MAX ? luxValue : SFH7776_LUXVALUE_MAX;
    
    input_report_abs(data->input_dev_als, ABS_MISC, (int)luxValue);
    input_sync(data->input_dev_als);
    SFH7776_DEBUG_LOG("%s: input_report_abs ABS_MISC:%d\n",__func__,(int)luxValue);

    SFH7776_DEBUG_LOG("%s: lux = %d  vis = %d  ir = %d\n", __func__, 
                      (int)luxValue, data->als_vis_data, data->als_ir_data);

    schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(data->als_poll_delay));
}

static void sfh7776_ps_work_handler(struct work_struct *work)
{
    struct sfh7776_data *data = container_of(work, struct sfh7776_data, dwork.work);
    
    sfh7776_get_ps_data(data, &data->ps_data);

#ifdef SFH7776_ESD_RECOVERY
    if(0 == data->ps_data)
    {
        sfh7776_reg_init_config(data->client);
    }
    else
#endif
    {
        sfh7776_check_ps_thresh();

        sfh7776_proximity_change_data(data);

        sfh7776_proximity_change_setting(data);

#ifdef TUNE_DEBUG
        printk("@@@tune_proximity drvstr:%d raw data :%d\n",(data->als_ps_control & 0x3),data->ps_data);
        input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_data);
#else
        input_report_abs(data->input_dev_ps, ABS_DISTANCE, SFH7776_INPUT_DUMMY_VALUE);
        input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_lv);
#endif /* TUNE_DEBUG */
        input_sync(data->input_dev_ps);
        SFH7776_NOTICE_LOG("%s: input_report_abs ABS_DISTANCE:%d\n",__func__,data->ps_lv);
    }

    sfh7776_select_control(data);
    sfh7776_set_system_control(data, data->system_control | (0x1<<6));
    
    SFH7776_DEBUG_LOG("%s: ps_data:%d\n",__func__,data->ps_data);

#ifdef TUNE_DEBUG
    if(data->enable_ps_sensor > 0)
        sfh7776_ps_reschedule_work(data, msecs_to_jiffies(200));
#else
    enable_irq(data->ps_irq);
#endif /* TUNE_DEBUG */
}

static irqreturn_t sfh7776_interrupt(int vec, void *info)
{
    struct i2c_client *client=(struct i2c_client *)info;
    struct sfh7776_data *data = i2c_get_clientdata(client);
    uint32_t dev_status = 0;

    SFH7776_DEBUG_LOG("==> sfh7776_interrupt\n");

    disable_irq_nosync(data->ps_irq);
    dev_status = atomic_read(&g_dev_status);
    if( dev_status & SFH7776_DEV_STATUS_SUSPEND )
    {
        atomic_set(&g_dev_status, dev_status|SFH7776_DEV_STATUS_SUSPEND_INT);
    } else 
    {
        sfh7776_ps_reschedule_work(data, 0);
    }
    wake_lock_timeout(&data->proximity_wake_lock, 2 * HZ);

    return IRQ_HANDLED;
}

static int32_t sfh7776_ps_irq_cnt = 0;
static void sfh7776_enable_ps_irq( struct sfh7776_data *data )
{
    SFH7776_DEBUG_LOG("[IN]%s sfh7776_ps_irq_cnt=%d\n",__func__,sfh7776_ps_irq_cnt);
    if( sfh7776_ps_irq_cnt <= 0 )
    {
        SFH7776_DEBUG_LOG("enable_irq\n");
        enable_irq(data->ps_irq);
        enable_irq_wake(data->ps_irq);
        sfh7776_ps_irq_cnt++;
    }
    SFH7776_DEBUG_LOG("[OUT]%s\n",__func__);
}
static void sfh7776_disable_ps_irq( struct sfh7776_data *data )
{
    SFH7776_DEBUG_LOG("[IN]%s sfh7776_ps_irq_cnt=%d\n",__func__,sfh7776_ps_irq_cnt);
    if( sfh7776_ps_irq_cnt > 0 )
    {
        sfh7776_ps_irq_cnt--;
        disable_irq(data->ps_irq);
        disable_irq_wake(data->ps_irq);
        SFH7776_DEBUG_LOG("disable_irq\n");
    }
    SFH7776_DEBUG_LOG("[OUT]%s\n",__func__);
}


static ssize_t sfh7776_show_enable_ps_sensor(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sfh7776_data *data = i2c_get_clientdata(client);
    
    return sprintf(buf, "%d\n", data->enable_ps_sensor);
}

static ssize_t sfh7776_store_enable_ps_sensor(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sfh7776_data *data = i2c_get_clientdata(client);
    unsigned long val = simple_strtoul(buf, NULL, 10);
    static atomic_t serial = ATOMIC_INIT(0);

    if ((val != 0) && (val != 1)) {
        SFH7776_DEBUG_LOG("%s:store unvalid value=%ld\n", __func__, val);
        return count;
    }
    
    if(val == 1) {
        data->enable_ps_sensor++;
        if (data->enable_ps_sensor<=1) {
            data->enable_ps_sensor=1;
            sfh7776_check_ps_thresh();
#ifdef SFH7776_ESD_RECOVERY
            sfh7776_reg_init_config(client);
#endif 
            input_report_abs(data->input_dev_ps, ABS_MISC, atomic_inc_return(&serial));
            SFH7776_DEBUG_LOG("%s: input_report_abs ABS_MISC:%d\n",__func__,atomic_read(&serial));

            data->ps_lv = proximity_max_level;
            sfh7776_set_ps_th(data, 0x0000);
            sfh7776_set_ps_tl(data, 0xFFFF);
            sfh7776_set_interrupt(data, (data->interrupt & ~(0x3<<4)) | (0x2<<4));
            sfh7776_set_als_ps_control(data, 
               (data->als_ps_control & ~0x3) | (proximity_thres[proximity_max_level].drive & 0x3));

            sfh7776_select_control(data);

            sfh7776_enable_ps_irq(data);
#ifdef TUNE_DEBUG
            sfh7776_ps_reschedule_work(data, msecs_to_jiffies(200));
#endif /* TUNE_DEBUG */
        }
    } 
    else {
        data->enable_ps_sensor--;
        
        if (data->enable_ps_sensor<=0) {
            sfh7776_disable_ps_irq(data);

            sfh7776_select_control(data);
            sfh7776_set_system_control(data, data->system_control | (0x1<<6));

            if(data->enable_als_sensor > 0)
            {
                sfh7776_als_reschedule_work(data, msecs_to_jiffies(data->als_poll_delay));
            }
        }
    }
    SFH7776_DEBUG_LOG("%s: val=%ld, enable_ps_sensor=%d\n",__func__, val, data->enable_ps_sensor);
    
    return count;
}

static DEVICE_ATTR(enable_ps_sensor, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP,
                   sfh7776_show_enable_ps_sensor, sfh7776_store_enable_ps_sensor);





static ssize_t sfh7776_store_ps_crosstalk(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sfh7776_data *data = i2c_get_clientdata(client);
    unsigned long val = simple_strtoul(buf, NULL, 10);
    unsigned long flags;
    SFH7776_DEBUG_LOG("%s[IN]\n", __func__);
    SFH7776_DEBUG_LOG("%s: crosstalk val=%ld\n", __func__, val);
    spin_lock_irqsave(&data->update_lock.wait_lock, flags);
    if(val <= 5){
        atomic_set(&g_ct_val, (int)val);
        proximity_thres[2].lo = (int)val + 2;
        proximity_thres[3].lo = (int)val + 1;
        proximity_thres[4].hi = (int)val + 2;
        SFH7776_DEBUG_LOG("%s: Lev2OFF thresh=%d, Lev3OFF thresh=%d, Lev4ON thresh=%d\n",
                            __func__, proximity_thres[2].lo, proximity_thres[3]. lo,proximity_thres[4].hi);
        sfh7776_check_ps_thresh();
    }
    spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);
    SFH7776_DEBUG_LOG("%s[OUT]\n", __func__);

    return count;
}

static ssize_t sfh7776_show_ps_crosstalk(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    SFH7776_DEBUG_LOG("%s[IN]\n", __func__);
    SFH7776_DEBUG_LOG("%s[OUT]\n", __func__);
    return sprintf(buf, "%d %d %d %d %d %d %d %d %d %d\n",
                    proximity_thres[4].hi, proximity_thres[3].hi, proximity_thres[2].hi, proximity_thres[1].hi, proximity_thres[0].hi,
                    proximity_thres[4].lo, proximity_thres[3].lo, proximity_thres[2].lo, proximity_thres[1].lo, proximity_thres[0].lo);
}


static DEVICE_ATTR(ps_crosstalk, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP,
                   sfh7776_show_ps_crosstalk, sfh7776_store_ps_crosstalk);



static ssize_t sfh7776_show_enable_als_sensor(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sfh7776_data *data = i2c_get_clientdata(client);
    
    return sprintf(buf, "%d\n", data->enable_als_sensor);
}

static ssize_t sfh7776_store_enable_als_sensor(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sfh7776_data *data = i2c_get_clientdata(client);
    unsigned long val = simple_strtoul(buf, NULL, 10);
    static atomic_t serial = ATOMIC_INIT(0);
    
    if ((val != 0) && (val != 1))
    {
        SFH7776_DEBUG_LOG("%s: enable als sensor=%ld\n", __func__, val);
        return count;
    }
    
    if(val == 1) {
        data->enable_als_sensor++;
        if (data->enable_als_sensor<=1) {
            data->enable_als_sensor = 1;

#ifdef SFH7776_ESD_RECOVERY
            sfh7776_reg_init_config(client);
#endif
            input_report_abs(data->input_dev_als, ABS_VOLUME, atomic_inc_return(&serial));
            SFH7776_DEBUG_LOG("%s: input_report_abs ABS_VOLUME:%d\n",__func__,atomic_read(&serial));

            sfh7776_select_control(data);
            
            sfh7776_als_reschedule_work(data, msecs_to_jiffies(data->als_poll_delay));
        }
    }
    else {
        data->enable_als_sensor--;
        if( data->enable_als_sensor <= 0){

            sfh7776_select_control(data);
        
            sfh7776_als_reschedule_work(data, -1);
        }
    }
    SFH7776_DEBUG_LOG("%s: val=%ld, enable_als_sensor=%d\n",__func__, val, data->enable_als_sensor);
    
    return count;
}

static DEVICE_ATTR(enable_als_sensor, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP,
                   sfh7776_show_enable_als_sensor, sfh7776_store_enable_als_sensor);

static ssize_t sfh7776_show_als_poll_delay(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sfh7776_data *data = i2c_get_clientdata(client);
    
    return sprintf(buf, "%d\n", data->als_poll_delay*1000);
}

static ssize_t sfh7776_store_als_poll_delay(struct device *dev,
                    struct device_attribute *attr, const char *buf, size_t count)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sfh7776_data *data = i2c_get_clientdata(client);
    unsigned long val = simple_strtoul(buf, NULL, 10);
    
    data->als_poll_delay = val/1000;
    
    sfh7776_als_reschedule_work(data, msecs_to_jiffies(data->als_poll_delay));

    return count;
}

static DEVICE_ATTR(als_poll_delay, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP,
                   sfh7776_show_als_poll_delay, sfh7776_store_als_poll_delay);

static struct attribute *sfh7776_ps_attributes[] = {
    &dev_attr_enable_ps_sensor.attr,
    &dev_attr_ps_crosstalk.attr,
    NULL
};

static struct attribute *sfh7776_als_attributes[] = {
    &dev_attr_enable_als_sensor.attr,
    &dev_attr_als_poll_delay.attr,
    NULL
};

static const struct attribute_group sfh7776_ps_attr_group = {
    .attrs = sfh7776_ps_attributes,
};

static const struct attribute_group sfh7776_als_attr_group = {
    .attrs = sfh7776_als_attributes,
};

#ifdef TUNE_DEBUG
static ssize_t
proximity_drvstr_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sfh7776_data *data = i2c_get_clientdata(client);

    printk("%s: drive strength:%d\n",__func__,(data->als_ps_control & 0x3));
    return sprintf(buf, "%d\n", (data->als_ps_control & 0x3));
}
static ssize_t
proximity_drvstr_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sfh7776_data *data = i2c_get_clientdata(client);
    unsigned long val = simple_strtoul(buf, NULL, 10);

    if(count > 2) return count;

    if((0x00 <= val && val <= 0x03))
    {
        if(val != proximity_thres[data->ps_lv].drive) {
            sfh7776_set_als_ps_control(data, (data->als_ps_control & ~0x3) | (val & 0x3));
        }
        printk("%s: proximity led drive strength:%d\n",__func__,(data->als_ps_control & 0x3));
    }
    else
    {
        printk("%s: invalid  argument %ld\n",__func__,val);
    }
    return count;
}

static DEVICE_ATTR(drvstr, S_IRUGO|S_IWUGO, proximity_drvstr_show, proximity_drvstr_store);

static struct attribute *proximity_tune_debug_attributes[] = {
    &dev_attr_drvstr.attr,
    NULL
};

static struct attribute_group proximity_tune_debug_attribute_group = {
    .attrs = proximity_tune_debug_attributes
};
#endif /* TUNE_DEBUG */

static int sfh7776_open(struct inode *inode_type, struct file *file)
{
    SFH7776_DEBUG_LOG("[IN]sfh7776_open\n");
    SFH7776_DEBUG_LOG("[OUT]sfh7776_open\n");
    return 0;
}

static int sfh7776_release(struct inode *inode_type, struct file *file)
{
    SFH7776_DEBUG_LOG("[IN]sfh7776_release\n");
    SFH7776_DEBUG_LOG("[OUT]sfh7776_release\n");
    return 0;
}

static long sfh7776_ioctl(struct file *file_type, 
                          unsigned int unCmd, unsigned long ulArg)
{
    int32_t nRet = -EINVAL;

    T_PSALS_IOCTL_PS_DATA ps_data_type;
    struct sfh7776_data *data = i2c_get_clientdata(client_sfh7776);

    SFH7776_DEBUG_LOG("[IN]sfh7776_ioctl\n");
    
    memset((void*)&ps_data_type, 0, sizeof(T_PSALS_IOCTL_PS_DATA) );

    switch( unCmd )
    {
        case IOCTL_PS_DATA_GET:
            SFH7776_DEBUG_LOG("IOCTL_PS_DATA_GET START\n" );
            ps_data_type.ulps_data = data->ps_data;
                 SFH7776_DEBUG_LOG("error : ps_data=%d\n", data->ps_data );
            nRet = copy_to_user((void *)(ulArg),
                     &ps_data_type, sizeof(T_PSALS_IOCTL_PS_DATA) );
            if (nRet) {
                SFH7776_DEBUG_LOG("error : sfh7776_ioctl(unCmd = T_PSALS_IOCTL_PS_DATA)\n" );
                return -EFAULT;
            }
            SFH7776_DEBUG_LOG("IOCTL_PS_DATA_GET END\n" );
            break;
        case IOCTL_PS_FACTORY_DATA_SET:
            SFH7776_DEBUG_LOG("IOCTL_PS_FACTORY_DATA_SET START\n" );
            nRet = 0;
            break;
        default:
            break;
    }
    SFH7776_DEBUG_LOG("[OUT]sfh7776_ioctl nRet=%d\n",nRet);

    return nRet;
}

static struct file_operations sfh7776_fops = {
    .owner = THIS_MODULE,
    .open = sfh7776_open,
    .release = sfh7776_release,
    .unlocked_ioctl = sfh7776_ioctl,
};

static struct miscdevice sfh7776_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = PSALS_DEVICE_NAME,
    .fops = &sfh7776_fops,
};

static void sfh7776_reg_init_config(struct i2c_client *client)
{
    struct sfh7776_data *data = i2c_get_clientdata(client);
    
    SFH7776_DEBUG_LOG("%s(): start\n",__func__);

    sfh7776_set_system_control(data, 0x09);
    sfh7776_set_mode_control(data, 0x10);
    sfh7776_set_als_ps_control(data, 0x2A);
    sfh7776_set_als_ps_status(data, 0x01);
    sfh7776_set_interrupt(data, 0x21);
    
    sfh7776_set_ps_th(data, 0x0000);
    sfh7776_set_ps_tl(data, 0xFFFF);
    sfh7776_set_als_vis_th(data, 0xFFFF);
    sfh7776_set_als_vis_tl(data, 0x0000);
}

static int sfh7776_init_client(struct i2c_client *client)
{
    struct sfh7776_data *data = i2c_get_clientdata(client);

    SFH7776_DEBUG_LOG("[IN]%s\n",__func__);

    proximity_thres = (struct proximity_threshold *)proximity_thres_tbl;
    proximity_max_level = ARRAY_SIZE(proximity_thres_tbl) - 1;

    data->ps_lv = proximity_max_level;
    data->als_poll_delay = 250;
    
    data->enable_ps_sensor = 0;
    data->enable_als_sensor = 0;

    data->ps_data = 0;
    data->als_vis_data = 0;
    data->als_ir_data = 0;

    sfh7776_reg_init_config(client);

    SFH7776_DEBUG_LOG("[OUT]%s\n",__func__);

    return 0;
}


static struct i2c_driver sfh7776_driver;
static int __devinit sfh7776_probe(struct i2c_client *client,
                   const struct i2c_device_id *id)
{
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    struct sfh7776_data *data;
    int err = 0;
    uint32_t min_uV=0,max_uV=0,load_uA=0;

    SFH7776_DEBUG_LOG("[IN]%s\n",__func__);

    of_property_read_u32(client->dev.of_node, "light-prox-vdd-min-voltage", &min_uV);
    of_property_read_u32(client->dev.of_node, "light-prox-vdd-max-voltage", &max_uV);
    of_property_read_u32(client->dev.of_node, "light-prox-vdd-load-current", &load_uA);
    printk(KERN_NOTICE "[ALS_PS]%s regulator min_uV = %d, max_uV = %d, load_uA = %d\n",
        __func__, min_uV, max_uV, load_uA);

    sensor_vdd = regulator_get(&client->dev, "light-prox-vdd");
    if( IS_ERR(sensor_vdd) ) {
        printk(KERN_ERR "[ALS_PS]%s regulator_get fail.\n", __func__);
        err = PTR_ERR(sensor_vdd);
        goto exit;
    }

    err = regulator_set_voltage(sensor_vdd, min_uV, max_uV);
    if( err ) {
        printk(KERN_ERR "[ALS_PS]%s regulator_set_voltage fail. err=%d\n", __func__, err);
        goto exit_regu;
    }

    err = regulator_set_optimum_mode(sensor_vdd, load_uA);
    if( err < 0 ) {
        printk(KERN_ERR "[ALS_PS]%s regulator_set_optimum_mode fail. err=%d\n", __func__, err);
        goto exit_regu;
    }

    err = regulator_enable(sensor_vdd);
    if( err ) {
        printk(KERN_ERR "[ALS_PS]%s regulator_enable fail. err=%d\n", __func__, err);
        goto exit_regu;
    }

    usleep_range(5000,5000);

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
        err = -EIO;
        goto exit_vdd;
    }

    data = kzalloc(sizeof(struct sfh7776_data), GFP_KERNEL);
    if (!data) {
        printk("%s: Failed kzalloc\n",__func__);
        err = -ENOMEM;
        goto exit_vdd;
    }
    data->client = client;
    i2c_set_clientdata(client, data);

    data->ps_irq = gpio_to_irq(SFH7776_PROXIMITY_SENSOR_GPIO);
    
    err = gpio_request(SFH7776_PROXIMITY_SENSOR_GPIO, SFH7776_DRV_NAME);
    SFH7776_DEBUG_LOG("gpio_request err=%d\n",err);
    if (err < 0) {
        SFH7776_DEBUG_LOG("[%s] failed to request GPIO=%d, ret=%d\n",
               __func__,
               SFH7776_PROXIMITY_SENSOR_GPIO,
               err);
        goto exit_kfree;
    }
    err = gpio_direction_input(SFH7776_PROXIMITY_SENSOR_GPIO);
    SFH7776_DEBUG_LOG("gpio_direction_input err=%d\n",err);
    if (err < 0) {
        SFH7776_DEBUG_LOG("[%s] failed to configure direction for GPIO=%d, ret=%d\n",
               __func__,
               SFH7776_PROXIMITY_SENSOR_GPIO,
               err);
        goto exit_kfree;
    }

    INIT_DELAYED_WORK(&data->dwork, sfh7776_ps_work_handler);
    INIT_DELAYED_WORK(&data->als_dwork, sfh7776_als_work_handler);
 
    mutex_init(&data->update_lock);

    wake_lock_init(&data->proximity_wake_lock, WAKE_LOCK_SUSPEND, "proximity");


    err = sfh7776_init_client(client);
    if (err)
    {
        SFH7776_DEBUG_LOG("Failed sfh7776_init_client\n");
        goto exit_free_irq;
    }


    data->input_dev_als = input_allocate_device();
    if (!data->input_dev_als) {
        err = -ENOMEM;
        SFH7776_DEBUG_LOG("Failed to allocate input device als\n");
        goto exit_free_irq;
    }

    data->input_dev_ps = input_allocate_device();
    if (!data->input_dev_ps) {
        err = -ENOMEM;
        SFH7776_DEBUG_LOG("Failed to allocate input device ps\n");
        goto exit_free_dev_als;
    }

    set_bit(EV_ABS, data->input_dev_als->evbit);
    set_bit(EV_ABS, data->input_dev_ps->evbit);

    input_set_abs_params(data->input_dev_als, ABS_MISC, 0, SFH7776_LUXVALUE_MAX, 0, 0);
    input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, proximity_max_level, 0, 0);
    input_set_capability(data->input_dev_ps, EV_ABS, ABS_MISC);
    input_set_capability(data->input_dev_als, EV_ABS, ABS_VOLUME);

    data->input_dev_als->name = "light";
    data->input_dev_ps->name = "proximity";

    err = input_register_device(data->input_dev_als);
    if (err) {
        err = -ENOMEM;
        SFH7776_DEBUG_LOG("Unable to register input device als: %s\n",
               data->input_dev_als->name);
        goto exit_free_dev_ps;
    }

    err = input_register_device(data->input_dev_ps);
    if (err) {
        err = -ENOMEM;
        SFH7776_DEBUG_LOG("Unable to register input device ps: %s\n",
               data->input_dev_ps->name);
        goto exit_unregister_dev_als;
    }

    input_set_drvdata(data->input_dev_ps, data);
    if( 0 != sysfs_create_group(&data->input_dev_ps->dev.kobj, &sfh7776_ps_attr_group)) {
        printk("%s: Failed sysfs_create_group for ps.\n",__func__);
        goto exit_unregister_dev_ps;
    }

    input_set_drvdata(data->input_dev_als, data);
    if( 0 != sysfs_create_group(&data->input_dev_als->dev.kobj, &sfh7776_als_attr_group)) {
        printk("%s: Failed sysfs_create_group for als.\n",__func__);
        goto exit_unregister_dev_ps;
    }
#ifdef TUNE_DEBUG
    input_set_drvdata(data->input_dev_ps, data);
    err = sysfs_create_group(&data->input_dev_ps->dev.kobj, &proximity_tune_debug_attribute_group);
    if (err) {
        printk("%s: Failed sysfs_create_group for tune debug\n",__func__);
    }
#endif /* TUNE_DEBUG */


    device_init_wakeup(&client->dev, 1);
    atomic_set(&g_dev_status, SFH7776_DEV_STATUS_INIT);
    
    err = misc_register(&sfh7776_device);
    if (err)
    {
        SFH7776_DEBUG_LOG(KERN_ERR
               "sfh7776_probe: sfh7776 register failed\n");
        goto exit_sysfs_remove;
    }
    SFH7776_DEBUG_LOG("%s support ver. %s enabled\n", __func__, DRIVER_VERSION);
    err = request_any_context_irq(data->ps_irq, sfh7776_interrupt, IRQ_TYPE_LEVEL_LOW,
        SFH7776_DRV_NAME, (void *)client);
    SFH7776_DEBUG_LOG("request_any_context_irq err=%d\n",err);
    if(err < 0) {
        SFH7776_DEBUG_LOG("%s Could not allocate SFH7776_INT(%d) ! err=%d\n",
                 __func__,SFH7776_PROXIMITY_SENSOR_GPIO,err);
        goto exit_sysfs_remove;
    }
    disable_irq(data->ps_irq);

    SFH7776_DEBUG_LOG("%s interrupt is hooked\n", __func__);
    client_sfh7776 = client;
    
    SFH7776_DEBUG_LOG("[OUT]%s\n",__func__);
    return 0;

exit_sysfs_remove:
    sysfs_remove_group(&client->dev.kobj, &sfh7776_ps_attr_group);
    sysfs_remove_group(&client->dev.kobj, &sfh7776_als_attr_group);
exit_unregister_dev_ps:
    input_unregister_device(data->input_dev_ps);    
exit_unregister_dev_als:
    input_unregister_device(data->input_dev_als);
exit_free_dev_ps:
    input_free_device(data->input_dev_ps);
exit_free_dev_als:
    input_free_device(data->input_dev_als);
exit_free_irq:
    mutex_destroy(&data->update_lock);
    wake_lock_destroy(&data->proximity_wake_lock);
    free_irq(data->ps_irq, client);
exit_kfree:
    kfree(data);
exit_vdd:
    regulator_disable(sensor_vdd);
exit_regu:
    if( sensor_vdd ) {
        regulator_set_optimum_mode(sensor_vdd, 0);
        regulator_put(sensor_vdd);
        sensor_vdd = 0;
    }
exit:
    SFH7776_DEBUG_LOG("[OUT]%s err=%d\n",__func__,err);
    return err;
}

static int __devexit sfh7776_remove(struct i2c_client *client)
{
    struct sfh7776_data *data = i2c_get_clientdata(client);
    
    input_unregister_device(data->input_dev_als);
    input_unregister_device(data->input_dev_ps);
    
    input_free_device(data->input_dev_als);
    input_free_device(data->input_dev_ps);

    free_irq(data->ps_irq, client);

    sysfs_remove_group(&client->dev.kobj, &sfh7776_ps_attr_group);
    sysfs_remove_group(&client->dev.kobj, &sfh7776_als_attr_group);
    misc_deregister(&sfh7776_device);

    data->enable_ps_sensor = 0;
    data->enable_als_sensor = 0;
    
    sfh7776_select_control(data);

    kfree(data);

    if( sensor_vdd ) {
        regulator_disable(sensor_vdd);
        regulator_set_optimum_mode(sensor_vdd, 0);
        regulator_put(sensor_vdd);
        sensor_vdd = 0;
    }

    return 0;
}

#ifdef CONFIG_PM

static int sfh7776_suspend(struct i2c_client *client, pm_message_t mesg)
{
    SFH7776_DEBUG_LOG("[IN]%s\n", __func__);
    atomic_set(&g_dev_status, SFH7776_DEV_STATUS_SUSPEND);
    SFH7776_DEBUG_LOG("[OUT]%s\n", __func__);
    return 0;
}

static int sfh7776_resume(struct i2c_client *client)
{
    struct sfh7776_data *data = i2c_get_clientdata(client);
    SFH7776_DEBUG_LOG("[IN]%s\n", __func__);
    
    if(atomic_read(&g_dev_status) & SFH7776_DEV_STATUS_SUSPEND_INT)
    {
        sfh7776_ps_reschedule_work(data, 0);
    }
    
    atomic_set(&g_dev_status, SFH7776_DEV_STATUS_INIT);
    SFH7776_DEBUG_LOG("[OUT]%s\n", __func__);
    return 0;
}

#else

#define sfh7776_suspend    NULL
#define sfh7776_resume     NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id sfh7776_id[] = {
    { SFH7776_DRV_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, sfh7776_id);

static struct of_device_id sfh7776_match_table[] = {
	{ .compatible = SFH7776_DRV_NAME,},
	{ },
};

static struct i2c_driver sfh7776_driver = {
    .driver = {
        .name   = SFH7776_DRV_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = sfh7776_match_table,
    },
    .suspend = sfh7776_suspend,
    .resume = sfh7776_resume,
    .probe  = sfh7776_probe,
    .remove = __devexit_p(sfh7776_remove),
    .id_table = sfh7776_id,
};

static int __init sfh7776_init(void)
{
    int32_t rc;
    
    SFH7776_DEBUG_LOG("[IN]%s\n",__func__);
    rc = i2c_add_driver(&sfh7776_driver);
    if (rc != 0) {
        SFH7776_DEBUG_LOG("can't add i2c driver\n");
        rc = -ENOTSUPP;
        return rc;
    }

    SFH7776_DEBUG_LOG("[OUT]%s\n",__func__);
    return rc;
}

static void __exit sfh7776_exit(void)
{
    i2c_del_driver(&sfh7776_driver);
    
    i2c_unregister_device(client_sfh7776);
    client_sfh7776 = NULL;
}

MODULE_DESCRIPTION("SFH7776 ambient light + proximity sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(sfh7776_init);
module_exit(sfh7776_exit);
