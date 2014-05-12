/*
 * sensor_rs485.c
 *
 *  Created on: 2014年5月7日
 *      Author: qinbh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include "types.h"
#include "uart_ops.h"
#include "rtc_alarm.h"
#include "file_ops.h"
#include "io_util.h"
#include "device.h"

#define _DEBUG

extern pthread_mutex_t rs485_mutex;
extern unsigned int sensor_status;

extern unsigned int data_qixiang_flag;
extern int av_rs485_used;

extern int sample_avg(int *data, int size);
extern int Sensor_Get_AlarmValue(byte type, byte index, void *value);
extern void *sensor_qixiang_zigbee(void * arg);

#ifdef _DEBUG
static void debug_out(byte *buf, int len)
{
	int i;
	for (i = 0; i < len; i++)
		logcat_raw("0x%02x ", (unsigned char)buf[i]);
	logcat_raw("\n");
}
#endif

int Sensor_RS485_ReadData(byte addr, byte *buf)
{
	byte cmd[13] = {0xa5, 0x5a, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x35, 0x3a, 0xb5};
	usint crc16 = 0;
	int timeout = 2;
	int fd = -1;
	int ret = 0;

//	logcat("--------- Enter func: %s -----------\n", __func__);

	pthread_mutex_lock(&rs485_mutex);
	if (av_rs485_used == 1) {
		av_rs485_used = 0;
		Device_power_ctl(DEVICE_RS485_RESET, 1);
	}

	fd = uart_open_dev(UART_PORT_RS485);
	if (fd == -1) {
		logcat("RS485 Open port: %s\n", strerror(errno));
		pthread_mutex_unlock(&rs485_mutex);
		return -1;
	}
	uart_set_speed(fd, UART_RS485_SPEDD);
	if(uart_set_parity(fd, 8, 1, 'N') == -1) {
		logcat ("RS485 %s: Set Parity Error", UART_PORT_RS485);
		goto err;
	}

	if (buf == NULL)
		goto err;

	cmd[2] = addr;
	crc16 = RTU_CRC(cmd, 10);
	cmd[10] = (crc16 & 0xff00) >> 8;
	cmd[11] = crc16 & 0x00ff;

#ifdef _DEBUG
	logcat("RS485 Sensor 0x%x Send Cmd: ", addr);
	debug_out(cmd, 13);
#endif

	system("echo 1 >/sys/devices/platform/gpio-power.0/rs485_direction");
	ret = io_writen(fd, cmd, 13);
	if (ret != 13) {
		logcat("RS485 Sensor 0x%x: write error, ret = %d\n", addr, ret);
		goto err;
	}
	usleep(250 * 1000);

	system("echo 0 >/sys/devices/platform/gpio-power.0/rs485_direction");
	usleep(50 * 1000);
	memset(buf, 0, 16);
	ret = io_readn(fd, buf, 13, timeout);
	if (ret <= 0) {
		logcat("RS485 Sensor 0x%x: read error, ret = %d\n", addr, ret);
		goto err;
	}

#ifdef _DEBUG
	logcat("RS485 Sensor 0x%x Receive: ", addr);
	debug_out(buf, 13);
#endif

	crc16 = (buf[10] << 8) | buf[11];
//	logcat("crc16 = 0x%x, caculate = 0x%0x\n", crc16, RTU_CRC(buf, 10));
	if (crc16 != RTU_CRC(buf, 10)) {
		logcat("RS485 Sensor 0x%x: CRC Check error.\n", addr);
		goto err;
	}

	close(fd);

	pthread_mutex_unlock(&rs485_mutex);

	return 0;

err:
	close(fd);
	pthread_mutex_unlock(&rs485_mutex);
	return -1;
}

void *sensor_qixiang_rs485_temp(void * arg)
{
	int i, j;
	byte buf[64];
	int temp[6];
	int humi[6];
	int pres[6];
	float f_threshold = 0.0;
	int   i_threshold = 0;
	byte addr_temp = 0x01;
	Data_qixiang_t *pdata = (Data_qixiang_t *)arg;

	logcat("Sensor: Get Temp Humi Pressure from rs485 device.\n");

	memset(temp, 0, 6);
	memset(humi, 0, 6);
	memset(pres, 0, 6);
	j = 0;

	for (i = 0; i < 6; i++) {
		memset(buf, 0, 64);
		if (Sensor_RS485_ReadData(addr_temp, buf) == 0) {
			temp[j] = ((buf[4] & 0x7f) << 8) | buf[5];
			if (buf[4] & 0x80)
				temp[j] = 0 - temp[j];
			humi[j] = (buf[6] << 8) | buf[7];
			pres[j] = (buf[8] << 8) | buf[9];
			logcat("RS485 Sample: i = %d, j = %d\n", i, j);
			logcat("RS485 Sample: temp = %d, humi = %d, press = %d\n", temp[j], humi[j], pres[j]);
			j++;
			data_qixiang_flag++;
		}

		if ((i == 5) && (j > 0)) {
			pdata->Air_Temperature = (float)sample_avg(temp, j) / 10;
			pdata->Humidity = (usint)sample_avg(humi, j);
			pdata->Air_Pressure = (float)sample_avg(pres, j);
			CMA_Env_Parameter.temp = pdata->Air_Temperature;

			logcat("Temperature: i = %d, j = %d, %f, %d, %f\n", i, j, pdata->Air_Temperature,
					pdata->Humidity, pdata->Air_Pressure);

			if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 6, &f_threshold) == 0) {
//				logcat("temp = %f, f_threshold = %f \n", s_data.Air_Temperature, f_threshold);
				if (pdata->Air_Temperature > f_threshold) {
					pdata->Alerm_Flag |= (1 << 5);
				}
			}
			else {
				logcat("Sensor Get temp threshold error.\n");
			}

			if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 7, &i_threshold) == 0) {
				if (pdata->Humidity > i_threshold)
					pdata->Alerm_Flag |= (1 << 6);
			}

			if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 8, &f_threshold) == 0) {
				if (pdata->Air_Pressure > f_threshold)
					pdata->Alerm_Flag |= (1 << 7);
			}
		}

		if (i < 5)
			sleep(8);
	}

	if (j > 0) {
		sensor_status |= (1 << 0);
	}

	return 0;
}

void *sensor_qixiang_rs485_radiation(void * arg)
{
	int i, j;
	byte buf[64];
	int radia[6];
	int   i_threshold = 0;
	byte addr_radiation = 0x07;
	Data_qixiang_t *pdata = (Data_qixiang_t *)arg;

	logcat("Sensor: Get radiation data from rs485 device.\n");

	memset(radia, 0, 6);
	j = 0;

	for (i = 0; i < 6; i++) {
		memset(buf, 0, 64);
		if (Sensor_RS485_ReadData(addr_radiation, buf) == 0) {
			radia[j] = (buf[4] << 8) | buf[5];
			logcat("RS485 Sample: i = %d, j = %d, radiation = %d\n", i, j, radia[j]);
			j++;
			data_qixiang_flag++;
		}

		if ((i == 5) && (j > 0)) {
			pdata->Radiation_Intensity = sample_avg(radia, j);

			if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 11, &i_threshold) == 0) {
//				logcat("temp = %f, f_threshold = %f \n", s_data.Air_Temperature, f_threshold);
				if (pdata->Radiation_Intensity > i_threshold) {
					pdata->Alerm_Flag |= (1 << 10);
				}
			}
			else {
				logcat("Sensor Get radiation threshold error.\n");
			}
		}

		if (i < 5)
			sleep(8);
	}

	if (j > 0) {
		sensor_status |= (1 << 4);
	}

	return 0;
}

void *sensor_qixiang_rs485_wind(void * arg)
{
	int i = 0, j = 0;
	float f_threshold = 0.0;
	int   i_threshold = 0;
	int sum;
	byte buf[64];
	int speed[3];
	int speed_d[3];
	int max = 0;
	Data_qixiang_t *pdata = (Data_qixiang_t *)arg;

	pdata->Average_WindSpeed_10min = 0;
	pdata->Average_WindDirection_10min = 0;
	pdata->Max_WindSpeed = 0;
	pdata->Extreme_WindSpeed = 0;
	pdata->Standard_WindSpeed = 0;

	memset(speed, 0, 3);
	memset(speed_d, 0, 3);
	j = 0;
	for (i = 0; i < 3; i++) {
		memset(buf, 0, 64);
		if (Sensor_RS485_ReadData(0x05, buf) == 0) {
			speed[j] = (buf[4] << 8) | buf[5];
			speed_d[j] = (buf[6] << 8) | buf[7];
			if (max < speed[j])
				max = speed[j];
			logcat("Windy Sample RS485: i = %d, state = 0x%x\n", i, buf[18]);
			logcat("Windy Sample RS485: speed = %d, direction = %d\n", speed[j], speed_d[j]);

			sensor_status |= (3 << 1);

			data_qixiang_flag++;
			j++;
		}
		if (i < 2)
			sleep(1);
	}

	if (j > 0) {
		sum = 0;
		for (i = 0; i < j; i++)
			sum += speed[i];

		pdata->Average_WindSpeed_10min = (float)(sum) / j / 10.0;
		pdata->Average_WindDirection_10min = speed_d[j - 1];
		pdata->Max_WindSpeed = max / 10.0;
		pdata->Extreme_WindSpeed = max / 10.0;
		pdata->Standard_WindSpeed = speed[2] / 10.0;
	}

	if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 1, &f_threshold) == 0) {
		if (pdata->Average_WindSpeed_10min > f_threshold)
			pdata->Alerm_Flag |= (1 << 0);
	}

	if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 2, &i_threshold) == 0) {
		if (pdata->Average_WindDirection_10min > i_threshold)
			pdata->Alerm_Flag |= (1 << 1);
	}

	if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 3, &f_threshold) == 0) {
		if (pdata->Max_WindSpeed > f_threshold)
			pdata->Alerm_Flag |= (1 << 2);
	}

	if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 4, &f_threshold) == 0) {
		if (pdata->Extreme_WindSpeed > f_threshold)
			pdata->Alerm_Flag |= (1 << 3);
	}

	if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 5, &f_threshold) == 0) {
		if (pdata->Standard_WindSpeed > f_threshold)
			pdata->Alerm_Flag |= (1 << 4);
	}

	return 0;
}

int RS485_Sample_Qixiang(Data_qixiang_t *sp_data)
{
	int ret = 0;
	pthread_t p1 = -1, p2 = -1, p3 = -1, p4 = -1;
	struct record_qixiang record;
	int record_len = 0;

	data_qixiang_flag = 0;
	memset(&record, 0, sizeof(struct record_qixiang));
	record.tm = rtc_get_time();
	record.data.Time_Stamp = record.tm;

	logcat("CMD: RS485 sample Weather data start.\n");

	/* Zigbee Sensor Operation */
	ret = pthread_create(&p1, NULL, sensor_qixiang_zigbee, sp_data);
	if (ret != 0)
		logcat("Sensor: can't create zigbee thread.\n");

	/* RS485 Sensor Operation */
	ret = pthread_create(&p2, NULL, sensor_qixiang_rs485_temp, sp_data);
	if (ret != 0)
		logcat("Sensor: can't create rs485 temp thread.\n");

	ret = pthread_create(&p3, NULL, sensor_qixiang_rs485_radiation, sp_data);
	if (ret != 0)
		logcat("Sensor: can't create rs485 radiation thread.\n");

	ret = pthread_create(&p4, NULL, sensor_qixiang_rs485_wind, sp_data);
	if (ret != 0)
		logcat("Sensor: can't create rs485 wind thread.\n");

	memset(sp_data, 0, sizeof(Data_qixiang_t));
	sp_data->Time_Stamp = record.tm;
	memcpy(sp_data->Component_ID, CMA_Env_Parameter.id, 17);

	ret = pthread_join(p1, NULL);
	if (ret != 0)
		logcat("CMD: can't join with p1 thread.\n");

	ret = pthread_join(p2, NULL);
	if (ret != 0)
		logcat("CMD: can't join with p2 thread.\n");

	ret = pthread_join(p3, NULL);
	if (ret != 0)
		logcat("CMD: can't join with p3 thread.\n");

	ret = pthread_join(p4, NULL);
	if (ret != 0)
		logcat("CMD: can't join with p4 thread.\n");

	if (data_qixiang_flag) {
		record_len = sizeof(struct record_qixiang);
		memcpy(&record.data, sp_data, sizeof(Data_qixiang_t));
		if (File_AppendRecord(RECORD_FILE_QIXIANG, (char *)&record, record_len) < 0) {
			logcat("CMD: Recording Qixiang data error.\n");
		}

		logcat("Sample Qixiang Data: \n");
		logcat("平均风速： %f \n", sp_data->Average_WindSpeed_10min);
		logcat("平均风向： %d \n", sp_data->Average_WindDirection_10min);
		logcat("极大风速： %f \n", sp_data->Extreme_WindSpeed);
		logcat("最大风速： %f \n", sp_data->Max_WindSpeed);
		logcat("标准风速： %f \n", sp_data->Standard_WindSpeed);
		logcat("温 度： %f \n", sp_data->Air_Temperature);
		logcat("湿 度： %d \n", sp_data->Humidity);
		logcat("大气压： %f \n", sp_data->Air_Pressure);
		logcat("降雨量： %f \n", sp_data->Precipitation);
		logcat("降水强度： %f \n", sp_data->Precipitation_Intensity);
		logcat("光辐射： %d \n", sp_data->Radiation_Intensity);
		logcat("Alarm： 0x%x \n", sp_data->Alerm_Flag);
	}

	logcat("CMD: RS485 sample Weather data finished.\n");

	return ret;
}

