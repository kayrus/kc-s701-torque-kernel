/*
 * drivers/misc/logger.c
 *
 * A Logging Subsystem
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * Robert Love <rlove@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2011 KYOCERA Corporation
 * (C) 2012 KYOCERA Corporation
 * (C) 2013 KYOCERA Corporation
 * (C) 2014 KYOCERA Corporation
 */

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <mach/msm_smem.h>
#include "resetlog.h"
#include "logger.h"

#include <asm/ioctls.h>

#ifndef CONFIG_LOGCAT_SIZE
#define CONFIG_LOGCAT_SIZE 256
#endif

extern void rtc_time_to_tm(unsigned long time, struct rtc_time *tm);

/*
 * struct logger_log - represents a specific log, such as 'main' or 'radio'
 *
 * This structure lives from module insertion until module removal, so it does
 * not need additional reference counting. The structure is protected by the
 * mutex 'mutex'.
 */
struct logger_log {
	unsigned char		*buffer;/* the ring buffer itself */
	struct miscdevice	misc;	/* misc device representing the log */
	wait_queue_head_t	wq;	/* wait queue for readers */
	struct list_head	readers; /* this log's readers */
	struct mutex		mutex;	/* mutex protecting buffer */
	size_t			w_off;	/* current write head offset */
	size_t			head;	/* new readers start here */
	size_t			size;	/* size of the log */
	struct logger_log_info  *log_info;
#ifdef CONFIG_DEBUG_LAST_LOGCAT
	struct logger_log_info	*last_log_info;
#endif /* CONFIG_DEBUG_LAST_LOGCAT */
};

#ifdef CONFIG_DEBUG_LAST_LOGCAT
#define DEFINE_LOGGER_DEVICE(VAR, ADDR, NAME, SIZE, LOGGER_INFO, LOGGER_FOPS, LAST_LOGGER_INFO) \
static struct logger_log VAR = { \
        .buffer = ADDR, \
        .misc = { \
                .minor = MISC_DYNAMIC_MINOR, \
                .name = NAME, \
                .fops = LOGGER_FOPS, \
                .parent = NULL, \
        }, \
        .wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq), \
        .readers = LIST_HEAD_INIT(VAR .readers), \
        .mutex = __MUTEX_INITIALIZER(VAR .mutex), \
        .w_off = 0, \
        .head = 0, \
        .size = SIZE, \
        .log_info = LOGGER_INFO, \
        .last_log_info = LAST_LOGGER_INFO, \
};
#else
#define DEFINE_LOGGER_DEVICE(VAR, ADDR, NAME, SIZE, LOGGER_INFO, LOGGER_FOPS) \
static struct logger_log VAR = { \
	.buffer = (unsigned char *)(ADDR), \
	.misc = { \
		.minor = MISC_DYNAMIC_MINOR, \
		.name = NAME, \
		.fops = LOGGER_FOPS, \
		.parent = NULL, \
	}, \
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq), \
	.readers = LIST_HEAD_INIT(VAR .readers), \
	.mutex = __MUTEX_INITIALIZER(VAR .mutex), \
	.w_off = 0, \
	.head = 0, \
	.size = SIZE, \
	.log_info = LOGGER_INFO, \
};
#endif /* CONFIG_DEBUG_LAST_LOGCAT */

#define EMMC_INDEX_MAX               (4)
#define EMMC_SECTOR_BLK_SIZE         (512)   // eMMC Sector Size

#define SIZE_TO_SECTOR(size)         (((size % EMMC_SECTOR_BLK_SIZE) > 0) ? \
                                      ( size / EMMC_SECTOR_BLK_SIZE) + 1  : \
                                      ( size / EMMC_SECTOR_BLK_SIZE))

#define EMMC_INFO_SECTOR_SIZE        (SIZE_TO_SECTOR(sizeof(emmc_info_type)))
#define EMMC_INFO_START_SECTOR       (0)

#define EMMC_REC_SIZE                (0x0032AA00)
#define EMMC_REC_SECTOR_SIZE         (SIZE_TO_SECTOR(EMMC_REC_SIZE))

#define EMMC_REC_START_SECTOR(idx)   ((EMMC_INFO_START_SECTOR + EMMC_INFO_SECTOR_SIZE) + \
                                      (EMMC_REC_SECTOR_SIZE * idx))

#define EMMC_BLOCK_DEV_NAME_MIBIB       ("/dev/block/mmcblk0")
#define EMMC_BLOCK_DEV_NAME             ("/dev/block/mmcblk0p")

#define EMMC_TRUE                           (10)
#define EMMC_FALSE                          (0)
#define EMMC_INITIALIZED                    (10)

#define EMMC_DRV_BOOT_REC_SIG               (0xAA55)
#define EMMC_DRV_MBR_ENTRIES                (4)

#define EMMC_DRV_ERR_PARTITION_NUM          (0xFF)
#define EMMC_DEV_ERROR						(-1)
#define EMMC_NO_ERROR					    (0)

typedef enum
{
    EMMC_DEV_READ    = 0,
    EMMC_DEV_WRITE,
    EMMC_DEV_MAX
} emmc_dev_rw_type;

typedef struct _uint64_st {
    uint32_t low32;
    uint32_t high32;
} uint64_st;

struct emmc_drv_partition_entry_st
{
    uint8_t status;
    uint8_t rsvd0[3];
    uint8_t type;
    uint8_t rsvd1[3];
    uint32_t start_sector;
    uint32_t partition_size;
} __attribute__((__packed__));

typedef struct emmc_drv_partition_entry_st emmc_drv_partition_entry;

struct emmc_drv_boot_record_st {
    uint8_t                   rsvd0[446];
    emmc_drv_partition_entry entry[4];
    uint16_t                  sig;
} __attribute__((__packed__));

typedef struct emmc_drv_boot_record_st emmc_drv_boot_record;

typedef struct emmc_drv_GPT_header {
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
} emmc_drv_GPT_header;

typedef struct emmc_drv_GPT_partition_entry_st {
    uint8_t type_guid[16];
    uint8_t unique_guid[16];
    uint64_st first_LBA;
    uint64_st last_LBA;
    uint64_st attribute_flags;
    uint16_t name[36];
} emmc_drv_GPT_partition_entry;

typedef struct emmc_drv_LBA_st {
    union {
        uint8_t raw[EMMC_SECTOR_BLK_SIZE];
        emmc_drv_boot_record boot_record;
        emmc_drv_GPT_header gpt_header;
        emmc_drv_GPT_partition_entry gpt_partition_entry[4];
    } as;
} emmc_drv_LBA;


/* ResetLog control info */
typedef struct {
    unsigned char               crash_system[CRASH_SYSTEM_SIZE]; // Crash System
    unsigned char               crash_kind[CRASH_KIND_SIZE];   // Crash Kind
    unsigned char               crash_time[CRASH_TIME_SIZE];   // Crash Time
    unsigned char               linux_ver[VERSION_SIZE];       // Linux version
    unsigned char               modem_ver[VERSION_SIZE];       // AMSS version
    unsigned char               model[MODEL_SIZE];             // model string
    unsigned long               kernel_log_start;              // Kernel log top
    unsigned long               kernel_log_size;               // Kernel log size
    struct logger_log_info      logger[LOGGER_INFO_MAX];
    unsigned long               f3_msg_buff_head;              // F3 message read pointer
    unsigned long               f3_msg_trace_wrap_flg;         // F3 message wrap flag
    unsigned long               qcom_err_data_size;            // QCOM error data size
    unsigned long               pSmem_log_write_idx;           // smem log write index
    unsigned long               Sizeof_Smem_log;               // smem log size
    unsigned long               padding1[2];
    unsigned long               ocimem_size;                   // OCI MEMORY
    unsigned long               rpm_code_size;                 // RPM CODE RAM
    unsigned long               rpm_data_size;                 // RPM DATA RAM
    unsigned long               rpm_msg_size;                  // RPM MSG RAM
    unsigned long               lpm_size;                      // LPASS LPM
    unsigned long               reset_status_size;             // RESET STATUS
    unsigned long               padding2[2];
} emmc_rec_info_type;

typedef struct {
    unsigned char               herader_ver[HEADER_VERION_SIZE];    // resetlog.h version
    unsigned long               write_index;                        // Index of write (0 - (EMMC_INDEX_MAX - 1))
    unsigned char               valid_flg[EMMC_INDEX_MAX];          // valid(1) or invalid(0)
#if 0
    struct logger_log_info      fotalog[LOGGER_INFO_MAX];
#endif
    emmc_rec_info_type          rec_info[EMMC_INDEX_MAX];           // Record Info
    unsigned long               settings[SETTING_OPTION_SIZE];      // log control option(0 - 39)
    unsigned long               reserved[36];
} emmc_info_type;
emmc_info_type          Emmc_log_info;

static unsigned long bark_regaddr = 0;

/*
 * struct logger_reader - a logging device open for reading
 *
 * This object lives from open to release, so we don't need additional
 * reference counting. The structure is protected by log->mutex.
 */
struct logger_reader {
	struct logger_log	*log;	/* associated log */
	struct list_head	list;	/* entry in logger_log's list */
	size_t			r_off;	/* current read head offset */
	bool			r_all;	/* reader can read all entries */
	int			r_ver;	/* reader ABI version */
};

/* logger_offset - returns index 'n' into the log via (optimized) modulus */
size_t logger_offset(struct logger_log *log, size_t n)
{
	return n & (log->size-1);
}


