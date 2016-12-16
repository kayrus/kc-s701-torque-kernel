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

#include "dnand_fs.h"
#include "dnand_fs_mng.h"
#include "dnand_drv.h"

static uint32_t             dnand_fs_init = 0;
static dnand_fs_memng       dnand_fsinfo[DNAND_FS_SYSTEM_NUM];
static uint32_t             dnand_alloc_start = DNAND_FS_BLK_DATA_ST;

static void inter_init_fsinfo(void)
{
    memset( (void*)&dnand_fsinfo[0], 0xff, sizeof(dnand_fsinfo) );
    return;
}

static void inter_cpy_info( dnand_fs_flmng *pbuf )
{
    if( pbuf == NULL )
    {
        return;
    }

    memcpy( &dnand_fsinfo[0].clid[0], &pbuf->clid[0], sizeof(dnand_fsinfo[0].clid) );
    memcpy( &dnand_fsinfo[0].fat[0], &pbuf->fat[0], sizeof(dnand_fsinfo[0].fat) );
    memcpy( &dnand_fsinfo[1], &dnand_fsinfo[0], sizeof(dnand_fsinfo[1]) );
    return;
}

static int32_t dnand_blk_read(
            uint32_t          blk_no,
            uint32_t          offset,
            uint8_t           *pbuf,
            uint32_t          size )
{
    int32_t       rtn;
    uint32_t      s_sect, e_sect, n_sect, offset_sect, rest_sect;
    uint32_t      r_sect, c_size;
    uint8_t       *p_kbuf;

    s_sect        = (blk_no*DNAND_FS_BLK_SECT_NUM) + (offset/DNAND_DRV_SECTOR_BLK_SIZE);
    e_sect        = (offset+size+DNAND_DRV_SECTOR_BLK_SIZE-1)/DNAND_DRV_SECTOR_BLK_SIZE;
    e_sect        += (blk_no*DNAND_FS_BLK_SECT_NUM);
    n_sect        = e_sect-s_sect;

    offset_sect   = offset%DNAND_DRV_SECTOR_BLK_SIZE;
    rest_sect     = (offset+size)%DNAND_DRV_SECTOR_BLK_SIZE;

    p_kbuf        = NULL;
    p_kbuf       = (uint8_t*)kmalloc(DNAND_DRV_SECTOR_BLK_SIZE, GFP_KERNEL);
    if( p_kbuf == NULL )
    {
        return(DNAND_NOMEM_ERROR);
    }

    if( offset_sect > 0 )
    {
        r_sect       = 1;
        rtn          = dnand_drv_read(s_sect, r_sect, p_kbuf);
        if( rtn != DNAND_NO_ERROR )
        {
            kfree(p_kbuf);
            return(rtn);
        }
        if( (offset_sect+size) > DNAND_DRV_SECTOR_BLK_SIZE )
        {
            c_size    = DNAND_DRV_SECTOR_BLK_SIZE-offset_sect;
        }
        else
        {
            c_size    = size;
        }
        memcpy( pbuf, (p_kbuf+offset_sect), c_size );
        pbuf          += c_size;
        size          -= c_size;
        s_sect        += r_sect;
        n_sect        -= r_sect;
    }

    if( n_sect == 0 )
    {
        kfree(p_kbuf);
        return(DNAND_NO_ERROR);
    }

    if( rest_sect > 0 )
    {
        r_sect       = n_sect -1;
    }
    else
    {
        r_sect       = n_sect;
    }

    if( r_sect > 0 )
    {
        rtn          = dnand_drv_read(s_sect, r_sect, pbuf);
        if( rtn != DNAND_NO_ERROR )
        {
            kfree(p_kbuf);
            return(rtn);
        }
        c_size        = r_sect*DNAND_DRV_SECTOR_BLK_SIZE;
        pbuf          += c_size;
        size          -= c_size;
        s_sect        += r_sect;
        n_sect        -= r_sect;
    }

    if( n_sect > 0 )
    {
        r_sect       = 1;
        rtn          = dnand_drv_read(s_sect, r_sect, p_kbuf);
        if( rtn != DNAND_NO_ERROR )
        {
            kfree(p_kbuf);
            return(rtn);
        }
        memcpy( pbuf, p_kbuf, size );
    }
    kfree(p_kbuf);
    return(DNAND_NO_ERROR);
}

