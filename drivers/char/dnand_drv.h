#ifndef DNAND_DRV_H
#define DNAND_DRV_H
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
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/genhd.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <asm/page.h>

#define DNAND_DRV_SECTOR_BLK_SIZE            (512)

#define DNAND_MMC_BLOCK_DEV_NAME_MIBIB       ("/dev/block/mmcblk0")
#define DNAND_MMC_BLOCK_DEV_NAME             ("/dev/block/mmcblk0p")

#define DNAND_TRUE                           (10)
#define DNAND_FALSE                          (0)
#define DNAND_INITIALIZED                    (10)

#define DNAND_DRV_BOOT_REC_SIG               (0xAA55)
#define DNAND_DRV_MBR_ENTRIES                (4)

#define DNAND_DRV_ERR_PARTITION_NUM          (0xFF)

#define DNAND_DRV_UNUSED_ENTRY_GUID     {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
// DNAND GUID FE4035EC-B350-4FB2-8199-A2198093C17F
#define DNAND_DRV_DNAND_PARTITION_GUID  {0xec,0x35,0x40,0xfe,0x50,0xb3,0xb2,0x4f,0x81,0x99,0xa2,0x19,0x80,0x93,0xc1,0x7f}
// DNAND LABEL (UTF16)
#define DNAND_DRV_DNAND_PARTITION_LABEL {0x64,0x00,0x6E,0x00,0x61,0x00,0x6E,0x00,0x64,0x00,0x00,0x00}


typedef enum
{
    DNAND_DEV_READ    = 0,
    DNAND_DEV_WRITE,
    DNAND_DEV_MAX
} dnand_dev_rw_type;

typedef struct dnand_bdev_status_st
{
    struct completion       event;
    int                     error;
} dnand_bdev_status;

typedef struct _uint64_st {
    uint32_t low32;
    uint32_t high32;
} uint64_st;

struct dnand_drv_partition_entry_st
{
    uint8_t status;
    uint8_t rsvd0[3];
    uint8_t type;
    uint8_t rsvd1[3];
    uint32_t start_sector;
    uint32_t partition_size;
} __attribute__((__packed__));

typedef struct dnand_drv_partition_entry_st dnand_drv_partition_entry;

struct dnand_drv_boot_record_st {
    uint8_t                   rsvd0[446];
    dnand_drv_partition_entry entry[4];
    uint16_t                  sig;
} __attribute__((__packed__));

typedef struct dnand_drv_boot_record_st dnand_drv_boot_record;

typedef struct dnand_drv_GPT_header {
    uint8_t signature[8];
    uint8_t revision[4];
    uint32_t size;
    uint32_t crc32;
    uint8_t reserved[4];
    uint64_st current_LBA;
    uint64_st backup_LBA;
    uint64_st first_LBA;
    uint64_st last_LBA;
    uint8_t disk_guid[16];
    uint64_st entry_LBA;
    uint32_t entry_num;
    uint32_t entry_size;
    uint32_t entry_array_crc32;
} dnand_drv_GPT_header;

typedef struct dnand_drv_GPT_partition_entry_st {
    uint8_t type_guid[16];
    uint8_t unique_guid[16];
    uint64_st first_LBA;
    uint64_st last_LBA;
    uint64_st attribute_flags;
    uint16_t name[36];
} dnand_drv_GPT_partition_entry;

typedef struct dnand_drv_LBA_st {
    union {
        uint8_t raw[DNAND_DRV_SECTOR_BLK_SIZE];
        dnand_drv_boot_record boot_record;
        dnand_drv_GPT_header gpt_header;
        dnand_drv_GPT_partition_entry gpt_partition_entry[4];
    } as;
} dnand_drv_LBA;

int32_t dnand_drv_read( uint32_t sector, uint32_t num_sector, uint8_t *pbuf );
int32_t dnand_drv_write( uint32_t sector, uint32_t num_sector, uint8_t *pbuf );

#endif // DNAND_DRV_H