/*
 * file_get_log - Given a file structure, return the associated log
 *
 * This isn't aesthetic. We have several goals:
 *
 *	1) Need to quickly obtain the associated log during an I/O operation
 *	2) Readers need to maintain state (logger_reader)
 *	3) Writers need to be very fast (open() should be a near no-op)
 *
 * In the reader case, we can trivially go file->logger_reader->logger_log.
 * For a writer, we don't want to maintain a logger_reader, so we just go
 * file->logger_log. Thus what file->private_data points at depends on whether
 * or not the file was opened for reading. This function hides that dirtiness.
 */
static inline struct logger_log *file_get_log(struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		return reader->log;
	} else
		return file->private_data;
}

/*
 * get_entry_header - returns a pointer to the logger_entry header within
 * 'log' starting at offset 'off'. A temporary logger_entry 'scratch' must
 * be provided. Typically the return value will be a pointer within
 * 'logger->buf'.  However, a pointer to 'scratch' may be returned if
 * the log entry spans the end and beginning of the circular buffer.
 */
static struct logger_entry *get_entry_header(struct logger_log *log,
		size_t off, struct logger_entry *scratch)
{
	size_t len = min(sizeof(struct logger_entry), log->size - off);
	if (len != sizeof(struct logger_entry)) {
		memcpy(((void *) scratch), log->buffer + off, len);
		memcpy(((void *) scratch) + len, log->buffer,
			sizeof(struct logger_entry) - len);
		return scratch;
	}

	return (struct logger_entry *) (log->buffer + off);
}

/*
 * get_entry_msg_len - Grabs the length of the message of the entry
 * starting from from 'off'.
 *
 * An entry length is 2 bytes (16 bits) in host endian order.
 * In the log, the length does not include the size of the log entry structure.
 * This function returns the size including the log entry structure.
 *
 * Caller needs to hold log->mutex.
 */
static __u32 get_entry_msg_len(struct logger_log *log, size_t off)
{
	struct logger_entry scratch;
	struct logger_entry *entry;

	entry = get_entry_header(log, off, &scratch);
	return entry->len;
}

static size_t get_user_hdr_len(int ver)
{
	if (ver < 2)
		return sizeof(struct user_logger_entry_compat);
	else
		return sizeof(struct logger_entry);
}

static ssize_t copy_header_to_user(int ver, struct logger_entry *entry,
					 char __user *buf)
{
	void *hdr;
	size_t hdr_len;
	struct user_logger_entry_compat v1;

	if (ver < 2) {
		v1.len      = entry->len;
		v1.__pad    = 0;
		v1.pid      = entry->pid;
		v1.tid      = entry->tid;
		v1.sec      = entry->sec;
		v1.nsec     = entry->nsec;
		hdr         = &v1;
		hdr_len     = sizeof(struct user_logger_entry_compat);
	} else {
		hdr         = entry;
		hdr_len     = sizeof(struct logger_entry);
	}

	return copy_to_user(buf, hdr, hdr_len);
}

/*
 * do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * user-space buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->mutex.
 */
static ssize_t do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader,
				   char __user *buf,
				   size_t count)
{
	struct logger_entry scratch;
	struct logger_entry *entry;
	size_t len;
	size_t msg_start;

	/*
	 * First, copy the header to userspace, using the version of
	 * the header requested
	 */
	entry = get_entry_header(log, reader->r_off, &scratch);
	if (copy_header_to_user(reader->r_ver, entry, buf))
		return -EFAULT;

	count -= get_user_hdr_len(reader->r_ver);
	buf += get_user_hdr_len(reader->r_ver);
	msg_start = logger_offset(log,
		reader->r_off + sizeof(struct logger_entry));

	/*
	 * We read from the msg in two disjoint operations. First, we read from
	 * the current msg head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - msg_start);
	if (copy_to_user(buf, log->buffer + msg_start, len))
		return -EFAULT;

	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;

	reader->r_off = logger_offset(log, reader->r_off +
		sizeof(struct logger_entry) + count);

	return count + get_user_hdr_len(reader->r_ver);
}

/*
 * get_next_entry_by_uid - Starting at 'off', returns an offset into
 * 'log->buffer' which contains the first entry readable by 'euid'
 */
static size_t get_next_entry_by_uid(struct logger_log *log,
		size_t off, uid_t euid)
{
	while (off != log->w_off) {
		struct logger_entry *entry;
		struct logger_entry scratch;
		size_t next_len;

		entry = get_entry_header(log, off, &scratch);

		if (entry->euid == euid)
			return off;

		next_len = sizeof(struct logger_entry) + entry->len;
		off = logger_offset(log, off + next_len);
	}

	return off;
}

/*
 * logger_read - our log's read() method
 *
 * Behavior:
 *
 *	- O_NONBLOCK works
 *	- If there are no log entries to read, blocks until log is written to
 *	- Atomically reads exactly one log entry
 *
 * Will set errno to EINVAL if read
 * buffer is insufficient to hold next entry.
 */
static ssize_t logger_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
	DEFINE_WAIT(wait);

start:
	while (1) {
		mutex_lock(&log->mutex);

		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		ret = (log->w_off == reader->r_off);
		mutex_unlock(&log->mutex);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
		return ret;

	mutex_lock(&log->mutex);

	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
		mutex_unlock(&log->mutex);
		goto start;
	}

	/* get the size of the next entry */
	ret = get_user_hdr_len(reader->r_ver) +
		get_entry_msg_len(log, reader->r_off);
	if (count < ret) {
		ret = -EINVAL;
		goto out;
	}

	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, buf, ret);

out:
	mutex_unlock(&log->mutex);

	return ret;
}

/*
 * get_next_entry - return the offset of the first valid entry at least 'len'
 * bytes after 'off'.
 *
 * Caller must hold log->mutex.
 */
static size_t get_next_entry(struct logger_log *log, size_t off, size_t len)
{
	size_t count = 0;

	do {
		size_t nr = sizeof(struct logger_entry) +
			get_entry_msg_len(log, off);
		off = logger_offset(log, off + nr);
		count += nr;
	} while (count < len);

	return off;
}

/*
 * is_between - is a < c < b, accounting for wrapping of a, b, and c
 *    positions in the buffer
 *
 * That is, if a<b, check for c between a and b
 * and if a>b, check for c outside (not between) a and b
 *
 * |------- a xxxxxxxx b --------|
 *               c^
 *
 * |xxxxx b --------- a xxxxxxxxx|
 *    c^
 *  or                    c^
 */
static inline int is_between(size_t a, size_t b, size_t c)
{
	if (a < b) {
		/* is c between a and b? */
		if (a < c && c <= b)
			return 1;
	} else {
		/* is c outside of b through a? */
		if (c <= b || a < c)
			return 1;
	}

	return 0;
}

#ifdef CONFIG_DEBUG_LAST_LOGCAT
static void update_last_log_info(struct logger_log *log)
{
	if (log->last_log_info) {
		log->last_log_info->head  = log->head;
		log->last_log_info->w_off = log->w_off;
	}
}
#endif /* CONFIG_DEBUG_LAST_LOGCAT */

/*
 * fix_up_readers - walk the list of all readers and "fix up" any who were
 * lapped by the writer; also do the same for the default "start head".
 * We do this by "pulling forward" the readers and start head to the first
 * entry after the new write head.
 *
 * The caller needs to hold log->mutex.
 */
static void fix_up_readers(struct logger_log *log, size_t len)
{
	size_t old = log->w_off;
	size_t new = logger_offset(log, old + len);
	struct logger_reader *reader;

	if (is_between(old, new, log->head))
	{
		log->head = get_next_entry(log, log->head, len);
		log->log_info->head = log->head;
	}

	list_for_each_entry(reader, &log->readers, list)
		if (is_between(old, new, reader->r_off))
			reader->r_off = get_next_entry(log, reader->r_off, len);
}

/*
 * do_write_log - writes 'len' bytes from 'buf' to 'log'
 *
 * The caller needs to hold log->mutex.
 */
static void do_write_log(struct logger_log *log, const void *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	memcpy(log->buffer + log->w_off, buf, len);

	if (count != len)
		memcpy(log->buffer, buf + len, count - len);

	log->w_off = logger_offset(log, log->w_off + count);
	log->log_info->w_off = log->w_off;

}

/*
 * do_write_log_user - writes 'len' bytes from the user-space buffer 'buf' to
 * the log 'log'
 *
 * The caller needs to hold log->mutex.
 *
 * Returns 'count' on success, negative error code on failure.
 */
static ssize_t do_write_log_from_user(struct logger_log *log,
				      const void __user *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	if (len && copy_from_user(log->buffer + log->w_off, buf, len))
		return -EFAULT;

	if (count != len)
		if (copy_from_user(log->buffer, buf + len, count - len))
			/*
			 * Note that by not updating w_off, this abandons the
			 * portion of the new entry that *was* successfully
			 * copied, just above.  This is intentional to avoid
			 * message corruption from missing fragments.
			 */
			return -EFAULT;

	log->w_off = logger_offset(log, log->w_off + count);
	log->log_info->w_off = log->w_off;

	return count;
}

/*
 * logger_aio_write - our write method, implementing support for write(),
 * writev(), and aio_write(). Writes are our fast path, and we try to optimize
 * them above all else.
 */