static int32_t dnand_blk_write(
            uint32_t          blk_no,
            uint32_t          offset,
            uint8_t           *pbuf,
            uint32_t          size )
{
    int32_t       rtn;
    uint32_t      s_sect, e_sect, n_sect, offset_sect, rest_sect;
    uint32_t      rw_sect, c_size;
    uint8_t       *p_kbuf;

    s_sect        = (blk_no*DNAND_FS_BLK_SECT_NUM) + (offset/DNAND_DRV_SECTOR_BLK_SIZE);
    e_sect        = (offset+size+DNAND_DRV_SECTOR_BLK_SIZE-1)/DNAND_DRV_SECTOR_BLK_SIZE;
    e_sect        += (blk_no*DNAND_FS_BLK_SECT_NUM);
    n_sect        = e_sect-s_sect;

    offset_sect   = offset%DNAND_DRV_SECTOR_BLK_SIZE;
    rest_sect     = (offset+size)%DNAND_DRV_SECTOR_BLK_SIZE;

    p_kbuf       = (uint8_t*)kmalloc(DNAND_DRV_SECTOR_BLK_SIZE, GFP_KERNEL);
    if( p_kbuf == NULL )
    {
        return(DNAND_NOMEM_ERROR);
    }

    if( offset_sect > 0 )
    {
        rw_sect       = 1;
        rtn          = dnand_drv_read(s_sect, rw_sect, p_kbuf);
        if( rtn != DNAND_NO_ERROR )
        {
            kfree(p_kbuf);
            return(rtn);
        }
        if( (offset_sect+size) > DNAND_DRV_SECTOR_BLK_SIZE )
        {
            c_size    = DNAND_DRV_SECTOR_BLK_SIZE-offset_sect;
        }
        else
        {
            c_size    = size;
        }
        memcpy( (p_kbuf+offset_sect), pbuf, c_size );
        rtn          = dnand_drv_write(s_sect, rw_sect, p_kbuf);
        if( rtn != DNAND_NO_ERROR )
        {
            kfree(p_kbuf);
            return(rtn);
        }

        pbuf          += c_size;
        size          -= c_size;
        s_sect        += rw_sect;
        n_sect        -= rw_sect;
    }

    if( n_sect == 0 )
    {
        kfree(p_kbuf);
        return(DNAND_NO_ERROR);
    }

    if( rest_sect > 0 )
    {
        rw_sect       = n_sect -1;
    }
    else
    {
        rw_sect       = n_sect;
    }

    if( rw_sect > 0 )
    {
        rtn          = dnand_drv_write(s_sect, rw_sect, pbuf);
        if( rtn != DNAND_NO_ERROR )
        {
            kfree(p_kbuf);
            return(rtn);
        }
        c_size        = rw_sect*DNAND_DRV_SECTOR_BLK_SIZE;
        pbuf          += c_size;
        size          -= c_size;
        s_sect        += rw_sect;
        n_sect        -= rw_sect;
    }

    if( n_sect > 0 )
    {
        rw_sect       = 1;
        rtn          = dnand_drv_read(s_sect, rw_sect, p_kbuf);
        if( rtn != DNAND_NO_ERROR )
        {
            kfree(p_kbuf);
            return(rtn);
        }
        c_size    = size;
        memcpy( p_kbuf, pbuf, size );
        rtn          = dnand_drv_write(s_sect, rw_sect, p_kbuf);
        if( rtn != DNAND_NO_ERROR )
        {
            kfree(p_kbuf);
            return(rtn);
        }
    }
    kfree(p_kbuf);
    return(DNAND_NO_ERROR);
}

