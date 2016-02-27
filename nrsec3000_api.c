/*
 * nrsec3000_api.c
 *
 *  Created on: May 5, 2015
 *      Author: bqin
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>

#include "nrsec3000_spi.h"
#include "nrsec3000_api.h"

static const uint8_t crc7_table[256] = {
0x00, 0x09, 0x12, 0x1b, 0x24, 0x2d, 0x36, 0x3f,
0x48, 0x41, 0x5a, 0x53, 0x6c, 0x65, 0x7e, 0x77,
0x19, 0x10, 0x0b, 0x02, 0x3d, 0x34, 0x2f, 0x26,
0x51, 0x58, 0x43, 0x4a, 0x75, 0x7c, 0x67, 0x6e,
0x32, 0x3b, 0x20, 0x29, 0x16, 0x1f, 0x04, 0x0d,
0x7a, 0x73, 0x68, 0x61, 0x5e, 0x57, 0x4c, 0x45,
0x2b, 0x22, 0x39, 0x30, 0x0f, 0x06, 0x1d, 0x14,
0x63, 0x6a, 0x71, 0x78, 0x47, 0x4e, 0x55, 0x5c,
0x64, 0x6d, 0x76, 0x7f, 0x40, 0x49, 0x52, 0x5b,
0x2c, 0x25, 0x3e, 0x37, 0x08, 0x01, 0x1a, 0x13,
0x7d, 0x74, 0x6f, 0x66, 0x59, 0x50, 0x4b, 0x42,
0x35, 0x3c, 0x27, 0x2e, 0x11, 0x18, 0x03, 0x0a,
0x56, 0x5f, 0x44, 0x4d, 0x72, 0x7b, 0x60, 0x69,
0x1e, 0x17, 0x0c, 0x05, 0x3a, 0x33, 0x28, 0x21,
0x4f, 0x46, 0x5d, 0x54, 0x6b, 0x62, 0x79, 0x70,
0x07, 0x0e, 0x15, 0x1c, 0x23, 0x2a, 0x31, 0x38,
0x41, 0x48, 0x53, 0x5a, 0x65, 0x6c, 0x77, 0x7e,
0x09, 0x00, 0x1b, 0x12, 0x2d, 0x24, 0x3f, 0x36,
0x58, 0x51, 0x4a, 0x43, 0x7c, 0x75, 0x6e, 0x67,
0x10, 0x19, 0x02, 0x0b, 0x34, 0x3d, 0x26, 0x2f,
0x73, 0x7a, 0x61, 0x68, 0x57, 0x5e, 0x45, 0x4c,
0x3b, 0x32, 0x29, 0x20, 0x1f, 0x16, 0x0d, 0x04,
0x6a, 0x63, 0x78, 0x71, 0x4e, 0x47, 0x5c, 0x55,
0x22, 0x2b, 0x30, 0x39, 0x06, 0x0f, 0x14, 0x1d,
0x25, 0x2c, 0x37, 0x3e, 0x01, 0x08, 0x13, 0x1a,
0x6d, 0x64, 0x7f, 0x76, 0x49, 0x40, 0x5b, 0x52,
0x3c, 0x35, 0x2e, 0x27, 0x18, 0x11, 0x0a, 0x03,
0x74, 0x7d, 0x66, 0x6f, 0x50, 0x59, 0x42, 0x4b,
0x17, 0x1e, 0x05, 0x0c, 0x33, 0x3a, 0x21, 0x28,
0x5f, 0x56, 0x4d, 0x44, 0x7b, 0x72, 0x69, 0x60,
0x0e, 0x07, 0x1c, 0x15, 0x2a, 0x23, 0x38, 0x31,
0x46, 0x4f, 0x54, 0x5d, 0x62, 0x6b, 0x70, 0x79
};

/*
 * 描述:
 * 		计算出一串数据的CRC校验码
 * 参数:
 * 		buff: [输入],数据缓冲的首地址
 * 		len: [输入],数据长度
 * 返回结果:
 * 		计算出的CRC校验码
 */