ssize_t logger_aio_write(struct kiocb *iocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t ppos)
{
	struct logger_log *log = file_get_log(iocb->ki_filp);
	size_t orig = log->w_off;
	struct logger_entry header;
	struct timespec now;
	ssize_t ret = 0;

	now = current_kernel_time();

	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.euid = current_euid();
	header.len = min_t(size_t, iocb->ki_left, LOGGER_ENTRY_MAX_PAYLOAD);
	header.hdr_size = sizeof(struct logger_entry);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return 0;

	mutex_lock(&log->mutex);

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

	while (nr_segs-- > 0) {
		size_t len;
		ssize_t nr;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

		/* write out this segment's payload */
		nr = do_write_log_from_user(log, iov->iov_base, len);
		if (unlikely(nr < 0)) {
			log->w_off = orig;
			log->log_info->w_off = log->w_off;
#ifdef CONFIG_DEBUG_LAST_LOGCAT
			update_last_log_info(log);
#endif /* CONFIG_DEBUG_LAST_LOGCAT */
			mutex_unlock(&log->mutex);
			return nr;
		}

		iov++;
		ret += nr;
	}
#ifdef CONFIG_DEBUG_LAST_LOGCAT
	update_last_log_info(log);
#endif /* CONFIG_DEBUG_LAST_LOGCAT */

	mutex_unlock(&log->mutex);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

	return ret;
}

static struct logger_log *get_log_from_minor(int);

/*
 * logger_open - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int logger_open(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		reader->r_ver = 1;
		reader->r_all = in_egroup_p(inode->i_gid) ||
			capable(CAP_SYSLOG);

		INIT_LIST_HEAD(&reader->list);

		mutex_lock(&log->mutex);
		reader->r_off = log->head;
		list_add_tail(&reader->list, &log->readers);
		mutex_unlock(&log->mutex);

		file->private_data = reader;
	} else
		file->private_data = log;

	return 0;
}

/*
 * logger_release - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int logger_release(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		struct logger_log *log = reader->log;

		mutex_lock(&log->mutex);
		list_del(&reader->list);
		mutex_unlock(&log->mutex);

		kfree(reader);
	}

	return 0;
}

/*
 * logger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int logger_poll(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	mutex_lock(&log->mutex);
	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	if (log->w_off != reader->r_off)
		ret |= POLLIN | POLLRDNORM;
	mutex_unlock(&log->mutex);

	return ret;
}

static long logger_set_version(struct logger_reader *reader, void __user *arg)
{
	int version;
	if (copy_from_user(&version, arg, sizeof(int)))
		return -EFAULT;

	if ((version < 1) || (version > 2))
		return -EINVAL;

	reader->r_ver = version;
	return 0;
}

static long logger_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -EINVAL;
	void __user *argp = (void __user *) arg;

	mutex_lock(&log->mutex);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off >= reader->r_off)
			ret = log->w_off - reader->r_off;
		else
			ret = (log->size - reader->r_off) + log->w_off;
		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;

		if (!reader->r_all)
			reader->r_off = get_next_entry_by_uid(log,
				reader->r_off, current_euid());

		if (log->w_off != reader->r_off)
			ret = get_user_hdr_len(reader->r_ver) +
				get_entry_msg_len(log, reader->r_off);
		else
			ret = 0;
		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
		list_for_each_entry(reader, &log->readers, list)
			reader->r_off = log->w_off;
		log->head = log->w_off;
		log->log_info->head = log->head;
		ret = 0;
		break;
	case LOGGER_GET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = reader->r_ver;
		break;
	case LOGGER_SET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = logger_set_version(reader, argp);
		break;
	}

	mutex_unlock(&log->mutex);

	return ret;
}

static const struct file_operations logger_fops = {
	.owner = THIS_MODULE,
	.read = logger_read,
	.aio_write = logger_aio_write,
	.poll = logger_poll,
	.unlocked_ioctl = logger_ioctl,
	.compat_ioctl = logger_ioctl,
	.open = logger_open,
	.release = logger_release,
};

#if 0
/*
 * Defines a log structure with name 'NAME' and a size of 'SIZE' bytes, which
 * must be a power of two, and greater than
 * (LOGGER_ENTRY_MAX_PAYLOAD + sizeof(struct logger_entry)).
 */
#define DEFINE_LOGGER_DEVICE(VAR, NAME, SIZE) \
static unsigned char _buf_ ## VAR[SIZE]; \
static struct logger_log VAR = { \
	.buffer = _buf_ ## VAR, \
	.misc = { \
		.minor = MISC_DYNAMIC_MINOR, \
		.name = NAME, \
		.fops = &logger_fops, \
		.parent = NULL, \
	}, \
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq), \
	.readers = LIST_HEAD_INIT(VAR .readers), \
	.mutex = __MUTEX_INITIALIZER(VAR .mutex), \
	.w_off = 0, \
	.head = 0, \
	.size = SIZE, \
};

DEFINE_LOGGER_DEVICE(log_main, LOGGER_LOG_MAIN, CONFIG_LOGCAT_SIZE*1024)
DEFINE_LOGGER_DEVICE(log_events, LOGGER_LOG_EVENTS, CONFIG_LOGCAT_SIZE*1024)
DEFINE_LOGGER_DEVICE(log_radio, LOGGER_LOG_RADIO, CONFIG_LOGCAT_SIZE*1024)
DEFINE_LOGGER_DEVICE(log_system, LOGGER_LOG_SYSTEM, CONFIG_LOGCAT_SIZE*1024)
#else
#ifdef CONFIG_DEBUG_LAST_LOGCAT
DEFINE_LOGGER_DEVICE( log_main  , ADDR_LOGCAT_MAIN  , LOGGER_LOG_MAIN  , SIZE_LOGCAT_MAIN  , ADDR_LOGGER_INFO_MAIN  , (&logger_fops) , ADDR_LASTLOG_INFO_MAIN   )
DEFINE_LOGGER_DEVICE( log_system, ADDR_LOGCAT_SYSTEM, LOGGER_LOG_SYSTEM, SIZE_LOGCAT_SYSTEM, ADDR_LOGGER_INFO_SYSTEM, (&logger_fops) , ADDR_LASTLOG_INFO_SYSTEM )
DEFINE_LOGGER_DEVICE( log_events, ADDR_LOGCAT_EVENTS, LOGGER_LOG_EVENTS, SIZE_LOGCAT_EVENTS, ADDR_LOGGER_INFO_EVENTS, (&logger_fops) , ADDR_LASTLOG_INFO_EVENTS )
DEFINE_LOGGER_DEVICE( log_radio , ADDR_LOGCAT_RADIO , LOGGER_LOG_RADIO , SIZE_LOGCAT_RADIO , ADDR_LOGGER_INFO_RADIO , (&logger_fops) , ADDR_LASTLOG_INFO_RADIO  )
DEFINE_LOGGER_DEVICE( log_last_main   , NULL , LOGGER_LOG_LAST_MAIN  , SIZE_LOGCAT_MAIN  , ADDR_LASTLOG_INFO_MAIN  , (&logger_fops) , NULL )
DEFINE_LOGGER_DEVICE( log_last_system , NULL , LOGGER_LOG_LAST_SYSTEM, SIZE_LOGCAT_SYSTEM, ADDR_LASTLOG_INFO_SYSTEM, (&logger_fops) , NULL )
DEFINE_LOGGER_DEVICE( log_last_events , NULL , LOGGER_LOG_LAST_EVENTS, SIZE_LOGCAT_EVENTS, ADDR_LASTLOG_INFO_EVENTS, (&logger_fops) , NULL )
DEFINE_LOGGER_DEVICE( log_last_radio  , NULL , LOGGER_LOG_LAST_RADIO , SIZE_LOGCAT_RADIO , ADDR_LASTLOG_INFO_RADIO , (&logger_fops) , NULL )
#else
DEFINE_LOGGER_DEVICE( log_main  , ADDR_LOGCAT_MAIN  , LOGGER_LOG_MAIN  , SIZE_LOGCAT_MAIN  , ADDR_LOGGER_INFO_MAIN  , (&logger_fops) )
DEFINE_LOGGER_DEVICE( log_system, ADDR_LOGCAT_SYSTEM, LOGGER_LOG_SYSTEM, SIZE_LOGCAT_SYSTEM, ADDR_LOGGER_INFO_SYSTEM, (&logger_fops) )
DEFINE_LOGGER_DEVICE( log_events, ADDR_LOGCAT_EVENTS, LOGGER_LOG_EVENTS, SIZE_LOGCAT_EVENTS, ADDR_LOGGER_INFO_EVENTS, (&logger_fops) )
DEFINE_LOGGER_DEVICE( log_radio , ADDR_LOGCAT_RADIO , LOGGER_LOG_RADIO , SIZE_LOGCAT_RADIO , ADDR_LOGGER_INFO_RADIO , (&logger_fops) )
#endif /* CONFIG_DEBUG_LAST_LOGCAT */
#endif

static struct logger_log *get_log_from_minor(int minor)
{
	if (log_main.misc.minor == minor)
		return &log_main;
	if (log_events.misc.minor == minor)
		return &log_events;
	if (log_radio.misc.minor == minor)
		return &log_radio;
	if (log_system.misc.minor == minor)
		return &log_system;
#ifdef CONFIG_DEBUG_LAST_LOGCAT
	if (log_last_main.misc.minor == minor)
		return &log_last_main;
	if (log_last_system.misc.minor == minor)
		return &log_last_system;
	if (log_last_events.misc.minor == minor)
		return &log_last_events;
	if (log_last_radio.misc.minor == minor)
		return &log_last_radio;
#endif /* CONFIG_DEBUG_LAST_LOGCAT */
	return NULL;
}

static int __init init_log(struct logger_log *log)
{
	int ret;

	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		printk(KERN_ERR "logger: failed to register misc "
		       "device for log '%s'!\n", log->misc.name);
		return ret;
	}

	printk(KERN_INFO "logger: created %luK log '%s'\n",
	       (unsigned long) log->size >> 10, log->misc.name);

	return 0;
}