int RS485_Sample_TGQingXie(Data_incline_t *data)
{
	int ret = -1;
	byte buf[64];
	int angle_x, angle_y;
	struct record_incline record;
	int record_len = 0;
	float f_threshold = 0.0;
	byte addr_rs485_angle = 0x06;

	memset(&record, 0, sizeof(struct record_incline));
	record.tm = rtc_get_time();
	record.data.Time_Stamp = record.tm;

	logcat("CMD: RS485 Sample Incline data start.\n");

	data->Time_Stamp = record.tm;
	memcpy(data->Component_ID, CMA_Env_Parameter.id, 17);
	memset(buf, 0, 64);
	angle_x = angle_y = 0;

	if (Sensor_RS485_ReadData(addr_rs485_angle, buf) == 0) {
		angle_x = ((buf[6] & 0x7f) << 8) | buf[7];
		if (buf[6] & 0x80)
			angle_x = 0 - angle_x;
		angle_y = ((buf[8] & 0x7f) << 8) | buf[9];
		if (buf[8] & 0x80)
			angle_y = 0 - angle_y;
		logcat("Sample: angle_x = %d, angle_y = %d\n", angle_x, angle_y);
	}
	else {
		logcat("CMD: Incline Sensor is not Online.\n");
		sensor_status &= (~(1 << 5));

		goto Finish;
	}

	data->Angle_X = angle_x / 100.0;
	data->Angle_Y = angle_y / 100.0;
	record_len = sizeof(struct record_incline);
	memcpy(&record.data, data, sizeof(Data_incline_t));
	if (File_AppendRecord(RECORD_FILE_TGQXIE, (char *)&record, record_len) < 0) {
		logcat("CMD: Recording Incline data error.\n");
	}
	logcat("Sample Incline Data: \n");
	logcat("顺线倾斜角： %f \n", data->Angle_X);
	logcat("横向倾斜角： %f \n", data->Angle_Y);
	logcat("Alarm： 0x%x \n", data->Alerm_Flag);

	if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_TGQX_PAR, 4, &f_threshold) == 0) {
		if (data->Angle_X > f_threshold)
			data->Alerm_Flag |= (1 << 3);
	}

	if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_TGQX_PAR, 5, &f_threshold) == 0) {
		if (data->Angle_Y > f_threshold)
			data->Alerm_Flag |= (1 << 4);
	}

	ret = 0;

	sensor_status |= (1 << 5);

