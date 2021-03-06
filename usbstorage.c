/*   
	Custom IOS Module (FAT)

	Copyright (C) 2008 neimod.
	Copyright (C) 2009 WiiGator.
	Copyright (C) 2009 Waninkoko.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "syscalls.h"
#include "timer.h"
#include "types.h"
#include "usbstorage.h"

/* Device type */
#define DEVICE_TYPE_WII_USB		(('W'<<24) | ('U'<<16) | ('S'<<8) | 'B')

/* IOCTL commands */
#define UMS_BASE			(('U'<<24) | ('M'<<16) | ('S'<<8))
#define USB_IOCTL_UMS_INIT	        (UMS_BASE + 0x1)
#define USB_IOCTL_UMS_GET_CAPACITY      (UMS_BASE + 0x2)
#define USB_IOCTL_UMS_READ_SECTORS      (UMS_BASE + 0x3)
#define USB_IOCTL_UMS_WRITE_SECTORS	(UMS_BASE + 0x4)
#define USB_IOCTL_UMS_READ_STRESS	(UMS_BASE + 0x5)
#define USB_IOCTL_UMS_SET_VERBOSE	(UMS_BASE + 0x6)

/* Constants */
#define USB_MAX_SECTORS			64

/* Variables */
static char fs[] ATTRIBUTE_ALIGN(32) = "/dev/usb2";
static s32  fd = -1;

static u32 sectorSz = 0;

/* Buffers */
static ioctlv __iovec[3]   ATTRIBUTE_ALIGN(32);
static u32    __buffer1[1] ATTRIBUTE_ALIGN(32);
static u32    __buffer2[1] ATTRIBUTE_ALIGN(32);


bool __usbstorage_Read(u32 sector, u32 numSectors, void *buffer)
{
	ioctlv *vector = __iovec;
	u32    *offset = __buffer1;
	u32    *length = __buffer2;

	u32 cnt, len = (sectorSz * numSectors);
	s32 ret;

	/* Device not opened */
	if (fd < 0)
		return false;

	/* Sector info */
	*offset = sector;
	*length = numSectors;

	/* Setup vector */
	vector[0].data = offset;
	vector[0].len  = sizeof(u32);
	vector[1].data = length;
	vector[1].len  = sizeof(u32);
	vector[2].data = buffer;
	vector[2].len  = len;

	/* Flush cache */
	for (cnt = 0; cnt < 3; cnt++)
		os_sync_after_write(vector[cnt].data, vector[cnt].len);

	os_sync_after_write(vector, sizeof(ioctlv) * 3);

	/* Read data */
	ret = os_ioctlv(fd, USB_IOCTL_UMS_READ_SECTORS, 2, 1, vector);
	if (ret < 0)
		return false;

	/* Invalidate cache */
	for (cnt = 0; cnt < 3; cnt++)
		os_sync_before_read(vector[cnt].data, vector[cnt].len);

	return true;
}

bool __usbstorage_Write(u32 sector, u32 numSectors, void *buffer)
{
	ioctlv *vector = __iovec;
	u32    *offset = __buffer1;
	u32    *length = __buffer2;

	u32 cnt, len = (sectorSz * numSectors);
	s32 ret;

	/* Device not opened */
	if (fd < 0)
		return false;

	/* Sector info */
	*offset = sector;
	*length = numSectors;

	/* Setup vector */
	vector[0].data = offset;
	vector[0].len  = sizeof(u32);
	vector[1].data = length;
	vector[1].len  = sizeof(u32);
	vector[2].data = buffer;
	vector[2].len  = len;

	/* Flush cache */
	for (cnt = 0; cnt < 3; cnt++)
		os_sync_after_write(vector[cnt].data, vector[cnt].len);

	os_sync_after_write(vector, sizeof(ioctlv) * 3);

	/* Write data */
	ret = os_ioctlv(fd, USB_IOCTL_UMS_WRITE_SECTORS, 3, 0, vector);
	if (ret < 0)
		return false;

	/* Invalidate cache */
	for (cnt = 0; cnt < 3; cnt++)
		os_sync_before_read(vector[cnt].data, vector[cnt].len);

	return true;
}