static int __init logger_init(void)
{
	int ret;

	ret = init_log(&log_main);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_events);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_radio);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_system);
	if (unlikely(ret))
		goto out;

out:
	return ret;
}

static void get_crash_time(unsigned char *buf, unsigned int bufsize)
{
	struct timespec ts_now;
	struct rtc_time tm;

	ts_now = current_kernel_time();
	rtc_time_to_tm(ts_now.tv_sec, &tm);

	memset( buf ,0x00, bufsize );
	snprintf( buf, bufsize, "%d-%02d-%02d %02d:%02d:%02d.%09lu UTC",
	            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	            tm.tm_hour, tm.tm_min, tm.tm_sec, ts_now.tv_nsec);
}

static unsigned char check_smem_crash_system( ram_log_info_type *plog_info )
{
	unsigned char ret = 0;

	if ( (0 == strncmp((const char *)plog_info->crash_system, CRASH_SYSTEM_KERNEL  , strlen(CRASH_SYSTEM_KERNEL ))) ||
	     (0 == strncmp((const char *)plog_info->crash_system, CRASH_SYSTEM_MODEM   , strlen(CRASH_SYSTEM_MODEM  ))) ||
	     (0 == strncmp((const char *)plog_info->crash_system, CRASH_SYSTEM_PRONTO  , strlen(CRASH_SYSTEM_PRONTO ))) ||
	     (0 == strncmp((const char *)plog_info->crash_system, CRASH_SYSTEM_ADSP    , strlen(CRASH_SYSTEM_ADSP   ))) ||
	     (0 == strncmp((const char *)plog_info->crash_system, CRASH_SYSTEM_VENUS   , strlen(CRASH_SYSTEM_VENUS  ))) ||
	     (0 == strncmp((const char *)plog_info->crash_system, CRASH_SYSTEM_ANDROID , strlen(CRASH_SYSTEM_ANDROID))) )
	{
		ret = 1;
	}

	return ret;
}
static void set_smem_crash_system(const char *system)
{
	ram_log_info_type *plog_info;

	mb();
	plog_info = (ram_log_info_type *)kc_smem_alloc(SMEM_CRASH_LOG, SIZE_SMEM_ALLOC);

	if ((plog_info == NULL) || (system == NULL)) {
		return;
	}
	if (check_smem_crash_system(plog_info) == 1) {
		return;
	}
	memset( &plog_info->crash_system[0], 0x00, CRASH_SYSTEM_SIZE );
	memcpy( &plog_info->crash_system[0], system, strlen(system) );
    get_crash_time( &plog_info->crash_time[0], CRASH_SYSTEM_SIZE );
	mb();
}
static void clear_smem_crash_system(void)
{
	ram_log_info_type *plog_info;

	mb();
	plog_info = (ram_log_info_type *)kc_smem_alloc(SMEM_CRASH_LOG, SIZE_SMEM_ALLOC);

	if (plog_info == NULL) {
		return;
	}
	memset( &plog_info->crash_system[0], 0x00, CRASH_SYSTEM_SIZE );
	mb();
}
void set_smem_crash_system_kernel(void)
{
	set_smem_crash_system(CRASH_SYSTEM_KERNEL);
}
void set_smem_crash_system_modem(void)
{
	set_smem_crash_system(CRASH_SYSTEM_MODEM);
}
void set_smem_crash_system_pronto(void)
{
	set_smem_crash_system(CRASH_SYSTEM_PRONTO);
}
void set_smem_crash_system_adsp(void)
{
	set_smem_crash_system(CRASH_SYSTEM_ADSP);
}
void set_smem_crash_system_venus(void)
{
	set_smem_crash_system(CRASH_SYSTEM_VENUS);
}
void set_smem_crash_system_android(void)
{
	set_smem_crash_system(CRASH_SYSTEM_ANDROID);
}
void set_smem_crash_system_unknown(void)
{
	set_smem_crash_system(CRASH_SYSTEM_UNKNOWN);
}

static unsigned char check_smem_crash_kind( ram_log_info_type *plog_info )
{
	unsigned char ret = 0;

	if ( (0 == strncmp((const char *)plog_info->crash_kind, CRASH_KIND_PANIC      , strlen(CRASH_KIND_PANIC     ))) ||
	     (0 == strncmp((const char *)plog_info->crash_kind, CRASH_KIND_FATAL      , strlen(CRASH_KIND_FATAL     ))) ||
	     (0 == strncmp((const char *)plog_info->crash_kind, CRASH_KIND_EXCEPTION  , strlen(CRASH_KIND_EXCEPTION ))) ||
	     (0 == strncmp((const char *)plog_info->crash_kind, CRASH_KIND_WDOG_HW    , strlen(CRASH_KIND_WDOG_HW   ))) ||
	     (0 == strncmp((const char *)plog_info->crash_kind, CRASH_KIND_WDOG_SW    , strlen(CRASH_KIND_WDOG_SW   ))) ||
	     (0 == strncmp((const char *)plog_info->crash_kind, CRASH_KIND_SYS_SERVER , strlen(CRASH_KIND_SYS_SERVER))) ||
	     (0 == strncmp((const char *)plog_info->crash_kind, CRASH_KIND_PWR_KEY    , strlen(CRASH_KIND_PWR_KEY   ))) ||
	     (0 == strncmp((const char *)plog_info->crash_kind, CRASH_KIND_KDFS_REBOOT, strlen(CRASH_KIND_KDFS_REBOOT))) )
	{
		ret = 1;
	}

	return ret;
}
void set_smem_crash_kind(const char *kind)
{
	ram_log_info_type *plog_info;

	mb();
	plog_info = (ram_log_info_type *)kc_smem_alloc(SMEM_CRASH_LOG, SIZE_SMEM_ALLOC);

	if ((plog_info == NULL) || (kind == NULL)) {
		return;
	}
	if (check_smem_crash_kind(plog_info) == 1) {
		return;
	}
	memset( &plog_info->crash_kind[0], 0x00, CRASH_KIND_SIZE );
	memcpy( &plog_info->crash_kind[0], kind, strlen(kind) );
	mb();
}
static void clear_smem_crash_kind(void)
{
	ram_log_info_type *plog_info;

	mb();
	plog_info = (ram_log_info_type *)kc_smem_alloc(SMEM_CRASH_LOG, SIZE_SMEM_ALLOC);

	if (plog_info == NULL) {
		return;
	}
	memset( &plog_info->crash_kind[0], 0x00, CRASH_KIND_SIZE );
	mb();
}
void set_smem_crash_kind_panic(void)
{
	set_smem_crash_kind(CRASH_KIND_PANIC);
}
void set_smem_crash_kind_fatal(void)
{
	set_smem_crash_kind(CRASH_KIND_FATAL);
}
void set_smem_crash_kind_exception(void)
{
	set_smem_crash_kind(CRASH_KIND_EXCEPTION);
}
void set_smem_crash_kind_wdog_hw(void)
{
	set_smem_crash_kind(CRASH_KIND_WDOG_HW);
}
void set_smem_crash_kind_wdog_sw(void)
{
	set_smem_crash_kind(CRASH_KIND_WDOG_SW);
}
void set_smem_crash_kind_android(void)
{
	set_smem_crash_kind(CRASH_KIND_SYS_SERVER);
}
void set_smem_crash_kind_pwr_key(void)
{
	set_smem_crash_kind(CRASH_KIND_PWR_KEY);
}
void set_smem_crash_kind_kdfs_reboot(void)
{
	set_smem_crash_kind(CRASH_KIND_KDFS_REBOOT);
}
static void set_smem_crash_kind_unknown(void)
{
	set_smem_crash_kind(CRASH_KIND_UNKNOWN);
}

void set_kcj_pet_time(void)
{
	ram_log_info_type *pPanicInfo;

	pPanicInfo   = (ram_log_info_type *)ADDR_CONTROL_INFO;
	get_crash_time( &pPanicInfo->crash_time[0], CRASH_TIME_SIZE );
	strncat( &pPanicInfo->crash_time[0], " (LAST PET)", CRASH_TIME_SIZE-1 );
}

void set_smem_crash_info_data( const char *pdata )
{
	ram_log_info_type *plog_info;
	size_t            len_data;

	mb();
	plog_info = (ram_log_info_type *)kc_smem_alloc( SMEM_CRASH_LOG, SIZE_SMEM_ALLOC );

	if ( ( plog_info == NULL ) || ( pdata == NULL ) ) {
		return;
	}
	if ( strnlen( (const char *)plog_info->crash_info_data, CRASH_INFO_DATA_SIZE ) > 0 ) {
		return;
	}
	memset( plog_info->crash_info_data, '\0', sizeof(plog_info->crash_info_data) );
	len_data = strnlen( pdata, CRASH_INFO_DATA_SIZE );
	memcpy( plog_info->crash_info_data, pdata, len_data );
	mb();
}

static void clear_smem_crash_info_data( void )
{
	ram_log_info_type *plog_info;

	mb();
	plog_info = (ram_log_info_type *)kc_smem_alloc( SMEM_CRASH_LOG, SIZE_SMEM_ALLOC );

	if ( plog_info == NULL ) {
		return;
	}
	memset( plog_info->crash_info_data, 0x00, sizeof(plog_info->crash_info_data) );
	mb();
}

