/*
 * sensor_can.c
 *
 *  Created on: 2014年5月7日
 *      Author: qinbh
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <net/if.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <pthread.h>
#include <errno.h>
#include "sensor_ops.h"
#include "uart_ops.h"
#include "rtc_alarm.h"
#include "file_ops.h"
#include "io_util.h"
#include "list.h"
#include "device.h"

#define  CAN_DEVICE_NAME	"can0"

extern pthread_mutex_t can_mutex;
extern unsigned int sensor_status;

extern unsigned int data_qixiang_flag;

extern int sample_avg(int *data, int size);
extern int Sensor_Get_AlarmValue(byte type, byte index, void *value);
extern void *sensor_qixiang_zigbee(void * arg);

struct can_device {
	int type;
	usint addr;
};

enum sensor_type {
	SENSOR_TEMP = 0,
	SENSOR_WSPEED,
	SENSOR_WDIRECTION,
	SENSOR_RAIN,
	SENSOR_RADIATION,
	SENSOR_ANGLE,
	SENSOR_TENSION
};

static struct can_device Sensor_CAN_List_Qixiang[] = {
	{
		.type = SENSOR_TEMP,
		.addr = 0x000a,
	},
	{
		.type = SENSOR_WSPEED,
		.addr = 0x000f,
	},
	{
		.type = SENSOR_WDIRECTION,
		.addr = 0x000f,
	},
	{
		.type = SENSOR_RAIN,
		.addr = 0x001e,
	},
	{
		.type = SENSOR_RADIATION,
		.addr = 0x0023,
	},
};

static struct can_device Sensor_CAN_List_Angle[] = {
	{
		.type = SENSOR_ANGLE,
		.addr = 0x0028,
	},
};

static struct can_device Sensor_CAN_List_PullForce[] = {
	{
		.type = SENSOR_TENSION,
		.addr = 0x003c,
	},
	{
		.type = SENSOR_TENSION,
		.addr = 0x003d,
	},
	{
		.type = SENSOR_TENSION,
		.addr = 0x003e,
	},
};

#define _DEBUG

#ifdef _DEBUG
static void debug_out(byte *buf, int len)
{
	int i;
	for (i = 0; i < len; i++)
		logcat_raw("0x%02x ", (unsigned char)buf[i]);
	logcat_raw("\n");
}
#endif

static int can_socket_init(char *interface)
{
	struct ifreq ifr;
	struct sockaddr_can addr;
	int family = PF_CAN, type = SOCK_RAW, proto = CAN_RAW;
	int s;

	if (interface == NULL)
		return -1;

	s = socket(family, type, proto);
	if (s < 0) {
		logcat("socket: %s\n", strerror(errno));
		return -1;
	}

	addr.can_family = family;
	strcpy(ifr.ifr_name, interface);
	if (ioctl(s, SIOCGIFINDEX, &ifr)) {
		logcat("ioctl: %s\n", strerror(errno));
		close(s);
		return -1;
	}
	addr.can_ifindex = ifr.ifr_ifindex;

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		logcat("bind: %s\n", strerror(errno));
		close(s);
		return -1;
	}

/*  int s_buf_size = 64 * 1024;
    unsigned int m = sizeof(s_buf_size);
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&s_buf_size, sizeof(int)) < 0) {
		logcat("setsockopt fail to change SNDbuf.\n");
		return -1;
	}
	getsockopt(s, SOL_SOCKET, SO_SNDBUF, (void *)&s_buf_size, &m);
	logcat("Socket Write Buffer size = %d \n", s_buf_size);
*/
	return s;
}

int Can_Send(byte *buf, int len)
{
	struct can_frame frame = {
		.can_id = 1,
	};
	int s, ret;
	int loopcount = 1;

	if (buf == NULL)
		return -1;

	if ((s = can_socket_init(CAN_DEVICE_NAME)) < 0)
		return -1;

	if (len > 8) {
		logcat("Can Send buf length larger than 8, set to 8.\n");
		len = 8;
	}

	memcpy(frame.data, buf, len);
	frame.can_dlc = len;
	frame.can_id &= CAN_EFF_MASK;
	frame.can_id |= CAN_EFF_FLAG;

	while (loopcount--) {
		ret = write(s, &frame, sizeof(frame));
		if (ret == -1) {
			logcat("CAN Device write: %s\n", strerror(errno));
			break;
		}
	}

	close(s);
	return 0;
}

