#ifndef _RESET_LOG_H
#define _RESET_LOG_H
/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
                              <resetlog.h>
DESCRIPTION

EXTERNALIZED FUNCTIONS

This software is contributed or developed by KYOCERA Corporation.
(C) 2012 KYOCERA Corporation
(C) 2013 KYOCERA Corporation
(C) 2014 KYOCERA Corporation

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 and
only version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

#define HEADER_VERION           "v2.0.0"
#define HEADER_VERION_SIZE      (8)

#define OCIMEM_BASE             (0xFE800000)         /* SCL_IMEM_BASE          0xFE800000 */
#define OCIMEM_SIZE             (0x00006000)         /* SCL_IMEM_SIZE          0x00006000 */
#define RPM_CODE_BASE           (0xFC100000)         /* SCL_RPM_CODE_RAM_BASE  0xFC100000 */
#define RPM_CODE_SIZE           (0x00020000)         /* SCL_RPM_CODE_RAM_SIZE  0x00020000 */
#define RPM_DATA_BASE           (0xFC190000)         /* SCL_RPM_DATA_RAM_BASE  0xFC190000 */
#define RPM_DATA_SIZE           (0x00010000)         /* SCL_RPM_DATA_RAM_SIZE  0x00010000 */
#define RPM_MSG_BASE            (0xFC428000)         /* RPM_MSG_RAM_BASE       0xFC428000 */
#define RPM_MSG_SIZE            (0x00004000)         /* RPM_MSG_RAM_SIZE       0x00004000 */
#define LPM_BASE                (0xFE090000)         /* LPASS_LPM_BASE         0xFE090000 */
#define LPM_SIZE                (0x00010000)         /* LPASS_LPM_SIZE         0x00010000 */
#define RESET_STATUS_SIZE       (0x00000004)

#define MSM_KCJLOG_BASE         (MSM_UNINIT_RAM_BASE)

#define SIZE_SMEM_ALLOC         (512)
#define SIZE_CONTROL_INFO       (SIZE_SMEM_ALLOC)
#define SIZE_KERNEL_LOG         (512 * 1024)
#define SIZE_LOGCAT_MAIN        (512 * 1024)
#define SIZE_LOGCAT_SYSTEM      (512 * 1024)
#define SIZE_LOGCAT_EVENTS      (256 * 1024)
#define SIZE_LOGCAT_RADIO       (512 * 1024)
#define SIZE_SMEM_EVENT_LOG     ( 50 * 1024)
#define SIZE_MODEM_F3_LOG       (512 * 1024)
#define SIZE_MODEM_ERR_DATA_LOG ( 16 * 1024)
#define SIZE_CPU_CONTEXT_INFO   (  1 * 1024)
#define SIZE_LASTLOGCAT_INFO    (SIZE_SMEM_ALLOC)

#define SIZE_OCIMEM             (OCIMEM_SIZE)
#define SIZE_RPM_CODE           (RPM_CODE_SIZE)
#define SIZE_RPM_DATA           (RPM_DATA_SIZE)
#define SIZE_RPM_MSG            (RPM_MSG_SIZE)
#define SIZE_LPM                (LPM_SIZE)
#define SIZE_RESET_STATUS       (RESET_STATUS_SIZE)

#define ADDR_CONTROL_INFO       (MSM_KCJLOG_BASE)
#define ADDR_KERNEL_LOG         (ADDR_CONTROL_INFO  + SIZE_CONTROL_INFO )
#define ADDR_LOGCAT_MAIN        (ADDR_KERNEL_LOG    + SIZE_KERNEL_LOG   )
#define ADDR_LOGCAT_SYSTEM      (ADDR_LOGCAT_MAIN   + SIZE_LOGCAT_MAIN  )
#define ADDR_LOGCAT_EVENTS      (ADDR_LOGCAT_SYSTEM + SIZE_LOGCAT_SYSTEM)
#define ADDR_LOGCAT_RADIO       (ADDR_LOGCAT_EVENTS + SIZE_LOGCAT_EVENTS)
#define ADDR_SMEM_EVENT_LOG     (ADDR_LOGCAT_RADIO  + SIZE_LOGCAT_RADIO )
#define ADDR_MODEM_F3_LOG       (ADDR_SMEM_EVENT_LOG + SIZE_SMEM_EVENT_LOG )
#define ADDR_MODEM_ERR_DATA_LOG (ADDR_MODEM_F3_LOG   + SIZE_MODEM_F3_LOG   )
#define ADDR_CPU_CONTEXT_INFO   (ADDR_MODEM_ERR_DATA_LOG + SIZE_MODEM_ERR_DATA_LOG )
#define ADDR_LASTLOGCAT_INFO    (ADDR_CPU_CONTEXT_INFO   + SIZE_CPU_CONTEXT_INFO)