void set_smem_panic_info_data( const char *pdata )
{
	ram_log_info_type *plog_info;
	size_t            len_data;

	mb();
	plog_info = (ram_log_info_type *)kc_smem_alloc( SMEM_CRASH_LOG, SIZE_SMEM_ALLOC );

	if ( ( plog_info == NULL ) || ( pdata == NULL ) ) {
		return;
	}
	if ( strnlen( (const char *)plog_info->panic_info_data, CRASH_INFO_DATA_SIZE ) > 0 ) {
		return;
	}
	memset( plog_info->panic_info_data, '\0', sizeof(plog_info->panic_info_data) );
	len_data = strnlen( pdata, CRASH_INFO_DATA_SIZE );
	memcpy( plog_info->panic_info_data, pdata, len_data );
	mb();
}

void clear_smem_panic_info_data( void )
{
	ram_log_info_type *plog_info;

	mb();
	plog_info = (ram_log_info_type *)kc_smem_alloc( SMEM_CRASH_LOG, SIZE_SMEM_ALLOC );

	if ( plog_info == NULL ) {
		return;
	}
	memset( plog_info->panic_info_data, 0x00, sizeof(plog_info->panic_info_data) );
	mb();
}

void set_kcj_regsave_addr(unsigned long regsave_addr)
{
	ram_log_info_type *pPanicInfo;

	pPanicInfo   = (ram_log_info_type *)ADDR_CONTROL_INFO;
	pPanicInfo->regsave_addr = regsave_addr;
	pPanicInfo->regsave_addr_check = ~regsave_addr;
	bark_regaddr = regsave_addr;
}

void set_kcj_fixed_info(void)
{
	ram_log_info_type *pPanicInfo;

	pPanicInfo   = (ram_log_info_type *)ADDR_CONTROL_INFO;
	memcpy( &pPanicInfo->magic_code[0]  , CRASH_MAGIC_CODE   , strlen(CRASH_MAGIC_CODE  ) );
	memcpy( &pPanicInfo->linux_ver[0]   , BUILD_DISPLAY_ID   , strlen(BUILD_DISPLAY_ID  ) );
	memcpy( &pPanicInfo->model[0]       , PRODUCT_MODEL_NAME , strlen(PRODUCT_MODEL_NAME) );
}

void set_kcj_crash_info(void)
{
	ram_log_info_type *pPanicInfo;
	ram_log_info_type *pSmemLogInfo;

	mb();
	pPanicInfo   = (ram_log_info_type *)ADDR_CONTROL_INFO;
	pSmemLogInfo = (ram_log_info_type *)kc_smem_alloc(SMEM_CRASH_LOG, SIZE_SMEM_ALLOC);

	if ( (pPanicInfo == NULL) || (pSmemLogInfo == NULL) ) {
		return;
	}
	if (check_smem_crash_system(pSmemLogInfo) == 0) {
		return;
	}
	if (check_smem_crash_kind(pSmemLogInfo) == 0) {
		set_smem_crash_kind_unknown();
		set_smem_crash_info_data(" ");
	}

	set_kcj_fixed_info();

	memset( &pPanicInfo->crash_system[0], 0x00, CRASH_SYSTEM_SIZE );
	memset( &pPanicInfo->crash_kind[0]  , 0x00, CRASH_KIND_SIZE );

	memcpy( &pPanicInfo->crash_system[0], &pSmemLogInfo->crash_system[0], CRASH_SYSTEM_SIZE );
	memcpy( &pPanicInfo->crash_kind[0]  , &pSmemLogInfo->crash_kind[0]  , CRASH_KIND_SIZE );

	memcpy( &pPanicInfo->crash_info_data[0], &pSmemLogInfo->crash_info_data[0], CRASH_INFO_DATA_SIZE );
	memcpy( &pPanicInfo->panic_info_data[0], &pSmemLogInfo->panic_info_data[0], CRASH_INFO_DATA_SIZE );

	get_crash_time( &pPanicInfo->crash_time[0], CRASH_TIME_SIZE );
}

void get_smem_crash_system(unsigned char* crash_system,unsigned int bufsize)
{
	ram_log_info_type *pSmemLogInfo;

	mb();
	pSmemLogInfo = (ram_log_info_type *)kc_smem_alloc(SMEM_CRASH_LOG, SIZE_SMEM_ALLOC);

	if (pSmemLogInfo == NULL) {
		return;
	}
	if (check_smem_crash_system(pSmemLogInfo) == 0) {
		return;
	}
	if (check_smem_crash_kind(pSmemLogInfo) == 0) {
		return;
	}

	if (bufsize > CRASH_SYSTEM_SIZE) {
		bufsize = CRASH_SYSTEM_SIZE;
	}

	memcpy( crash_system, &pSmemLogInfo->crash_system[0], bufsize );
}

void set_kcj_init_info(void)
{
	ram_log_info_type *pKcjLogInfo;
	pKcjLogInfo   = (ram_log_info_type *)ADDR_CONTROL_INFO;
	memcpy( &pKcjLogInfo->crash_system[0], CRASH_SYSTEM_UNKNOWN, strlen(CRASH_SYSTEM_UNKNOWN) );
	memcpy( &pKcjLogInfo->crash_kind[0]  , CRASH_KIND_UNKNOWN  , strlen(CRASH_KIND_UNKNOWN) );
}

void clear_kcj_init_info(void)
{
	ram_log_info_type *pKcjLogInfo;
	pKcjLogInfo   = (ram_log_info_type *)ADDR_CONTROL_INFO;
	if (0 == strncmp((const char *)pKcjLogInfo->crash_system, CRASH_SYSTEM_UNKNOWN, strlen(CRASH_SYSTEM_UNKNOWN)) &&
			0 == strncmp((const char *)pKcjLogInfo->crash_kind  , CRASH_KIND_UNKNOWN  , strlen(CRASH_KIND_UNKNOWN  ))) {
		memset( &pKcjLogInfo->crash_system[0], 0x00, CRASH_SYSTEM_SIZE );
		memset( &pKcjLogInfo->crash_kind[0]  , 0x00, CRASH_KIND_SIZE );
	}
}

void clear_kcj_crash_info( void )
{
	ram_log_info_type *pPanicInfo;

	pPanicInfo = (ram_log_info_type *)ADDR_CONTROL_INFO;

	if ( pPanicInfo == NULL ) {
		return;
	}

	memset( &pPanicInfo->crash_system[0], 0x00, CRASH_SYSTEM_SIZE );
	memset( &pPanicInfo->crash_kind[0]  , 0x00, CRASH_KIND_SIZE );
	memset( &pPanicInfo->crash_time[0]  , 0x00, CRASH_TIME_SIZE );

	memset( &pPanicInfo->crash_info_data[0], 0x00, CRASH_INFO_DATA_SIZE );
	memset( &pPanicInfo->panic_info_data[0], 0x00, CRASH_INFO_DATA_SIZE );
}

void set_crash_unknown( void )
{
	ram_log_info_type *pPanicInfo;

	pPanicInfo = (ram_log_info_type *)ADDR_CONTROL_INFO;

	if ( pPanicInfo == NULL ) {
		return;
	}

	memcpy( &pPanicInfo->crash_system[0], CRASH_SYSTEM_UNKNOWN, strlen(CRASH_SYSTEM_UNKNOWN) );
	memcpy( &pPanicInfo->crash_kind[0]  , CRASH_KIND_UNKNOWN  , strlen(CRASH_KIND_UNKNOWN  ) );
}

void set_modemlog_info(void)
{
	ram_log_info_type *pSmemLogInfo;
	ram_log_info_type *pPanicInfo;

	mb();
	pSmemLogInfo = (ram_log_info_type *)kc_smem_alloc(SMEM_CRASH_LOG, SIZE_SMEM_ALLOC);
	pPanicInfo   = (ram_log_info_type *)ADDR_CONTROL_INFO;

	if ( (pSmemLogInfo == NULL) || (pPanicInfo == NULL) ) {
		return;
	}

	pPanicInfo->pErr_F3_Trace_Buffer       = pSmemLogInfo->pErr_F3_Trace_Buffer;
	pPanicInfo->Sizeof_Err_F3_Trace_Buffer = pSmemLogInfo->Sizeof_Err_F3_Trace_Buffer;
	pPanicInfo->Err_F3_Trace_Buffer_Head   = pSmemLogInfo->Err_F3_Trace_Buffer_Head;
	pPanicInfo->Err_F3_Trace_Wrap_Flag     = pSmemLogInfo->Err_F3_Trace_Wrap_Flag;
	pPanicInfo->pErr_Data                  = pSmemLogInfo->pErr_Data;
	pPanicInfo->Sizeof_Err_Data            = pSmemLogInfo->Sizeof_Err_Data;
	pPanicInfo->pSmem_log_events           = pSmemLogInfo->pSmem_log_events;
	pPanicInfo->pSmem_log_write_idx        = pSmemLogInfo->pSmem_log_write_idx;
	pPanicInfo->Sizeof_Smem_log            = pSmemLogInfo->Sizeof_Smem_log;
	memcpy( &pPanicInfo->modem_ver[0]   , &pSmemLogInfo->modem_ver[0]   , VERSION_SIZE    );
}

const static uint8_t log_partition_label[] = {'l', '\0', 'o', '\0', 'g', '\0', '\0', '\0'};

static int8_t local_mmc_dev_name[32];
static uint32_t init_dev_name_flg = 0;

