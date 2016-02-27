/*
 * nrsec3000_spi.h
 *
 *  Created on: May 5, 2015
 *      Author: bqin
 */

#ifndef NRSEC3000_SPI_H_
#define NRSEC3000_SPI_H_

extern int  nrsec3000_spi_init(void);
extern void nrsec3000_spi_shutdown(void);
extern int  nrsec3000_spi_read_byte(uint8_t *data);
extern int  nrsec3000_spi_write_byte(uint8_t *data);
extern int  nrsec3000_spi_read(uint8_t *data, int len);
extern int  nrsec3000_spi_write(uint8_t *data, int len);
extern int  nrsec3000_spi_wait(uint8_t *result, int len, int timeout);

#endif /* NRSEC3000_SPI_H_ */
