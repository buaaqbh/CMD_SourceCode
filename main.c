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

void *tcpServerThrFxn(void * arg)
{
	ServerEnv * envp = (ServerEnv *)arg;
	int socketfd = envp->m_hSocket;
	byte rbuf[MAX_COMBUF_SIZE];
	int ret;

	printf("socke fd = %d\n", socketfd);
	while (1) {
		memset(rbuf, 0, MAX_DATA_BUFSIZE);
		ret = Commu_GetPacket(socketfd, rbuf, MAX_COMBUF_SIZE);
		if (ret == -2) {
			printf("CMD Server receive MSG error!\n");
			break;
		}
		else if (ret < 0)
			continue;

		system_sleep_enable(0);

		if (CMA_Server_Process(socketfd, rbuf) < 0) {
			printf("CMD Server Process error.\n");
			continue;
		}

		system_sleep_enable(1);
	}

	close_socket(socketfd);
	return 0;
}

void udpServerThrFxn(void)
{
	byte rbuf[MAX_COMBUF_SIZE];
	int ret;

	while (1) {
		memset(rbuf, 0, MAX_DATA_BUFSIZE);
		ret = Commu_GetPacket(-1, rbuf, MAX_COMBUF_SIZE);
		if (ret < 0)
			continue;

		system_sleep_enable(0);

		if (CMA_Server_Process(-1, rbuf) < 0) {
			printf("CMD: Server Process error.\n");
			continue;
		}

		system_sleep_enable(1);
	}

	return;
}

void *main_sample_loop(void * arg)
{
	int cycle = 30;
	printf("Enter func: %s \n", __func__);
	while (1) {
		/*
		 *  Get Data from Sensor
		 */
//		system_sleep_enable(0);
//		CMA_Send_SensorData(-1, CMA_MSG_TYPE_DATA_QXENV);
//		Sensor_Sample_Qixiang();
//		system_sleep_enable(1);

//		alarm(2);
		sleep(30);
//		printf("RTC timer: %d sec\n", cycle);
/*
		if (rtc_timer(cycle) < 0) {
			printf("CMD: Rtc time error.\n");
			alarm(0);
			sleep(cycle);
			continue;
		}
*/
	}

	return 0;
}

void * rtc_alarm_func1(void *arg)
{
	printf("Enter func: %s \n", __func__);

	return 0;
}

void * rtc_alarm_func2(void *arg)
{
	printf("Enter func: %s \n", __func__);

	return 0;
}

void * rtc_alarm_func3(void *arg)
{
	printf("Enter func: %s \n", __func__);

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
	pid_t pid;
	int s_socket;
	pthread_t pthread;
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

	printf("Device ID: %s\n", CMA_Env_Parameter.id);

	if (Device_can_init() < 0) {
//		return -1;
	}

	pthread_spin_init(&spinlock, 0);

	if (signal(SIGALRM, enter_sleep) == SIG_ERR) {
		printf("CMD: sinal init error.\n");
		return -1;
	}

//	{
		struct rtc_alarm_dev timer1, timer2, timer3;
		time_t tm1;

		rtc_alarm_init();

		memset(&timer1, 0, sizeof(struct rtc_alarm_dev));
		memset(&timer2, 0, sizeof(struct rtc_alarm_dev));
		memset(&timer3, 0, sizeof(struct rtc_alarm_dev));
		timer1.expect = rtc_get_time() + 20;
		timer1.func = rtc_alarm_func1;
		timer2.expect = rtc_get_time() + 10;
		timer2.func = rtc_alarm_func2;
		timer2.repeat = 1;
		timer2.interval = 5;
		timer3.expect = rtc_get_time() + 17;
		timer3.func = rtc_alarm_func3;
		timer3.repeat = 1;
		timer3.interval = 7;

		tm1 = rtc_get_time();
		printf("Now: %d\n", tm1);
		printf("Now: %s", asctime(localtime(&tm1)));

		rtc_alarm_add(&timer1);
		rtc_alarm_add(&timer2);
		rtc_alarm_add(&timer3);
		rtc_alarm_update();
//	}


	if (Zigbee_Device_Init() < 0) {
		printf("Zigbee Device Init Error.\n");
	}

	pid = fork();
	if(pid == -1) {
		perror("fork error");
		exit(EXIT_FAILURE);
	}
	if(pid == 0){
		if (CMA_Env_Parameter.s_protocal == 0) {
			udpServerThrFxn();
		}
		else {
			if ((s_socket = start_server(CMA_Env_Parameter.local_port, tcpServerThrFxn)) < 0) {
				printf("Start Local Socket Server error.\n");
				return -1;
			}
			close_socket(s_socket);
		}
		exit(0);
	}

	ret = pthread_create(&pthread, NULL, main_sample_loop, NULL);
	if (ret != 0)
		printf("CMD: can't create thread.");

	ret = pthread_join(pthread, NULL);
	if (ret != 0)
		printf("CMD: can't join with thread.");

	pthread_spin_destroy(&spinlock);

	return 0;
}

