#ifndef DNAND_FS_MNG_H
#define DNAND_FS_MNG_H
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

#include "dnand_drv.h"

#define DNAND_FS_PARTITION_SIZE           (1024*1024*8)
#define DNAND_FS_CLID_NUM                 (100)

#define DNAND_FS_BLK_SECT_NUM             (4)
#define DNAND_FS_BLK_SIZE                 (DNAND_FS_BLK_SECT_NUM*DNAND_DRV_SECTOR_BLK_SIZE)
#define DNAND_FS_BLK_NUM                  (DNAND_FS_PARTITION_SIZE/DNAND_FS_BLK_SIZE)

#define DNAND_FS_BLK_CKCD_NUM             (1)

#define DNAND_FS_BLK_CLID_NUM             ( (DNAND_FS_CLID_NUM*sizeof(uint32_t)+DNAND_FS_BLK_SIZE-1)/DNAND_FS_BLK_SIZE )
#define DNAND_FS_BLK_FAT_NUM              ( (DNAND_FS_BLK_NUM*sizeof(uint16_t)+DNAND_FS_BLK_SIZE-1)/DNAND_FS_BLK_SIZE )

#define DNAND_FS_SYSTEM_NUM               (2)

#define DNAND_FS_BLK_SYS_NUM              (DNAND_FS_BLK_CKCD_NUM+DNAND_FS_BLK_CLID_NUM+DNAND_FS_BLK_FAT_NUM)

#define DNAND_FS_BLK_DATA_ST              (DNAND_FS_BLK_SYS_NUM*DNAND_FS_SYSTEM_NUM)

#define DNAND_FS_CKCD_SIZE                (6)
#define DNAND_FS_CKCD                     ("DNAND")

#define DNAND_FS_BLK_UNUSE                (0xFFFF)
#define DNAND_FS_BLK_EOF                  (0x7FFF)

typedef struct dnand_fs_flmng_st
{
    uint8_t  ckcd[DNAND_FS_CKCD_SIZE];
    uint8_t  dmya[DNAND_FS_BLK_SIZE-DNAND_FS_CKCD_SIZE];
    uint32_t clid[(DNAND_FS_BLK_CLID_NUM*DNAND_FS_BLK_SIZE)/sizeof(uint32_t)];
    uint16_t fat[(DNAND_FS_BLK_FAT_NUM*DNAND_FS_BLK_SIZE)/sizeof(uint16_t)];
}dnand_fs_flmng;

typedef struct dnand_fs_memng_st
{
    uint32_t clid[DNAND_FS_CLID_NUM];
    uint16_t fat[DNAND_FS_BLK_NUM];
}dnand_fs_memng;

#endif