static int32_t update_flmng(
            uint32_t              sysno,
            dnand_fs_flmng        *p_flmng )
{
    uint32_t      blk_no;
    int32_t       rtn;

    if( p_flmng == NULL )
    {
        return(DNAND_INTERNAL_ERROR);
    }
    if( sysno == 0 )
    {
        blk_no    = 0;
    }
    else
    {
        blk_no    = DNAND_FS_BLK_SYS_NUM;
    }

    memset( &p_flmng->ckcd[0], 0xFF, sizeof(p_flmng->ckcd) );
    memset( &p_flmng->dmya[0], 0xFF, sizeof(p_flmng->dmya) );

    rtn        = dnand_blk_write( blk_no, 0, (uint8_t*)p_flmng, DNAND_FS_BLK_SIZE );
    if( rtn != DNAND_NO_ERROR )
    {
        return(rtn);
    }

    memset( &p_flmng->clid[0], 0xFF, sizeof(p_flmng->clid) );
    memcpy( &p_flmng->clid[0], &dnand_fsinfo[1].clid[0], sizeof(dnand_fsinfo[1].clid) );
    memset( &p_flmng->fat[0], 0xFF, sizeof(p_flmng->fat) );
    memcpy( &p_flmng->fat[0], &dnand_fsinfo[1].fat[0], sizeof(dnand_fsinfo[1].fat) );

    rtn        = dnand_blk_write( blk_no, 0, (uint8_t*)p_flmng, sizeof(dnand_fs_flmng) );
    if( rtn != DNAND_NO_ERROR )
    {
        return(rtn);
    }

    strcpy( (char*)&p_flmng->ckcd[0], DNAND_FS_CKCD );

    rtn        = dnand_blk_write( blk_no, 0, (uint8_t*)p_flmng, DNAND_FS_BLK_SIZE );
    return(rtn);
}

static void inter_memng_clr( uint32_t  cid )
{
    uint32_t      i;
    uint16_t      *pfat0, *pfat1;
    uint16_t      blk0, blk1, nxtblk0, nxtblk1;

    if( cid >= DNAND_FS_CLID_NUM )
    {
        return;
    }

    blk0     = (uint16_t)dnand_fsinfo[0].clid[cid];
    blk1     = (uint16_t)dnand_fsinfo[1].clid[cid];
    pfat0    = &dnand_fsinfo[0].fat[0];
    pfat1    = &dnand_fsinfo[1].fat[0];
    nxtblk0  = DNAND_FS_BLK_UNUSE;
    nxtblk1  = DNAND_FS_BLK_UNUSE;

    dnand_fsinfo[1].clid[cid]    = dnand_fsinfo[0].clid[cid];

    for( i=0; i<DNAND_FS_BLK_NUM; i++ )
    {
        if( (blk0 >= DNAND_FS_BLK_NUM)&&(blk1 >= DNAND_FS_BLK_NUM) )
        {
            break;
        }

        if( blk0 < DNAND_FS_BLK_NUM )
        {
            nxtblk0    = pfat0[blk0];
        }
        if( blk1 < DNAND_FS_BLK_NUM )
        {
            nxtblk1    = pfat1[blk1];
        }
        if( blk0 != blk1 )
        {
            if( blk1 < DNAND_FS_BLK_NUM )
            {
                pfat0[blk1]    = DNAND_FS_BLK_UNUSE;
                pfat1[blk1]    = DNAND_FS_BLK_UNUSE;
            }
            if( blk0 < DNAND_FS_BLK_NUM )
            {
                pfat1[blk0]    = nxtblk0;
            }
        }
        else
        {
            if( blk0 < DNAND_FS_BLK_NUM )
            {
                pfat1[blk0]    = nxtblk0;
            }
        }

        if( nxtblk0 < DNAND_FS_BLK_NUM )
        {
            blk0               = nxtblk0;
        }
        else
        {
            blk0               = DNAND_FS_BLK_UNUSE;
        }
        if( nxtblk1 < DNAND_FS_BLK_NUM )
        {
            blk1               = nxtblk1;
        }
        else
        {
            blk1               = DNAND_FS_BLK_UNUSE;
        }
    }
    return;
}

