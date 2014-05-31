/*
 * camera_control.c
 *
 *  Created on: 2013年10月28日
 *      Author: qinbh
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "camera_control.h"
#include "uart_ops.h"
#include "io_util.h"
#include "device.h"

/* PELCO-D Protocol */

#define  ACTION_INTERVAL	(800*1000)

extern pthread_mutex_t rs485_mutex;
volatile int av_rs485_used = 0;

#define CAMERA_RS485_PORT	"/dev/ttymxc1"

//#define _DEBUG

#ifdef _DEBUG
#define Enter_func()  logcat("----- Enter func: %s --------\n", __func__)
#else
#define Enter_func()
#endif

#ifdef _DEBUG
static void print_message(byte *buf, int len)
{
	int i;
	logcat("");
	for(i = 0; i<len; i++) {
		logcat_raw("0x%02x ", buf[i]);
		if (((i+1)%16) == 0)
			logcat("\n");
	}
	logcat("\n");
}
#endif

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
	byte cmd_stop[7] = {0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01};

	pthread_mutex_lock(&rs485_mutex);
	av_rs485_used = 1;

//	fd = uart_open_dev(UART_PORT_RS485);
	fd = uart_open_dev(CAMERA_RS485_PORT);
	if (fd == -1) {
		logcat("serial port open error: %s\n", strerror(errno));
		pthread_mutex_unlock(&rs485_mutex);
		return -1;
	}
	uart_set_speed(fd, UART_RS485_SPEDD);
	if(uart_set_parity(fd, 8, 1, 'N') == -1) {
		logcat ("Set Parity Error\n");
		pthread_mutex_unlock(&rs485_mutex);
		return -1;
	}

	system("echo 11 >/sys/devices/platform/gpio-power.0/rs485_direction");
	usleep(10 * 1000);
	cmd[len - 1] = checksum((cmd + 1), 5);
	err = io_writen(fd, cmd, len);
	if (err > 0)
		logcat("RS485: Send Command Sucess.\n");
	else {
		logcat("write error, ret = %d\n", err);
		pthread_mutex_unlock(&rs485_mutex);
		return -1;
	}

	usleep(ACTION_INTERVAL);

    /* Stop Cmd */
    cmd_stop[6] = checksum((cmd_stop + 1), 5);
    err = io_writen(fd, cmd_stop, 7);
    if (err == 7)
            logcat("RS485: Send Stop Command Sucess.\n");
    else {
            logcat("write error, ret = %d\n", err);
            pthread_mutex_unlock(&rs485_mutex);
            return -1;
    }

#ifdef _DEBUG
	print_message(cmd, len);
#endif

	pthread_mutex_unlock(&rs485_mutex);

	return 0;
}

void Camera_PowerOn(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x88, 0x00, 0x00, 0x00, 0x89};
	Enter_func();

	Device_power_ctl(DEVICE_AV, 1);

	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_PowerOff(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x08, 0x00, 0x00, 0x00, 0x89};
	Enter_func();
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);

	Device_power_ctl(DEVICE_AV, 0);
}

void Camera_CallPreset(byte addr, byte index)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x07, 0x00, 0x01, 0x09};
	Enter_func();
	logcat("Camera_CallPreset: index = %d \n", index);
	cmd[1] = addr;
	cmd[5] = index;
	Camera_SendCmd(cmd, 7);
}

void Camera_SetPreset(byte addr, byte index)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x03, 0x00, 0x01, 0x05};
	Enter_func();
	logcat("Camera_SetPreset: index = %d \n", index);
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
	byte cmd[7] = {0xff, 0x01, 0x00, 0x04, 0x20, 0x00, 0x25};
	Enter_func();
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_MoveRight(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x02, 0x20, 0x00, 0x23};
	Enter_func();
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_MoveUp(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x08, 0x00, 0x20, 0x29};
	Enter_func();
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_MoveDown(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x10, 0x00, 0x20, 0x31};
	Enter_func();
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_FocusFar(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x80, 0x00, 0x00, 0x81};
	Enter_func();
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_FocusNear(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x01, 0x00, 0x00, 0x00, 0x02};
	Enter_func();
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}

void Camera_CmdStop(byte addr)
{
	byte cmd[7] = {0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01};
	Enter_func();
	cmd[1] = addr;
	Camera_SendCmd(cmd, 7);
}
