/*
 * logcat.h
 *
 *  Created on: 2014年2月27日
 *      Author: qinbh
 */

#ifndef LOGCAT_H_
#define LOGCAT_H_

#include <string.h>
#include <errno.h>

void logcat(const char *frm, ...);
void logcat_raw(const char *frm, ...);

#endif /* LOGCAT_H_ */
