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

#include <string.h>
#include <fcntl.h>

#include "errors.h"
#include "fat.h"
#include "fat_wrapper.h"
#include "ipc.h"
#include "mem.h"
#include "syscalls.h"
#include "types.h"

#include "libfat/fatdir.h"
#include "libfat/fatfile.h"

/* Variables */
static struct _reent fReent;


char *__FAT_CheckPath(const char *path)
{
	static char newpath[MAX_FILENAME_LENGTH];
	char       *ptr;

	/* Copy path */
	strcpy(newpath, path);

	/* Find '/' */
	ptr = strchr(newpath, '/');
	if (ptr) {
		u32 cnt;

		/* Check path */
		for (cnt = 0; ptr[cnt]; cnt++) {
			/* Check character */
			switch (ptr[cnt]) {
			case '"':
			case '*':
			case ':':
			case '<':
			case '>':
			case '?':
			case '|':
				/* Replace character */
				ptr[cnt] = '_';
				break;
			}
		}
	}

	/* Return path */
	return newpath;
}

s32 __FAT_GetError(void)
{
	/* Return error code */
	return fReent._errno;
}

s32 __FAT_OpenDir(const char *dirpath, DIR_ITER *dir)
{
	DIR_ITER         *result = NULL;
	DIR_STATE_STRUCT *state  = NULL;

	/* Check path */
	dirpath = __FAT_CheckPath(dirpath);

	/* Allocate memory */
	state = Mem_Alloc(sizeof(DIR_STATE_STRUCT));
	if (!state)
		return IPC_ENOMEM;

	/* Clear buffer */
	memset(state, 0, sizeof(DIR_STATE_STRUCT));

	/* Prepare dir iterator */
	dir->device    = 0;
	dir->dirStruct = state;

	/* Clear error code */
	fReent._errno = 0;

	/* Open directory */
	result = _FAT_diropen_r(&fReent, dir, dirpath);

	if (!result) {
		/* Free memory */
		Mem_Free(state);

		/* Return error */
		return __FAT_GetError();
	}

	return 0;
}

void __FAT_CloseDir(DIR_ITER *dir)
{
	/* Close directory */
	_FAT_dirclose_r(&fReent, dir);

	/* Free memory */
	if (dir->dirStruct)
		Mem_Free(dir->dirStruct);
}


s32 FAT_Open(const char *path, u32 mode)
{
	FILE_STRUCT *fs = NULL;

	s32 ret;

	/* Check path */
	path = __FAT_CheckPath(path);

	/* Allocate memory */
	fs = Mem_Alloc(sizeof(FILE_STRUCT));
	if (!fs)
		return IPC_ENOMEM;

	/* Set mode */
	if (mode > 0)
		mode--;

	/* Clear error code */
	fReent._errno = 0;

	/* Open file */
	ret = _FAT_open_r(&fReent, fs, path, 2, 0);	
	if (ret < 0) {
		/* Free memory */
		Mem_Free(fs);

		return __FAT_GetError();
	}

	return ret;
}

s32 FAT_Close(s32 fd)
{
	/* Close file */
	return _FAT_close_r(&fReent, fd);
}

s32 FAT_Read(s32 fd, void *buffer, u32 len)
{
	s32 ret;

	/* Clear error code */
	fReent._errno = 0;

	/* Read file */
	ret = _FAT_read_r(&fReent, fd, buffer, len);
	if (ret < 0)
		ret = __FAT_GetError();

	return ret;
}

s32 FAT_Write(s32 fd, void *buffer, u32 len)
{
	s32 ret;

	/* Clear error code */
	fReent._errno = 0;

	/* Write file */
	ret = _FAT_write_r(&fReent, fd, buffer, len);
	if (ret < 0)
		ret = __FAT_GetError();

	return ret;
}

s32 FAT_Seek(s32 fd, u32 where, u32 whence)
{
	s32 ret;

	/* Clear error code */
	fReent._errno = 0;

	/* Seek file */
	ret = _FAT_seek_r(&fReent, fd, where, whence);
	if (ret < 0)
		ret = __FAT_GetError();

	return ret;
}

