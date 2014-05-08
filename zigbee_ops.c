/*
 * zigbee_ops.c
 *
 *  Created on: 2013年9月29日
 *      Author: qinbh
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "io_util.h"
#include "zigbee_ops.h"
#include "uart_ops.h"
#include "device.h"

#define  ZIGBEE_UART_NAME 	"/dev/ttymxc1"
//#define  ZIGBEE_UART_NAME 	"/dev/ttyS0"
#define  ZIGBEE_UART_SPEED 	9600

static int zigbee_bitrate[] = {9600, 19200, 38400, 57600, 115200};

#define _DEBUG
#ifdef _DEBUG
static void debug_out(byte *buf, int len)
{
	int i;
	for (i = 0; i < len; i++)
		logcat_raw("0x%02x ", (unsigned char)buf[i]);
	logcat_raw("\n");
}
#endif

static unsigned char sum_check(unsigned char *buf, int len)
{
	unsigned int sum = 0;
	int i;
	for (i = 0; i < len; i++)
		sum += buf[i];

	return (sum & 0xff);
}

int Zigbee_Get_Device(int speed)
{
	int fd;

	fd = uart_open_dev(ZIGBEE_UART_NAME);
	if (fd == -1) {
		logcat("serial port open error: %s\n", strerror(errno));
		return -1;
	}

	uart_set_speed(fd, speed);
	if(uart_set_parity(fd, 8, 1, 'N') == -1) {
		logcat ("Set Parity Error\n");
		return -1;
	}

	return fd;
}

void Zigbee_Release_Device(int fd)
{
	uart_close_dev(fd);
}

static int zigbee_send_cmd(int fd, byte *cmd)
{
	int err = 0;

	if (cmd == NULL)
		return -1;

	cmd[6] = sum_check(cmd, 6);

#ifdef _DEBUG
	logcat("Zigbee Send Command: ");
	debug_out(cmd, 7);
#endif

	err = io_writen(fd, cmd, 7);
	if (err > 0)
		logcat("uart write %d bytes sucess.\n", err);
	else {
		logcat("write error, ret = %d\n", err);
		return -1;
	}

	return 0;
}

int Zigbee_Set_PanID(int fd, byte *pan_id)
{
	byte cmd[8] = {0xfc, 0x02, 0x91, 0x01};
	byte rbuf[2] = {0, 0};
	int err = 0;

	if (pan_id == NULL)
		return -1;

	cmd[4] = pan_id[0];
	cmd[5] = pan_id[1];

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, rbuf, 2, 5)) != 2) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(rbuf, 2);
#endif

	if (memcmp(rbuf, pan_id, 2) != 0)
		return -1;

	return 0;
}

int Zigbee_Read_PanID(int fd, byte *pan_id)
{
	byte cmd[8] = {0xfc, 0x00, 0x91, 0x03, 0xa3, 0xb3};
	int err = 0;

	if (pan_id == NULL)
		return -1;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, pan_id, 2, 5)) != 2) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(pan_id, 2);
#endif

	return 0;
}

int Zigbee_Read_ShortAddr(int fd, byte *short_addr)
{
	byte cmd[8] = {0xfc, 0x00, 0x91, 0x04, 0xc4, 0xd4};
	int err = 0;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, short_addr, 2, 5)) != 2) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(short_addr, 2);
#endif

	return 0;
}

int Zigbee_Set_Bitrate(int fd, int speed)
{
	byte cmd[8] = {0xfc, 0x01, 0x91, 0x06, 0x00, 0xf6};
	byte rbuf[6] = {0};
	int err = 0;

	switch (speed) {
	case 9600:
		cmd[4] = 1;
		break;
	case 19200:
		cmd[4] = 2;
		break;
	case 38400:
		cmd[4] = 3;
		break;
	case 57600:
		cmd[4] = 4;
		break;
	case 115200:
		cmd[4] = 5;
		break;
	default:
		logcat("Zigbee: Invalid uart speed.\n");
		return -1;
	}

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, rbuf, 6, 5)) != 6) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(rbuf, 6);
#endif

	err = rbuf[0]*100000 + rbuf[1]*10000 + rbuf[2]*1000 + rbuf[3]*100;
	if (err != (speed / 100))
		return -1;

	return 0;
}

int Zigbee_Read_MAC(int fd, byte *mac)
{
	byte cmd[8] = {0xfc, 0x00, 0x91, 0x08, 0xa8, 0xb8};
	int err = 0;

	if (mac == NULL)
		return -1;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, mac, 8, 5)) != 8) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(mac, 8);
#endif

	return 0;
}

int Zigbee_Set_type(int fd, int type)
{
	byte *cmd = NULL, *res = NULL;
	byte cmd0[8] = {0xfc, 0x00, 0x91, 0x09, 0xa9, 0xc9};
	byte cmd1[8] = {0xfc, 0x00, 0x91, 0x0a, 0xba, 0xda};
	byte res0[8] = {0x43, 0x6f, 0x6f, 0x72, 0x64, 0x3b, 0x00, 0x19};
	byte res1[8] = {0x52, 0x6f, 0x75, 0x74, 0x65, 0x3b, 0x00, 0x19};
	byte rbuf[8] = {0};
	int err = 0;

	if (type == 0) {
		cmd = cmd0;
		res = res0;
	}
	else if (type == 1) {
		cmd = cmd1;
		res = res1;
	}
	else
		return -1;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, rbuf, 8, 10)) != 8) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(rbuf, 8);
#endif

	if (memcmp(rbuf, res, 8) != 0)
		return -1;

	return 0;
}

int Zigbee_Get_type(int fd)
{
	byte cmd[8] = {0xfc, 0x00, 0x91, 0x0b, 0xcb, 0xeb};
	byte res0[6] = {0x43, 0x6f, 0x6f, 0x72, 0x64, 0x69};
	byte res1[6] = {0x52, 0x6f, 0x75, 0x74, 0x65, 0x72};
	byte rbuf[6] = {0};
	int err = 0;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, rbuf, 6, 5)) != 6) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(rbuf, 6);
#endif

	if (memcmp(rbuf, res0, 6) == 0)
		return 0;
	else if (memcmp(rbuf, res1, 6) == 0)
		return 1;
	else
		return -1;
}

int Zigbee_Set_Channel(int fd, int channel)
{
	byte cmd[8] = {0xfc, 0x01, 0x91, 0x0c, 0x00, 0x1a};
	byte rbuf[5] = {0};
	int err = 0;
	unsigned int data = 0;

	if ((channel < 0x0b) || (channel > 0x1a))
		return -1;
	else
		cmd[4] = channel;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, rbuf, 5, 5)) != 5) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(rbuf, 5);
#endif

	memcpy(&data, rbuf, 4);
	if ((rbuf[4] != channel) || (!(data & (1 << channel))))
		return -1;

	return 0;
}

int Zigbee_Get_channel(int fd)
{
	byte cmd[8] = {0xfc, 0x00, 0x91, 0x0d, 0x34, 0x2b};
	byte rbuf[6] = {0};
	int err = 0;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, rbuf, 6, 5)) != 6) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(rbuf, 6);
#endif

	return rbuf[5];
}

int Zigbee_Set_TransType(int fd, int type)
{
	byte cmd[8] = {0xfc, 0x01, 0x91, 0x64, 0x58, 0x00};
	byte res[6] = {0x06, 0x07, 0x08, 0x09, 0x0a, 0x00};
	byte rbuf[6] = {0};
	int err = 0;

	if ((type < 0) || (type > 0x07))
		type = 0;

	cmd[5] = type;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, rbuf, 6, 5)) != 6) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(rbuf, 6);
#endif

	res[5] = type;
	if (memcmp(rbuf, res, 6) != 0)
		return -1;

	return 0;
}

int Zigbee_Reset(int fd)
{
	byte cmd[8] = {0xfc, 0x00, 0x91, 0x87, 0x6a, 0x35};

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	sleep(2);

	return 0;
}

int Zigbee_Set_RouterAddr(int fd, usint addr)
{
	byte cmd[8] = {0xfc, 0x32, 0xc3, 0x00, 0x00, 0x01};
	byte rbuf[2] = {0};
	usint res = 0;
	int err = 0;

	if ((addr < 1) || (addr > 0xff00))
		return -1;

	cmd[3] = (addr & 0xff00) >> 8;
	cmd[4] = addr & 0x00ff;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, rbuf, 2, 5)) != 2) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(rbuf, 2);
#endif

	res = (rbuf[0] << 8) | (rbuf[1]);
	if (res != addr)
		return -1;

	return 0;
}

usint Zigbee_Read_RouterAddr(int fd)
{
	byte cmd[8] = {0xfc, 0x33, 0xd4, 0xa1, 0xa2, 0x01};
	byte rbuf[2] = {0};
	usint res = 0;
	int err = 0;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	if ((err = io_readn(fd, rbuf, 2, 5)) != 2) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(rbuf, 2);
#endif

	res = (rbuf[0] << 8) | (rbuf[1]);

	return res;
}

usint Zigbee_Test_SerialPort(int fd)
{
	byte cmd[8] = {0xfc, 0x00, 0x91, 0x07, 0x97, 0xa7};
	byte res[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0xff};
	byte rbuf[8] = {0};
	int err = 0;

	if (zigbee_send_cmd(fd, cmd) < 0)
		return -1;

	memset(rbuf, 0, 8);
	if ((err = io_readn(fd, rbuf, 8, 5)) != 8) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		return -1;
	}

#ifdef _DEBUG
	logcat("Zigbee Cmd Return: ");
	debug_out(rbuf, 8);
#endif

	if (memcmp(res, rbuf, 6) == 0)
		return 0;
	else
		return -1;
}

int Zigbee_Get_BitRate(int fd)
{
	int i;
	int num = sizeof(zigbee_bitrate) / sizeof(int);

	for (i = 0; i < num; i++) {
		uart_set_speed(fd, zigbee_bitrate[i]);
		if (Zigbee_Test_SerialPort(fd) == 0)
			break;
	}

	if (i == num)
		return -1;
	else
		return zigbee_bitrate[i];
}

int Zigbee_Device_Init(void)
{
	byte pan_id[2] = {0x19, 0x9b};
	int channel = 0x16;
	int type = 0; /* Coordinator */
	int bitrate;
	byte rbuf[8] = {0};
	int fd;

	Device_power_ctl(DEVICE_ZIGBEE_CHIP, 1);
	Device_power_ctl(DEVICE_ZIGBEE_12V, 0);

	if ((fd = Zigbee_Get_Device(ZIGBEE_UART_SPEED)) < 0)
		return -1;


	bitrate = Zigbee_Get_BitRate(fd);
	logcat("Zigbee Get Bitrate: %d .\n", bitrate);

	if (bitrate != 9600) {
		Zigbee_Set_Bitrate(fd, 9600);
		if(Zigbee_Reset(fd) < 0)
			return -1;
		uart_set_speed(fd, 9600);
	}

	if (Zigbee_Get_type(fd) != type) {
		/* Set Zigbee to Coordinator Mode */
		if(Zigbee_Set_type(fd, type) < 0)
			return -1;
	}

	if(Zigbee_Read_PanID(fd, rbuf) < 0)
		return -1;

	if (memcmp(rbuf, pan_id, 2) != 0) {
		if(Zigbee_Set_PanID(fd, pan_id) < 0)
			return -1;
	}

	if (Zigbee_Get_channel(fd) != channel) {
		if(Zigbee_Set_Channel(fd, channel) < 0)
			return -1;
	}

	if(Zigbee_Reset(fd) < 0)
		return -1;

	Zigbee_Release_Device(fd);

	Device_power_ctl(DEVICE_ZIGBEE_12V, 1);

	return 0;
}