static int32_t update_memng( uint32_t  cid )
{
    int32_t            rtn;
    uint8_t            *p_kbuf;

    if( cid >= DNAND_FS_CLID_NUM )
    {
        return(DNAND_INTERNAL_ERROR);
    }
    inter_memng_clr( cid );

    p_kbuf    = (uint8_t*)kmalloc(sizeof(dnand_fs_flmng), GFP_KERNEL);
    if( p_kbuf == NULL )
    {
        return(DNAND_NOMEM_ERROR);
    }
    rtn       = update_flmng( 1, (dnand_fs_flmng*)p_kbuf );
    if( rtn != DNAND_NO_ERROR )
    {
        kfree(p_kbuf);
        return(rtn);
    }
    rtn       = update_flmng( 0, (dnand_fs_flmng*)p_kbuf );
    kfree(p_kbuf);
    return(rtn);
}

static int32_t pre_init(void)
{
    int32_t            rtn, cmprtn;
    int32_t            flg_valid[DNAND_FS_SYSTEM_NUM];
    uint8_t            *p_kbuf0;
    uint8_t            *p_kbuf1;
    dnand_fs_flmng     *p_flmng0;
    dnand_fs_flmng     *p_flmng1;

    if( dnand_fs_init == DNAND_INITIALIZED )
    {
        return(DNAND_NO_ERROR);
    }

    dnand_alloc_start = DNAND_FS_BLK_DATA_ST;

    p_kbuf0    = (uint8_t*)kmalloc(sizeof(dnand_fs_flmng), GFP_KERNEL);
    if( p_kbuf0 == NULL )
    {
        return(DNAND_NOMEM_ERROR);
    }
    rtn        = dnand_blk_read( 0, 0, p_kbuf0, sizeof(dnand_fs_flmng) );
    if( rtn != DNAND_NO_ERROR )
    {
        kfree(p_kbuf0);
        return(rtn);
    }

    p_kbuf1    = (uint8_t*)kmalloc(sizeof(dnand_fs_flmng), GFP_KERNEL);
    if( p_kbuf1 == NULL )
    {
        kfree(p_kbuf0);
        return(DNAND_NOMEM_ERROR);
    }
    rtn        = dnand_blk_read( DNAND_FS_BLK_SYS_NUM, 0, p_kbuf1, sizeof(dnand_fs_flmng) );
    if( rtn != DNAND_NO_ERROR )
    {
        kfree(p_kbuf0);
        kfree(p_kbuf1);
        return(rtn);
    }

    p_flmng0        = (dnand_fs_flmng*)p_kbuf0;
    p_flmng1        = (dnand_fs_flmng*)p_kbuf1;
    flg_valid[0]    = DNAND_FALSE;
    flg_valid[1]    = DNAND_FALSE;
    rtn             = DNAND_NO_ERROR;

    cmprtn          = strcmp( (char*)p_flmng0->ckcd, DNAND_FS_CKCD );
    if( cmprtn == 0 )
    {
        flg_valid[0]    = DNAND_TRUE;
    }
    cmprtn          = strcmp( (char*)p_flmng1->ckcd, DNAND_FS_CKCD );
    if( cmprtn == 0 )
    {
        flg_valid[1]    = DNAND_TRUE;
    }

    if( (flg_valid[0] != DNAND_FALSE)&&(flg_valid[0] == flg_valid[1]) )
    {
       inter_cpy_info( p_flmng0 );
       cmprtn       = memcmp( p_flmng0, p_flmng1, sizeof(dnand_fs_flmng) );
       if( cmprtn != 0 )
       {
           rtn    = update_flmng( 1, p_flmng0 );
       }
    }
    else if( flg_valid[0] != DNAND_FALSE )
    {
        inter_cpy_info( p_flmng0 );
        rtn       = update_flmng( 1, p_flmng0 );
    }
    else if( flg_valid[1] != DNAND_FALSE )
    {
        inter_cpy_info( p_flmng1 );
        rtn       = update_flmng( 0, p_flmng1 );
    }
    else
    {
        inter_init_fsinfo();
        rtn       = update_flmng( 0, p_flmng0 );
        if( rtn == DNAND_NO_ERROR )
        {
            rtn       = update_flmng( 1, p_flmng0 );
        }
    }

    kfree(p_kbuf0);
    kfree(p_kbuf1);
    if( rtn == DNAND_NO_ERROR )
    {
        dnand_fs_init = DNAND_INITIALIZED;
    }
    return(rtn);
}

