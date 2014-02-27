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

#define CMD_LOGFILE		"/root/cma_log.txt"

void logcat(const char *frm, ...)
{
	FILE *fp = NULL;
	va_list vl;
	time_t t = time((time_t *)NULL);
	char *str = ctime(&t);

	if ((fp=fopen(CMD_LOGFILE, "a"))==NULL) {
		mkdir("Log", 0755);
		if ((fp=fopen("Log/cma_log.tx", "a"))==NULL) {
			printf("Log File Open Failed.\n");
		}
	}

	str[strlen(str) - 1] = '\0';
	printf("[%s] ", str);
	if (fp != NULL)
		fprintf(fp, "[%s] ", str);

	va_start(vl, frm);
	vprintf(frm, vl);
	if (fp != NULL)
		vfprintf(fp, frm, vl);
	va_end(vl);

	if (fp != NULL)
		fclose(fp);
}
