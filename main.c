#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "cma_commu.h"
#include "device.h"
#include "socket_lib.h"
#include "sensor_ops.h"
#include "zigbee_ops.h"
#include "list.h"
#include "types.h"
#include "rtc_alarm.h"
#include "sms_function.h"

char *config_file = NULL;
pthread_spinlock_t spinlock;
volatile int System_Sleep_Enable = 0;
struct rtc_alarm_dev sample_dev;
struct rtc_alarm_dev sample_dev_1;
struct rtc_alarm_dev sample_dev_2;
static volatile int CMD_status_regist = 0;
pthread_mutex_t com_mutex = PTHREAD_MUTEX_INITIALIZER;

#define RCV_BUFFER_NUM		10
#define CMD_SERVERTHREAD_NUM	10
static volatile int readIndex = 0;
static volatile int writeIndex = 0;
byte *rcvBuffer[10];
sem_t semFull;
sem_t semEmpty;
pthread_mutex_t rcvMutex;
pthread_mutex_t sndMutex;
pthread_mutex_t imgMutex;

static time_t lastReceive_t = -1;

static void usage(FILE * fp, int argc, char **argv)
{
	fprintf (fp,
	   "Usage: %s [options]\n\n"
	   "Options:\n"
	   "-c | --config        config file \n"
	   "-h | --help          Print this message\n"
	   "", argv[0]);
}

static const char short_options[] = "c:h";