static uint8_t get_crc7(uint8_t *buff, int len)
{
	int i;
	uint8_t crc7_accum = 0;

	for (i=0; i < len; i++) {
		crc7_accum = crc7_table[(crc7_accum << 1) ^ buff[i]];
	}

	return crc7_accum;
}


static void nrsec3000_reset(void)
{
	system("echo 0 > /sys/devices/platform/gpio-power.0/power_nrsec");
	sleep(1);
	system("echo 1 > /sys/devices/platform/gpio-power.0/power_nrsec");
}

void nrsec3000_init(void)
{
	nrsec3000_reset();
	nrsec3000_spi_init();
}

int  nrsec3000_SM1_import_key(uint8_t *key)
{
	uint8_t cmd1[] = {0x80, 0xd4, 0x01, 0x00, 0x10};
	uint8_t cmd2[] = {0x80, 0xd4, 0x02, 0x00, 0x10};
	uint8_t ins = 0xd4;
	uint8_t sw[] = { 0x90, 0x00 };
	uint8_t sbuf[256] = { 0x00 };

	nrsec3000_spi_write(cmd1, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, key, 16);
	sbuf[16] = get_crc7(key, 16);
	nrsec3000_spi_write(sbuf, 17);

	if (nrsec3000_spi_wait(sw, 2, 0) < 0)
		return -1;

	nrsec3000_spi_write(cmd2, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memset(sbuf, 0, 16);
	sbuf[16] = get_crc7((uint8_t *)sbuf, 16);
	nrsec3000_spi_write(sbuf, 17);

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);

	if (nrsec3000_spi_wait(sw, 2, 0) < 0)
		return -1;

	return 0;
}

int  nrsec3000_SM1_import_IV(uint8_t *IV)
{
	uint8_t cmd[] = {0x80, 0xd4, 0x04, 0x00, 0x10};
	uint8_t ins = 0xd4;
	uint8_t sw[] = { 0x90, 0x00 };
	uint8_t sbuf[256] = { 0x00 };

	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, IV, 16);
	sbuf[16] = get_crc7(IV, 16);
	nrsec3000_spi_write(sbuf, 17);

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);

	if (nrsec3000_spi_wait(sw, 2, 0) < 0)
		return -1;

	return 0;
}

static  int  nrsec3000_SM1_enc_dec(uint8_t *in, uint8_t *out, uint16_t len, uint16_t *out_len, int type)
{
	uint8_t cmd_enc[] = {0xa0, 0xe0, 0x80, 0x00, 0x00};
	uint8_t cmd_dec[] = {0xa0, 0xe0, 0x81, 0x00, 0x00};
	uint8_t *cmd = NULL;
	uint8_t ins = 0xe0;
	uint8_t sw[] = { 0x00, 0x00 };
	uint8_t sbuf[2049] = { 0x00 };
	uint8_t rbuf[2049] = { 0x00 };

	if (type == 1)
		cmd = cmd_enc;
	else
		cmd = cmd_dec;

	cmd[3] = (len >> 8) & 0xff;
	cmd[4] = len & 0xff;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, in, len);
	sbuf[len] = get_crc7(in, len);
	nrsec3000_spi_write(sbuf, len + 1);

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);

	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read((uint8_t *)out_len, 2);

	memset(sbuf, 0xaa, *out_len);
	nrsec3000_spi_write(sbuf, *out_len);
	nrsec3000_spi_read(rbuf, *out_len);
	if (rbuf[*out_len - 1] != get_crc7(rbuf, (*out_len - 1)))
		return -1;
	memcpy(out, rbuf, *out_len -1);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;;
}

int  nrsec3000_SM1_encrypt(uint8_t *in, uint8_t *out, uint16_t len, uint16_t *out_len)
{
	return nrsec3000_SM1_enc_dec(in, out, len, out_len, 1);
}

int  nrsec3000_SM1_decrypt(uint8_t *in, uint8_t *out, uint16_t len, uint16_t *out_len)
{
	return nrsec3000_SM1_enc_dec(in, out, len, out_len, 0);
}

