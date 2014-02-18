/*
 * sms_function.c
 *
 *  Created on: 2013年12月7日
 *      Author: qinbh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include "io_util.h"
#include "sms_function.h"
#include "uart_ops.h"

#define SMS_SERIAL_DEV 		"/dev/ttyUSB3"
#define SMS_SERIAL_SPEED 	115200

#define SMS_NEWMESSAGE		"+CMTI"

static ssize_t readn(int fd, void *buf, size_t nbytes, unsigned int timout)
{
	int		nfds;
	fd_set	readfds;
	struct timeval	tv;

	tv.tv_sec = timout;
	tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	nfds = select(fd+1, &readfds, NULL, NULL, &tv);
	if (nfds <= 0) {
		if (nfds == 0)
			errno = ETIME;
		return(-1);
	}
	return(read(fd, buf, nbytes));
}

int Modem_Init(void)
{
	int fd;

	fd = uart_open_dev(SMS_SERIAL_DEV);
	if (fd == -1) {
		perror("Modem port open error");
		return -1;
	}

	uart_set_speed(fd, SMS_SERIAL_SPEED);
	if(uart_set_parity(fd, 8, 1, 'N') == -1) {
		printf ("Set Parity Error");
		return -1;
	}

	return fd;
}

int Modem_SendCmd(int fd, char *buf, int retry)
{
	int i, err;

	printf("SMS: Send CMD, %s\n", buf);
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

int Modem_WaitResponse(int fd, char *expect, int retry)
{
	char rbuf[512];
	int i, nread;
	char *p = NULL;
	const char delim[] = "\n\n";

	for (i = 0; i < retry; i++) {
		memset(rbuf, 0, 512);
		nread = readn(fd, rbuf, 512, 2);
		if (nread < 0) {
			if (errno == ETIME)
				continue;
			else {
				perror("Modem read uart");
				return -1;
			}
		}
		if (rbuf[2] != 0x5e)
			break;
	}
	if (i == 5)
		return -1;

	p = strtok(rbuf, delim);
	while (p != NULL) {
		if (strcmp(p, expect) == 0)
			return 0;
		p = strtok(NULL, delim);
	}

	return -1;
}

static int SMS_GetPhoneNum(char *msg, char *phone)
{
	int len = strlen(msg);
	char tmp[32];
	int i, j = 0;
	int count = 0;

//	printf("msg: %s \n", msg);

	memset(tmp, 0, 32);
	for (i = 0; i < len; i++) {
		if (count == 1)
			tmp[j++] = msg[i];
		if (msg[i] == ',')
			++count;
	}

	if (count == 0)
		return -1;

	len = strlen(tmp);
	if (tmp[len -1] == ',')
		tmp[len - 1] = '\0';
	memcpy(phone, tmp + 1, strlen(tmp) - 2);
//	printf("SMS Phone Number: %s , len = %d \n", phone, strlen(phone));

	return 0;
}

int SMS_ReadMessage(int fd, int index, char *data, char *phone)
{
	char rbuf[512];
	char cmd[256];
	char *p = NULL, *sp = NULL;
	char* const delim = "\n\n";
	int nread;
	int i;
	int retry = 5;

SMS_Retry:
	printf("SMS Start to read Message, index = %d\n", index);
	memset(cmd, 0, 256);
	sprintf(cmd, "at+cmgr=%d\r", index);

	if (Modem_SendCmd(fd, cmd, 5) < 0)
		return -1;

	for (i = 0; i < 5; i++) {
		memset(rbuf, 0, 512);
		nread = readn(fd, rbuf, 512, 2);
		if (nread < 0) {
			if (errno == ETIME)
				continue;
			else {
				perror("Modem read uart");
				return -1;
			}
		}
		if (rbuf[2] != 0x5e)
			break;
	}
	if (i == 5) {
		if (retry > 0) {
			retry--;
			sleep(2);
			goto SMS_Retry;
		}

		return -1;
	}

//	printf("Receive: num = %d, %s \n", nread, rbuf);
	sp = rbuf;

	while ((p = strsep(&sp, delim)) != NULL) {
		if (strlen(p) > 0) {
//			printf("strsep: %s \n", p);
			break;
		}
	}
//	printf("p = %s\nlen = %d\n", p, strlen(p));
	if (memcmp(p, "+CMGR", 5) == 0) {
		if (SMS_GetPhoneNum(p, phone) < 0) {
			printf("SMS Get Phone Number Error.\n");
			return -1;
		}
		while ((p = strsep(&sp, delim)) != NULL) {
			if (strlen(p) > 0) {
				break;
			}
		}
//		printf("p = %s , len = %d\n", p, strlen(p));
		if (p != NULL)
			memcpy(data, p, strlen(p));
		while ((p = strsep(&sp, delim)) != NULL) {
			if (strlen(p) > 0) {
				break;
			}
		}
		if (memcmp(p, "OK", 2) != 0)
			return -1;
	}
	else
		return -1;

	printf("SMS Read Message: phone = %s, data = %s \n", phone, data);

	return 0;
}

int SMS_DelMessage(int fd, int index)
{
	char cmd[256];

	printf("SMS Start to Delet Message, index = %d\n", index);
	memset(cmd, 0, 256);
	sprintf(cmd, "AT+CMGD=%d\r", index);
	if (Modem_SendCmd(fd, cmd, 5) < 0)
		return -1;

	if (Modem_WaitResponse(fd, "OK", 5) < 0) {
		printf("SMS Delet Message error.\n");
		return -1;
	}

	return 0;
}

int SMS_CMDProcess(char *data, char *phone)
{

	printf("CMD: Receive Message, phone = %s, data = %s \n", phone, data);

	return 0;
}

int SMS_ProcessMessage(int fd, char *msg)
{
	int index = 0;
	char *p = NULL;
	char data[512];
	char phone[32];

	p = strrchr(msg, 0x2c);
	if (p != NULL) {
		index = atoi(p+1);
		printf("New Message Index = %d \n", index);
	}
	else
		return -1;

	memset(data, 0, 512);
	memset(phone, 0, 32);
	if (SMS_ReadMessage(fd, index, data, phone) < 0) {
		printf("SMS Read message error.\n");
		return -1;
	}

	SMS_CMDProcess(data, phone);

	SMS_DelMessage(fd, index);

	return 0;
}

void *SMS_WaitNewMessage(void *arg)
{
	char rbuf[512];
	char* const delim = "\n\n";
	int fd = Modem_Init();
	char *p = NULL, *sp = NULL;
	char *result[5] = { NULL };
	int i = 0, num = 0;
	int nread;

	pthread_detach(pthread_self());

	while (1) {
		if (fd < 0) {
			sleep(10);
			fd = Modem_Init();
			continue;
		}
		memset(rbuf, 0, 512);
		nread = read(fd, rbuf, 512);
		if (nread < 0) {
			perror("Modem read uart");
			close(fd);
			fd = -1;
			continue;
		}

		num = 0;
		sp = rbuf;
		while ((p = strsep(&sp, delim)) != NULL) {
			if (strlen(p) > 0) {
				result[num++] = p;
//				printf("p = %s\n", p);
			}
		}

		for (i = 0; i < num; i++) {
			if (memcmp(result[i], SMS_NEWMESSAGE, 5) == 0) {
				fprintf(stdout, "SMS: New Message arrived.\n");
				usleep(100 * 1000);
				if (SMS_ProcessMessage(fd, result[i]) < 0) {
					close(fd);
					fd = -1;
				}
			}
		}

	}

	return 0;
}

int SMS_Init(void)
{
	pthread_t pid;
	int ret;

	ret = pthread_create(&pid, NULL, SMS_WaitNewMessage, NULL);
	if (ret != 0)
		printf("CMD: can't create SMS thread.");

	return ret;
}