Finish:
	logcat("CMD: RS485 Sample Incline data finished.\n");

	return ret;
}


int RS485_Sample_FuBing(Data_ice_thickness_t *data)
{
	int ret = -1;
	byte buf[64];
	int force, angle_x, angle_y, wav_cycle, wav_x, wav_y;
	struct record_fubing record;
	int record_len = 0;
	int i, j;
	float f_threshold = 0.0;
	byte addr_rs485_tension = 0x02;

	memset(&record, 0, sizeof(struct record_fubing));
	record.tm = rtc_get_time();
	record.data.Time_Stamp = record.tm;

	logcat("CMD: RS485 Sample Fu Bing data start.\n");

	data->Time_Stamp = record.tm;
	memcpy(data->Component_ID, CMA_Env_Parameter.id, 17);
	force = angle_x = angle_y = wav_cycle = wav_x = wav_y = 0;

	for (i = 0; i < 3; i++) {
		memset(buf, 0, 64);
		for (j = 0; j < 3; j++) {
		if (Sensor_RS485_ReadData(addr_rs485_tension, buf) == 0) {
			force = (buf[4] << 8) | buf[5];
			angle_x = ((buf[6] & 0x7f) << 8) | buf[7];
			if (buf[6] & 0x80)
				angle_x = 0 - angle_x;
			angle_y = ((buf[8] & 0x7f) << 8) | buf[9];
			if (buf[8] & 0x80)
				angle_y = 0 - angle_y;

			logcat("Sample: force = %d, angle_x = %d, angle_y = %d\n", force, angle_x, angle_y);

			ret = 0;

			goto Sample_finish;
		}
		sleep(5);
		}

		addr_rs485_tension++;
	}

Sample_finish:
	if (ret == 0) {
		data->Tension = force * 9.8; // force: kgf, Tension: N
//		data->Windage_Yaw_Angle = asin((angle_x - 1024.0) / 819.0) * 180 / 3.14;
//		data->Deflection_Angle = asin((angle_y - 1024.0) / 819.0) * 180 / 3.14;
		data->Windage_Yaw_Angle = angle_x / 100;
		data->Deflection_Angle = angle_y  / 100;
		data->Tension_Difference = wav_cycle;
		data->Reserve1 = wav_x;
		data->Reserve2 = wav_y;

		if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_FUBING_PAR, 1, &f_threshold) == 0) {
			if (data->Tension > f_threshold)
				data->Alerm_Flag |= (1 << 0);
		}

		if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_FUBING_PAR, 2, &f_threshold) == 0) {
			if (data->Windage_Yaw_Angle > f_threshold)
				data->Alerm_Flag |= (1 << 1);
		}

		if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_FUBING_PAR, 3, &f_threshold) == 0) {
			if (data->Deflection_Angle > f_threshold)
				data->Alerm_Flag |= (1 << 2);
		}

		if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_FUBING_PAR, 4, &f_threshold) == 0) {
			if (data->Tension_Difference > f_threshold)
				data->Alerm_Flag |= (1 << 3);
		}

		record_len = sizeof(struct record_incline);
		memcpy(&record.data, data, sizeof(Data_ice_thickness_t));
		if (File_AppendRecord(RECORD_FILE_FUBING, (char *)&record, record_len) < 0) {
			logcat("CMD: Recording Incline data error.\n");
		}
		logcat("Sample Fubing Data: \n");
		logcat("等值覆冰厚度： %f \n", data->Equal_IceThickness);
		logcat("综合悬挂载荷： %f \n", data->Tension);
		logcat("不均衡张力差： %f \n", data->Tension_Difference);
		logcat("绝缘子串风偏角： %f \n", data->Windage_Yaw_Angle);
		logcat("绝缘子串倾斜角： %f \n", data->Deflection_Angle);
		logcat("Alarm： 0x%x \n", data->Alerm_Flag);

		sensor_status |= (1 << 6);
	}
	else {
		sensor_status &= (~(1 << 6));
	}

	logcat("CMD: RS485 Sample Fubing data finished.\n");

	return ret;
}