static int32_t inter_blk_dev_rw(int8_t *pname, uint32_t sector,
                                uint32_t num_sector, uint8_t *pbuf,
                                emmc_dev_rw_type rwtype )
{
    struct file *log_file;
    int32_t flags;
    uint32_t size,srtn;
    mm_segment_t _segfs;

    if( (pname == NULL)||(pbuf == NULL) )
    {
        return(EMMC_DEV_ERROR);
    }

    if( rwtype == EMMC_DEV_READ )
    {
        flags = O_RDONLY|O_SYNC|O_LARGEFILE;
    }
    else
    {
        flags = O_RDWR|O_SYNC|O_LARGEFILE;
    }

    log_file = filp_open(pname, flags, 0);
    if( log_file == NULL )
    {
        return(EMMC_DEV_ERROR);
    }

    _segfs = get_fs();
    set_fs(get_ds());

    size = EMMC_SECTOR_BLK_SIZE*sector;
    srtn = log_file->f_op->llseek(log_file, size, SEEK_SET);
    if( srtn != size )
    {
        set_fs(_segfs);
        filp_close(log_file,NULL);
        return(EMMC_DEV_ERROR);
    }

    size = EMMC_SECTOR_BLK_SIZE*num_sector;

    if( rwtype == EMMC_DEV_READ )
    {
        srtn = log_file->f_op->read(log_file, pbuf, size, &log_file->f_pos);
    }
    else
    {
        srtn = log_file->f_op->write(log_file, pbuf, size, &log_file->f_pos);
    }

    set_fs(_segfs);
    filp_close(log_file,NULL);
    if( srtn == size )
    {
        return(EMMC_NO_ERROR);
    }
    else
    {
        return(EMMC_DEV_ERROR);
    }
}

static uint32_t search_log_partition(void)
{
    emmc_drv_LBA lba;
    int32_t rtn;
    uint32_t partition_num = EMMC_DRV_ERR_PARTITION_NUM;
    int i, j;
    int entry_num = 0;
    uint32_t entry_lba_low;

    rtn = inter_blk_dev_rw(EMMC_BLOCK_DEV_NAME_MIBIB,
                           0, 1, lba.as.raw, EMMC_DEV_READ);

    if( rtn != EMMC_NO_ERROR )
    {
        return(EMMC_DRV_ERR_PARTITION_NUM);
    }

    if( lba.as.boot_record.sig != EMMC_DRV_BOOT_REC_SIG )
    {
        return(EMMC_DRV_ERR_PARTITION_NUM);
    }

    rtn = inter_blk_dev_rw(EMMC_BLOCK_DEV_NAME_MIBIB,
                           1, 1, lba.as.raw, EMMC_DEV_READ);

    if( rtn != EMMC_NO_ERROR )
    {
        return(EMMC_DRV_ERR_PARTITION_NUM);
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
        return(EMMC_DRV_ERR_PARTITION_NUM);
    }

    entry_num = lba.as.gpt_header.entry_num;
    entry_lba_low = lba.as.gpt_header.entry_LBA.low32;
    for( i = 0; i < entry_num / 4; i++ )
    {
        rtn = inter_blk_dev_rw(EMMC_BLOCK_DEV_NAME_MIBIB,
                               entry_lba_low + i, 1,
                               lba.as.raw, EMMC_DEV_READ);

        if( rtn != EMMC_NO_ERROR )
        {
            return(EMMC_DRV_ERR_PARTITION_NUM);
        }

        for( j = 0; j < 4; j++ )
        {
            if( !memcmp(log_partition_label, lba.as.gpt_partition_entry[j].name,
                        sizeof(log_partition_label)) )
            {
                partition_num = i * 4 + j + 1;
                return(partition_num);
            }
        }
    }

    return(EMMC_DRV_ERR_PARTITION_NUM);
}

static int32_t inter_make_dev_name(void)
{
    uint32_t  log_part_num;

    if( init_dev_name_flg == EMMC_INITIALIZED )
    {
        return(EMMC_NO_ERROR);
    }

    log_part_num = search_log_partition();
    if( (log_part_num == 0) || (log_part_num >= EMMC_DRV_ERR_PARTITION_NUM) )
    {
        return(EMMC_DEV_ERROR);
    }

    sprintf( (char*)&local_mmc_dev_name[0], "%s%d", EMMC_BLOCK_DEV_NAME, log_part_num );
    init_dev_name_flg = EMMC_INITIALIZED;

    return(EMMC_NO_ERROR);
}

static int8_t* inter_get_dev_name(void)
{
    int32_t rtn;
    uint32_t len;

    rtn = inter_make_dev_name();
    if( rtn != EMMC_NO_ERROR )
    {
        return(NULL);
    }

    len = strlen(EMMC_BLOCK_DEV_NAME);
    rtn = memcmp(EMMC_BLOCK_DEV_NAME, local_mmc_dev_name, len);
    if( rtn == 0 )
    {
        return( &local_mmc_dev_name[0] );
    }
    else
    {
        return(NULL);
    }
}

int32_t emmc_drv_read(uint32_t sector, uint32_t num_sector, uint8_t *pbuf)
{
    int32_t rtn;
    int8_t *pname;

    pname = inter_get_dev_name();
    if( pname == NULL )
    {
        return(EMMC_DEV_ERROR);
    }
    rtn = inter_blk_dev_rw(pname, sector, num_sector, pbuf, EMMC_DEV_READ );

    return(rtn);
}

int32_t emmc_drv_write(uint32_t sector, uint32_t num_sector, uint8_t *pbuf)
{
    int32_t rtn;
    int8_t *pname;

    pname = inter_get_dev_name();
    if( pname == NULL )
    {
        return(EMMC_DEV_ERROR);
    }
    rtn = inter_blk_dev_rw(pname, sector, num_sector, pbuf, EMMC_DEV_WRITE );

    return(rtn);
}

static uint32_t resetlog_flash_read_sub( uint32_t *s_sect, unsigned char *rd_buff, uint32_t rd_size )
{
    uint32_t num_sect;

    num_sect = (rd_size / EMMC_SECTOR_BLK_SIZE);
    if (num_sect == 0) {
        return 0;
    }

    if (emmc_drv_read(*s_sect, num_sect, rd_buff) < 0) {
        return 0;
    }
    *s_sect += num_sect;

    return (num_sect * EMMC_SECTOR_BLK_SIZE);
}

int resetlog_flash_read( uint32_t *s_sect, unsigned char *rd_buff, uint32_t rd_size )
{
    uint32_t        i;
    uint32_t        ret_size;
    uint32_t        tmp_size;
    unsigned char tmp_buff[EMMC_SECTOR_BLK_SIZE];

    if (rd_size >= EMMC_SECTOR_BLK_SIZE) {
        ret_size = resetlog_flash_read_sub(s_sect, rd_buff, rd_size);
        if (ret_size == 0) {
            return -1;
        }
    } else {
        if (resetlog_flash_read_sub(s_sect, tmp_buff, EMMC_SECTOR_BLK_SIZE) == 0) {
            return -1;
        }
        for (i = 0; i < rd_size; i++) {
            rd_buff[i] = tmp_buff[i];
        }
        goto READ_END;
    }

    if (ret_size < rd_size) {
        tmp_size = (rd_size - ret_size);
        if (resetlog_flash_read_sub(s_sect, tmp_buff, EMMC_SECTOR_BLK_SIZE) == 0) {
            return -1;
        }
        for (i = 0; i < tmp_size; i++) {
            rd_buff[ret_size + i] = tmp_buff[i];
        }
    }
READ_END:
    return 0;
}

static uint32_t resetlog_flash_write_sub( uint32_t *s_sect, unsigned char *wr_buff, uint32_t wr_size )
{
    uint32_t num_sect;

    num_sect = (wr_size / EMMC_SECTOR_BLK_SIZE);
    if (num_sect == 0) {
        return 0;
    }

    if (emmc_drv_write(*s_sect, num_sect, wr_buff) < 0) {
        return 0;
    }
    *s_sect += num_sect;

    return (num_sect * EMMC_SECTOR_BLK_SIZE);
}

int resetlog_flash_write( uint32_t *s_sect, unsigned char *wr_buff, uint32_t wr_size )
{
    uint32_t        i;
    uint32_t        ret_size;
    uint32_t        tmp_size;
    unsigned char tmp_buff[EMMC_SECTOR_BLK_SIZE];

    for (i = 0; i < EMMC_SECTOR_BLK_SIZE; i++) {
        tmp_buff[i] = 0;
    }

    if (wr_size >= EMMC_SECTOR_BLK_SIZE) {
        ret_size = resetlog_flash_write_sub(s_sect, wr_buff, wr_size);
        if (ret_size == 0) {
            return -1;
        }
    } else {
        for (i = 0; i < wr_size; i++) {
            tmp_buff[i] = wr_buff[i];
        }
        if (resetlog_flash_write_sub(s_sect, tmp_buff, EMMC_SECTOR_BLK_SIZE) == 0) {
            return -1;
        }
        goto WRITE_END;
    }

    if (ret_size < wr_size) {
        tmp_size = (wr_size - ret_size);
        for (i = 0; i < tmp_size; i++) {
            tmp_buff[i] = wr_buff[ret_size + i];
        }
        if (resetlog_flash_write_sub(s_sect, tmp_buff, EMMC_SECTOR_BLK_SIZE) == 0) {
            return -1;
        }
    }
WRITE_END:
    return 0;
}

static void resetlog_clear_info( emmc_info_type *emmc_log_info )
{
    uint32_t i;

    emmc_log_info->write_index = 0;
    for (i = 0; i < EMMC_INDEX_MAX; i++ ) {
        emmc_log_info->valid_flg[i] = 0;
    }
}

