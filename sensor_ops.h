/*
 * sensor_ops.h
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

#ifndef SENSOR_OPS_H
#define SENSOR_OPS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

extern pthread_mutex_t can_mutex;

void Sensor_Scanning(void);
int Sensor_GetData(byte *buf, int type);
int Camera_SetParameter(Ctl_image_device_t *par);
int Camera_GetParameter(Ctl_image_device_t *par);
int Camera_SetTimetable(Ctl_image_timetable_t *tb, byte groups, byte channel);
int Camera_StartCapture(char *filename, byte channel, byte presetting);
int Camera_Control(byte action, byte presetting, byte channel);
int Camera_Video_Start(byte channel, byte control, usint port);
int Camera_GetImages(char *ImageName, byte presetting, byte channel);
int Camera_NextTimer(void);

int Can_Send(byte *buf, int len);
int Can_Recv(byte *buf);
int Sensor_Can_ReadData(usint addr, byte *buf);
int Sensor_Can_Config(usint addr, usint t);

extern int Sensor_Zigbee_ReadData(byte *buf, int len);

int Sensor_Sample_Qixiang(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* SENSOR_OPS_H */