s32 __usbstorage_GetCapacity(u32 *_sectorSz)
{
	ioctlv *vector = __iovec;
	u32    *buffer = __buffer1;

	if (fd >= 0) {
		s32 ret;

		/* Setup vector */
		vector[0].data = buffer;
		vector[0].len  = sizeof(u32);

		os_sync_after_write(vector, sizeof(ioctlv));

		/* Get capacity */
		ret = os_ioctlv(fd, USB_IOCTL_UMS_GET_CAPACITY, 0, 1, vector);

		/* Flush cache */
		os_sync_after_write(buffer, sizeof(u32));

		/* Set sector size */
		sectorSz = buffer[0];

		if (ret && _sectorSz)
			*_sectorSz = sectorSz;

		return ret;
	}

	return IPC_ENOENT;
}


bool usbstorage_Init(void)
{
	s32 ret;

	/* Already open */
	if (fd >= 0)
		return true;

	/* Open USB device */
	fd = os_open(fs, 0);
	if (fd < 0)
		return false;

	/* Initialize USB storage */
	os_ioctlv(fd, USB_IOCTL_UMS_INIT, 0, 0, NULL);

	/* Get device capacity */
	ret = __usbstorage_GetCapacity(NULL);
	if (ret <= 0)
		goto err;

	return true;

err:
	/* Close USB device */
	usbstorage_Shutdown();	

	return false;
}

bool usbstorage_Shutdown(void)
{
	/* Close USB device */
	if (fd >= 0)
		os_close(fd);

	/* Reset descriptor */
	fd = -1;

	return true;
}

bool usbstorage_IsInserted(void)
{
	s32 ret;

	/* Get device capacity */
	ret = __usbstorage_GetCapacity(NULL);

	return (ret > 0);
}

bool usbstorage_ReadSectors(u32 sector, u32 numSectors, void *buffer)
{
	u32 cnt = 0;
	s32 ret;

	/* Device not opened */
	if (fd < 0)
		return false;

	while (cnt < numSectors) {
		void *ptr = (char *)buffer + (sectorSz * cnt);

		u32  _sector     = sector + cnt;
		u32  _numSectors = numSectors - cnt;

		/* Limit sector count */
		if (_numSectors > USB_MAX_SECTORS)
			_numSectors = USB_MAX_SECTORS;

		/* Read sectors */
		ret = __usbstorage_Read(_sector, _numSectors, ptr);
		if (!ret)
			return false;

		/* Increase counter */
		cnt += _numSectors;
	}

	return true;
}

bool usbstorage_WriteSectors(u32 sector, u32 numSectors, void *buffer)
{
	u32 cnt = 0;
	s32 ret;

	/* Device not opened */
	if (fd < 0)
		return false;

	while (cnt < numSectors) {
		void *ptr = (char *)buffer + (sectorSz * cnt);

		u32  _sector     = sector + cnt;
		u32  _numSectors = numSectors - cnt;

		/* Limit sector count */
		if (_numSectors > USB_MAX_SECTORS)
			_numSectors = USB_MAX_SECTORS;

		/* Write sectors */
		ret = __usbstorage_Write(_sector, _numSectors, ptr);
		if (!ret)
			return false;

		/* Increase counter */
		cnt += _numSectors;
	}

	return true;
}

bool usbstorage_ClearStatus(void)
{
	return true;
}


/* Disc interface */
const DISC_INTERFACE __io_usbstorage = {
	DEVICE_TYPE_WII_USB,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_USB,
	(FN_MEDIUM_STARTUP)     &usbstorage_Init,
	(FN_MEDIUM_ISINSERTED)  &usbstorage_IsInserted,
	(FN_MEDIUM_READSECTORS) &usbstorage_ReadSectors,
	(FN_MEDIUM_WRITESECTORS)&usbstorage_WriteSectors,
	(FN_MEDIUM_CLEARSTATUS) &usbstorage_ClearStatus,
	(FN_MEDIUM_SHUTDOWN)    &usbstorage_Shutdown
};
