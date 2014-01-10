/*
 * sms_function.c
 *
 *  Created on: 2013年12月7日
 *      Author: qinbh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io_util.h"
#include "sms_function.h"
#include "uart_ops.h"


int SMS_Init(char *Modem, int speed)
{
	int fd;

	fd = uart_open_dev(Modem);
	if (fd == -1) {
		perror("Modem port open error");
		return -1;
	}

	uart_set_speed(fd, speed);
	if(uart_set_parity(fd, 8, 1, 'N') == -1) {
		printf ("Set Parity Error");
		return -1;
	}

	return fd;
}

int SMS_SendCmd(int fd, char *buf, int retry)
{
	int i, err;

	fprintf(stdout, "SMS: Send CMD, %s \n", buf);
	for (i = 0; i < retry; i++) {
		err = io_writen(fd, buf, strlen(buf));
		if (err > 0)
			break;
	}
	if (i == retry)
		return -1;
	else
		return 0;
}

int SMS_WaitResponse(int fd, char *expect, int retry)
{
	int i, err;
	char buf[256];

	for (i = 0; i < retry; i++) {
		memset(buf, 256, 0);
		err = io_readn(fd, buf, 256, 2);
		if (err < 0) {
			perror("Read SMS Modem");
			continue;
		}
		if((strcmp(buf, expect)>=0))
			return 1;
		else
			return -1;
	}

	return 0;
}