int  nrsec3000_SM2_gen_key(uint8_t index)
{
	uint8_t cmd[] = {0x80, 0xb2, 0x00, 0x00, 0x00};
	uint8_t sw[] = { 0x90, 0x00 };

	cmd[2] = index;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(sw, 2, 0) < 0)
		return -1;;

	return 0;
}

static int  nrsec3000_SM2_export_Key(uint8_t *key, int index, int type)
{
	uint8_t cmd_public[] = {0x80, 0xb8, 0x01, 0x00, 0x40};
	uint8_t cmd_private[] = {0x80, 0xb8, 0x02, 0x00, 0x20};
	uint8_t *cmd = NULL;
	uint8_t ins = 0xb8;
	uint8_t sw[] = { 0x90, 0x00 };
	uint8_t sbuf[256] = { 0x00 };
	uint8_t rbuf[128] = { 0x00 };
	uint8_t len = 0;

	if (type == 1) {
		cmd = cmd_public;
		len = 0x40;
	}
	else {
		cmd = cmd_private;
		len = 0x20;
	}

	cmd[3] = index;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;;

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	nrsec3000_spi_read(rbuf, 1);
	if (rbuf[0] != (len + 1))
		return -1;

	memset(sbuf, 0xaa, len + 1);
	nrsec3000_spi_write(sbuf, len + 1);
	nrsec3000_spi_read(rbuf, len + 1);

	if (rbuf[len] != get_crc7(rbuf, len))
		return -1;
	memcpy(key, rbuf, len);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;
}

int  nrsec3000_SM2_export_pubKey(uint8_t *key, int index)
{
	return nrsec3000_SM2_export_Key(key, index, 1);
}

int  nrsec3000_SM2_export_priKey(uint8_t *key, int index)
{
	return nrsec3000_SM2_export_Key(key, index, 0);
}

static int  nrsec3000_SM2_import_Key(uint8_t *key, int index, int type)
{
	uint8_t cmd_public[] = {0x80, 0xba, 0x01, 0x00, 0x40};
	uint8_t cmd_private[] = {0x80, 0xba, 0x02, 0x00, 0x20};
	uint8_t *cmd = NULL;
	uint8_t ins = 0xba;
	uint8_t sw[] = { 0x90, 0x00 };
	uint8_t sbuf[256] = { 0x00 };
	uint8_t len = 0;

	if (type == 1) {
		cmd = cmd_public;
		len = 0x40;
	}
	else {
		cmd = cmd_private;
		len = 0x20;
	}

	cmd[3] = index;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, key, len);
	sbuf[len] = get_crc7(key, len);
	nrsec3000_spi_write(sbuf, len + 1);

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);

	if (nrsec3000_spi_wait(sw, 2, 0) < 0)
		return -1;;

	return 0;
}

int  nrsec3000_SM2_import_pubKey(uint8_t *key, int index)
{
	return nrsec3000_SM2_import_Key(key, index, 1);
}

int  nrsec3000_SM2_import_priKey(uint8_t *key, int index)
{
	return nrsec3000_SM2_import_Key(key, index, 0);
}

int  nrsec3000_SM2_Sign(uint8_t *in, uint32_t inl, uint8_t *out, int index)
{
	uint8_t cmd[] = {0x80, 0xb4, 0x00, 0x00, 0x20};
	uint8_t ins = 0xb4;
	uint8_t sw[] = { 0x00 };
	uint8_t sbuf[256] = { 0x00 };
	uint8_t rbuf[2049] = { 0x00 };

	cmd[3] = index;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, in, inl);
	sbuf[inl] = get_crc7(in, inl);
	nrsec3000_spi_write(sbuf, inl + 1);

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	nrsec3000_spi_read(rbuf, 1);
	if (rbuf[0] != 65)
		return -1;

	memset(sbuf, 0xaa, 65);
	memset(rbuf, 0, 2049);
	nrsec3000_spi_write(sbuf, 65);
	nrsec3000_spi_read(rbuf, 65);

	if (rbuf[32] != get_crc7(rbuf, 64))
		return -1;
	memcpy(out, rbuf, 34);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;

}