int Can_Recv(byte *buf)
{
	struct can_frame frame;
	int	s = -1;
	int nbytes, i;
	byte *pbuf = NULL;

	if (buf == NULL)
		return -1;

	if ((s = can_socket_init(CAN_DEVICE_NAME)) < 0)
		return -1;

	pbuf = buf;
	for (i = 0; i < 3; i++) {
		if ((nbytes = read(s, &frame, sizeof(struct can_frame))) < 0) {
			logcat("CAN Device read: %s\n", strerror(errno));
			return -1;
		}
		memcpy(pbuf, frame.data, frame.can_dlc);
		pbuf += frame.can_dlc;
		logcat("nbytes = %d, dlc = %d\n", nbytes, frame.can_dlc);
		logcat("");
		for (i = 0; i < 8; i++)
			logcat_raw("0x%02x ", frame.data[i]);
		logcat("\n");
	}

	close(s);
	return 0;
}

int Sensor_Can_ReadData(usint addr, byte *buf)
{
	struct can_frame frame = {
		.can_id = 1,
	};
	int s, ret, i;
	byte cmd[8] = {0x05, 0x01};
	usint crc16 = 0;
	byte *pbuf = NULL;
	int timeout = 2;

//	logcat("--------- Enter func: %s -----------\n", __func__);

	if (buf == NULL)
		return -1;

	cmd[2] = (addr & 0xff00) >> 8;
	cmd[3] = addr & 0x00ff;
	crc16 = RTU_CRC(cmd, 6);
	cmd[6] = (crc16 & 0xff00) >> 8;
	cmd[7] = crc16 & 0x00ff;

#ifdef _DEBUG
//	logcat("CAN Sensor Send Cmd: ");
//	debug_out(cmd, 8);
#endif

	pthread_mutex_lock(&can_mutex);
	Device_power_ctl(DEVICE_CAN_12V, 1);

	if ((s = can_socket_init(CAN_DEVICE_NAME)) < 0) {
		Device_power_ctl(DEVICE_CAN_12V, 0);
		pthread_mutex_unlock(&can_mutex);
		return -1;
	}

	memcpy(frame.data, cmd, 8);
	frame.can_dlc = 8;
	frame.can_id &= CAN_EFF_MASK;
	frame.can_id |= CAN_EFF_FLAG;

	ret = write(s, &frame, sizeof(frame));
	if (ret == -1) {
		if (errno == ENOBUFS) {
			logcat("CAN No write buffer left.\n");
		}
		logcat("CAN Device write: %s\n", strerror(errno));
		goto err;
	}

	pbuf = buf;
	for (i = 0; i < 3; i++) {
		if ((ret = io_readn(s, &frame, sizeof(struct can_frame), timeout)) < 0) {
			logcat("CAN Device read: %s\n", strerror(errno));
			goto err;
		}
		memcpy(pbuf, frame.data, frame.can_dlc);
		pbuf += frame.can_dlc;
#ifdef _DEBUG
//		logcat("nbytes = %d, dlc = %d\n", ret, frame.can_dlc);
//		int j;
//		logcat("");
//		for (j = 0; j < 8; j++)
//			logcat_raw("%02x ", frame.data[j]);
//		logcat_raw("\n");
#endif
	}

	pbuf = buf + 2;
	crc16 = RTU_CRC(pbuf, 17);
//	logcat("crc16 = %x \n", crc16);
	if (crc16 != ((buf[19] << 8) | buf[20])) {
		logcat("CRC check Error.\n");
		goto err;
	}

	Device_power_ctl(DEVICE_CAN_12V, 0);
	pthread_mutex_unlock(&can_mutex);

	close(s);
	return 0;

err:
	Device_power_ctl(DEVICE_CAN_12V, 0);
	pthread_mutex_unlock(&can_mutex);
	close(s);
	return -1;
}

