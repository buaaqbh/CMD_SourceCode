/*
 * file_ops.h
 *
 *  Created on: 2013年10月23日
 *      Author: qinbh
 */

#ifndef FILE_OPS_H_
#define FILE_OPS_H_

int File_Open(const char *FileName);
void File_Close(int fd);
int File_Create(const char *FileName, int size);
int File_Clear(const char *FileName);
int File_Delete(const char *FileName);
int File_Exist(const char *FileName);
long File_GetFreeSpace(const char *drive);
int File_GetNumberOfRecords(const char *FileName, int Record_Len);
int File_AppendRecord(const char *FileName, void *Record, int Record_Len);
int File_GetRecordByIndex(const char *FileName, void *Record, int Record_Len, int Record_Index);
int File_UpdateRecordByIndex(const char *FileName, void *Record, int Record_Len, int Record_Index);
int File_DeleteRecordByIndex(const char *FileName, int Record_Len, int Record_Index);
int File_UpdateGetLost(const char *FileName, int num, int *lost);
int File_UpgradeConstruct(const char *inFile, const char *outFile);
int File_UpgradeWrite(const char *FileName, int index, int data_len, void *buf);
int File_UpgradeWrite_mmap(const char *FileName, int index, int data_len, void *buf);
int File_UpdateBitmap(const char *FileName, int index, int num);

#endif /* FILE_OPS_H_ */
