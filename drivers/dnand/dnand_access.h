#ifndef _DNAND_ACCESS_H_
#define _DNAND_ACCESS_H_
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2014 KYOCERA Corporation
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

#define IOC_MAGIC 'd'
#define IOCTL_DNAND_ACCESS_READ_CMD  _IOWR(IOC_MAGIC, 1, int)
#define IOCTL_DNAND_ACCESS_WRITE_CMD _IOWR(IOC_MAGIC, 2, int)


#endif /* _DNAND_ACCESS_H_ */