static void resetlog_to_eMMC_kyocera_write( void )
{
    long                    ret;
    uint                    write_sector;   /* sector to write data on the EMMC. */
    uint                    read_sector;    /* sector to read data on the EMMC.  */
    unsigned long           write_buff;     /* address of the data buffer.       */
    unsigned long           write_size;     /* size of the data to be written.   */
    unsigned long           write_index;
    ram_log_info_type       *pLogInfo = (ram_log_info_type *)ADDR_CONTROL_INFO;

    /* Read Control info */
    read_sector = EMMC_INFO_START_SECTOR;
    ret = resetlog_flash_read( &read_sector, (unsigned char *)&Emmc_log_info, sizeof(Emmc_log_info) );
    if ( ret < 0 ) return;

    /* Check resetlog.h version */
    if ( memcmp(&Emmc_log_info.herader_ver[0], (void *)HEADER_VERION, strlen(HEADER_VERION)) != 0 ) {
        resetlog_clear_info(&Emmc_log_info);
    }
    memcpy( &Emmc_log_info.herader_ver[0], (void *)HEADER_VERION, strlen(HEADER_VERION) );

    /* Check write_sector */
    if (Emmc_log_info.write_index >= EMMC_INDEX_MAX) {
        resetlog_clear_info(&Emmc_log_info);
    }
    write_index = Emmc_log_info.write_index;

    /* Set write_sector */
    write_sector = EMMC_REC_START_SECTOR(write_index);

    /* Copy to eMMC from SMEM */
    memcpy( &Emmc_log_info.rec_info[write_index].crash_system[0], pLogInfo->crash_system, CRASH_SYSTEM_SIZE );
    memcpy( &Emmc_log_info.rec_info[write_index].crash_kind[0]  , pLogInfo->crash_kind  , CRASH_KIND_SIZE );
    memcpy( &Emmc_log_info.rec_info[write_index].crash_time[0]  , pLogInfo->crash_time  , CRASH_TIME_SIZE );
    memcpy( &Emmc_log_info.rec_info[write_index].linux_ver[0]   , pLogInfo->linux_ver   , VERSION_SIZE    );
    memcpy( &Emmc_log_info.rec_info[write_index].modem_ver[0]   , pLogInfo->modem_ver   , VERSION_SIZE    );
    memcpy( &Emmc_log_info.rec_info[write_index].model[0]       , pLogInfo->model       , MODEL_SIZE      );

    /* WRITE : KERNEL LOG */
    write_buff = (unsigned long)ADDR_KERNEL_LOG;
    write_size = SIZE_KERNEL_LOG;
    ret = resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
    if ( ret < 0 ) return;
    Emmc_log_info.rec_info[write_index].kernel_log_start = ((ram_console_type *)MSM_KCJLOG_BASE)->header.start;
    Emmc_log_info.rec_info[write_index].kernel_log_size  = ((ram_console_type *)MSM_KCJLOG_BASE)->header.size;

    /* WRITE : LOGCAT MAIN */
    write_buff = (unsigned long)ADDR_LOGCAT_MAIN;
    write_size = SIZE_LOGCAT_MAIN;
    ret = resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
    if ( ret < 0 ) return;
    Emmc_log_info.rec_info[write_index].logger[LOGGER_INFO_MAIN].w_off = pLogInfo->info[LOGGER_INFO_MAIN].w_off;
    Emmc_log_info.rec_info[write_index].logger[LOGGER_INFO_MAIN].head  = pLogInfo->info[LOGGER_INFO_MAIN].head;

    /* WRITE : LOGCAT SYSTEM */
    write_buff = (unsigned long)ADDR_LOGCAT_SYSTEM;
    write_size = SIZE_LOGCAT_SYSTEM;
    ret = resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
    if ( ret < 0 ) return;
    Emmc_log_info.rec_info[write_index].logger[LOGGER_INFO_SYSTEM].w_off = pLogInfo->info[LOGGER_INFO_SYSTEM].w_off;
    Emmc_log_info.rec_info[write_index].logger[LOGGER_INFO_SYSTEM].head  = pLogInfo->info[LOGGER_INFO_SYSTEM].head;

    /* WRITE : LOGCAT EVENTS */
    write_buff = (unsigned long)ADDR_LOGCAT_EVENTS;
    write_size = SIZE_LOGCAT_EVENTS;
    ret = resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
    if ( ret < 0 ) return;
    Emmc_log_info.rec_info[write_index].logger[LOGGER_INFO_EVENTS].w_off = pLogInfo->info[LOGGER_INFO_EVENTS].w_off;
    Emmc_log_info.rec_info[write_index].logger[LOGGER_INFO_EVENTS].head  = pLogInfo->info[LOGGER_INFO_EVENTS].head;

    /* WRITE : LOGCAT RADIO */
    write_buff = (unsigned long)ADDR_LOGCAT_RADIO;
    write_size = SIZE_LOGCAT_RADIO;
    ret = resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
    if ( ret < 0 ) return;
    Emmc_log_info.rec_info[write_index].logger[LOGGER_INFO_RADIO].w_off = pLogInfo->info[LOGGER_INFO_RADIO].w_off;
    Emmc_log_info.rec_info[write_index].logger[LOGGER_INFO_RADIO].head  = pLogInfo->info[LOGGER_INFO_RADIO].head;

    /* WRITE : SMEM EVENT LOG */
    write_buff = (unsigned long)ADDR_SMEM_EVENT_LOG;
    write_size = SIZE_SMEM_EVENT_LOG + 4;       /* +4 header size */
    ret = resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
    Emmc_log_info.rec_info[write_index].pSmem_log_write_idx = pLogInfo->pSmem_log_write_idx;
    Emmc_log_info.rec_info[write_index].Sizeof_Smem_log     = pLogInfo->Sizeof_Smem_log;
    if ( ret < 0 ) return;

    /* WRITE : F3 TRACE */
    write_buff = (unsigned long)ADDR_MODEM_F3_LOG;
    write_size = SIZE_MODEM_F3_LOG;
    ret = resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
    if ( ret < 0 ) return;
    Emmc_log_info.rec_info[write_index].f3_msg_buff_head      = pLogInfo->Err_F3_Trace_Buffer_Head;
    Emmc_log_info.rec_info[write_index].f3_msg_trace_wrap_flg = pLogInfo->Err_F3_Trace_Wrap_Flag;

    /* WRITE : ERR DATA(QUALCOMM) */
    write_buff = (unsigned long)ADDR_MODEM_ERR_DATA_LOG;
    write_size = SIZE_MODEM_ERR_DATA_LOG;
    ret = resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
    if ( ret < 0 ) return;
    Emmc_log_info.rec_info[write_index].qcom_err_data_size = pLogInfo->Sizeof_Err_Data;

	/* WRITE : CPU CONTEXT INFO */
	write_buff = (unsigned long)ADDR_CPU_CONTEXT_INFO;
	write_size = SIZE_CPU_CONTEXT_INFO;
	ret = resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
	if ( ret < 0 ) return;

    /* WRITE : USB DUMP DATA(QUALCOMM) */
    memset(ADDR_USB_DUMP_START, 0x00, (ADDR_USB_DUMP_END - ADDR_USB_DUMP_START));
    write_buff = (unsigned long)ADDR_USB_DUMP_START;
    write_size = ADDR_USB_DUMP_END - ADDR_USB_DUMP_START;
    ret = resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
    if ( ret < 0 ) return;
    Emmc_log_info.rec_info[write_index].ocimem_size       = SIZE_OCIMEM;
    Emmc_log_info.rec_info[write_index].rpm_code_size     = SIZE_RPM_CODE;
    Emmc_log_info.rec_info[write_index].rpm_data_size     = SIZE_RPM_DATA;
    Emmc_log_info.rec_info[write_index].rpm_msg_size      = SIZE_RPM_MSG;
    Emmc_log_info.rec_info[write_index].lpm_size          = SIZE_LPM;
    Emmc_log_info.rec_info[write_index].reset_status_size = SIZE_RESET_STATUS;

    /* Write to eMMC */
    Emmc_log_info.valid_flg[write_index] = 1;
    Emmc_log_info.write_index = ((write_index + 1) % EMMC_INDEX_MAX);
    write_buff = (unsigned long)&Emmc_log_info;
    write_size = sizeof(Emmc_log_info);
    write_sector = EMMC_INFO_START_SECTOR;
    resetlog_flash_write( &write_sector, (unsigned char *)write_buff, write_size );
}

