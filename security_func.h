/*
 * security_func.h
 *
 *  Created on: May 5, 2015
 *      Author: bqin
 */

#ifndef SECURITY_FUNC_H_
#define SECURITY_FUNC_H_

#include <stdint.h>

#pragma pack(1)

typedef struct _plaintext_msg
{
	uint8_t 	type;
	uint8_t 	subType;
	uint16_t 	len;
	uint8_t 	ver[2];
	uint16_t    sn;
	uint8_t		sim[16];
	uint8_t		id[18];
	uint8_t		magic[16];
} Plaintext_req_t;

#pragma pack()

int ReqPlainTextCommu();


#endif /* SECURITY_FUNC_H_ */
