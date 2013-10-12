/*
 * cma_commu.h
 * 
 * Copyright 2013 qinbh <buaaqbh@gmail.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301  USA.
 * 
 */

#ifndef CMA_COMMU_H
#define CMA_COMMU_H
#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

int Commu_GetPacket(int fd, byte *rbuf, int len);
int Commu_GetPacket_Udp(int fd, byte *rbuf, int len);
int Commu_SendPacket(int fd, frame_head_t *head, byte *data);

int CMA_Send_SensorData(int fd, int type);
int CMA_Server_Process(int fd, byte *rbuf);
int CMA_Time_SetReq_Response(int fd, byte *rbuf);
int CMA_NetAdapter_SetReq_Response(int fd, byte *rbuf);
int CMA_RequestData_Response(int fd, byte *rbuf);
int CMA_SamplePar_SetReq_Response(int fd, byte *rbuf);
int CMA_ModelPar_SetReq_Response(int fd, byte *rbuf);
int CMA_Alarm_SetReq_Response(int fd, byte *rbuf);
int CMA_UpDevice_SetReq_Response(int fd, byte *rbuf);
int CMA_BasicInfo_SetReq_Response(int fd, byte *rbuf);
int CMA_SoftWare_Update_Response(int fd, byte *rbuf);
int CMA_DeviceId_SetReq_Response(int fd, byte *rbuf);
int CMA_DeviceRset_Response(int fd, byte *rbuf);
int CMA_WakeupTime_Response(int fd, byte *rbuf);
int CMA_SetImagePar_Response(int fd, byte *rbuf);
int CMA_CaptureTimetable_Response(int fd, byte *rbuf);
int CMA_ManualCapture_Response(int fd, byte *rbuf);
int CMA_Image_SendRequest(int fd, char *id);
int CMA_Image_SendData(int fd, char *id);
int CMA_Image_SendData_End(int fd, char *id);
int CMA_CameraControl_Response(int fd, byte *rbuf);
int CMA_Video_StopStart_Response(int fd, byte *rbuf);
int CMA_Send_HeartBeat(int fd, char *id);
int CMA_Send_BasicInfo(int fd, char *id);
int CMA_Send_WorkStatus(int fd, char *id);
int CMA_Send_Fault_Info(int fd, char *id, char *fault_desc);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* CMA_COMMU_H */