static uint16_t get_clid( uint32_t  cid )
{
    uint16_t      blk;

    if( cid >= DNAND_FS_CLID_NUM )
    {
        return(DNAND_FS_BLK_UNUSE);
    }
    blk    = (uint16_t)dnand_fsinfo[0].clid[cid];
    if( (blk < DNAND_FS_BLK_DATA_ST)||(blk >= DNAND_FS_BLK_NUM) )
    {
        blk    = DNAND_FS_BLK_UNUSE;
    }
    return(blk);
}

static void set_clid( uint32_t  cid, uint16_t  pos )
{
    uint32_t      blk;

    blk        = pos;
    if( cid >= DNAND_FS_CLID_NUM )
    {
        return;
    }
    if( (blk < DNAND_FS_BLK_DATA_ST)||(blk >= DNAND_FS_BLK_NUM) )
    {
        blk    = DNAND_FS_BLK_UNUSE;
    }

    dnand_fsinfo[0].clid[cid]    = blk;
    return;
}

static uint16_t get_fat( uint16_t  blk )
{
    uint16_t      nxtblk;

    if( (blk < DNAND_FS_BLK_DATA_ST)||(blk >= DNAND_FS_BLK_NUM) )
    {
        nxtblk    = DNAND_FS_BLK_UNUSE;
    }
    else
    {
        nxtblk    = (uint16_t)dnand_fsinfo[0].fat[blk];
    }
    return(nxtblk);
}

static void set_fat( uint16_t  pos, uint16_t  blk )
{
    if( (pos < DNAND_FS_BLK_DATA_ST)||(pos >= DNAND_FS_BLK_NUM) )
    {
        return;
    }
    dnand_fsinfo[0].fat[pos]    = blk;
    return;
}

static uint16_t alloc_fat( void )
{
    uint32_t      i, blk;
    uint16_t      rtnblk, tmpblk;

    if( (dnand_alloc_start<DNAND_FS_BLK_DATA_ST) ||
        (dnand_alloc_start>=DNAND_FS_BLK_NUM) )
    {
        dnand_alloc_start = DNAND_FS_BLK_DATA_ST;
    }

    rtnblk    = DNAND_FS_BLK_UNUSE;
    for( i=DNAND_FS_BLK_DATA_ST; i<DNAND_FS_BLK_NUM; i++ )
    {
        blk    = dnand_fsinfo[0].fat[dnand_alloc_start];
        tmpblk = dnand_alloc_start;

        dnand_alloc_start++;
        if( dnand_alloc_start >= DNAND_FS_BLK_NUM )
        {
            dnand_alloc_start = DNAND_FS_BLK_DATA_ST;
        }

        if( blk == DNAND_FS_BLK_UNUSE )
        {
            rtnblk    = tmpblk;
            break;
        }
    }
    if( (rtnblk < DNAND_FS_BLK_DATA_ST)||(rtnblk >= DNAND_FS_BLK_NUM) )
    {
        rtnblk    = DNAND_FS_BLK_UNUSE;
    }
    return(rtnblk);
}