int Sensor_Can_Config(usint addr, usint t)
{
	struct can_frame frame = {
		.can_id = 1,
	};
	int s, ret, i;
	byte cmd[8] = {0x05, 0x0a};
	usint crc16 = 0;
	byte rbuf[8] = {0};

	if (t < 10)
		t = 10;

	cmd[2] = (addr & 0xff00) >> 8;
	cmd[3] = addr & 0x00ff;
	cmd[4] = (t & 0xff00) >> 8;
	cmd[5] = t & 0x00ff;
	crc16 = RTU_CRC(cmd, 6);
	cmd[6] = (crc16 & 0xff00) >> 8;
	cmd[7] = crc16 & 0x00ff;

#ifdef _DEBUG
	logcat("CAN Sensor Send Cmd: ");
	debug_out(cmd, 8);
#endif

	if ((s = can_socket_init(CAN_DEVICE_NAME)) < 0)
		return -1;

	memcpy(frame.data, cmd, 8);
	frame.can_dlc = 8;
	frame.can_id &= CAN_EFF_MASK;
	frame.can_id |= CAN_EFF_FLAG;

	ret = write(s, &frame, sizeof(frame));
	if (ret == -1) {
		logcat("CAN Device write: %s\n", strerror(errno));
		close(s);
		return -1;
	}

	if ((ret = read(s, &frame, sizeof(struct can_frame))) < 0) {
		logcat("CAN Device read: %s\n", strerror(errno));
		close(s);
		return -1;
	}
	if (frame.can_dlc != 8) {
		close(s);
		return -1;
	}

	logcat("nbytes = %d, dlc = %d\n", ret, frame.can_dlc);
	logcat("");
	for (i = 0; i < 8; i++)
		logcat_raw("0x%02x ", frame.data[i]);
	logcat("\n");

	memcpy(rbuf, frame.data, 8);
	crc16 = (rbuf[6] << 8) | rbuf[7];
	if (crc16 != RTU_CRC(rbuf, 6)) {
		logcat("CRC Check error.\n");
		close(s);
		return -1;
	}

	cmd[4] = 0;
	cmd[5] = 0;

	if (memcmp(rbuf, cmd, 6) != 0) {
		close(s);
		return -1;
	}

	close(s);
	return 0;
}


