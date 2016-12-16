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
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/genhd.h>
#include <linux/gfp.h>
#include <linux/mm.h>

#include "dnand_fs.h"
#include "dnand_drv.h"

const static uint8_t dnand_partition_label[] = DNAND_DRV_DNAND_PARTITION_LABEL;

static int8_t local_mmc_dev_name[32];
static uint32_t init_dev_name_flg = 0;

static int32_t inter_blk_dev_rw(int8_t *pname, uint32_t sector,
                                uint32_t num_sector, uint8_t *pbuf,
                                dnand_dev_rw_type rwtype )
{
    struct file *dnand_file;
    int32_t flags;
    uint32_t size,srtn;
    mm_segment_t _segfs;

    if( (pname == NULL)||(pbuf == NULL) )
    {
        return(DNAND_DEV_ERROR);
    }

    if( rwtype == DNAND_DEV_READ )
    {
        flags = O_RDONLY|O_SYNC|O_LARGEFILE;
    }
    else
    {
        flags = O_RDWR|O_SYNC|O_LARGEFILE;
    }

    dnand_file = filp_open(pname, flags, 0);
    if( IS_ERR(dnand_file) )
    {
        return(DNAND_DEV_ERROR);
    }

    _segfs = get_fs();
    set_fs(get_ds());

    size = DNAND_DRV_SECTOR_BLK_SIZE*sector;
    srtn = dnand_file->f_op->llseek(dnand_file, size, SEEK_SET);
    if( srtn != size )
    {
        set_fs(_segfs);
        filp_close(dnand_file,NULL);
        return(DNAND_DEV_ERROR);
    }

    size = DNAND_DRV_SECTOR_BLK_SIZE*num_sector;

    if( rwtype == DNAND_DEV_READ )
    {
        srtn = dnand_file->f_op->read(dnand_file, pbuf, size, &dnand_file->f_pos);
    }
    else
    {
        srtn = dnand_file->f_op->write(dnand_file, pbuf, size, &dnand_file->f_pos);
    }

    set_fs(_segfs);
    filp_close(dnand_file,NULL);
    if( srtn == size )
    {
        return(DNAND_NO_ERROR);
    }
    else
    {
        return(DNAND_DEV_ERROR);
    }
}

static uint32_t search_dnand_partition(void)
{
    dnand_drv_LBA lba;
    int32_t rtn;
    uint32_t partition_num = DNAND_DRV_ERR_PARTITION_NUM;
    int i, j;
    int entry_num = 0;
    uint32_t entry_lba_low;

    rtn = inter_blk_dev_rw(DNAND_MMC_BLOCK_DEV_NAME_MIBIB,
                           0, 1, lba.as.raw, DNAND_DEV_READ);

    if( rtn != DNAND_NO_ERROR )
    {
        return(DNAND_DRV_ERR_PARTITION_NUM);
    }

    if( lba.as.boot_record.sig != DNAND_DRV_BOOT_REC_SIG )
    {
        return(DNAND_DRV_ERR_PARTITION_NUM);
    }

    rtn = inter_blk_dev_rw(DNAND_MMC_BLOCK_DEV_NAME_MIBIB,
                           1, 1, lba.as.raw, DNAND_DEV_READ);

    if( rtn != DNAND_NO_ERROR )
    {
        return(DNAND_DRV_ERR_PARTITION_NUM);
    }

    if(( lba.as.gpt_header.signature[0] != 'E' ) ||
       ( lba.as.gpt_header.signature[1] != 'F' ) ||
       ( lba.as.gpt_header.signature[2] != 'I' ) ||
       ( lba.as.gpt_header.signature[3] != ' ' ) ||
       ( lba.as.gpt_header.signature[4] != 'P' ) ||
       ( lba.as.gpt_header.signature[5] != 'A' ) ||
       ( lba.as.gpt_header.signature[6] != 'R' ) ||
       ( lba.as.gpt_header.signature[7] != 'T' ))
    {
        return(DNAND_DRV_ERR_PARTITION_NUM);
    }

    entry_num = lba.as.gpt_header.entry_num;
    entry_lba_low = lba.as.gpt_header.entry_LBA.low32;
    for( i = 0; i < entry_num / 4; i++ )
    {
        rtn = inter_blk_dev_rw(DNAND_MMC_BLOCK_DEV_NAME_MIBIB,
                               entry_lba_low + i, 1,
                               lba.as.raw, DNAND_DEV_READ);

        if( rtn != DNAND_NO_ERROR )
        {
            return(DNAND_DRV_ERR_PARTITION_NUM);
        }

        for( j = 0; j < 4; j++ )
        {
            if( !memcmp(dnand_partition_label, lba.as.gpt_partition_entry[j].name,
                        sizeof(dnand_partition_label)) )
            {
                partition_num = i * 4 + j + 1;
                return(partition_num);
            }
        }
    }

    return(DNAND_DRV_ERR_PARTITION_NUM);
}

static int32_t inter_make_dev_name(void)
{
    uint32_t  dnand_part_num;

    if( init_dev_name_flg == DNAND_INITIALIZED )
    {
        return(DNAND_NO_ERROR);
    }

    dnand_part_num = search_dnand_partition();
    if( (dnand_part_num == 0) || (dnand_part_num >= DNAND_DRV_ERR_PARTITION_NUM) )
    {
        return(DNAND_DEV_ERROR);
    }

    sprintf( (char*)&local_mmc_dev_name[0], "%s%d", DNAND_MMC_BLOCK_DEV_NAME, dnand_part_num );
    init_dev_name_flg = DNAND_INITIALIZED;

    return(DNAND_NO_ERROR);
}

static int8_t* inter_get_dev_name(void)
{
    int32_t rtn;
    uint32_t len;

    rtn = inter_make_dev_name();
    if( rtn != DNAND_NO_ERROR )
    {
        return(NULL);
    }

    len = strlen(DNAND_MMC_BLOCK_DEV_NAME);
    rtn = memcmp(DNAND_MMC_BLOCK_DEV_NAME, local_mmc_dev_name, len);
    if( rtn == 0 )
    {
        return( &local_mmc_dev_name[0] );
    }
    else
    {
        return(NULL);
    }
}

int32_t dnand_drv_read(uint32_t sector, uint32_t num_sector, uint8_t *pbuf)
{
    int32_t rtn;
    int8_t *pname;

    pname = inter_get_dev_name();
    if( pname == NULL )
    {
        return(DNAND_DEV_ERROR);
    }
    rtn = inter_blk_dev_rw(pname, sector, num_sector, pbuf, DNAND_DEV_READ );

    return(rtn);
}

int32_t dnand_drv_write(uint32_t sector, uint32_t num_sector, uint8_t *pbuf)
{
    int32_t rtn;
    int8_t *pname;

    pname = inter_get_dev_name();
    if( pname == NULL )
    {
        return(DNAND_DEV_ERROR);
    }
    rtn = inter_blk_dev_rw(pname, sector, num_sector, pbuf, DNAND_DEV_WRITE );

    return(rtn);
}