void dump_kcj_log( void )
{
	ram_log_info_type *pPanicInfo   = (ram_log_info_type *)ADDR_CONTROL_INFO;
	unsigned long save_Err_F3_Trace_Buffer_Head;
	unsigned long save_Err_F3_Trace_Wrap_Flag;
	unsigned long save_pSmem_log_write_idx;
#if 0
	unsigned long tmp_pSmem_log_events;
	unsigned long tmp_pSmem_log_write_idx;
#endif

	if ( (0 == strncmp((const char *)pPanicInfo->crash_system, CRASH_SYSTEM_KERNEL  , strlen(CRASH_SYSTEM_KERNEL ))) ||
	     (0 == strncmp((const char *)pPanicInfo->crash_system, CRASH_SYSTEM_MODEM   , strlen(CRASH_SYSTEM_MODEM  ))) ||
	     (0 == strncmp((const char *)pPanicInfo->crash_system, CRASH_SYSTEM_PRONTO  , strlen(CRASH_SYSTEM_PRONTO ))) ||
	     (0 == strncmp((const char *)pPanicInfo->crash_system, CRASH_SYSTEM_ADSP    , strlen(CRASH_SYSTEM_ADSP   ))) ||
	     (0 == strncmp((const char *)pPanicInfo->crash_system, CRASH_SYSTEM_VENUS   , strlen(CRASH_SYSTEM_VENUS  ))) ||
	     (0 == strncmp((const char *)pPanicInfo->crash_system, CRASH_SYSTEM_ANDROID , strlen(CRASH_SYSTEM_ANDROID))) ||
	     (0 == strncmp((const char *)pPanicInfo->crash_system, CRASH_SYSTEM_UNKNOWN , strlen(CRASH_SYSTEM_UNKNOWN))) )
	{
		save_Err_F3_Trace_Buffer_Head = pPanicInfo->Err_F3_Trace_Buffer_Head;
		save_Err_F3_Trace_Wrap_Flag   = pPanicInfo->Err_F3_Trace_Wrap_Flag;
		save_pSmem_log_write_idx      = pPanicInfo->pSmem_log_write_idx;

#if 0
		if (0 == strncmp((const char *)pPanicInfo->crash_system, CRASH_SYSTEM_MODEM   , strlen(CRASH_SYSTEM_MODEM  )))
		{
			void *device_mem = NULL;

			device_mem = ioremap_nocache(pPanicInfo->pErr_F3_Trace_Buffer, pPanicInfo->Sizeof_Err_F3_Trace_Buffer);
			if (device_mem)
			{
				memcpy( (void *)ADDR_MODEM_F3_LOG, device_mem, min((unsigned long)SIZE_MODEM_F3_LOG, pPanicInfo->Sizeof_Err_F3_Trace_Buffer) );
				iounmap(device_mem);
			}
			device_mem = ioremap_nocache(pPanicInfo->pErr_Data, pPanicInfo->Sizeof_Err_Data);
			if (device_mem)
			{
				memcpy( (void *)ADDR_MODEM_ERR_DATA_LOG, device_mem, min((unsigned long)SIZE_MODEM_ERR_DATA_LOG, pPanicInfo->Sizeof_Err_Data) );
				iounmap(device_mem);
			}
			device_mem = ioremap_nocache(pPanicInfo->pSmem_log_events, sizeof(unsigned long));
			if (device_mem)
			{
				tmp_pSmem_log_events = *(unsigned long *)device_mem;
				iounmap(device_mem);

				device_mem = ioremap_nocache(tmp_pSmem_log_events, pPanicInfo->Sizeof_Smem_log);
				if (device_mem)
				{
					memcpy( (void *)ADDR_SMEM_EVENT_LOG, device_mem, min((unsigned long)SIZE_SMEM_EVENT_LOG, pPanicInfo->Sizeof_Smem_log) );
					iounmap(device_mem);
				}
			}
			device_mem = ioremap_nocache(pPanicInfo->Err_F3_Trace_Buffer_Head, sizeof(unsigned long));
			if (device_mem)
			{
				pPanicInfo->Err_F3_Trace_Buffer_Head = *(unsigned long *)device_mem;
				iounmap(device_mem);
			}
			device_mem = ioremap_nocache(pPanicInfo->Err_F3_Trace_Wrap_Flag, sizeof(unsigned long));
			if (device_mem)
			{
				pPanicInfo->Err_F3_Trace_Wrap_Flag   = *(unsigned long *)device_mem;
				iounmap(device_mem);
			}
			device_mem = ioremap_nocache(pPanicInfo->pSmem_log_write_idx, sizeof(unsigned long));
			if (device_mem)
			{
				tmp_pSmem_log_write_idx = *(unsigned long *)device_mem;
				iounmap(device_mem);

				device_mem = ioremap_nocache(tmp_pSmem_log_write_idx, sizeof(unsigned long));
				if (device_mem)
				{
					pPanicInfo->pSmem_log_write_idx      = *(unsigned long *)device_mem;
					iounmap(device_mem);
				}
			}
		}
		else
#endif
		{
			memset( (void *)ADDR_MODEM_F3_LOG, 0x00, SIZE_MODEM_F3_LOG );
			memset( (void *)ADDR_MODEM_ERR_DATA_LOG, 0x00, SIZE_MODEM_ERR_DATA_LOG );
			memset( (void *)ADDR_SMEM_EVENT_LOG, 0x00, SIZE_SMEM_EVENT_LOG );
			pPanicInfo->Err_F3_Trace_Buffer_Head = 0x00;
			pPanicInfo->Err_F3_Trace_Wrap_Flag   = 0x00;
			pPanicInfo->pSmem_log_write_idx      = 0x00;
		}
		memset(ADDR_CPU_CONTEXT_INFO, 0x00, SIZE_CPU_CONTEXT_INFO);

		resetlog_to_eMMC_kyocera_write();

		pPanicInfo->Err_F3_Trace_Buffer_Head = save_Err_F3_Trace_Buffer_Head;
		pPanicInfo->Err_F3_Trace_Wrap_Flag   = save_Err_F3_Trace_Wrap_Flag;
		pPanicInfo->pSmem_log_write_idx      = save_pSmem_log_write_idx;
	}

    clear_kcj_crash_info();
	clear_smem_crash_system();
	clear_smem_crash_kind();
	clear_smem_crash_info_data();
	clear_smem_panic_info_data();
}

static int __init kcjlog_init(void)
{
    memset( ADDR_CONTROL_INFO, 0x00, SIZE_CONTROL_INFO );
	if(bark_regaddr != 0)
	{
		set_kcj_regsave_addr(bark_regaddr);
	}
	set_kcj_fixed_info();
	set_crash_unknown();
	set_kcj_init_info();
	return 0;
}

#ifdef CONFIG_DEBUG_LAST_LOGCAT
static int __init create_last_log_device(struct logger_log *log, unsigned char *log_addr, unsigned long log_size)
{
	int ret;

	log->buffer = log_addr;
	log->w_off  = log_size;

	ret = init_log(log);
	if (ret != 0)
	{
		return ret;
	}

	return 0;
}

static unsigned char * __init create_last_log_info(struct logger_log_info *pLastLogInfo, const unsigned char *src,
								unsigned long max_size, unsigned long *log_size)
{
	unsigned long write_size = 0;
	unsigned long write_size_second = 0;
	unsigned char *dst = NULL;

	if ( (pLastLogInfo->head  >= max_size) ||
	     (pLastLogInfo->w_off >= max_size) )
	{
		return NULL;
	}

	if (pLastLogInfo->head > pLastLogInfo->w_off)
	{
		write_size = max_size - pLastLogInfo->head;
		write_size_second = pLastLogInfo->w_off;
		*log_size = write_size + write_size_second;

		dst = kmalloc(max_size, GFP_KERNEL);
		if (dst == NULL)
			return NULL;

		memcpy(dst, src + pLastLogInfo->head, write_size);
		memcpy(dst + write_size, src, write_size_second);
	}
	else
	{
		write_size = pLastLogInfo->w_off - pLastLogInfo->head;
		*log_size = write_size;
		dst = kmalloc(max_size, GFP_KERNEL);
		if (dst == NULL)
			return NULL;

		memcpy(dst, src, write_size);
	}

	return dst;
}

static void __init create_last_log(struct logger_log_info *pLastLogInfo, const char *name,
							const unsigned char *src_addr, unsigned long max_size, struct logger_log *log)
{
	int ret = 0;
	unsigned char *dst_addr = NULL;
	unsigned long log_size = 0;

	dst_addr = create_last_log_info(pLastLogInfo, src_addr, max_size, &log_size);
	if (dst_addr == NULL)
	{
		pr_err("%s: failed to allocate buffer\n", name);
		return;
	}
	ret = create_last_log_device(log, dst_addr, log_size);
	if (ret != 0)
	{
		pr_err("%s: failed to register misc\n", name);
		kfree(dst_addr);
		return;
	}
}

static int __init last_log_init(void)
{
	last_log_uninit_info_type *pLastLogInfo = (last_log_uninit_info_type *)ADDR_LASTLOGCAT_INFO;

	if (pLastLogInfo->sig != LASTLOG_RAM_SIG)
	{
		pLastLogInfo->sig = LASTLOG_RAM_SIG;
		return 0;
	}

	create_last_log(ADDR_LASTLOG_INFO_MAIN,
					LOGGER_LOG_LAST_MAIN,
					(unsigned char*)ADDR_LOGCAT_MAIN,
					(unsigned long)SIZE_LOGCAT_MAIN,
					&log_last_main
	);

	create_last_log(ADDR_LASTLOG_INFO_SYSTEM,
					LOGGER_LOG_LAST_SYSTEM,
					(unsigned char*)ADDR_LOGCAT_SYSTEM,
					(unsigned long)SIZE_LOGCAT_SYSTEM,
					&log_last_system
	);

	create_last_log(ADDR_LASTLOG_INFO_EVENTS,
					LOGGER_LOG_LAST_EVENTS,
					(unsigned char*)ADDR_LOGCAT_EVENTS,
					(unsigned long)SIZE_LOGCAT_EVENTS,
					&log_last_events
	);

	create_last_log(ADDR_LASTLOG_INFO_RADIO,
					LOGGER_LOG_LAST_RADIO,
					(unsigned char*)ADDR_LOGCAT_RADIO,
					(unsigned long)SIZE_LOGCAT_RADIO,
					&log_last_radio
	);

	return 0;
}
#endif /* CONFIG_DEBUG_LAST_LOGCAT */

device_initcall(logger_init);
device_initcall(kcjlog_init);
#ifdef CONFIG_DEBUG_LAST_LOGCAT
device_initcall(last_log_init);
#endif /* CONFIG_DEBUG_LAST_LOGCAT */