void *sensor_qixiang_can_temp(void * arg)
{
	int i, j;
	byte buf[64];
	int temp[6];
	int humi[6];
	int pres[6];
	float f_threshold = 0.0;
	int   i_threshold = 0;
	Data_qixiang_t *pdata = (Data_qixiang_t *)arg;

	logcat("Sensor: Get Temp Humi Pressure from CAN device.\n");

	memset(temp, 0, 6);
	memset(humi, 0, 6);
	memset(pres, 0, 6);
	j = 0;

	for (i = 0; i < 6; i++) {
		memset(buf, 0, 64);
		if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[0].addr, buf) == 0) {
			temp[i] = ((buf[6] & 0x7f) << 8) | buf[7];
			if (buf[6] & 0x80)
				temp[i] = 0 - temp[i];
			humi[i] = (buf[8] << 8) | buf[9];
			pres[i] = (buf[10] << 8) | buf[11];
			logcat("CAN Sample: i = %d, state = 0x%x\n", i, buf[18]);
			logcat("CAN Sample: temp = %d, humi = %d, press = %d\n", temp[i], humi[i], pres[i]);
			j++;
			data_qixiang_flag++;
		}

		if ((i == 5) && (j > 0)) {
			pdata->Air_Temperature = (float)sample_avg(temp, j) / 10;
			pdata->Humidity = (usint)sample_avg(humi, j);
			pdata->Air_Pressure = (float)sample_avg(pres, j);
			CMA_Env_Parameter.temp = pdata->Air_Temperature;

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

void *sensor_qixiang_can_radiation(void * arg)
{
	int i, j;
	byte buf[64];
	int radia[6];
	int   i_threshold = 0;
	Data_qixiang_t *pdata = (Data_qixiang_t *)arg;

	logcat("Sensor: Get radiation data from CAN device.\n");

	memset(radia, 0, 6);
	j = 0;

	for (i = 0; i < 6; i++) {
		memset(buf, 0, 64);
		if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[4].addr, buf) == 0) {
			radia[i] = (buf[6] << 8) | buf[7];
			logcat("CAN Sample: i = %d, state = 0x%x\n", i, buf[18]);
			logcat("CAN Sample: radia = %d\n", radia[i]);
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

void *sensor_qixiang_can_wind(void * arg)
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
		if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[1].addr, buf) == 0) {
			speed[j] = (buf[6] << 8) | buf[7];
			speed_d[j] = (buf[8] << 8) | buf[9];
			if (max < speed[j])
				max = speed[j];
			logcat("Windy Sample CAN: i = %d, state = 0x%x\n", i, buf[18]);
			logcat("Windy Sample CAN: speed = %d, direction = %d\n", speed[j], speed_d[j]);

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

int CAN_Sample_Qixiang(Data_qixiang_t *sp_data)
{
	int ret = 0;
	pthread_t p1 = -1, p2 = -1, p3 = -1, p4 = -1;
	struct record_qixiang record;
	int record_len = 0;

	data_qixiang_flag = 0;
	memset(&record, 0, sizeof(struct record_qixiang));
	record.tm = rtc_get_time();
	record.data.Time_Stamp = record.tm;

	logcat("CMD: CAN sample Weather data start.\n");

	/* Zigbee Sensor Operation */
	ret = pthread_create(&p1, NULL, sensor_qixiang_zigbee, sp_data);
	if (ret != 0)
		logcat("Sensor: can't create zigbee thread.\n");

	/* CAN Sensor Operation */
	ret = pthread_create(&p2, NULL, sensor_qixiang_can_temp, sp_data);
	if (ret != 0)
		logcat("Sensor: can't create CAN temp thread.\n");

	ret = pthread_create(&p3, NULL, sensor_qixiang_can_radiation, sp_data);
	if (ret != 0)
		logcat("Sensor: can't create CAN radiation thread.\n");

	ret = pthread_create(&p4, NULL, sensor_qixiang_can_wind, sp_data);
	if (ret != 0)
		logcat("Sensor: can't create CAN wind thread.\n");

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

	logcat("CMD: CAN sample Weather data finished.\n");

	return ret;
}

int CAN_Sample_TGQingXie(Data_incline_t *data)
{
	int ret = -1;
	byte buf[64];
	int angle_x, angle_y;
	struct record_incline record;
	int record_len = 0;
	float f_threshold = 0.0;

	memset(&record, 0, sizeof(struct record_incline));
	record.tm = rtc_get_time();
	record.data.Time_Stamp = record.tm;

	logcat("CMD: CAN Sample Incline data start.\n");

	data->Time_Stamp = record.tm;
	memcpy(data->Component_ID, CMA_Env_Parameter.id, 17);
	memset(buf, 0, 64);
	angle_x = angle_y = 0;

	if (Sensor_Can_ReadData(Sensor_CAN_List_Angle[0].addr, buf) == 0) {
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
	logcat("CMD: CAN Sample Incline data finished.\n");

	return ret;
}


int CAN_Sample_FuBing(Data_ice_thickness_t *data)
{
	int ret = -1;
	byte buf[64];
	int force, angle_x, angle_y, wav_cycle, wav_x, wav_y;
	struct record_fubing record;
	int record_len = 0;
	int s_num = sizeof(Sensor_CAN_List_PullForce) / sizeof(struct can_device);
	int i;
	float f_threshold = 0.0;

	memset(&record, 0, sizeof(struct record_fubing));
	record.tm = rtc_get_time();
	record.data.Time_Stamp = record.tm;

	logcat("CMD: CAN Sample Fu Bing data start.\n");

	data->Time_Stamp = record.tm;
	memcpy(data->Component_ID, CMA_Env_Parameter.id, 17);
	force = angle_x = angle_y = wav_cycle = wav_x = wav_y = 0;

	for (i = 0; i < s_num; i++) {
		memset(buf, 0, 64);
		if (Sensor_Can_ReadData(Sensor_CAN_List_PullForce[i].addr, buf) == 0) {
			force = (buf[6] << 8) | buf[7];
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
		else
			logcat("CMD: FuBing %d Sensor is not Online.\n", i);
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

		sensor_status |= (1 << 6);
	}
	else {
		sensor_status &= (~(1 << 6));
	}

	logcat("CMD: CAN Sample Fubing data finished.\n");

	return ret;
}
