/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2011 KYOCERA Corporation
 * (C) 2012 KYOCERA Corporation
 * (C) 2013 KYOCERA Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 and
 *  only version 2 as published by the Free Software Foundation.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/dnand_cdev_driver.h>

#include <linux/dnand_clid.h>
#include "dnand_fs.h"
#include "dnand_fs_mng.h"


static DEFINE_MUTEX(dnand_lock);

static int32_t inter_param_check(
            uint32_t            cid,
            uint32_t            offset,
            uint8_t             *pbuf,
            uint32_t            size )
{
    int32_t  rtn;

    rtn    = DNAND_NO_ERROR;
    if( cid >= DNAND_ID_ENUM_MAX )
    {
        rtn    = DNAND_PARAM_ERROR;
    }
    if( pbuf == NULL )
    {
        rtn    = DNAND_PARAM_ERROR;
    }
    if( size == 0 )
    {
        rtn    = DNAND_PARAM_ERROR;
    }
    return(rtn);
}

int32_t        kdnand_id_read(
                uint32_t            cid,
                uint32_t            offset,
                uint8_t             *pbuf,
                uint32_t            size )
{
    int32_t      rtn;

    rtn    = inter_param_check( cid, offset, pbuf, size );
    if( rtn != DNAND_NO_ERROR )
    {
        if( size == 0 )
        {
            rtn    = DNAND_NO_ERROR;
        }
        return(rtn);
    }

    mutex_lock(&dnand_lock);

    rtn    = dnand_fs_read(cid, offset, pbuf, size);

    mutex_unlock(&dnand_lock);

    return(rtn);
}

int32_t        kdnand_id_write(
                uint32_t            cid,
                uint32_t            offset,
                uint8_t             *pbuf,
                uint32_t            size )
{
    int32_t      rtn;

    rtn    = inter_param_check( cid, offset, pbuf, size );
    if( rtn != DNAND_NO_ERROR )
    {
        if( size == 0 )
        {
            rtn    = DNAND_NO_ERROR;
        }
        return(rtn);
    }

    mutex_lock(&dnand_lock);

    rtn    = dnand_fs_write(cid, offset, pbuf, size);

    mutex_unlock(&dnand_lock);

    return(rtn);
}
