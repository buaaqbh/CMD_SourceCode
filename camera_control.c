/*
 * camera_control.c
 *
 *  Created on: 2013年10月28日
 *      Author: qinbh
 */

#include <stdio.h>
#include <stdlib.h>
#include "camera_control.h"
#include "uart_ops.h"
#include "io_util.h"

/* PELCO-D Protocol */

#define  UART_PORT_RS485	"/dev/ttymxc4"
#define  UART_RS485_SPEDD 	9600

static byte checksum(byte *buf, int len)
{
	int i;
	int sum = 0;
	for (i = 0; i < len; i++)
		sum += buf[i];
	return (sum & 0xff);
}

int Camera_SendCmd(byte *cmd, int len)
{
	int fd;
	int err = 0;

	fd = uart_open_dev(UART_PORT_RS485);
	if (fd == -1) {
		perror("serial port open error");
		return -1;
	}
	uart_set_speed(fd, UART_RS485_SPEDD);
	if(uart_set_parity(fd, 8, 1, 'N') == -1) {
		printf ("Set Parity Error");
		return -1;
	}

	cmd[len - 1] = checksum((cmd + 1), 5);
	err = io_writen(fd, cmd, len);
	if (err > 0)
		printf("RS485: Send Command Sucess.\n");
	else {
		printf("write error, ret = %d\n", err);
		return -1;
	}

	return 0;
}

void Camera_PowerOn(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x88, 0x00, 0x00, 0x00, 0x89};
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_PowerOff(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x08, 0x00, 0x00, 0x00, 0x89};
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_CallPreset(byte addr, byte index)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x07, 0x00, 0x01, 0x09};
	cmd[1] = addr;
	cmd[5] = index;
	Camera_SendCmd(cmd, 7);
}

void Camera_SetPreset(byte addr, byte index)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x03, 0x00, 0x01, 0x05};
	cmd[1] = addr;
	cmd[5] = index;
	Camera_SendCmd(cmd, 7);
}

void Camera_DelPreset(byte addr, byte index)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x05, 0x00, 0x01, 0x07};
	cmd[1] = addr;
	cmd[5] = index;
	Camera_SendCmd(cmd, 7);
}

void Camera_MoveLeft(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x04, 0xff, 0x00, 0x04};
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_MoveRight(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x02, 0xff, 0x00, 0x02};
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_MoveUp(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x08, 0x00, 0xff, 0x08};
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_MoveDown(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x10, 0x00, 0xff, 0x10};
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_FocusFar(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x80, 0x00, 0x00, 0x81};
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_FocusNear(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x01, 0x00, 0x00, 0x00, 0x02};
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_CmdStop(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01};
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}
