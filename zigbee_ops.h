/*
 * zigbee_ops.h
 *
 *  Created on: 2013年9月29日
 *      Author: qinbh
 */

#ifndef ZIGBEE_OPS_H_
#define ZIGBEE_OPS_H_

#include "types.h"

int   Zigbee_Device_Init(void);
int   Zigbee_Get_Device(int speed);
void  Zigbee_Release_Device(int fd);
int   Zigbee_Set_PanID(int fd, unsigned char *pan_id);
int   Zigbee_Read_PanID(int fd, byte *pan_id);
int   Zigbee_Read_ShortAddr(int fd, byte *short_addr);
int   Zigbee_Set_Bitrate(int fd, int speed);
int   Zigbee_Read_MAC(int fd, byte *mac);
int   Zigbee_Set_type(int fd, int type);
int   Zigbee_Get_type(int fd);
int   Zigbee_Set_Channel(int fd, int channel);
int   Zigbee_Get_channel(int fd);
int   Zigbee_Set_TransType(int fd, int type);
int   Zigbee_Reset(int fd);
int   Zigbee_Set_RouterAddr(int fd, usint addr);
usint Zigbee_Read_RouterAddr(int fd);

#endif /* ZIGBEE_OPS_H_ */
