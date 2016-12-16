/* include/linux/logger.h
 *
 * Copyright (C) 2007-2008 Google, Inc.
 * Author: Robert Love <rlove@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2011 KYOCERA Corporation
 * (C) 2012 KYOCERA Corporation
 * (C) 2013 KYOCERA Corporation
 * (C) 2014 KYOCERA Corporation
 */

#ifndef _LINUX_LOGGER_H
#define _LINUX_LOGGER_H

#include <linux/types.h>
#include <linux/ioctl.h>

#include <linux/io.h>
#include <mach/msm_iomap.h>
#include "resetlog.h"

/*
 * The userspace structure for version 1 of the logger_entry ABI.
 * This structure is returned to userspace unless the caller requests
 * an upgrade to a newer ABI version.
 */
struct user_logger_entry_compat {
	__u16		len;	/* length of the payload */
	__u16		__pad;	/* no matter what, we get 2 bytes of padding */
	__s32		pid;	/* generating process's pid */
	__s32		tid;	/* generating process's tid */
	__s32		sec;	/* seconds since Epoch */
	__s32		nsec;	/* nanoseconds */
	char		msg[0];	/* the entry's payload */
};

/*
 * The structure for version 2 of the logger_entry ABI.
 * This structure is returned to userspace if ioctl(LOGGER_SET_VERSION)
 * is called with version >= 2
 */
struct logger_entry {
	__u16		len;		/* length of the payload */
	__u16		hdr_size;	/* sizeof(struct logger_entry_v2) */
	__s32		pid;		/* generating process's pid */
	__s32		tid;		/* generating process's tid */
	__s32		sec;		/* seconds since Epoch */
	__s32		nsec;		/* nanoseconds */
	uid_t		euid;		/* effective UID of logger */
	char		msg[0];		/* the entry's payload */
};

#define LOGGER_LOG_RADIO	"log_radio"	/* radio-related messages */
#define LOGGER_LOG_EVENTS	"log_events"	/* system/hardware events */
#define LOGGER_LOG_SYSTEM	"log_system"	/* system/framework messages */
#define LOGGER_LOG_MAIN		"log_main"	/* everything else */

#ifdef CONFIG_DEBUG_LAST_LOGCAT
#define LOGGER_LOG_LAST_RADIO	"log_last_radio"
#define LOGGER_LOG_LAST_EVENTS	"log_last_events"
#define LOGGER_LOG_LAST_SYSTEM	"log_last_system"
#define LOGGER_LOG_LAST_MAIN	"log_last_main"
#endif /* CONFIG_DEBUG_LAST_LOGCAT */

#define LOGGER_ENTRY_MAX_PAYLOAD	4076

#define __LOGGERIO	0xAE

#define LOGGER_GET_LOG_BUF_SIZE		_IO(__LOGGERIO, 1) /* size of log */
#define LOGGER_GET_LOG_LEN		_IO(__LOGGERIO, 2) /* used log len */
#define LOGGER_GET_NEXT_ENTRY_LEN	_IO(__LOGGERIO, 3) /* next entry len */
#define LOGGER_FLUSH_LOG		_IO(__LOGGERIO, 4) /* flush log */
#define LOGGER_GET_VERSION		_IO(__LOGGERIO, 5) /* abi version */
#define LOGGER_SET_VERSION		_IO(__LOGGERIO, 6) /* abi version */

#define LOGGER_INFO_SIZE        (sizeof(struct logger_log_info))
#define ADDR_LOGGER_INFO_MAIN   ( &((ram_log_info_type *)ADDR_CONTROL_INFO)->info[LOGGER_INFO_MAIN  ] )
#define ADDR_LOGGER_INFO_SYSTEM ( &((ram_log_info_type *)ADDR_CONTROL_INFO)->info[LOGGER_INFO_SYSTEM] )
#define ADDR_LOGGER_INFO_EVENTS ( &((ram_log_info_type *)ADDR_CONTROL_INFO)->info[LOGGER_INFO_EVENTS] )
#define ADDR_LOGGER_INFO_RADIO  ( &((ram_log_info_type *)ADDR_CONTROL_INFO)->info[LOGGER_INFO_RADIO ] )
#ifdef CONFIG_DEBUG_LAST_LOGCAT
#define ADDR_LASTLOG_INFO_MAIN   ( &((last_log_uninit_info_type *)ADDR_LASTLOGCAT_INFO)->info[LOGGER_INFO_MAIN  ] )
#define ADDR_LASTLOG_INFO_SYSTEM ( &((last_log_uninit_info_type *)ADDR_LASTLOGCAT_INFO)->info[LOGGER_INFO_SYSTEM] )
#define ADDR_LASTLOG_INFO_EVENTS ( &((last_log_uninit_info_type *)ADDR_LASTLOGCAT_INFO)->info[LOGGER_INFO_EVENTS] )
#define ADDR_LASTLOG_INFO_RADIO  ( &((last_log_uninit_info_type *)ADDR_LASTLOGCAT_INFO)->info[LOGGER_INFO_RADIO ] )
#endif /* CONFIG_DEBUG_LAST_LOGCAT */

#endif /* _LINUX_LOGGER_H */
