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
	fd = open(FileName, O_RDWR | O_CREAT);
	if (fd == -1)
		return -1;
	return fd;
}

void File_Close(int fd)
{
	close(fd);
}

int File_Clear(const char *FileName)
{
	return truncate(FileName, 0);
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
	printf("File size = %d\n", size);
	if (size == 0)
		return 0;

	if (Record_Len <= 0)
		return 0;

	return (size / Record_Len);
}

int File_AppendRecord(const char *FileName, char *Record, int Record_Len)
{
	int fd;

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	if (lseek(fd, 0, SEEK_END) < 0)
		return -1;

	if (write(fd, Record, Record_Len) != Record_Len)
		return -1;

	File_Close(fd);

	return Record_Len;
}

int File_GetRecordByIndex(const char *FileName, void *Record, int Record_Len, int Record_Index)
{
	int fd;

	if ((fd = File_Open(FileName)) < 0)
		return -1;

	if (lseek(fd, (Record_Index * Record_Len), SEEK_SET) < 0)
		return -1;

	if (read(fd, Record, Record_Len) != Record_Len)
		return -1;

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
		return -1;
	}

	to = fbp + (Record_Len * Record_Index);
	from = fbp + (Record_Len * (Record_Index + 1));
	memmove(to, from, (size - (Record_Len * (Record_Index + 1))));

	if (munmap(fbp, size) < 0) {
		printf("file munmap error.\n");
	}

	File_Close(fd);

	return 0;
}

