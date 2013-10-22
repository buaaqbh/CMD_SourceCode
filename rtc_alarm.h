/*
 * rtc_alarm.h
 *
 *  Created on: 2013年10月22日
 *      Author: qinbh
 */

#ifndef RTC_ALARM_H_
#define RTC_ALARM_H_

#include <linux/rtc.h>
#include "list.h"

struct rtc_alarm_dev {
		time_t expect;
		int repeat;
		int interval;
		void * (*func)(void *);
		struct list_head list;
};

void rtc_trigger_alarm(time_t cur_time);
int rtc_alarm_init(void);
int rtc_alarm_update(void);
int rtc_alarm_add(struct rtc_alarm_dev *timer);
time_t rtc_get_time(void);

#endif /* RTC_ALARM_H_ */
