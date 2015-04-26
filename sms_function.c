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
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include "io_util.h"
#include "sms_function.h"
#include "uart_ops.h"
#include "device.h"
#include "types.h"
#include "file_ops.h"

//#define USE_EVDO

#ifdef USE_EVDO
#define SMS_SERIAL_DEV 		"/dev/ttyUSB1"
#else
#define SMS_SERIAL_DEV 		"/dev/ttyUSB3"
#endif
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
		logcat("Modem open uart: %s\n", strerror(errno));
		return -1;
	}

	uart_set_speed(fd, SMS_SERIAL_SPEED);
	if(uart_set_parity(fd, 8, 1, 'N') == -1) {
		logcat ("Set Parity Error\n");
		return -1;
	}

	return fd;
}

int Modem_SendCmd(int fd, char *buf, int retry)
{
	int i, err;

//	logcat("SMS: Send CMD, %s\n", buf);
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

	for (i = 0; i < retry; i++) {
		memset(rbuf, 0, 512);
		nread = readn(fd, rbuf, 512, 5);
		if (nread < 0) {
			if (errno == ETIME)
				continue;
			else {
				logcat("Modem read uart: %s\n", strerror(errno));
				return -1;
			}
		}

/*		{
			logcat("nread = %d \n", nread);
			int j;
			logcat("");
			for (j = 0; j < nread; j++) {
					if ((j % 16) == 0)
							logcat("\n");
					logcat_raw("0x%x ", rbuf[j]);
			}
			logcat("\n");
		} */
		if (strstr(rbuf, expect)) {
			logcat("SMS WaitResponse: found string: %s \n", expect);
			return 0;
		}
	}

	return -1;
}

#ifdef USE_EVDO
static int SMS_GetPhoneNum(char *msg, char *phone)
{
	char tmp[32];
	int i, j = 0;
	char *p = NULL;

//	logcat("msg: %s \n", msg);

	memset(tmp, 0, 32);

	p = strchr(msg, ' ');
	if (p == NULL)
		return -1;

	p++;
	for (i = 0; i < strlen(p); i++) {
		if (p[i] == ',')
			break;
		tmp[j++] = p[i];
	}

	memcpy(phone, tmp, strlen(tmp));
//	logcat("SMS Phone Number: %s , len = %d \n", phone, strlen(phone));

	return 0;
}
#else
static int SMS_GetPhoneNum(char *msg, char *phone)
{
	int len = strlen(msg);
	char tmp[32];
	int i, j = 0;
	int count = 0;

//	logcat("msg: %s \n", msg);

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
//	logcat("SMS Phone Number: %s , len = %d \n", phone, strlen(phone));

	return 0;
}
#endif

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
//	logcat("SMS Start to read Message, index = %d, retry = %d\n", index, retry);
	memset(cmd, 0, 256);
#ifdef USE_EVDO
	sprintf(cmd, "at^hcmgr=%d\r", index);
#else
	sprintf(cmd, "at+cmgr=%d\r", index);
#endif

	if (Modem_SendCmd(fd, cmd, 5) < 0) {
		logcat("SMS Send CMD error.\n");
		return -1;
	}

	usleep(400 * 1000);
	nread = 0;
	for (i = 0; i < 5; i++) {
		memset(rbuf, 0, 512);
		nread = readn(fd, (rbuf + nread), 512, 2);
		if (nread < 0) {
			if (errno == ETIME)
				continue;
			else {
				logcat("Modem read uart: %s\n", strerror(errno));
				return -1;
			}
		}
		if (strstr(rbuf, "OK")) {
			break;
		}

		if (strstr(rbuf, "ERROR")) {
			logcat("Modem: read message error.\n");
			return -1;
		}
	}
	if (i == 5) {
		if (retry > 0) {
			retry--;
			sleep(2);
			goto SMS_Retry;
		}

		return -1;
	}

