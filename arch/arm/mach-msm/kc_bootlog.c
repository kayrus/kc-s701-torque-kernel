/* This software is contributed or developed by KYOCERA Corporation.
 * (C) 2014 KYOCERA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

MODULE_DESCRIPTION("KC Bootlog Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

#define FILEPATH "/dev/block/platform/msm_sdcc.1/by-name/bootwork"
#define BASE_OFFSET (0x100000ULL)
#define INDEX_OFFSET (BASE_OFFSET + 0x40)
#define RECORD_OFFSET (BASE_OFFSET + 0x200)
#define RECORD_SIZE (0x1600)

static char bootlog[RECORD_SIZE];
static int bootlog_initialized = false;
static int bootlog_valid = false;

static int read_current_record(char *buf, size_t buf_size)
{
	struct file *log_file;
	int32_t flags = O_RDONLY|O_SYNC|O_LARGEFILE;
	size_t size,srtn;
	mm_segment_t _segfs;
	uint32_t index;
	loff_t ofs;

	log_file = filp_open(FILEPATH, flags, 0);
	if (IS_ERR_OR_NULL(log_file)) {
		return -1;
	}

	_segfs = get_fs();
	set_fs(get_ds());

	ofs = INDEX_OFFSET;
	srtn = log_file->f_op->llseek(log_file, ofs, SEEK_SET);
	if (srtn != ofs) {
		set_fs(_segfs);
		filp_close(log_file,NULL);
		return -1;
	}

	size = 4;
	srtn = log_file->f_op->read(log_file, (char *)&index, size, &log_file->f_pos);
	if (srtn != size) {
		set_fs(_segfs);
		filp_close(log_file,NULL);
		return -1;
	}

	ofs = RECORD_OFFSET + (index * RECORD_SIZE);
	srtn = log_file->f_op->llseek(log_file, ofs, SEEK_SET);
	if (srtn != ofs) {
		set_fs(_segfs);
		filp_close(log_file,NULL);
		return -1;
	}

	size = min_t(size_t, RECORD_SIZE, buf_size);
	srtn = log_file->f_op->read(log_file, buf, size, &log_file->f_pos);
	if (srtn != size) {
		set_fs(_segfs);
		filp_close(log_file,NULL);
		return -1;
	}

	set_fs(_segfs);
	filp_close(log_file,NULL);

	return 0;
}

static void kc_bootlog_init_func(struct work_struct *work) {
	if (0 == read_current_record(bootlog, sizeof(bootlog))) {
		bootlog_valid = true;
	}
	else {
		pr_warn("%s: failed read bootlog\n", __func__);
	}
}

static DECLARE_WORK(kc_bootlog_init_work, kc_bootlog_init_func);

static int kc_bootlog_proc_show(struct seq_file *m, void *v)
{
	char *p = bootlog;
	size_t size = sizeof(bootlog);
	int i;

	if (!bootlog_initialized) {
		schedule_work(&kc_bootlog_init_work);
		flush_work(&kc_bootlog_init_work);
		bootlog_initialized = true;
	}

	if (bootlog_valid) {
		for (i = 0; i < size; i++) {
			if (*p && *p != '\r') {
				seq_putc(m, *p);
			}
			p++;
		}
	}

	return 0;
}

static int kc_bootlog_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, kc_bootlog_proc_show, NULL);
}

static const struct file_operations kc_bootlog_file_ops = {
	.open		= kc_bootlog_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init kc_bootlog_init(void)
{
	proc_create("kc_bootlog", S_IFREG | S_IRUGO, NULL, &kc_bootlog_file_ops);
	return 0;
}
late_initcall(kc_bootlog_init);
