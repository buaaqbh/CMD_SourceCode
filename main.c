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
#include <time.h>
#include "cma_commu.h"
#include "device.h"
#include "socket_lib.h"
#include "sensor_ops.h"
#include "zigbee_ops.h"
#include "list.h"
#include "types.h"
#include "rtc_alarm.h"

char *config_file = NULL;
pthread_spinlock_t spinlock;
volatile int System_Sleep_Enable = 0;
struct rtc_alarm_dev sample_dev;
struct rtc_alarm_dev sample_dev_1;
static volatile int CMD_status_regist = 0;
pthread_mutex_t com_mutex = PTHREAD_MUTEX_INITIALIZER;

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

	fprintf(stdout, "Enter func: %s --\n", __func__);

	while(1) {
		if (CMA_Env_Parameter.socket_fd > 0) {
			memset(rbuf, 0, MAX_DATA_BUFSIZE);
			printf("CMD: Start to read socket message, fd = %d.\n", CMA_Env_Parameter.socket_fd);
			ret = Commu_GetPacket(CMA_Env_Parameter.socket_fd, rbuf, MAX_COMBUF_SIZE, 0);
			if (ret < 0) {
				if (ret == -2) {
					printf("CMD Server receive MSG error!\n");
					pthread_mutex_lock(&com_mutex);
					close(CMA_Env_Parameter.socket_fd);
					CMA_Env_Parameter.socket_fd = -1;
					pthread_mutex_unlock(&com_mutex);
				}
				continue;
			}

			system_sleep_enable(0);

			if (CMA_Server_Process(CMA_Env_Parameter.socket_fd, rbuf) < 0) {
				printf("CMD Server Process error.\n");
				continue;
			}

			system_sleep_enable(1);
		}

		sleep(2);
	}

	return 0;
}

/*
static int CMD_WaitStatus_Res(int timeout)
{
	while ((CMD_Response_data == -1) && timeout) {
		sleep(1);
		timeout--;
	}

	if (timeout == 0)
		return -1;
	else
		return CMD_Response_data;
}
*/

void *socket_heartbeat_func(void * arg)
{
	int ret;
	fprintf(stdout, "Enter func: %s --\n", __func__);

	while (1) {
		printf("~~~~~~~~~ HeartBeat Cycle Start --------\n");
		if (CMA_Env_Parameter.socket_fd < 0) {
			pthread_mutex_lock(&com_mutex);
			CMA_Env_Parameter.socket_fd = connect_server(CMA_Env_Parameter.cma_ip, CMA_Env_Parameter.cma_port, 0);
			pthread_mutex_unlock(&com_mutex);
			printf("------ fd = %d\n", CMA_Env_Parameter.socket_fd);
			if (CMA_Env_Parameter.socket_fd < 0) {
				fprintf(stderr, "CMD: Can't Connect to socket server.\n");
				sleep(5);
				continue;
			}
		}

		fprintf(stdout, "CMD: Send HeartBeat Message.\n");
		pthread_mutex_lock(&com_mutex);
		if (CMA_Send_HeartBeat(CMA_Env_Parameter.socket_fd, CMA_Env_Parameter.id) < 0) {
			fprintf(stderr, "CMD: Send HeartBeat Message error.\n");
			CMA_Env_Parameter.socket_fd = -1;
		}
		pthread_mutex_unlock(&com_mutex);

		if (CMD_status_regist == 0) {
			fprintf(stdout, "CMD: Send Basic Info Message, regist = %d.\n", CMD_status_regist);
			CMD_Response_data = -1;
			pthread_mutex_lock(&com_mutex);
			ret = CMA_Send_BasicInfo(CMA_Env_Parameter.socket_fd, CMA_Env_Parameter.id, 0);
			pthread_mutex_unlock(&com_mutex);
			if (ret == 0) {
//				if (CMD_WaitStatus_Res(5) == 0xff)
					CMD_status_regist = 1;
			}
		}

		printf("~~~~~~~~~ HeartBeat Sleep 60s --------\n");
		sleep(60);
	}

	return 0;
}

void *main_sample_loop(void * arg)
{
	byte data_buf[MAX_DATA_BUFSIZE];
	int ret;

	printf("Enter func: %s\n", __func__);

	system_sleep_enable(0);

	memset(data_buf, 0, MAX_DATA_BUFSIZE);
	if (Sensor_GetData(data_buf, CMA_MSG_TYPE_DATA_QXENV) < 0) {
		fprintf(stderr, "CMD: Sample Env Data error.\n");
	}
	else if (CMA_Env_Parameter.socket_fd > 0) {
		ret = CMA_Send_SensorData(CMA_Env_Parameter.socket_fd, CMA_MSG_TYPE_DATA_QXENV, data_buf);
		if (ret < 0) {
			fprintf(stderr, "CMD: Socket Send Env Data error.\n");
		}
	}

	system_sleep_enable(1);

	alarm(2);

	return 0;
}