//	logcat("Receive: num = %d, %s \n", nread, rbuf);
	sp = rbuf;

	while ((p = strsep(&sp, delim)) != NULL) {
#ifdef USE_EVDO
		if (strncmp(p, "^HCMGR", 6) == 0) {
#else
		if (memcmp(p, "+CMGR", 5) == 0) {
#endif
//			logcat("strsep: %s \n", p);
			break;
		}
	}
	if (p == NULL) return -1;
//	logcat("p = %s\nlen = %d\n", p, strlen(p));
#ifdef USE_EVDO
	if (memcmp(p, "^HCMGR", 5) == 0) {
#else
	if (memcmp(p, "+CMGR", 5) == 0) {
#endif
		if (SMS_GetPhoneNum(p, phone) < 0) {
			logcat("SMS Get Phone Number Error.\n");
			return -1;
		}
		while ((p = strsep(&sp, delim)) != NULL) {
			if (strlen(p) > 0) {
				break;
			}
		}
		if (p == NULL) {
			logcat("Message Data is NULL.\n");
		}
		else {
//			logcat("p2 = %s , len = %d\n", p, strlen(p));
			memcpy(data, p, strlen(p));
		}

		while ((p = strsep(&sp, delim)) != NULL) {
			if (strlen(p) > 0) {
				break;
			}
		}
		if (p != NULL) {
			if (memcmp(p, "OK", 2) != 0)
				return -1;
		}
		else
			return -1;
	}
	else
		return -1;

	logcat("SMS Read Message: phone = %s, data = %s \n", phone, data);

	return 0;
}

int SMS_SendMessage(int fd, char *phone, char *msg)
{
	char cmd[256];
	char buf[256] = { 0 };

	logcat("SMS Start to Send Message, phone = %s, msg = %s\n", phone, msg);

#ifdef USE_EVDO
	memset(cmd, 0, 256);
	sprintf(cmd, "AT^HSMSSS=0,0,1,0\r");

	if (Modem_SendCmd(fd, cmd, 5) < 0)
		return -1;

	usleep(500 * 1000);
	if (Modem_WaitResponse(fd, "OK", 5) < 0) {
		logcat("SMS Wait > error.\n");
		return -1;
	}
#endif

	memset(cmd, 0, 256);
#ifdef USE_EVDO
	sprintf(cmd, "AT^HCMGS=\"%s\"\r", phone);
#else
	sprintf(cmd, "AT+CMGS=\"%s\"\r", phone);
#endif
	if (Modem_SendCmd(fd, cmd, 5) < 0)
		return -1;

	usleep(500 * 1000);
	if (Modem_WaitResponse(fd, ">", 5) < 0) {
		logcat("SMS Wait > error.\n");
		return -1;
	}

	usleep(500 * 1000);
	memset(buf, 0, 256);
	memcpy(buf, msg, strlen(msg));
	buf[strlen(msg)] = 0x1a;
	if (Modem_SendCmd(fd, buf, 5) < 0)
		return -1;

	usleep(500 * 1000);
	if (Modem_WaitResponse(fd, "OK", 30) < 0) {
		logcat("SMS Send Message error.\n");
		return -1;
	}

	return 0;
}

int SMS_DelMessage(int fd, int index)
{
	char cmd[256];
	int retry = 5;
	int i;

	logcat("SMS Start to Delet Message, index = %d\n", index);
	memset(cmd, 0, 256);
	sprintf(cmd, "AT+CMGD=%d\r", index);

	for (i = 0; i < retry; i++) {
		if (Modem_SendCmd(fd, cmd, 5) < 0)
			continue;

		usleep(500 * 1000);
		if (Modem_WaitResponse(fd, "OK", 3) == 0)
			break;
	}

	if (i == retry) {
		logcat("SMS Delet Message error.\n");
		return -1;
	}

	return 0;
}

#define COMMAND_SERVER		"server"
#define COMMAND_RESET		"reset"
#define COMMAND_SETID		"setid"
#define COMMAND_ADDPHONE	"addphone"
#define COMMAND_DELPHONE	"delphone"

#define FILE_SMS_PHONE		"/CMD_Data/.sms_phone.txt"
#define SMS_SUPERPHONE1		"13811187586"
#define SMS_SUPERPHONE2		"18576420690"

static int SMS_AddPhone(char *phone)
{
	int num = 0;
	int i;
	char buf[16] = { 0 };

	num = File_GetNumberOfRecords(FILE_SMS_PHONE, 16);

	for (i = 0; i < num; i++) {
		memset(buf, 0, 16);
		if (File_GetRecordByIndex(FILE_SMS_PHONE, buf, 16, i) == 16) {
			if (strcmp(buf, phone) == 0)
				return 0;
		}
	}

	memset(buf, 0, 16);
	memcpy(buf, phone, strlen(phone));

	return File_AppendRecord(FILE_SMS_PHONE, buf, 16);
}

static int SMS_DelPhone(char *phone)
{
	int num = 0;
	int i;
	char buf[16] = { 0 };

	num = File_GetNumberOfRecords(FILE_SMS_PHONE, 16);

	for (i = 0; i < num; i++) {
		memset(buf, 0, 16);
		if (File_GetRecordByIndex(FILE_SMS_PHONE, buf, 16, i) == 16) {
			if (strcmp(buf, phone) == 0) {
				return File_DeleteRecordByIndex(FILE_SMS_PHONE, 16, i);
			}
		}
	}

	return 0;
}

static int SMS_CheckPhone(char *phone)
{
	int num = 0;
	int i;
	char buf[16] = { 0 };

	logcat("Enter func: %s ------\n", __func__);

	if ((strcmp(phone, SMS_SUPERPHONE1) == 0) || (strcmp(phone, SMS_SUPERPHONE2) == 0)) {
		return 0;
	}

	if (File_Exist(FILE_SMS_PHONE) == 0)
		return -1;

	num = File_GetNumberOfRecords(FILE_SMS_PHONE, 16);
	if (num == 0)
		return -1;

	for (i = 0; i < num; i++) {
		memset(buf, 0, 16);
		if (File_GetRecordByIndex(FILE_SMS_PHONE, buf, 16, i) == 16) {
			logcat("buf = %s, phone = %s\n", buf, phone);
			if (strncmp(buf, phone, 11) == 0)
				return 0;
		}
	}

	return -1;
}

int SMS_CMDProcess(char *data, char *phone)
{
	char *p = NULL, *sp = NULL;
	char* const delim = "+";
	char* const delim2 = ":";
	char *p_cmd = NULL;
	char *p_data = NULL;
	char *p_ip = NULL;
	char *p_port = NULL;

	logcat("CMD: Receive Message, phone = %s, data = %s \n", phone, data);

	sp = data;
	p = strsep(&sp, delim);
	p_cmd = p;
	p = strsep(&sp, delim);
	p_data = p;

	logcat("CMD: command = %s, data = %s \n", p_cmd, p_data);

	if (memcmp(p_cmd, COMMAND_SERVER, strlen(COMMAND_SERVER)) == 0) {
		Ctl_up_device_t  up_device;

		logcat("SMS: Change server to: %s\n", p_data);
		sp = p_data;
		p = strsep(&sp, delim2);
		p_ip = p;
		p = strsep(&sp, delim2);
		p_port = p;
		logcat("Server: ip = %s, port = %d \n", p_ip, atoi(p_port));

		if (Device_getServerInfo(&up_device) < 0)
			return -1;
		up_device.IP_Address = inet_addr(p_ip);
		up_device.Port = atoi(p_port);

		if (Device_setServerInfo(&up_device) < 0)
			return -1;
	}
	else if (memcmp(p_cmd, COMMAND_RESET, strlen(COMMAND_RESET)) == 0) {
		logcat("SMS: Reset system.\n");
//		Device_reset(0);
		system("/sbin/reboot -d 30 &");
	}
	else if (memcmp(p_cmd, COMMAND_SETID, strlen(COMMAND_SETID)) == 0) {
		logcat("SMS: Set Device ID to: %s\n", p_data);
		if (strlen(p_data) < 17) {
			logcat("SMS: Invalid ID Value, strlen = %d.\n", strlen(p_data));
			return -1;
		}
		if (Device_setId((byte *)p_data, (byte *)CMA_Env_Parameter.c_id, (byte *)CMA_Env_Parameter.org_id) < 0)
			return -1;
	}
	else if (memcmp(p_cmd, COMMAND_ADDPHONE, strlen(COMMAND_ADDPHONE)) == 0) {
		logcat("SMS: Add Control Phone: %s\n", p_data);
		if ((strcmp(phone, SMS_SUPERPHONE1) != 0) && (strcmp(phone, SMS_SUPERPHONE2) != 0)) {
			logcat("SMS: phone %s isn't super user.\n", phone);
			return -1;
		}

		if (SMS_AddPhone(p_data) < 0)
			return -1;
	}
	else if (memcmp(p_cmd, COMMAND_DELPHONE, strlen(COMMAND_DELPHONE)) == 0) {
		logcat("SMS: Del Control Phone: %s\n", p_data);
		if ((strcmp(phone, SMS_SUPERPHONE1) != 0) && (strcmp(phone, SMS_SUPERPHONE2) != 0)) {
			logcat("SMS: phone %s isn't super user.\n", phone);
			return -1;
		}

		if (SMS_DelPhone(p_data) < 0)
			return -1;
	}
	else {
		logcat("SMS: Invalid SMS Command, cmd = %s.\n", p_cmd);
	}

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
		logcat("New Message Index = %d \n", index);
	}
	else
		return -1;

	memset(data, 0, 512);
	memset(phone, 0, 32);
	if (SMS_ReadMessage(fd, index, data, phone) < 0) {
		logcat("SMS Read message error.\n");
		return -1;
	}

	if (memcmp(phone, "+86", 3) == 0) {
		int len = strlen(phone);
		memmove(phone, (phone + 3), len -3);
		memset((phone + len - 3), 0, 3);
	}

	if (SMS_CheckPhone(phone) == 0) {
		if (SMS_CMDProcess(data, phone) == 0) {
			SMS_SendMessage(fd, phone, "Command Sucess.");
		}
		else {
			SMS_SendMessage(fd, phone, "Command Failure.");
		}
	}

//	index = (index + 1) % 20;
	SMS_DelMessage(fd, index);

	return 0;
}

