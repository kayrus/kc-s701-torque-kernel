/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2014 KYOCERA Corporation
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#include <linux/regulator/qpnp-regulator.h>
#include <linux/msm_tsens.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/restart.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/clk-provider.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/msm_smem.h>
#include <linux/msm_thermal.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"
#include "spm.h"
#include "pm.h"
#include "modem_notifier.h"
#ifdef CONFIG_ANDROID_RAM_CONSOLE
#include <mach/msm_iomap.h>
#include <linux/persistent_ram.h>
#endif	/* #ifdef CONFIG_ANDROID_RAM_CONSOLE */

static struct memtype_reserve msm8226_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm8226_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct of_dev_auxdata msm8226_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9864000, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9864900, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,hsic-host", 0xF9A00000, "msm_hsic_host", NULL),

	{}
};

static struct reserve_info msm8226_reserve_info __initdata = {
	.memtype_reserve_table = msm8226_reserve_table,
	.paddr_to_memtype = msm8226_paddr_to_memtype,
};

static void __init msm8226_early_memory(void)
{
	reserve_info = &msm8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msm8226_reserve_table);
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE
#define SMEM_ALLOC_SIZE			(0x00000200)
#define RAM_CONSOLE_HEAD_SIZE	(0x000000C0)
#define RAM_CONSOLE_BUFF_ADDR	(MSM_UNINIT_RAM_BASE + SMEM_ALLOC_SIZE)
#define RAM_CONSOLE_BUFF_SIZE	(0x00080000 - RAM_CONSOLE_HEAD_SIZE)

static struct persistent_ram_descriptor pram_descs[] = {
	{
		.name = "ram_console",
		.size = RAM_CONSOLE_BUFF_SIZE,
	},
};

static struct persistent_ram kcj_persistent_ram = {
	.size  = RAM_CONSOLE_BUFF_SIZE,
	.num_descs = ARRAY_SIZE(pram_descs),
	.descs = pram_descs,
};

void __init kcj_add_persistent_ram( void )
{
	struct persistent_ram *pram = &kcj_persistent_ram;
	pram->start = (phys_addr_t)RAM_CONSOLE_BUFF_ADDR;

	persistent_ram_early_init(pram);
}

void __init kcj_reserve( void )
{
	kcj_add_persistent_ram();
}
#endif	/* #ifdef CONFIG_ANDROID_RAM_CONSOLE */