#define ADDR_USB_DUMP_START     (ADDR_LASTLOGCAT_INFO   + SIZE_LASTLOGCAT_INFO)
#define ADDR_OCIMEM             (ADDR_USB_DUMP_START)
#define ADDR_RPM_CODE           (ADDR_OCIMEM             + SIZE_OCIMEM         )
#define ADDR_RPM_DATA           (ADDR_RPM_CODE           + SIZE_RPM_CODE       )
#define ADDR_RPM_MSG            (ADDR_RPM_DATA           + SIZE_RPM_DATA       )
#define ADDR_LPM                (ADDR_RPM_MSG            + SIZE_RPM_MSG        )
#define ADDR_RESET_STATUS       (ADDR_LPM                + SIZE_LPM            )
#define ADDR_USB_DUMP_END       (ADDR_RESET_STATUS       + SIZE_RESET_STATUS   )

#define CRASH_MAGIC_CODE        "KC ERROR"

#define CRASH_SYSTEM_KERNEL      "KERNEL"
#define CRASH_SYSTEM_MODEM       "MODEM"
#define CRASH_SYSTEM_PRONTO      "PRONTO"
#define CRASH_SYSTEM_ADSP        "ADSP"
#define CRASH_SYSTEM_VENUS       "VENUS"
#define CRASH_SYSTEM_ANDROID     "ANDROID"
#define CRASH_SYSTEM_UNKNOWN     "UNKNOWN"

#define CRASH_KIND_PANIC         "KERNEL PANIC"
#define CRASH_KIND_FATAL         "ERR FATAL"
#define CRASH_KIND_EXCEPTION     "EXCEPTION"
#define CRASH_KIND_WDOG_HW       "HW WATCH DOG"
#define CRASH_KIND_WDOG_SW       "SW WATCH DOG"
#define CRASH_KIND_SYS_SERVER    "SYSTEM SERVER CRASH"
#define CRASH_KIND_PWR_KEY       "PWR KEY"
#define CRASH_KIND_KDFS_REBOOT   "KDFS REBOOT"
#define CRASH_KIND_UNKNOWN       "UNKNOWN"


/* RAM_CONSOLE Contol */
typedef struct {
    unsigned char               magic[4];       // RAM_CONSOLE MAGIC("DEBG")
    unsigned long               start;          // RAM_CONSOLE start
    unsigned long               size;           // RAM_CONSOLE size
} ram_console_header_type;

typedef struct {
    ram_console_header_type     header;         // RAM_CONSOLE header
    unsigned char               msg[1];         // RAM_CONSOLE message
} ram_console_type;

enum {
	LOGGER_INFO_MAIN,
	LOGGER_INFO_SYSTEM,
	LOGGER_INFO_EVENTS,
	LOGGER_INFO_RADIO,
	LOGGER_INFO_MAX,
};

/* Log Control */
struct logger_log_info {
	unsigned long               w_off;
	unsigned long               head;
};

#define MAGIC_CODE_SIZE         (16)
#define CRASH_SYSTEM_SIZE       (16)
#define CRASH_KIND_SIZE         (32)
#define CRASH_TIME_SIZE         (48)
#define VERSION_SIZE            (64)
#define SETTING_OPTION_SIZE     (40)
#define MODEL_SIZE              (32)
#define CRASH_INFO_DATA_SIZE    (32)

typedef struct {
	unsigned char           magic_code[MAGIC_CODE_SIZE];            // Magic Code(16byte)
	unsigned char           crash_system[CRASH_SYSTEM_SIZE];        // Crash System(16byte)
	unsigned char           crash_kind[CRASH_KIND_SIZE];            // Crash Kind(32byte)
	unsigned char           crash_time[CRASH_TIME_SIZE];            // Crash Time(48byte)
    unsigned char           linux_ver[VERSION_SIZE];
    unsigned char           modem_ver[VERSION_SIZE];
    unsigned char           model[MODEL_SIZE];
	struct logger_log_info  info[LOGGER_INFO_MAX];
	unsigned long           pErr_F3_Trace_Buffer;
	unsigned long           Sizeof_Err_F3_Trace_Buffer;
	unsigned long           Err_F3_Trace_Buffer_Head;
	unsigned long           Err_F3_Trace_Wrap_Flag;
	unsigned long           pErr_Data;
	unsigned long           Sizeof_Err_Data;
	unsigned long           pSmem_log_events;
	unsigned long           pSmem_log_write_idx;
	unsigned long           Sizeof_Smem_log;
    unsigned long           regsave_addr;
    unsigned long           regsave_addr_check;
    unsigned long           f3_msg_buff_head;
    unsigned long           f3_msg_trace_wrap_flg;
    unsigned long           smem_log_write_idx;
	unsigned long           ocimem_size;
	unsigned long           rpm_code_size;
	unsigned long           rpm_data_size;
	unsigned long           rpm_msg_size;
	unsigned long           lpm_size;
	unsigned long           reset_status_size;
    unsigned char           crash_info_data[CRASH_INFO_DATA_SIZE];
    unsigned char           panic_info_data[CRASH_INFO_DATA_SIZE];
	unsigned long           reserved[16];
} ram_log_info_type;

#define LASTLOG_RAM_SIG (0x43474244) /* DBGC */

typedef struct {
	unsigned long sig;
	struct logger_log_info info[LOGGER_INFO_MAX];
} last_log_uninit_info_type;

#endif /* _RESET_LOG_H */
