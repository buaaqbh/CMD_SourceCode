/*
 * file_ops.c
 *
 *  Created on: 2013年10月23日
 *      Author: qinbh
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "file_ops.h"

static int get_file_size(const char *filename)
{
	struct stat buf;
	if(stat(filename, &buf)<0) {
		return 0;
	}
	return buf.st_size;
}

int File_Open(const char *FileName)
{
	int fd;
	fd = open(FileName, O_RDWR | O_CREAT, 0664);
	if (fd == -1)
		return -1;
	return fd;
}

void File_Close(int fd)
{
	close(fd);
}

int File_Create(const char *FileName, int size)
{
	int fd;

	if (File_Exist(FileName))
		return 0;

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	File_Close(fd);

	if (truncate(FileName, size) < 0)
		return -1;

	return 0;
}

int File_Clear(const char *FileName)
{
	return truncate(FileName, 0);
}

int File_Delete(const char *FileName)
{
	return remove(FileName);
}

int File_Exist(const char *FileName)
{
	if(access(FileName, F_OK) == 0)
		return 1;
	else
		return 0;
}

long File_GetFreeSpace(const char *drive)
{
	struct statfs diskInfo;
	unsigned long long blocksize;
	unsigned long long availableDisk;

	statfs(drive, &diskInfo);

	blocksize = diskInfo.f_bsize;
	availableDisk = diskInfo.f_bavail * blocksize;

	printf("Disk_available = %llu GB = %llu MB = %lluKB = %lluB\n",
			availableDisk>>30, availableDisk>>20, availableDisk>>10, availableDisk);

	return availableDisk;
}

int File_GetNumberOfRecords(const char *FileName, int Record_Len)
{
	int size;

	size = get_file_size(FileName);
//	printf("File size = %d\n", size);
	if (size == 0)
		return 0;

	if (Record_Len <= 0)
		return 0;

	return (size / Record_Len);
}

int File_AppendRecord(const char *FileName, void *Record, int Record_Len)
{
	int fd;

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	if (lseek(fd, 0, SEEK_END) < 0) {
		File_Close(fd);
		return -1;
	}

	if (write(fd, Record, Record_Len) != Record_Len) {
		File_Close(fd);
		return -1;
	}

	File_Close(fd);

	return Record_Len;
}

int File_GetRecordByIndex(const char *FileName, void *Record, int Record_Len, int Record_Index)
{
	int fd;

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	if (lseek(fd, (Record_Index * Record_Len), SEEK_SET) < 0) {
		File_Close(fd);
		return -1;
	}

	if (read(fd, Record, Record_Len) != Record_Len) {
		File_Close(fd);
		return -1;
	}

	File_Close(fd);

	return Record_Len;
}

int File_UpdateRecordByIndex(const char *FileName, void *Record, int Record_Len, int Record_Index)
{
	int fd;
	char *fbp = NULL;
	int size = get_file_size(FileName);

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	fbp = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fbp == MAP_FAILED) {
		printf("file mmap error.\n");
		File_Close(fd);
		return -1;
	}

	memcpy((fbp + (Record_Len * Record_Index)), Record, Record_Len);

	if (munmap(fbp, size) < 0) {
		printf("file munmap error.\n");
	}

	File_Close(fd);

	return Record_Len;
}

int File_DeleteRecordByIndex(const char *FileName, int Record_Len, int Record_Index)
{
	int fd;
	char *fbp = NULL;
	int size = get_file_size(FileName);
	char *to, *from;

	if ((Record_Index + 1) > (size / Record_Len)) {
		printf("File: Invalid Record Index.\n");
		return -1;
	}

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	fbp = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fbp == MAP_FAILED) {
		printf("file mmap error.\n");
		File_Close(fd);
		return -1;
	}

	to = fbp + (Record_Len * Record_Index);
	from = fbp + (Record_Len * (Record_Index + 1));
	memmove(to, from, (size - (Record_Len * (Record_Index + 1))));

	if (munmap(fbp, size) < 0) {
		printf("file munmap error.\n");
	}

	File_Close(fd);

	truncate(FileName, (size - Record_Len));

	return 0;
}

int File_UpgradeWrite(const char *FileName, int index, int data_len, void *buf)
{
	int fd;
	int i, len;
	int ret;

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	i = len = 0;

	lseek(fd, 0, SEEK_SET);
	while (1) {
		if ((ret = read(fd, &i, 4)) <= 0) {
			break;
		}
		if (read(fd, &len, 4) <= 0)
			break;
		lseek(fd, len, SEEK_CUR);
		if (index < i) {
			lseek(fd, (-8 - len), SEEK_CUR);
			if (write(fd, &index, 4) != 4) {
				File_Close(fd);
				return -1;
			}
			if (write(fd, &data_len, 4) != 4) {
				File_Close(fd);
				return -1;
			}
			if (write(fd, buf, data_len) != data_len) {
				File_Close(fd);
				return -1;
			}
			break;
		}
	}

	if (read(fd, &i, 4) == 0) {
		if (write(fd, &index, 4) != 4) {
			File_Close(fd);
			return -1;
		}
		if (write(fd, &data_len, 4) != 4) {
			File_Close(fd);
			return -1;
		}
		if (write(fd, buf, data_len) != data_len) {
			File_Close(fd);
			return -1;
		}
	}

	File_Close(fd);

	return 0;
}

int File_UpgradeWrite_mmap(const char *FileName, int index, int data_len, void *buf)
{
	char *fbp = NULL, *p = NULL;
	int fd;
	int i, len;
	int size;

	if (File_Exist(FileName) == 0)
		File_Create(FileName, 0);

	size = get_file_size(FileName);
	size += data_len + 8;
	truncate(FileName, size);

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	fbp = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fbp == MAP_FAILED) {
		printf("file mmap error.\n");
		File_Close(fd);
		return -1;
	}

	i = len = 0;
	p = fbp;
	memset((fbp + size - data_len -8), 0, (data_len + 8));
	while (1) {
		memcpy(&i, p, 4);
		memcpy(&len, (p + 4), 4);
		if ((i == 0) && (len == 0))
			break;

		if (index < i) {
			memmove((p + data_len + 8), p, (fbp + size - p - data_len - 8));
			memcpy(p, &index, 4);
			memcpy((p + 4), &data_len, 4);
			memcpy((p + 8), buf, data_len);
			break;
		}
		p += len + 8;
	}

	if ((i == 0) && (len == 0)) {
		memcpy(p, &index, 4);
		memcpy((p + 4), &data_len, 4);
		memcpy((p + 8), buf, data_len);
	}

	if (munmap(fbp, size) < 0) {
		printf("file munmap error.\n");
	}

	File_Close(fd);

	return 0;
}

int File_UpgradeConstruct(const char *inFile, const char *outFile)
{
	int in_fd, out_fd;
	int i, len;
	char buf[2048];

	if ((in_fd = File_Open(inFile)) < 0)
		return -1;
	if ((out_fd = File_Open(outFile)) < 0)
		return -1;

	while (read(in_fd, &i, 4) > 0) {
		if (read(in_fd, &len, 4) <= 0)
			break;
		memset(buf, 0, 2048);
		if (read(in_fd, buf, len) <= 0)
			break;

		if (write(out_fd, buf, len) != len)
			break;
	}

	File_Close(in_fd);
	File_Close(out_fd);

	return 0;
}

int File_UpdateBitmap(const char *FileName, int index, int num)
{
	int fd;
	char *fbp = NULL;
	int file_size;

	if ((index < 1) || (index > num))
		return -1;

	if (File_Exist(FileName) == 0) {
		if (File_Create(FileName, num) < 0)
			return -1;
	}

	file_size = get_file_size(FileName);
	if (file_size != num)
		truncate(FileName, num);

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	fbp = mmap(NULL, num, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fbp == MAP_FAILED) {
		printf("file mmap error.\n");
		File_Close(fd);
		return -1;
	}

	fbp[index - 1] = 1;

	if (munmap(fbp, num) < 0) {
		printf("file munmap error.\n");
	}

	File_Close(fd);

	return 0;
}

int File_UpdateGetLost(const char *FileName, int num, int *lost)
{
	int fd;
	char *fbp = NULL;
	int i;
	int count = 0;

	if (num  < 0)
		return -1;

	if (File_Exist(FileName) == 0) {
		return -1;
	}

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	fbp = mmap(NULL, num, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fbp == MAP_FAILED) {
		printf("file mmap error.\n");
		File_Close(fd);
		return -1;
	}

	for (i = 0; i < num; i++) {
		if (fbp[i] != 1) {
			*lost++ = i + 1;
			count++;
		}
	}

	if (munmap(fbp, num) < 0) {
		printf("file munmap error.\n");
	}

	File_Close(fd);

	return count;
}

