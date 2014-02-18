/*
 * sms_function.h
 *
 *  Created on: 2013年12月7日
 *      Author: qinbh
 */

#ifndef SMS_FUNCTION_H_
#define SMS_FUNCTION_H_

#define SMS_CMD_SET_FORMAT 		"at+cmgf=1\r"
#define SMS_CMD_SET_STORAGE 	"at+cpms=\"SM\",\"SM\",\"SM\"\r"
//#define SMS_CMD_READ			"at^hcmgl=0\r"
#define SMS_CMD_READ			"at+cmgr=28\r"
#define SMS_CMD_SEND			"at^hcmgs\r"
#define SMS_CMD_GET_CONTENT		"at^hcmgr=0\r"

int Modem_Init(void);
int Modem_SendCmd(int fd, char *buf, int retry);
int Modem_WaitResponse(int fd, char *expect, int retry);

#endif /* SMS_FUNCTION_H_ */