void *Sensor_Sample_loop_TGQingXie(void * arg)
{
	byte data_buf[MAX_DATA_BUFSIZE];
	int ret;

	printf("Enter func: %s\n", __func__);

	system_sleep_enable(0);

	memset(data_buf, 0, MAX_DATA_BUFSIZE);
	if (Sensor_GetData(data_buf, CMA_MSG_TYPE_DATA_TGQXIE) < 0) {
		fprintf(stderr, "CMD: Sample Env Data error.\n");
	}
	else if (CMA_Env_Parameter.socket_fd > 0) {
		ret = CMA_Send_SensorData(CMA_Env_Parameter.socket_fd, CMA_MSG_TYPE_DATA_TGQXIE, data_buf);
		if (ret < 0) {
			fprintf(stderr, "CMD: Socket Send Env Data error.\n");
		}
	}

	system_sleep_enable(1);

	alarm(2);

	return 0;
}

void enter_sleep(int sig)
{
//	char *cmd_shell = "echo mem >/sys/power/state";
	printf("Enter func: %s\n", __func__);

	if (System_Sleep_Enable) {
		printf("CMD: System Enter Sleep.\n");
//	system(cmd_shell);
	}

	return;
}

int main(int argc, char *argv[])
{
	int index, c;
	int l2_type = 0;
	pthread_t pid_socket, p_heartbeat;
	time_t now, expect;
	int cycle;
	struct tm *tm;
	char *entry = NULL;
	int ret;

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
		printf("CMD: Init Env data error.\n");
		return -1;
	}

	rtc_alarm_init();

	l2_type = CMA_Env_Parameter.l2_type;
	printf("L2 type: %d\n", l2_type);
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
		printf("Invalid L2 interface type.\n");
	}

	printf("Device ID: %s, Component ID: %s, Original ID: %d\n",
			CMA_Env_Parameter.id, CMA_Env_Parameter.c_id, CMA_Env_Parameter.org_id);

	if (Device_can_init() < 0) {
//		return -1;
	}

	pthread_mutex_init(&can_mutex, NULL);
	pthread_mutex_init(&com_mutex, NULL);

	pthread_spin_init(&spinlock, 0);

	if (signal(SIGALRM, enter_sleep) == SIG_ERR) {
		printf("CMD: sinal init error.\n");
		return -1;
	}

//	if (Zigbee_Device_Init() < 0) {
//		printf("Zigbee Device Init Error.\n");
//	}

	printf("Connect to server: %s \n", CMA_Env_Parameter.cma_ip);
	CMA_Env_Parameter.socket_fd = connect_server(CMA_Env_Parameter.cma_ip, CMA_Env_Parameter.cma_port, 0);
	if (CMA_Env_Parameter.socket_fd < 0) {
		fprintf(stdout, "CMD: Connect to server error.\n");
	}

	ret = pthread_create(&p_heartbeat, NULL, socket_heartbeat_func, NULL);
	if (ret != 0)
		printf("Sensor: can't create thread.");

	ret = pthread_create(&pid_socket, NULL, socket_receive_func, NULL);
	if (ret != 0)
		printf("Sensor: can't create thread.");

	entry = "qixiang:samp_period";

	printf("Set Sampling cycle.\n");
	/* Main Sampling data loop Timer Init */
	if ((cycle = Device_getSampling_Cycle(entry)) < 0)
			cycle = 10;
	memset(&sample_dev, 0, sizeof(struct rtc_alarm_dev));
	sample_dev.func = main_sample_loop;
	sample_dev.repeat = 1;
	sample_dev.interval = cycle * 60; /* Sampling Cycle */
	now = rtc_get_time();
	tm = gmtime(&now);
	printf("Now: %s", asctime(tm));
	expect = now - tm->tm_sec - (tm->tm_min % 10) * 60;
	expect += 10 * 60;
	expect = now + 10;
	tm = gmtime(&expect);
	printf("QiXiang Expect: %s", asctime(tm));
	sample_dev.expect = expect;
	rtc_alarm_add(&sample_dev);

	entry = "tgqingxie:samp_period";
	if ((cycle = Device_getSampling_Cycle(entry)) < 0)
			cycle = 60;
	memset(&sample_dev_1, 0, sizeof(struct rtc_alarm_dev));
	sample_dev_1.func = Sensor_Sample_loop_TGQingXie;
	sample_dev_1.repeat = 1;
	sample_dev_1.interval = cycle * 60; /* Sampling Cycle */
	now = rtc_get_time();
	tm = gmtime(&now);
	expect = now + 10;
	tm = gmtime(&expect);
	printf("TGQingXie Expect: %s", asctime(tm));
	sample_dev_1.expect = expect;
	rtc_alarm_add(&sample_dev_1);

	rtc_alarm_update();

	Camera_NextTimer();

	while (1) {
		sleep(60);
//		if (rtc_alarm_isActive(&sample_dev))
//			printf("Main Sample rtc timer is atcive.\n");
	}

	ret = pthread_join(p_heartbeat, NULL);
	if (ret != 0)
		printf("CMD: can't join with p2 thread.");

	ret = pthread_join(pid_socket, NULL);
	if (ret != 0)
		printf("CMD: can't join with p2 thread.");

	pthread_spin_destroy(&spinlock);

	return 0;
}

