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
#include <linux/spi/spidev.h>

#include "nrsec3000_spi.h"

//#define USE_IO_MODE		1

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *device = "/dev/spidev1.0";
static uint8_t mode = 0;
static uint8_t bits = 8;
static uint32_t speed = 1000000;
//static uint16_t delay;
static uint8_t lsb = 1;
static int spi_fd = -1;

static int pabort(const char *s)
{
	perror(s);
	return -1;
}

int nrsec3000_spi_init(void)
{
	int ret = 0;

	spi_fd = open(device, O_RDWR);
	if (spi_fd < 0)
		return pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		return pabort("can't set spi mode");

	ret = ioctl(spi_fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		return pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		return pabort("can't set bits per word");

	ret = ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		return pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		return pabort("can't set max speed hz");

	ret = ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		return pabort("can't get max speed hz");

	/*
	 * LSB first value
	 */
	lsb = 0;
	ret = ioctl(spi_fd, SPI_IOC_WR_LSB_FIRST, &lsb);
	if (ret == -1)
		return pabort("can't set LSB first");

	ret = ioctl(spi_fd, SPI_IOC_RD_LSB_FIRST, &lsb);
	if (ret == -1)
		return pabort("can't get LSB first");

	return 0;
}

void nrsec3000_spi_shutdown(void)
{
	close(spi_fd);
}

#ifndef USE_IO_MODE
static int nrsec3000_spi_transfer(int fd, int len, uint8_t *rbuf, uint8_t *wbuf)
{
	int ret;

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)wbuf,
		.rx_buf = (unsigned long)rbuf,
		.len = len,
		.delay_usecs = 0,
		.speed_hz = 0,
		.bits_per_word = 0,
		.cs_change = 0,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 0)
		perror("can't send spi message");

	return ret;
}
#endif

int nrsec3000_spi_read_byte(uint8_t *data)
{
	if (spi_fd < 0)
		if (nrsec3000_spi_init() < 0)
			return -1;

#ifndef USE_IO_MODE
	if (nrsec3000_spi_transfer(spi_fd, 1, data, NULL) == 0)
		return 1;
	else
		return -1;
#else
	return read(fd, data, 1);
#endif
}

int nrsec3000_spi_write_byte(uint8_t *data)
{
	if (spi_fd < 0)
		if (nrsec3000_spi_init() < 0)
			return -1;

#ifndef USE_IO_MODE
	if (nrsec3000_spi_transfer(spi_fd, 1, NULL, data) == 0)
		return 1;
	else
		return -1;
#else
	return write(fd, data, 1);
#endif
}

int nrsec3000_spi_read(uint8_t *data, int len)
{
	if (spi_fd < 0)
		if (nrsec3000_spi_init() < 0)
			return -1;

#ifndef USE_IO_MODE
	if (nrsec3000_spi_transfer(spi_fd, len, data, NULL) == 0)
		return 1;
	else
		return -1;
#else
	return read(fd, data, len);
#endif
}

int nrsec3000_spi_write(uint8_t *data, int len)
{
	int i = 0;

	if (spi_fd < 0)
		if (nrsec3000_spi_init() < 0)
			return -1;

	for (i = 0; i < len; i++) {
		nrsec3000_spi_write_byte(data + i);
		usleep(100);
	}

	return 0;
}

int  nrsec3000_spi_wait(uint8_t *result, int len, int timeout)
{
	uint8_t cmd[] = { 0xaa };
	uint8_t rbuf[16] = { 0x00 };

	while (1) {
		nrsec3000_spi_write_byte(cmd);
		usleep(100);
		nrsec3000_spi_read(rbuf, len);

		if (memcmp(result, rbuf, len) == 0)
			return 0;
		if (!timeout--)
			break;
	}

	return -1;
}