int  nrsec3000_SM2_CheckSign(uint8_t *hash, uint8_t *sign, int index)
{
	uint8_t cmd[] = {0x80, 0xb6, 0x00, 0x00, 0x60};
	uint8_t ins = 0xb4;
	uint8_t sw[] = { 0x00 };
	uint8_t sbuf[256] = { 0x00 };

	cmd[3] = index;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, hash, 32);
	memcpy(sbuf + 32, sign, 64);
	sbuf[96] = get_crc7(sbuf, 96);
	nrsec3000_spi_write(sbuf, 97);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;
}

/*
 * inl: must be 32 bytes
 */
int  nrsec3000_SM2_encrypt(uint8_t *in, uint32_t inl, uint8_t *out, int index)
{
	uint8_t cmd[] = {0x80, 0xb3, 0x01, 0x00, 0x20};
	uint8_t ins = 0xb3;
	uint8_t sw[] = { 0x00 };
	uint8_t sbuf[256] = { 0x00 };
	uint8_t rbuf[2049] = { 0x00 };

	if (inl != 32) {
		printf("SM2 encryption wrong data length.\n");
		return -1;
	}

	cmd[3] = index;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, in, inl);
	sbuf[inl] = get_crc7(in, inl);
	nrsec3000_spi_write(sbuf, inl + 1);

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	nrsec3000_spi_read(rbuf, 1);
	if (rbuf[0] != 129)
		return -1;

	memset(sbuf, 0xaa, 129);
	memset(rbuf, 0, 2049);
	nrsec3000_spi_write(sbuf, 129);
	nrsec3000_spi_read(rbuf, 129);

	if (rbuf[128] != get_crc7(rbuf, 128))
		return -1;
	memcpy(out, rbuf, 128);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;
}

/*
 * inl: must be 128 bytes
 */
int  nrsec3000_SM2_decrypt(uint8_t *in, uint32_t inl, uint8_t *out, int index)
{
	uint8_t cmd[] = {0x80, 0xb3, 0x81, 0x00, 0x80};
	uint8_t ins = 0xb3;
	uint8_t sw[] = { 0x00 };
	uint8_t sbuf[256] = { 0x00 };
	uint8_t rbuf[2049] = { 0x00 };

	if (inl != 128) {
		printf("SM2 decryption wrong data length.\n");
		return -1;
	}

	cmd[3] = index;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, in, inl);
	sbuf[inl] = get_crc7(in, inl);
	nrsec3000_spi_write(sbuf, inl + 1);

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	nrsec3000_spi_read(rbuf, 1);
	if (rbuf[0] != 33)
		return -1;

	memset(sbuf, 0xaa, 33);
	memset(rbuf, 0, 2049);
	nrsec3000_spi_write(sbuf, 33);
	nrsec3000_spi_read(rbuf, 33);

	if (rbuf[128] != get_crc7(rbuf, 32))
		return -1;
	memcpy(out, rbuf, 32);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;
}

int  nrsec3000_SM2_gen_CertRequest(uint8_t *in, uint32_t inl, uint8_t *out, uint16_t *outl, int format, int index)
{
	uint8_t cmd[] = {0x80, 0xb7, 0x00, 0x00, 0x00};
	uint8_t ins = 0xb7;
	uint8_t sw[] = { 0x00 };
	uint8_t sbuf[256] = { 0x00 };
	uint8_t rbuf[2049] = { 0x00 };

	cmd[2] = format;
	cmd[3] = index;
	cmd[4] = inl;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, in, inl);
	sbuf[inl] = get_crc7(in, inl);
	nrsec3000_spi_write(sbuf, inl + 1);

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0xaa;
	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read((uint8_t *)outl, 2);

	memset(sbuf, 0xaa, *outl);
	memset(rbuf, 0, 2049);
	nrsec3000_spi_write(sbuf, *outl);
	nrsec3000_spi_read((uint8_t *)rbuf, *outl);

	if (rbuf[*outl - 1] != get_crc7(rbuf, (*outl - 1)))
		return -1;
	memcpy(out, rbuf, *outl -1);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;
}

