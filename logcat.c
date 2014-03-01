/*
 * logcat.c
 *
 *  Created on: 2014年2月27日
 *      Author: qinbh
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include "rtc_alarm.h"

#define CMD_LOGFILE		"/root/cma_log.txt"

static int get_date_str(char *date)
{
	time_t t = rtc_get_time();
	struct tm *tm;

	if (date == NULL)
		return -1;

	tm = gmtime(&t);
//	system("date +\"%F %T\"");

	memset(date, 0, 64);
	sprintf(date, "%d-%02d-%02d %02d:%02d:%02d", (tm->tm_year + 1900), (tm->tm_mon + 1), tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	return 0;
}

void logcat(const char *frm, ...)
{
	FILE *fp = NULL;
	va_list vl;
	char date[64];

	memset(date, 0, 64);
	get_date_str(date);
	if (get_date_str(date) < 0) {
		printf("Logcat: Get date error.\n");
		goto out;
	}

	if ((fp=fopen(CMD_LOGFILE, "a"))==NULL) {
		mkdir("Log", 0755);
		if ((fp=fopen("Log/cma_log.tx", "a"))==NULL) {
			printf("Log File Open Failed.\n");
		}
	}

	printf("[%s] ", date);
	if (fp != NULL)
		fprintf(fp, "[%s] ", date);

out:
	va_start(vl, frm);
	vprintf(frm, vl);
	if (fp != NULL)
		vfprintf(fp, frm, vl);
	va_end(vl);

	if (fp != NULL)
		fclose(fp);
}

void logcat_raw(const char *frm, ...)
{
	FILE *fp = NULL;
	va_list vl;

	if ((fp=fopen(CMD_LOGFILE, "a"))==NULL) {
		mkdir("Log", 0755);
		if ((fp=fopen("Log/cma_log.tx", "a"))==NULL) {
			printf("Log File Open Failed.\n");
		}
	}

	va_start(vl, frm);
	vprintf(frm, vl);
	if (fp != NULL)
		vfprintf(fp, frm, vl);
	va_end(vl);

	if (fp != NULL)
		fclose(fp);
}