static void __init msm8226_reserve(void)
{
	reserve_info = &msm8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm8226_reserve_table);
	msm_reserve();
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	kcj_reserve();
#endif	/* #ifdef CONFIG_ANDROID_RAM_CONSOLE */
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm8226_add_drivers(void)
{
	msm_smem_init();
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	msm_spm_device_init();
	msm_pm_sleep_status_init();
	rpm_regulator_smd_driver_init();
	qpnp_regulator_init();
	if (of_board_is_rumi())
		msm_clock_init(&msm8226_rumi_clock_init_data);
	else
		msm_clock_init(&msm8226_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE
#include <../drivers/staging/android/ram_console.h>
static char bootreason[128] = {0,};
int __init kcj_boot_reason(char *s)
{
	int n;

	if (*s == '=')
		s++;
	n = snprintf(bootreason, sizeof(bootreason),
		 "Boot info:\n"
		 "Last boot reason: %s\n", s);
	bootreason[n] = '\0';
	return 1;
}
//__setup("bootreason", kcj_boot_reason);

struct ram_console_platform_data ram_console_pdata = {
	.bootinfo = bootreason,
};

static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.dev = {
		.platform_data = &ram_console_pdata,
	}
};

void __init kcj_add_ramconsole_devices( void )
{
	platform_device_register(&ram_console_device);
}
#endif /* #ifdef CONFIG_ANDROID_RAM_CONSOLE */

void __init msm8226_init(void)
{
	struct of_dev_auxdata *adata = msm8226_auxdata_lookup;

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm8226_init_gpiomux();
	board_dt_populate(adata);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	kcj_add_ramconsole_devices();
#endif /* #ifdef CONFIG_ANDROID_RAM_CONSOLE */
	msm8226_add_drivers();

	/* boot_reason */
	boot_reason = *(unsigned int *)
		(kc_smem_alloc(SMEM_KC_POWER_ON_STATUS_INFO, 8));
	printk(KERN_NOTICE "Boot Reason = 0x%08x\n", boot_reason);
}

#define KCC_SMEM_KERR_LOG_SIZE        65537
#define KCC_SMEM_SBL_BOOTVER_SIZE     16
#define KCC_SMEM_SBL_DNAND_DATA_SIZE  (50 * 1024)
#define KCC_SMEM_DDR_DATA_INFO_SIZE   8
#define KCC_SMEM_KC_WM_UUID_SIZE      6
#define KCC_SMEM_FACTORY_CMDLINE_SIZE 1024
#define KCC_SMEM_FACTORY_USB_SIZE     8
#define KCC_SMEM_CRASH_LOG_SIZE       512
#define KCC_SMEM_FACTORY_OPTIONS_SIZE 4
#define KCC_SMEM_CHG_PARAM_SIZE       160
#define KCC_SMEM_UICC_INFO_SIZE       2
#define KCC_SMEM_LINUXSDDL_FLAG_SIZE  8
#define KCC_SMEM_SECUREBOOT_FLAG_SIZE 4
#define KCC_SMEM_HW_ID_SIZE           4
#define KCC_SMEM_VENDOR_ID_SIZE       4
#define KCC_SMEM_BFSS_DATA_SIZE       32768
#define KCC_SMEM_KC_POWER_ON_STATUS_INFO_SIZE  8
#define KCC_SMEM_BOOT_PW_ON_CHECK_SIZE         8
#define KCC_SMEM_CHG_CYCLE_SIZE                100

typedef struct {
  unsigned int kerr_log[ (KCC_SMEM_KERR_LOG_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int sbl_bootver[ (KCC_SMEM_SBL_BOOTVER_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int sbl_dnand_data[ (KCC_SMEM_SBL_DNAND_DATA_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int sbl_ddr_info[ (KCC_SMEM_DDR_DATA_INFO_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int sbl_wm_uuid[ (KCC_SMEM_KC_WM_UUID_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int factory_cmdline[ (KCC_SMEM_FACTORY_CMDLINE_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int factory_usb[ (KCC_SMEM_FACTORY_USB_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int crash_log[ (KCC_SMEM_CRASH_LOG_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int factory_options[ (KCC_SMEM_FACTORY_OPTIONS_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int chg_param[ (KCC_SMEM_CHG_PARAM_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int uicc_info[ (KCC_SMEM_UICC_INFO_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int linuxsddl_flag[ (KCC_SMEM_LINUXSDDL_FLAG_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int secureboot_flag[ (KCC_SMEM_SECUREBOOT_FLAG_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int hw_id[ (KCC_SMEM_HW_ID_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int vendor_id[ (KCC_SMEM_VENDOR_ID_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int bfss_data[ (KCC_SMEM_BFSS_DATA_SIZE + 4) / sizeof(unsigned int) ];
  unsigned int kc_power_on_status_info[ ( KCC_SMEM_KC_POWER_ON_STATUS_INFO_SIZE + 4 ) / sizeof(unsigned int) ];
  unsigned int boot_pw_on_check[ ( KCC_SMEM_BOOT_PW_ON_CHECK_SIZE + 4 ) / sizeof(unsigned int) ];
  unsigned int chg_cycle[ ( KCC_SMEM_CHG_CYCLE_SIZE + 4 ) / sizeof(unsigned int) ];
} type_kcc_smem;

#define QFPROM_SECBOOT_ON         (0x4E4F4253)
#define QFPROM_SECBOOT_OFF        (0x464F4253)
#define QFPROM_ERR_READ_FUSE      (0x46525F45)


bool is_secboot_enabled(void)
{
	unsigned int secboot_mode = 0;
	void* ptr = kc_smem_alloc(SMEM_SECUREBOOT_FLAG, sizeof(secboot_mode));

	if(ptr){
		secboot_mode = *(unsigned int*)ptr;
		return (secboot_mode == QFPROM_SECBOOT_ON);
	}

	return false;
}

static int kc_secboot_mode_show(struct seq_file *m, void *unused)
{
	unsigned int secboot_mode = 0;
	void* ptr = kc_smem_alloc(SMEM_SECUREBOOT_FLAG, sizeof(secboot_mode));

	if (ptr) {
		secboot_mode = *(unsigned int*)ptr;
	}

	seq_printf(m, "%d:0x%08x\n", QFPROM_SECBOOT_ON == secboot_mode, secboot_mode);

	return 0;
}

static int kc_secboot_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, kc_secboot_mode_show, NULL);
}

static const struct file_operations kc_secboot_mode_fops = {
	.owner = THIS_MODULE,
	.open = kc_secboot_mode_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init kc_secboot_mode_init(void)
{
	proc_create("secboot_mode", S_IRUGO, NULL, &kc_secboot_mode_fops);
	return 0;
}
core_initcall(kc_secboot_mode_init);

static type_kcc_smem    *kc_smem_addr = NULL;

static void kc_smem_init( void )
{
  kc_smem_addr = (type_kcc_smem *)smem_find(SMEM_ID_VENDOR2, sizeof(type_kcc_smem));

  if (kc_smem_addr == NULL)
  {
      printk(KERN_ERR
          "%s: kc_smem_init ret error=NULL\n",__func__);
  }

  return;
}

void *kc_smem_alloc(unsigned smem_type, unsigned buf_size)
{
  if (kc_smem_addr == NULL)
  {
    kc_smem_init();

      if (kc_smem_addr == NULL)
      {
        return( NULL );
      }

  }

  switch (smem_type)
  {
  case SMEM_KERR_LOG:
    if (buf_size <= KCC_SMEM_KERR_LOG_SIZE)
      return( (void *)&kc_smem_addr->kerr_log[0] );
    break;

  case SMEM_SBL_BOOTVER:
    if (buf_size <= KCC_SMEM_SBL_BOOTVER_SIZE)
      return( (void *)&kc_smem_addr->sbl_bootver[0] );
    break;

  case SMEM_SBL_DNAND_DATA:
    if (buf_size <= KCC_SMEM_SBL_DNAND_DATA_SIZE)
      return( (void *)&kc_smem_addr->sbl_dnand_data[0] );
    break;

  case SMEM_DDR_DATA_INFO:
    if (buf_size <= KCC_SMEM_DDR_DATA_INFO_SIZE)
      return( (void *)&kc_smem_addr->sbl_ddr_info[0] );
    break;

  case SMEM_KC_WM_UUID:
    if (buf_size <= KCC_SMEM_KC_WM_UUID_SIZE)
      return( (void *)&kc_smem_addr->sbl_wm_uuid[0] );
    break;

  case SMEM_FACTORY_CMDLINE:
	if (buf_size <= KCC_SMEM_FACTORY_CMDLINE_SIZE)
      return( (void *)&kc_smem_addr->factory_cmdline[0] );
	break;

  case SMEM_FACTORY_USB:
	if (buf_size <= KCC_SMEM_FACTORY_USB_SIZE)
      return( (void *)&kc_smem_addr->factory_usb[0] );
	break;

  case SMEM_CRASH_LOG:
    if (buf_size <= KCC_SMEM_CRASH_LOG_SIZE)
      return( (void *)&kc_smem_addr->crash_log[0] );
    break;

  case SMEM_FACTORY_OPTIONS:
    if (buf_size <= KCC_SMEM_FACTORY_OPTIONS_SIZE)
      return( (void *)&kc_smem_addr->factory_options[0] );
    break;

  case SMEM_CHG_PARAM:
    if (buf_size <= KCC_SMEM_CHG_PARAM_SIZE)
      return( (void *)&kc_smem_addr->chg_param[0] );
    break;

  case SMEM_UICC_INFO:
    if (buf_size <= KCC_SMEM_UICC_INFO_SIZE)
      return( (void *)&kc_smem_addr->uicc_info[0] );
    break;

  case SMEM_LINUXSDDL_FLAG:
    if (buf_size <= KCC_SMEM_LINUXSDDL_FLAG_SIZE)
      return( (void *)&kc_smem_addr->linuxsddl_flag[0] );
    break;

  case SMEM_SECUREBOOT_FLAG:
    if (buf_size <= KCC_SMEM_SECUREBOOT_FLAG_SIZE)
      return( (void *)&kc_smem_addr->secureboot_flag[0] );
    break;

  case SMEM_HW_ID:
    if (buf_size <= KCC_SMEM_HW_ID_SIZE)
      return( (void *)&kc_smem_addr->hw_id[0] );
    break;

  case SMEM_VENDOR_ID:
    if (buf_size <= KCC_SMEM_VENDOR_ID_SIZE)
      return( (void *)&kc_smem_addr->vendor_id[0] );
    break;

  case SMEM_BFSS_DATA:
    if (buf_size <= KCC_SMEM_BFSS_DATA_SIZE)
      return( (void *)&kc_smem_addr->bfss_data[ 0 ] );
    break;

  case SMEM_KC_POWER_ON_STATUS_INFO:
    if ( buf_size <= KCC_SMEM_KC_POWER_ON_STATUS_INFO_SIZE )
      return( ( void * )&kc_smem_addr->kc_power_on_status_info[ 0 ] );
    break;

  case SMEM_BOOT_PW_ON_CHECK:
    if ( buf_size <= KCC_SMEM_BOOT_PW_ON_CHECK_SIZE )
      return( ( void * )&kc_smem_addr->boot_pw_on_check[ 0 ] );
    break;

  case SMEM_CHG_CYCLE:
    if ( buf_size <= KCC_SMEM_CHG_CYCLE_SIZE )
      return( ( void * )&kc_smem_addr->chg_cycle[ 0 ] );
    break;

  default:
    break;
  }
  return( NULL );
}

int oem_is_off_charge(void)
{
	static int status = -1;

	if (status == -1) {
		status = (strstr(saved_command_line, "androidboot.mode=kccharger") != NULL);
	}

	return status;
}
EXPORT_SYMBOL(oem_is_off_charge);

static int oem_off_charge_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "%d\n", oem_is_off_charge());

	return 0;
}

static int oem_off_charge_open(struct inode *inode, struct file *file)
{
	return single_open(file, oem_off_charge_show, NULL);
}

static const struct file_operations oem_off_charge_fops = {
	.owner = THIS_MODULE,
	.open = oem_off_charge_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init oem_off_charge_init(void)
{
	proc_create("off_charge", S_IRUGO, NULL, &oem_off_charge_fops);
	return 0;
}
core_initcall(oem_off_charge_init);

static const char *msm8226_dt_match[] __initconst = {
	"qcom,msm8226",
	"qcom,msm8926",
	"qcom,apq8026",
	NULL
};

DT_MACHINE_START(MSM8226_DT, "Qualcomm MSM 8226 (Flattened Device Tree)")
	.map_io = msm_map_msm8226_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msm8226_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm8226_dt_match,
	.reserve = msm8226_reserve,
	.init_very_early = msm8226_early_memory,
	.restart = msm_restart,
	.smp = &arm_smp_ops,
MACHINE_END