int  nrsec3000_gen_random(uint8_t *out, uint8_t len)
{
	uint8_t cmd[] = {0x00, 0x84, 0x00, 0x00, 0x00};
	uint8_t ins = 0x84;
	uint8_t sw[] = { 0x00 };
	uint8_t sbuf[256] = { 0x00 };
	uint8_t rbuf[256] = { 0x00 };

	cmd[4] = len;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	nrsec3000_spi_read(rbuf, 1);
	len = rbuf[0];

	memset(sbuf, 0xaa, len);
	nrsec3000_spi_write(sbuf, len);
	nrsec3000_spi_read(rbuf, len);

	if (rbuf[len - 1] != get_crc7(rbuf, (len - 1)))
		return -1;

	memcpy(out, rbuf, len - 1);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;
}
int  nrsec3000_get_version(uint8_t *out)
{
	uint8_t cmd[] = {0x00, 0x5b, 0x00, 0x00, 0x40};
	uint8_t ins = 0x5b;
	uint8_t sw[] = { 0x00 };
	uint8_t sbuf[256] = { 0x00 };
	uint8_t rbuf[256] = { 0x00 };
	uint8_t len = 0;

	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	nrsec3000_spi_read(rbuf, 1);
	len = rbuf[0];

	memset(sbuf, 0xaa, len);
	nrsec3000_spi_write(sbuf, len);
	nrsec3000_spi_read(rbuf, len);

	if (rbuf[len - 1] != get_crc7(rbuf, (len - 1)))
		return -1;

	memcpy(out, rbuf, len - 1);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;
}

int  nrsec3000_security_verify(uint8_t *in, uint8_t *out)
{
	uint8_t cmd[] = {0x80, 0xb3, 0x01, 0x04, 0x20};
	uint8_t ins = 0xb3;
	uint8_t sw[] = { 0x00 };
	uint8_t sbuf[256] = { 0x00 };
	uint8_t rbuf[2049] = { 0x00 };

	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, in, 32);
	sbuf[32] = get_crc7(in, 32);
	nrsec3000_spi_write(sbuf, 33);

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	nrsec3000_spi_read(rbuf, 1);
	if (rbuf[0] != 147)
		return -1;

	memset(sbuf, 0xaa, 147);
	memset(rbuf, 0, 2049);
	nrsec3000_spi_write(sbuf, 147);
	nrsec3000_spi_read(rbuf, 147);

	if (rbuf[146] != get_crc7(rbuf, 146))
		return -1;
	memcpy(out, rbuf, 146);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;
}

/*
 * 描述:
 * 		NRSEC3000安全芯片的SM3 Hash操作
 * 参数:
 * 		in: [输入],需要进行hash运算的数据的首地址
 * 		inl: [输入],数据的长度(字节数)
 * 		out: [输出],接收Hash结果的内存首地址(结果长度为32字节)
 * 返回结果:
 * 		0:成功
 *		其他:失败
 */
int nrsec3000_SM3_Hash(uint8_t *in, uint32_t inl, uint8_t *out)
{
	uint8_t cmd[] = {0x80, 0xb5, 0x00, 0x00, 0x00};
	uint8_t ins = 0xb5;
	uint8_t sw[] = { 0x00 };
	uint8_t sbuf[32] = { 0x00 };
	uint8_t rbuf[2049] = { 0x00 };

	cmd[3] = (inl >> 8) & 0xff;
	cmd[4] = inl & 0xff;
	nrsec3000_spi_write(cmd, 5);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0x55;
	nrsec3000_spi_write_byte(sbuf);

	memcpy(sbuf, in, inl);
	sbuf[inl] = get_crc7(in, inl);
	nrsec3000_spi_write(sbuf, inl + 1);

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	if (nrsec3000_spi_wait(&ins, 1, 0) < 0)
		return -1;

	sbuf[0] = 0xaa;
	nrsec3000_spi_write_byte(sbuf);
	nrsec3000_spi_read(rbuf, 1);
	if (rbuf[0] != 33)
		return -1;

	memset(sbuf, 0xaa, 33);
	memset(rbuf, 0, 2049);
	nrsec3000_spi_write(sbuf, 33);
	nrsec3000_spi_read(rbuf, 33);

	if (rbuf[32] != get_crc7(rbuf, 32))
		return -1;
	memcpy(out, rbuf, 32);

	memset(sbuf, 0xaa, 2);
	nrsec3000_spi_write(sbuf, 2);
	nrsec3000_spi_read(sw, 2);

	return 0;
}