s32 FAT_CreateDir(const char *dirpath)
{
	s32 ret;

	/* Check path */
	dirpath = __FAT_CheckPath(dirpath);

	/* Clear error code */
	fReent._errno = 0;

	/* Create directory */
	ret = _FAT_mkdir_r(&fReent, dirpath, 0);
	if (ret < 0)
		ret = __FAT_GetError();

	return ret;
}

s32 FAT_CreateFile(const char *filepath)
{
	FILE_STRUCT fs;
	s32         ret;

	/* Check path */
	filepath = __FAT_CheckPath(filepath);

	/* Clear error code */
	fReent._errno = 0;

	/* Create file */
	ret = _FAT_open_r(&fReent, &fs, filepath, O_CREAT | O_RDWR, 0);
	if (ret < 0)
		return __FAT_GetError();

	/* Close file */
	_FAT_close_r(&fReent, ret);

	return 0;
}

s32 FAT_ReadDir(const char *dirpath, char *outbuf, u32 *outlen, u32 entries)
{
	DIR_ITER dir;

	u32 cnt = 0, pos = 0;
	s32 ret;

	/* Open directory */
	ret = __FAT_OpenDir(dirpath, &dir);
	if (ret < 0)
		return ret;

	/* Read entries */
	while (!entries || (entries > cnt)) {
		char filename[MAX_FILENAME_LENGTH];
		u32  len;

		/* Read entry */
		if (_FAT_dirnext_r(&fReent, &dir, filename, NULL))
			break;

		/* Non valid entry */
		if (!strcmp(filename, ".") || !strcmp(filename, ".."))
			continue;

		/* Filename length */
		len = strlen(filename);

		/* Filename too long */
		if (len >= MAX_ALIAS_LENGTH)
			continue;

		/* Copy to output */
		if (outbuf) {
			/* Copy filename */
			strcpy(outbuf + pos, filename);

			/* Update position */
			pos += len + 1;
		}

		/* Increase counter */
		cnt++;
	}

	/* Output values */
	*outlen = cnt;

	/* Close directory */
	__FAT_CloseDir(&dir);

	return 0;
}

s32 FAT_ReadDirLfn(const char *dirpath, char *outbuf, u32 *outlen, u32 entries)
{
	DIR_ITER dir;

	u32 cnt = 0, pos = 0;
	s32 ret;

	/* Open directory */
	ret = __FAT_OpenDir(dirpath, &dir);
	if (ret < 0)
		return ret;

	/* Read entries */
	while (!entries || (entries > cnt)) {
		char filename[MAX_FILENAME_LENGTH];
		u32  len;

		/* Read entry */
		if (_FAT_dirnext_r(&fReent, &dir, filename, NULL))
			break;

		/* Non valid entry */
		if (!strcmp(filename, ".") || !strcmp(filename, ".."))
			continue;

		/* Filename length */
		len = strlen(filename);

		/* Copy to output */
		if (outbuf) {
			/* Copy filename */
			strcpy(outbuf + pos, filename);

			/* Update position */
			pos += len + 1;
		}

		/* Increase counter */
		cnt++;
	}

	/* Output values */
	*outlen = cnt;

	/* Close directory */
	__FAT_CloseDir(&dir);

	return 0;
}

s32 FAT_Delete(const char *path)
{
	s32 ret;

	/* Check path */
	path = __FAT_CheckPath(path);

	/* Clear error code */
	fReent._errno = 0;

	/* Delete file/directory */
	ret = _FAT_unlink_r(&fReent, path);
	if (ret < 0)
		ret = __FAT_GetError();

	return ret;
}

