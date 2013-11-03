/*
 * rtc_alarm.h
 *
 *  Created on: 2013年10月22日
 *      Author: qinbh
 */

#ifndef RTC_ALARM_H_
#define RTC_ALARM_H_

#include <linux/rtc.h>
#include <time.h>
#include "list.h"

struct rtc_alarm_dev {
		time_t expect;
		int repeat;
		volatile int interval;
		void * (*func)(void *);
		struct list_head list;
};

extern struct rtc_alarm_dev sample_dev;

void rtc_trigger_alarm(time_t cur_time);
int rtc_alarm_init(void);
int rtc_alarm_update(void);
int rtc_alarm_add(struct rtc_alarm_dev *timer);
int rtc_alarm_del(struct rtc_alarm_dev *timer);
int rtc_alarm_isActive(struct rtc_alarm_dev *timer);
time_t rtc_get_time(void);
time_t mktime_k(struct tm *tm);

#endif /* RTC_ALARM_H_ */