static const struct option long_options[] = {
	{"config", required_argument, NULL, 'c'},
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

void system_sleep_enable(int enable)
{
	pthread_spin_lock(&spinlock);
	if (enable) {
		System_Sleep_Enable = 1;
	}
	else {
		System_Sleep_Enable = 0;
	}
	pthread_spin_unlock(&spinlock);
}

void *socket_receive_func(void * arg)
{
	byte rbuf[MAX_COMBUF_SIZE];
	int ret;

	logcat("Enter func: %s --\n", __func__);

	while(1) {
		if (CMA_Env_Parameter.socket_fd > 0) {
			memset(rbuf, 0, MAX_DATA_BUFSIZE);
//			logcat("------ CMD: Start to read socket message, fd = %d.\n", CMA_Env_Parameter.socket_fd);
			ret = Commu_GetPacket(CMA_Env_Parameter.socket_fd, rbuf, MAX_COMBUF_SIZE, 10);
			if (ret < 0) {
				if (ret == -2) {
					logcat("------ CMD Server receive MSG error!\n");
					pthread_mutex_lock(&com_mutex);
					close(CMA_Env_Parameter.socket_fd);
					CMA_Env_Parameter.socket_fd = -1;
					pthread_mutex_unlock(&com_mutex);
				}
				continue;
			}

			sem_wait(&semEmpty);

//			logcat("--------- writeIndex = %d --------\n", writeIndex);
			pthread_mutex_lock(&rcvMutex);
			writeIndex = writeIndex % RCV_BUFFER_NUM;
			memcpy(rcvBuffer[writeIndex], rbuf, MAX_COMBUF_SIZE);
			writeIndex++;
			pthread_mutex_unlock(&rcvMutex);

			sem_post(&semFull);

			lastReceive_t = time((time_t *)NULL);;
		}

		sleep(2);
	}

	return 0;
}

void *cmd_server_func(void * arg)
{
	byte rbuf[MAX_COMBUF_SIZE];

//	logcat("Enter func: %s --\n", __func__);

	while(1) {
		sem_wait(&semFull);

//		logcat("----- readIndex = %d --------\n", readIndex);
		pthread_mutex_lock(&rcvMutex);
		readIndex = readIndex % RCV_BUFFER_NUM;
		memset(rbuf, 0, MAX_COMBUF_SIZE);
		memcpy(rbuf, rcvBuffer[readIndex], MAX_COMBUF_SIZE);
		readIndex++;
		pthread_mutex_unlock(&rcvMutex);

		sem_post(&semEmpty);

		if (CMA_Server_Process(CMA_Env_Parameter.socket_fd, rbuf) < 0) {
			logcat("CMD Server Process error.\n");
			continue;
		}
	}

	return 0;
}

static void CMD_Sleep(int sec)
{
	int i = 0;
	for (i = 0; i < sec; i += 5) {
		if (CMA_Env_Parameter.socket_fd < 0)
			break;
		sleep(5);
	}
}

void *socket_heartbeat_func(void * arg)
{
	int ret;
	logcat("Enter func: %s --\n", __func__);

	while (1) {
		logcat("~~~~~~~~~ HeartBeat Cycle Start --------\n");
		if (CMA_Env_Parameter.socket_fd < 0) {
			pthread_mutex_lock(&com_mutex);
			CMA_Env_Parameter.socket_fd = connect_server(CMA_Env_Parameter.cma_ip, CMA_Env_Parameter.cma_port, CMA_Env_Parameter.s_protocal, 10);
			pthread_mutex_unlock(&com_mutex);
			logcat("------ fd = %d\n", CMA_Env_Parameter.socket_fd);
			if (CMA_Env_Parameter.socket_fd < 0) {
				logcat("CMD: Connect to server error, type: %s.\n", CMA_Env_Parameter.s_protocal ? "UDP":"TCP");
				sleep(5);
				continue;
			}
		}

		logcat("CMD: Send HeartBeat Message.\n");
//		pthread_mutex_lock(&com_mutex);
		if (CMA_Send_HeartBeat(CMA_Env_Parameter.socket_fd, CMA_Env_Parameter.id) < 0) {
			logcat("CMD: Send HeartBeat Message error.\n");
			close(CMA_Env_Parameter.socket_fd);
			CMA_Env_Parameter.socket_fd = -1;
		}
//		pthread_mutex_unlock(&com_mutex);

		if (CMD_status_regist == 0) {
			logcat("CMD: Send Basic Info Message, regist = %d.\n", CMD_status_regist);
			pthread_mutex_lock(&com_mutex);
			ret = CMA_Send_BasicInfo(CMA_Env_Parameter.socket_fd, CMA_Env_Parameter.id, 0);
			pthread_mutex_unlock(&com_mutex);
			if (ret == 0) {
//				if (CMD_WaitStatus_Res(5) == 0xff)
					CMD_status_regist = 1;
			}
		}

		logcat("~~~~~~~~~ HeartBeat Sleep 60s --------\n");
		CMD_Sleep(60);
	}

	return 0;
}

void *Sensor_Sample_loop_QiXiang(void * arg)
{
	byte data_buf[MAX_DATA_BUFSIZE];
	int ret;

	logcat("Enter func: %s\n", __func__);

	system_sleep_enable(0);

	memset(data_buf, 0, MAX_DATA_BUFSIZE);
	if (Sensor_GetData(data_buf, CMA_MSG_TYPE_DATA_QXENV) < 0) {
		logcat("CMD: Sample Env Data error.\n");
	}
	else if (CMA_Env_Parameter.socket_fd > 0) {
//		ret = CMA_Send_SensorData(CMA_Env_Parameter.socket_fd, CMA_MSG_TYPE_DATA_QXENV, data_buf);
		ret = CMA_Check_Send_SensorData(CMA_Env_Parameter.socket_fd, CMA_MSG_TYPE_DATA_QXENV);
		if (ret < 0) {
			logcat("CMD: Socket Send Env Data error.\n");
		}
	}

	Sensor_FaultStatus();

	system_sleep_enable(1);

	return 0;
}

void *Sensor_Sample_loop_TGQingXie(void * arg)
{
	byte data_buf[MAX_DATA_BUFSIZE];
	int ret;

	logcat("Enter func: %s\n", __func__);

	system_sleep_enable(0);

	memset(data_buf, 0, MAX_DATA_BUFSIZE);
	if (Sensor_GetData(data_buf, CMA_MSG_TYPE_DATA_TGQXIE) < 0) {
		logcat("CMD: Sample Angle Data error.\n");
	}
	else if (CMA_Env_Parameter.socket_fd > 0) {
//		ret = CMA_Send_SensorData(CMA_Env_Parameter.socket_fd, CMA_MSG_TYPE_DATA_TGQXIE, data_buf);
		ret = CMA_Check_Send_SensorData(CMA_Env_Parameter.socket_fd, CMA_MSG_TYPE_DATA_TGQXIE);
		if (ret < 0) {
			logcat("CMD: Socket Send Angle Data error.\n");
		}
	}

	system_sleep_enable(1);

	return 0;
}

void *Sensor_Sample_loop_FuBing(void * arg)
{
	byte data_buf[MAX_DATA_BUFSIZE];
	int ret;

	logcat("Enter func: %s\n", __func__);

	system_sleep_enable(0);

	memset(data_buf, 0, MAX_DATA_BUFSIZE);
	if (Sensor_GetData(data_buf, CMA_MSG_TYPE_DATA_FUBING) < 0) {
		logcat("CMD: Sample Tension Data error.\n");
	}
	else if (CMA_Env_Parameter.socket_fd > 0) {
//		ret = CMA_Send_SensorData(CMA_Env_Parameter.socket_fd, CMA_MSG_TYPE_DATA_FUBING, data_buf);
		ret = CMA_Check_Send_SensorData(CMA_Env_Parameter.socket_fd, CMA_MSG_TYPE_DATA_FUBING);
		if (ret < 0) {
			logcat("CMD: Socket Send Tension Data error.\n");
		}
	}

	system_sleep_enable(1);

	return 0;
}

void enter_sleep(int sig)
{
//	char *cmd_shell = "echo mem >/sys/power/state";
//	logcat("Enter func: %s\n", __func__);

	if (System_Sleep_Enable) {
//		logcat("CMD: System Enter Sleep.\n");
//	system(cmd_shell);
	}

	return;
}

int main(int argc, char *argv[])
{
	int index, c;
	int l2_type = 0;
	pthread_t pid_socket, p_heartbeat, pid_server[CMD_SERVERTHREAD_NUM];
	time_t now, expect;
	int cycle;
	struct tm *tm;
	char *entry = NULL;
	int ret;
	int i;

	logcat("CMA Online Sofeware, Version 1.08, build at %s, %s.\n", __TIME__, __DATE__);

	config_file = CMA_CONFIG_FILE;

	for (;;) {
		c = getopt_long (argc, argv, short_options, long_options, &index);

		if (-1 == c)
			break;
		switch (c) {
		case 0:		/* getopt_long() flag */
			break;
		case 'c':
			config_file = optarg;
			break;
		case 'h':
			usage (stdout, argc, argv);
			exit (EXIT_SUCCESS);
		default:
			usage (stderr, argc, argv);
			return 0;
		}
	}
	
	if (Device_Env_init() < 0){
		logcat("CMD: Init Env data error.\n");
		return -1;
	}

	rtc_alarm_init();

	l2_type = CMA_Env_Parameter.l2_type;
	logcat("L2 type: %d\n", l2_type);
	switch (l2_type) {
	case 0:
		/* 3G Module init */
		if (Device_W3G_init() < 0)
			return -1;
		break;
	case 1:
		/* Wifi device Init */
		if (Device_wifi_init() < 0)
			return -1;
		break;
	case 2:
		/* Ethernet configuration */
		if (Device_eth_init() < 0)
			return -1;
		break;
	default:
		logcat("Invalid L2 interface type.\n");
	}

	logcat("Device ID: %s, Component ID: %s, Original ID: %d\n",
			CMA_Env_Parameter.id, CMA_Env_Parameter.c_id, CMA_Env_Parameter.org_id);

	if (CMA_Env_Parameter.sensor_type == 1) {
		if (Device_can_init() < 0) {
			return -1;
		}
		else {
			Device_power_ctl(DEVICE_RS485, 0);
		}
	}
	else {
		Device_power_ctl(DEVICE_CAN_CHIP, 0);
		Device_power_ctl(DEVICE_CAN_12V, 0);
	}

	pthread_mutex_init(&can_mutex, NULL);
	pthread_mutex_init(&rs485_mutex, NULL);
	pthread_mutex_init(&com_mutex, NULL);
	pthread_mutex_init(&av_mutex, NULL);

	pthread_spin_init(&spinlock, 0);

/*
	if (signal(SIGALRM, enter_sleep) == SIG_ERR) {
		logcat("CMD: sinal init error.\n");
		return -1;
	}
*/

	if (Zigbee_Device_Init() < 0) {
		logcat("Zigbee Device Init Error.\n");
	}

	/* Init Receive Buffer */
	for (i = 0; i < RCV_BUFFER_NUM; i++) {
		rcvBuffer[i] = (byte *)malloc(MAX_COMBUF_SIZE);
		memset(rcvBuffer[i], 0, MAX_COMBUF_SIZE);
	}
	ret = sem_init(&semEmpty, 0, RCV_BUFFER_NUM);
	if (ret != 0) {
		logcat("CMD: sem empty init failed \n");
		exit(1);
	}
	ret = sem_init(&semFull, 0, 0);
	if (ret != 0) {
		logcat("CMD: sem full init failed \n");
		exit(1);
	}
	pthread_mutex_init(&rcvMutex, NULL);
	pthread_mutex_init(&sndMutex, NULL);
	pthread_mutex_init(&imgMutex, NULL);
	readIndex = writeIndex = 0;

#if 1
	logcat("Connect to server: %s, protocal: %s\n", CMA_Env_Parameter.cma_ip, CMA_Env_Parameter.s_protocal ? "UDP":"TCP");
	CMA_Env_Parameter.socket_fd = connect_server(CMA_Env_Parameter.cma_ip, CMA_Env_Parameter.cma_port, CMA_Env_Parameter.s_protocal, 10);
	if (CMA_Env_Parameter.socket_fd < 0) {
		logcat("CMD: Connect to server error, type: %s.\n", CMA_Env_Parameter.s_protocal ? "UDP":"TCP");
	}

	ret = pthread_create(&p_heartbeat, NULL, socket_heartbeat_func, NULL);
	if (ret != 0)
		logcat("Sensor: can't create heartbeat thread.\n");

	ret = pthread_create(&pid_socket, NULL, socket_receive_func, NULL);
	if (ret != 0)
		logcat("Sensor: can't create receive thread.\n");

	for (i = 0;i < CMD_SERVERTHREAD_NUM; i++) {
		ret = pthread_create(&pid_server[i], NULL, cmd_server_func, NULL);
		if (ret != 0)
			logcat("Sensor: can't create server thread.\n");
	}

	entry = "qixiang:samp_period";

	logcat("Set Sampling cycle.\n");
	/* Main Sampling data loop Timer Init */
	if ((cycle = Device_getSampling_Cycle(entry)) < 0)
			cycle = 10;
	memset(&sample_dev, 0, sizeof(struct rtc_alarm_dev));
	sample_dev.func = Sensor_Sample_loop_QiXiang;
	sample_dev.repeat = 1;
	sample_dev.interval = cycle * 60; /* Sampling Cycle */
	now = rtc_get_time();
	tm = gmtime(&now);
	logcat("Now: %s", asctime(tm));
//	expect = now - tm->tm_sec - (tm->tm_min % 10) * 60;
//	expect += 10 * 60;
	expect = now - tm->tm_sec + (2 - tm->tm_min % 2) * 60;
//	expect = now + 10;
	tm = gmtime(&expect);
	logcat("QiXiang Expect: %s", asctime(tm));
	sample_dev.expect = expect;
	rtc_alarm_add(&sample_dev);

	entry = "tgqingxie:samp_period";
	if ((cycle = Device_getSampling_Cycle(entry)) < 0)
			cycle = 10;
	memset(&sample_dev_1, 0, sizeof(struct rtc_alarm_dev));
	sample_dev_1.func = Sensor_Sample_loop_TGQingXie;
	sample_dev_1.repeat = 1;
	sample_dev_1.interval = cycle * 60; /* Sampling Cycle */
	now = rtc_get_time();
	tm = gmtime(&now);
	expect = now - tm->tm_sec + (2 - tm->tm_min % 2) * 60;
//	expect = now + 10;
	tm = gmtime(&expect);
	logcat("TGQingXie Expect: %s", asctime(tm));
	sample_dev_1.expect = expect;
	rtc_alarm_add(&sample_dev_1);

	entry = "fubing:samp_period";
	if ((cycle = Device_getSampling_Cycle(entry)) < 0)
			cycle = 10;
	memset(&sample_dev_2, 0, sizeof(struct rtc_alarm_dev));
	sample_dev_2.func = Sensor_Sample_loop_FuBing;
	sample_dev_2.repeat = 1;
	sample_dev_2.interval = cycle * 60; /* Sampling Cycle */
	now = rtc_get_time();
	tm = gmtime(&now);
	expect = now - tm->tm_sec + (2 - tm->tm_min % 2) * 60;
//	expect = now + 10;
	tm = gmtime(&expect);
	logcat("FuBing Expect: %s", asctime(tm));
	sample_dev_2.expect = expect;
	rtc_alarm_add(&sample_dev_2);

	rtc_alarm_update();

	Camera_NextTimer();
#endif

	SMS_Init();

	while (1) {
		sleep(60);
//		if (rtc_alarm_isActive(&sample_dev))
//			logcat("Main Sample rtc timer is atcive.\n");
	}

	for (i = 0; i < RCV_BUFFER_NUM; i++) {
		free(rcvBuffer[i]);
	}

	ret = pthread_join(p_heartbeat, NULL);
	if (ret != 0)
		logcat("CMD: can't join with hearbeat thread.\n");

	ret = pthread_join(pid_socket, NULL);
	if (ret != 0)
		logcat("CMD: can't join with socket receive thread.\n");

	for (i = 0;i < CMD_SERVERTHREAD_NUM; i++) {
		ret = pthread_join(pid_server[i], NULL);
		if (ret != 0)
			logcat("CMD: can't join with server thread.\n");
	}

	pthread_spin_destroy(&spinlock);

	return 0;
}