s32 FAT_DeleteDir(const char *dirpath)
{
	DIR_ITER dir;

	s32 ret;

	/* Open directory */
	ret = __FAT_OpenDir(dirpath, &dir);
	if (ret < 0)
		return ret;

	/* Read entries */
	for (;;) {
		char filename[MAX_FILENAME_LENGTH];
		char  newpath[MAX_FILENAME_LENGTH];

		struct stat filestat;

		/* Read entry */
		if (_FAT_dirnext_r(&fReent, &dir, filename, &filestat))
			break;

		/* Non valid entry */
		if (!strcmp(filename, ".") || !strcmp(filename, ".."))
			continue;

		/* Generate entry path */
		strcpy(newpath, dirpath);
		strcat(newpath, "/");
		strcat(newpath, filename);

		/* Delete directory contents */
		if (filestat.st_mode & S_IFDIR)
			FAT_DeleteDir(newpath);

		/* Delete object */
		ret = FAT_Delete(newpath);

		/* Error */
		if (ret < 0)
			break;
	}

	/* Close directory */
	__FAT_CloseDir(&dir);

	return 0;
}

s32 FAT_Rename(const char *oldname, const char *newname)
{
	s32 ret;

	/* Check paths */
	oldname = __FAT_CheckPath(oldname);
	newname = __FAT_CheckPath(newname);

	/* Clear error code */
	fReent._errno = 0;

	/* Rename file/directory */
	ret = _FAT_rename_r(&fReent, oldname, newname);
	if (ret < 0)
		ret = __FAT_GetError();

	return ret;
} 

s32 FAT_Stat(const char *path, void *stats)
{
	s32 ret;

	/* Check path */
	path = __FAT_CheckPath(path);

	/* Clear error code */
	fReent._errno = 0;

	/* Get stats */
	ret = _FAT_stat_r(&fReent, path, stats);
	if (ret < 0)
		ret = __FAT_GetError();

	return ret;
}

s32 FAT_GetVfsStats(const char *path, void *stats)
{
	s32 ret;

	/* Check path */
	path = __FAT_CheckPath(path);

	/* Clear error code */
	fReent._errno = 0;

	/* Get filesystem stats */
	ret = _FAT_statvfs_r(&fReent, path, stats);
	if (ret < 0)
		ret = __FAT_GetError();

	return ret;
}

s32 FAT_GetFileStats(s32 fd, fstats *stats)
{
	FILE_STRUCT *fs = (FILE_STRUCT *)fd;

	if (!fs || !fs->inUse)
		return EINVAL;

	/* Fill file stats */
	stats->file_length = fs->filesize;
	stats->file_pos    = fs->currentPosition;

	return 0;
}

s32 FAT_GetUsage(const char *dirpath, u64 *size, u32 *files)
{
	DIR_ITER dir;

	u64 totalSz  = 0x4000;
	u32 totalCnt = 1;
	s32 ret;

	/* Open directory */
	ret = __FAT_OpenDir(dirpath, &dir);
	if (ret < 0)
		return ret;

	/* Read entries */
	for (;;) {
		char   filename[MAX_FILENAME_LENGTH];
		struct stat filestat;

		/* Read entry */
		if (_FAT_dirnext_r(&fReent, &dir, filename, &filestat))
			break;

		/* Non valid entry */
		if (!strcmp(filename, ".") || !strcmp(filename, ".."))
			continue;

		/* Directory or file */
		if (filestat.st_mode & S_IFDIR) {
			char newpath[MAX_FILENAME_LENGTH];

			u64  dirsize;
			u32  dirfiles;

			/* Generate directory path */
			strcpy(newpath, dirpath);
			strcat(newpath, "/");
			strcat(newpath, filename);

			/* Get directory usage */
			ret = FAT_GetUsage(newpath, &dirsize, &dirfiles);
			if (ret >= 0) {
				/* Update variables */
				totalSz  += dirsize;
				totalCnt += dirfiles;
			}
		} else
			totalSz += filestat.st_size;

		/* Increment counter */
		totalCnt++;
	}

	/* Output values */
	*size  = totalSz;
	*files = totalCnt;

	/* Close directory */
	__FAT_CloseDir(&dir);

	return 0;
}