static int32_t inter_fs_read(
            uint16_t            blk,
            uint32_t            offset,
            uint8_t             *pbuf,
            uint32_t            size )
{
    int32_t       rtn;
    uint32_t      i, cnt, c_size;

    rtn       = DNAND_NO_ERROR;
    if( size == 0 )
    {
        return(rtn);
    }

    cnt       = (offset+size+DNAND_FS_BLK_SIZE-1)/DNAND_FS_BLK_SIZE;
    for( i=0; i<cnt; i++ )
    {
        if( offset >= DNAND_FS_BLK_SIZE )
        {
            offset    -= DNAND_FS_BLK_SIZE;
            blk       = get_fat( blk );
            continue;
        }
        if( (offset+size) >= DNAND_FS_BLK_SIZE )
        {
            c_size    = DNAND_FS_BLK_SIZE-offset;
        }
        else
        {
            c_size    = size;
        }
        if( (blk < DNAND_FS_BLK_DATA_ST)||(blk >= DNAND_FS_BLK_NUM) )
        {
            rtn       = DNAND_EOF_ERROR;
            break;
        }
        else
        {
            rtn    = dnand_blk_read( blk, offset, pbuf, c_size );
            if( rtn != DNAND_NO_ERROR )
            {
                break;
            }
        }
        offset    = 0;
        size      -= c_size;
        pbuf      += c_size;
        blk       = get_fat( blk );
    }
    return(rtn);
}

static int32_t inter_fs_write(
            uint32_t            cid,
            uint16_t            blk,
            uint32_t            offset,
            uint8_t             *pbuf,
            uint32_t            size )
{
    int32_t       rtn;
    uint32_t      i, cnt, c_size;
    uint16_t      a_blk, prevblk, nextblk;
    uint8_t       *p_kbuf;

    if( size == 0 )
    {
        return(DNAND_NO_ERROR);
    }
    if( cid >= DNAND_FS_CLID_NUM )
    {
        return(DNAND_PARAM_ERROR);
    }
    if( ( (blk<DNAND_FS_BLK_DATA_ST)||(blk>=DNAND_FS_BLK_NUM) )&&
        (blk != DNAND_FS_BLK_UNUSE) )
    {
        return(DNAND_MNG_ERROR);
    }

    rtn       = DNAND_NO_ERROR;
    cnt       = (offset+size+DNAND_FS_BLK_SIZE-1)/DNAND_FS_BLK_SIZE;
    a_blk  = alloc_fat();
    if( a_blk >= DNAND_FS_BLK_NUM )
    {
        return(DNAND_NOSPC_ERROR);
    }

    prevblk   = DNAND_FS_BLK_UNUSE;
    p_kbuf    = (uint8_t*)kmalloc(DNAND_FS_BLK_SIZE, GFP_KERNEL);
    if( p_kbuf == NULL )
    {
        return(DNAND_NOMEM_ERROR);
    }

    for( i=0; i<cnt; i++ )
    {
        if( offset >= DNAND_FS_BLK_SIZE )
        {
            if( (blk>=DNAND_FS_BLK_DATA_ST)&&(blk<DNAND_FS_BLK_NUM) )
            {
                offset    -= DNAND_FS_BLK_SIZE;
                prevblk   = blk;
                blk       = get_fat( prevblk );
                continue;
            }
            memset(p_kbuf, 0x00, DNAND_FS_BLK_SIZE);
            rtn           = dnand_blk_write( a_blk, 0, p_kbuf, DNAND_FS_BLK_SIZE );
            offset        -= DNAND_FS_BLK_SIZE;
        }
        else
        {
            if( (offset+size) >= DNAND_FS_BLK_SIZE )
            {
                c_size    = DNAND_FS_BLK_SIZE-offset;
            }
            else
            {
                c_size    = size;
            }

            if( c_size < DNAND_FS_BLK_SIZE )
            {
                if( (blk>=DNAND_FS_BLK_DATA_ST)&&(blk<DNAND_FS_BLK_NUM) )
                {
                    rtn    = dnand_blk_read( blk, 0, p_kbuf, DNAND_FS_BLK_SIZE );
                    if( rtn == DNAND_NO_ERROR )
                    {
                        memcpy( p_kbuf+offset, pbuf, c_size );
                        rtn    = dnand_blk_write( a_blk, 0, p_kbuf, DNAND_FS_BLK_SIZE );
                    }
                }
                else
                {
                    memset( p_kbuf, 0x00, DNAND_FS_BLK_SIZE );
                    memcpy( p_kbuf+offset, pbuf, c_size );
                    rtn    = dnand_blk_write( a_blk, 0, p_kbuf, DNAND_FS_BLK_SIZE );
                }
            }
            else
            {
                rtn    = dnand_blk_write( a_blk, 0, pbuf, c_size );
            }
            offset = 0;
            size   -= c_size;
            pbuf   += c_size;
        }
        if( rtn != DNAND_NO_ERROR )
        {
            break;
        }

        nextblk   = get_fat( blk );
        blk       = nextblk;
        if(  ((nextblk >= DNAND_FS_BLK_DATA_ST)&&(nextblk < DNAND_FS_BLK_NUM)) ||
             (nextblk == DNAND_FS_BLK_EOF) )
        {
            set_fat( a_blk, nextblk );
        }
        else
        {
            set_fat( a_blk, DNAND_FS_BLK_EOF );
        }

        if( (prevblk >= DNAND_FS_BLK_DATA_ST)&&(prevblk < DNAND_FS_BLK_NUM) )
        {
            set_fat( prevblk, a_blk );
        }
        if( i == 0 )
        {
            set_clid( cid, a_blk );
        }
        prevblk   = a_blk;

        if( (i+1) < cnt )
        {
            a_blk     = alloc_fat();
            if( a_blk >= DNAND_FS_BLK_NUM )
            {
                rtn    = DNAND_NOSPC_ERROR;
                break;
            }
        }
    }

    kfree(p_kbuf);
    return(rtn);
}

