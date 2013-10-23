/*
 * rtc_timer.c
 *
 *  Created on: 2013年10月11日
 *      Author: qinbh
 */

#include <stdio.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "rtc_alarm.h"

//#define _DEBUG

static char *rtc_dev = "/dev/rtc0";

struct list_head rtc_alarm_list;

static int rtc_dev_fd = -1;

time_t rtc_get_time(void)
{
	struct rtc_time rtc_tm;
	int retval;

	if (rtc_dev_fd == -1)
		return -1;
	/* Read the RTC time/date */
	retval = ioctl(rtc_dev_fd, RTC_RD_TIME, &rtc_tm);
	if (retval == -1) {
		perror("RTC_RD_TIME ioctl");
		return -1;
	}

	return mktime((struct tm *)&rtc_tm);
}

int start_timer_function_thr( void * (*func)(void *) )
{
    pthread_t p;

#ifdef _DEBUG
    printf("Enter func: %s.\n", __func__);
#endif

    if(pthread_create( &p, NULL, func, NULL)) {
        printf("RTC Timer function start error!\n");
        return -1;
    }

    return 0;
}

void rtc_trigger_alarm(time_t cur_time)
{
	struct list_head *plist;
	struct rtc_alarm_dev *dev;

#ifdef _DEBUG
	printf("Enter func: %s.\n", __func__);
#endif

	list_for_each(plist, &rtc_alarm_list) {
		dev = list_entry(plist, struct rtc_alarm_dev, list);
//		printf("%s: dev->expect = %d, cur time = %d, repeat = %d\n", __func__, dev->expect, cur_time, dev->repeat);
		if (dev->expect > cur_time)
			break;
		/* trigger rtc timer function */
		start_timer_function_thr(dev->func);
		plist = plist->prev;
		list_del(&dev->list);
		if ((dev->repeat == 1) && (dev->interval > 0)) {
			dev->expect += dev->interval;
			rtc_alarm_add(dev);
		}
	}

	if (!list_empty(&rtc_alarm_list)) {
//		printf("rtc update ------------\n");
		rtc_alarm_update();
	}

	return;
}

void *rtc_alarm_wait(void * arg)
{
	unsigned long data = 0;
	time_t cur_time = 0;
	int retval;

	while (1) {
		printf("RTC_ALARM: Wait for rtc alarm.\n");
		if (read(rtc_dev_fd, &data, sizeof(unsigned long)) < 0) {
			perror("RTC read data");
			usleep(1000);
			continue;
		}
		/* Disable alarm interrupts */
		retval = ioctl(rtc_dev_fd, RTC_AIE_OFF, 0);
		if (retval == -1) {
			perror("RTC_AIE_OFF ioctl");
		}

		cur_time = rtc_get_time();
		rtc_trigger_alarm(cur_time);
	}

	return NULL;
}

int rtc_alarm_init(void)
{
	pthread_t p1 = -1;
	int ret;

	rtc_dev_fd = open(rtc_dev, O_RDWR);
	if (rtc_dev_fd ==  -1) {
		perror(rtc_dev);
		return -1;
	}

	INIT_LIST_HEAD(&rtc_alarm_list);

	ret = pthread_create(&p1, NULL, rtc_alarm_wait, NULL);
	if (ret != 0)
		printf("Sensor: can't create thread.");

	return rtc_dev_fd;
}

int rtc_alarm_update(void)
{
	struct rtc_time *rtc_tm;
	struct rtc_alarm_dev *dev;
	int retval;

#ifdef _DEBUG
	printf("Enter func: %s.\n", __func__);
#endif

	if (list_empty(&rtc_alarm_list))
		return 0;

	dev = list_entry(rtc_alarm_list.next, struct rtc_alarm_dev, list);
	rtc_tm = (struct rtc_time *)localtime(&dev->expect);

	printf("RTC Next Alarm: %s", asctime(localtime(&dev->expect)));

	/* Disable alarm interrupts */
	retval = ioctl(rtc_dev_fd, RTC_AIE_OFF, 0);
	if (retval == -1) {
		perror("RTC_AIE_OFF ioctl");
		return -1;
	}

	/* Set RTC Alarm Value */
	retval = ioctl(rtc_dev_fd, RTC_ALM_SET, rtc_tm);
	if (retval == -1) {
		if (errno == ENOTTY) {
			fprintf(stderr, "\n...Alarm IRQs not supported.\n");
			close(rtc_dev_fd);
		}
		perror("RTC_ALM_SET ioctl");
		return -1;
	}

	/* Enable alarm interrupts */
	retval = ioctl(rtc_dev_fd, RTC_AIE_ON, 0);
	if (retval == -1) {
		perror("RTC_AIE_ON ioctl");
		return -1;
	}

	return 0;
}

int rtc_alarm_add(struct rtc_alarm_dev *timer)
{
	struct list_head *plist;
	struct rtc_alarm_dev *dev;

#ifdef _DEBUG
	printf("Enter func: %s.\n", __func__);
#endif

	if (list_empty(&rtc_alarm_list)) {
		list_add(&timer->list, &rtc_alarm_list);
	}
	else {
		list_for_each(plist, &rtc_alarm_list) {
			dev = list_entry(plist, struct rtc_alarm_dev, list);
			if (dev->expect > timer->expect) {
				list_add(&timer->list, dev->list.prev);
				break;
			}
		}
		if (plist == &rtc_alarm_list)
			list_add(&timer->list, &dev->list);
	}

/*
	list_for_each(plist, &rtc_alarm_list) {
		dev = list_entry(plist, struct rtc_alarm_dev, list);
		printf("%s debug: dev->expect = %d\n", __func__, dev->expect);
	}
*/

//	rtc_alarm_update();

	return 0;
}