int SMS_ReadIMEI(int fd)
{
	// Strored in CMA_Env_Parameter.imei
	return 0;
}

static void SMS_SetFunction(int fd)
{
	char cmd[256];

	memset(cmd, 0, 256);
	sprintf(cmd, "AT+CNMI=2,1,0,0,0\r");
	Modem_SendCmd(fd, cmd, 5);
	Modem_WaitResponse(fd, "OK", 5);

	memset(cmd, 0, 256);
	sprintf(cmd, "AT+CMGF=1\r");
	Modem_SendCmd(fd, cmd, 5);
	Modem_WaitResponse(fd, "OK", 5);

	memset(cmd, 0, 256);
	sprintf(cmd, "AT+CPMS=\"SM\",\"SM\",\"SM\"\r");
	Modem_SendCmd(fd, cmd, 5);
	Modem_WaitResponse(fd, "OK", 5);

	return;
}

#include <dirent.h>

#define BUF_SIZE 			1024
static volatile int pid_3g = -1;

static int getPidByName(char* task_name)
{
	DIR *dir;
	struct dirent *ptr;
	FILE *fp;
	char filepath[64];
	char cur_task_name[64];
	char buf[BUF_SIZE];
	int pid_num = -1;

	dir = opendir("/proc");
	if (NULL != dir){
    	while ((ptr = readdir(dir)) != NULL) {
    		//如果读取到的是"."或者".."则跳过，读取到的不是文件夹名字也跳过
    		if ((strcmp(ptr->d_name, ".") == 0) || (strcmp(ptr->d_name, "..") == 0))
    			continue;
    		if (DT_DIR != ptr->d_type)
    			continue;

    		sprintf(filepath, "/proc/%s/status", ptr->d_name);//生成要读取的文件的路径
    		fp = fopen(filepath, "r");//打开文件
    		if (NULL != fp) {
    			if( fgets(buf, BUF_SIZE-1, fp)== NULL ) {
    				fclose(fp);
    				continue;
    			}
    			sscanf(buf, "%*s %s", cur_task_name);

    			//如果文件内容满足要求则打印路径的名字（即进程的PID）
    			if (!strcmp(task_name, cur_task_name)) {
//    				logcat("PID:  %s\n", ptr->d_name);
    				pid_num = atoi(ptr->d_name);
    			}
    			fclose(fp);
            }

    	}
    	closedir(dir);//关闭路径
	}

	return pid_num;
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
	int pid = -1;

	pthread_detach(pthread_self());

	SMS_SetFunction(fd);

	while (1) {
		if (fd < 0) {
			sleep(10);
			fd = Modem_Init();
			if (fd < 0)
				continue;
			else {
				sleep(20);
				SMS_SetFunction(fd);
			}
		}

		pid = getPidByName("pppd");
		if (pid_3g != pid) {
			pid_3g = pid;
			close(fd);
			fd = -1;
			continue;
		}

		memset(rbuf, 0, 512);
//		logcat("Start to Read Modem Uart: \n");
		nread = read(fd, rbuf, 512);
//		logcat("Modem Receive %d bytes\n", nread);
		if (nread <= 0) {
			logcat("Modem read uart: %s\n", strerror(errno));
			close(fd);
			fd = -1;
			continue;
		}

		num = 0;
		sp = rbuf;
		while ((p = strsep(&sp, delim)) != NULL) {
			if (strlen(p) > 0) {
				result[num++] = p;
				if (p[0] != 0x5e)
					logcat("p = %s\n", p);
			}
		}

		for (i = 0; i < num; i++) {
			if (memcmp(result[i], SMS_NEWMESSAGE, 5) == 0) {
				logcat("SMS: New Message arrived.\n");
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
		logcat("CMD: can't create SMS thread.");

	return ret;
}