int32_t        dnand_fs_read(
                uint32_t            cid,
                uint32_t            offset,
                uint8_t             *pbuf,
                uint32_t            size )
{
    int32_t      rtn;
    uint16_t     blk;

    if( pbuf == NULL )
    {
        return(DNAND_PARAM_ERROR);
    }

    if( cid >= DNAND_ID_ENUM_MAX )
    {
        return(DNAND_PARAM_ERROR);
    }
    if( cid >= DNAND_FS_CLID_NUM )
    {
        return(DNAND_PARAM_ERROR);
    }

    memset( pbuf, 0x00, size );

    rtn    = pre_init();
    if( rtn != DNAND_NO_ERROR )
    {
        return(rtn);
    }

    blk    = get_clid( cid );
    if( blk < DNAND_FS_BLK_DATA_ST )
    {
        return(DNAND_MNG_ERROR);
    }
    if( blk >= DNAND_FS_BLK_NUM )
    {
        return(DNAND_NOEXISTS_ERROR);
    }
    rtn    = inter_fs_read( blk, offset, pbuf, size );

    return(rtn);
}

int32_t        dnand_fs_write(
                uint32_t            cid,
                uint32_t            offset,
                uint8_t             *pbuf,
                uint32_t            size )
{
    int32_t      rtn;
    uint16_t     blk;

    if( pbuf == NULL )
    {
        return(DNAND_PARAM_ERROR);
    }
    if( cid >= DNAND_ID_ENUM_MAX )
    {
        return(DNAND_PARAM_ERROR);
    }
    if( cid >= DNAND_FS_CLID_NUM )
    {
        return(DNAND_PARAM_ERROR);
    }

    rtn    = pre_init();
    if( rtn != DNAND_NO_ERROR )
    {
        return(rtn);
    }

    blk    = get_clid( cid );
    rtn    = inter_fs_write( cid, blk, offset, pbuf, size );
    if( rtn == DNAND_NO_ERROR )
    {
        rtn    = update_memng( cid );
    }
    return(rtn);
}
