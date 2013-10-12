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

static const int default_time = 5;
static char *rtc_dev = "/dev/rtc0";

int rtc_timer(int interval)
{
	int fd, retval;
	struct rtc_time rtc_tm;
	int time;
	unsigned long data = 0;

	if (interval < 10)
		time = default_time;
	else
		time = interval;

	do {
		fd = open(rtc_dev, O_RDWR);
		if (fd ==  -1) {
			perror(rtc_dev);
			return -1;
		}
		/* Read the RTC time/date */
		retval = ioctl(fd, RTC_RD_TIME, &rtc_tm);
		if (retval == -1) {
			perror("RTC_RD_TIME ioctl");
			exit(errno);
		}

		/* Set the alarm to [time] seconds in the future, and check for rollover */
		rtc_tm.tm_sec += time;
		if (rtc_tm.tm_sec >= 60) {
			rtc_tm.tm_sec %= 60;
			rtc_tm.tm_min++;
		}
		if (rtc_tm.tm_min == 60) {
			rtc_tm.tm_min = 0;
			rtc_tm.tm_hour++;
		}
		if (rtc_tm.tm_hour == 24)
			rtc_tm.tm_hour = 0;
		retval = ioctl(fd, RTC_ALM_SET, &rtc_tm);
		if (retval == -1) {
			if (errno == ENOTTY) {
				fprintf(stderr, "\n...Alarm IRQs not supported.\n");
				close(fd);
			}
			perror("RTC_ALM_SET ioctl");
			return -1;
		}

		/* Enable alarm interrupts */
		retval = ioctl(fd, RTC_AIE_ON, 0);
		if (retval == -1) {
			perror("RTC_AIE_ON ioctl");
			exit(errno);
		}
		if (read(fd, &data, sizeof(unsigned long)) < 0) {
			perror("RTC read data");
			close(fd);
			return -1;
		}
		close(fd);
	} while (0);

	return 0;
}