/*
 * 描述:
 * 		SM3 Hash操作
 * 参数:
 * 		in: [输入],需要进行hash运算的数据的首地址
 * 		inl: [输入],数据的长度(字节数)
 * 		out: [输出],接收Hash结果的内存首地址(sm3结果长度为32字节)
 * 		pubkey: [输入],公钥内容(64字节)
 * 		pucID: [输入],用户标识
 * 		idl: [输入],用户标识的长度
 * 返回结果:
 * 		0:成功
 * 		其他:失败
 */
int sm3(uint8_t *in,int inl,uint8_t *out, uint8_t *pubkey, uint8_t *pucID, int idl)
{
	int nRet,l;
	uint8_t *Z = NULL;
	int entl = 0;
	uint8_t tmpm[32];
	uint8_t abxy[32 * 4] = {
		0xFF,0xFF,0xFF,0xFE,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, /* a */
		0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
		0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFC,
		0x28,0xE9,0xFA,0x9E,0x9D,0x9F,0x5E,0x34,0x4D,0x5A, /* b */
		0x9E,0x4B,0xCF,0x65,0x09,0xA7,0xF3,0x97,0x89,0xF5,
		0x15,0xAB,0x8F,0x92,0xDD,0xBC,0xBD,0x41,0x4D,0x94,
		0x0E,0x93,
		0x32,0xC4,0xAE,0x2C,0x1F,0x19,0x81,0x19,0x5F,0x99, /* x */
		0x04,0x46,0x6A,0x39,0xC9,0x94,0x8F,0xE3,0x0B,0xBF,
		0xF2,0x66,0x0B,0xE1,0x71,0x5A,0x45,0x89,0x33,0x4C,
		0x74,0xC7,
		0xBC,0x37,0x36,0xA2,0xF4,0xF6,0x77,0x9C,0x59,0xBD, /* y */
		0xCE,0xE3,0x6B,0x69,0x21,0x53,0xD0,0xA9,0x87,0x7C,
		0xC6,0x2A,0x47,0x40,0x02,0xDF,0x32,0xE5,0x21,0x39,
		0xF0,0xA0
	};

	l = 2 + idl + 32 * 6;
	Z = (uint8_t *)malloc(l);
	if (!Z)
		return -1;

	entl = idl * 8;
	memset(Z + 1, entl & 0xFF, 1);
	entl >>= 8;
	memset(Z, entl & 0xFF, 1);
	memcpy(Z + 2, pucID, idl);
	memcpy(Z + 2 + idl, abxy, 32 *4);
	memcpy(Z + 2 + idl + 4 * 32, pubkey, 32);
	memcpy(Z + 2 + idl + 5 * 32, pubkey+32, 32);

	nRet = nrsec3000_SM3_Hash(Z,l,tmpm);
	if (nRet != 0)
		goto quit;
	free(Z);

	l = inl + 32;
	Z = (uint8_t *)malloc(l);
	if (!Z) {
		nRet = -1;
		goto quit;
	}

	memcpy(Z,tmpm,32);
	memcpy(Z+32, in, inl);
	nRet = nrsec3000_SM3_Hash(Z,l,out);

quit:
	if (Z)
		free(Z);

	return nRet;
}