int Sensor_Zigbee_ReadData(byte *buf, int len)
{
	int fd;
	byte rbuf[256] = {0};
	byte res[] = {0xa5,0x06,0x01,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0xba,0xbb,0xb5};
	int err = 0;
	int timeout = 65;

	if ((buf == NULL) || (len > 256))
		return -1;

	if ((fd = Zigbee_Get_Device(ZIGBEE_UART_SPEED)) < 0)
		return -1;

#ifdef _DEBUG
	logcat("Zigbee Start to Read %d Bytes Data.\n", len);
#endif
	memset(rbuf, 0, 256);
	if ((err = io_readn(fd, rbuf, len, timeout)) != len) {
		if (err < 0) {
			logcat("read uart: %s\n", strerror(errno));
		}
		else {
			logcat("nread %d bytes\n", err);
		}
		Zigbee_Release_Device(fd);
		return -1;
	}
	else {
#ifdef _DEBUG
		logcat("Zigbee Read Data: ");
		debug_out(rbuf, len);
#endif
	}

	if ((memcmp(rbuf, res, 4))) // || (memcmp(rbuf + 8, res + 8, 5)))
		err = -1;
	else {
		memcpy(buf, rbuf, len);
		err = 0;
	}

	Zigbee_Release_Device(fd);

	return err;
}
