#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "sensor_ops.h"
#include "camera_control.h"
#include "rtc_alarm.h"
#include "file_ops.h"
#include "io_util.h"
#include "list.h"
#include "v4l2_lib.h"
#include "cma_commu.h"

#define RECORD_FILE_WINDSEC		"/CMD_Data/record_windsec.dat"
#define RECORD_FILE_WINDAVG		"/CMD_Data/record_windavg.dat"

struct record_winsec {
	time_t tm;
	int speed_sec;
};

struct record_winavg {
	time_t tm;
	int speed_avg;
	int speed_d_avg;
};

struct rtc_alarm_dev wind_sec;
struct rtc_alarm_dev wind_avg;

struct rtc_alarm_dev camera_dev;

#define  CAN_DEVICE_NAME	"can0"
unsigned int Sensor_Online_flag = 0;
struct can_device {
	int type;
	usint addr;
};

pthread_mutex_t can_mutex = PTHREAD_MUTEX_INITIALIZER;

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

typedef struct sensor_device {
	int interface; /* 0: CAN, 1: Zigbee */
	int type;
	usint addr;
	struct list_head list;
} sensor_device_t;

//#define _DEBUG
#ifdef _DEBUG
static void debug_out(byte *buf, int len)
{
	int i;
	for (i = 0; i < len; i++)
		printf("0x%02x ", (unsigned char)buf[i]);
	printf("\n");
}
#endif

int Sensor_Detect_Qixiang(void)
{
	int i, j, times = 3;
	byte buf[64];
	int flag = 0; /* Sensor Exist Flag: temperature bit0 */
	int s_num = sizeof(Sensor_CAN_List_Qixiang) / sizeof(struct can_device);

	/* Detect temperature Sensor */
	for (i = 0; i < times; i++) {
		for (j = 0; j < s_num; j++) {
			memset(buf, 0, 64);
			if ((flag & (1 << i)) != (1 << i)) {
				if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[i].addr, buf) == 0) {
					flag |= (1 << i);
				}
			}
		}
	}

	return flag;
}

static Data_qixiang_t s_data;
static int data_qixiang_flag = 0;

static int sample_avg(int *data, int size)
{
	int i;
	int max, min, total;
	float avg = 0.0;
	if ((data == NULL) | (size < 2))
		return 0;

	max = min = total = data[0];
	for (i = 1; i < size; i++) {
		total += data[i];
		if (data[i] > max)
			max = data[i];
		if (data[i] < min)
			min = data[i];
	}

	printf("total = %d, max = %d, min = %d\n", total, max, min);
	avg = (double)(total - max - min) / (double)(size - 2);

	return (int)(avg + 0.5);
}

static void get_wind_data(Data_qixiang_t *data)
{
	struct record_winsec r_sec;
	struct record_winavg r_avg;
	int r_num = 0;
	int r_len = 0;
	int i = 0;
	time_t now;

	now = rtc_get_time();

	data->Average_WindSpeed_10min = 0;
	data->Average_WindDirection_10min = 0;
	data->Max_WindSpeed = 0;
	data->Extreme_WindSpeed = 0;
	data->Standard_WindSpeed = 0;

	r_len = sizeof(struct record_winavg);
	r_num = File_GetNumberOfRecords(RECORD_FILE_WINDAVG, r_len);
	printf("Wind Average: num = %d\n", r_num);
	if (r_num > 0) {
		File_GetRecordByIndex(RECORD_FILE_WINDAVG, &r_avg, r_len, (r_num - 1));
		if (r_avg.tm > (now - 60 * 60)) {
			data->Average_WindSpeed_10min = r_avg.speed_avg;
			data->Average_WindDirection_10min = r_avg.speed_d_avg;
		}
		for (i = (r_num - 1); i >= 0; i--) {
			File_GetRecordByIndex(RECORD_FILE_WINDAVG, &r_avg, r_len, i);
			if (r_avg.tm < (now - 24 * 60 * 60))
				break;
			if (r_avg.speed_avg > data->Max_WindSpeed)
				data->Max_WindSpeed = r_avg.speed_avg;
		}
	}

	r_len = sizeof(struct record_winsec);
	r_num = File_GetNumberOfRecords(RECORD_FILE_WINDSEC, r_len);
	printf("Wind Second: num = %d\n", r_num);
	if (r_num > 0) {
		for (i = (r_num - 1); i >= 0; i--) {
			File_GetRecordByIndex(RECORD_FILE_WINDSEC, &r_sec, r_len, i);
			if (r_sec.tm < (now - 60 * 60))
				break;
			if (r_sec.speed_sec > data->Extreme_WindSpeed)
				data->Extreme_WindSpeed = r_sec.speed_sec;
		}
	}

	/* ?????????? */
	data->Standard_WindSpeed = data->Extreme_WindSpeed;

	return;
}

void *sensor_sample_qixiang_1(void * arg)
{
	int i;
	byte buf[64];
	int flag = *((int *)arg);
	int temp[6];
	int humi[6];
	int pres[6];
	int radia[6];
	usint alarm = 0;

	printf("Enter func: %s, flag = 0x%x\n", __func__, flag);
	memset(temp, 0, 6);
	memset(humi, 0, 6);
	memset(pres, 0, 6);
	memset(radia, 0, 6);

	for (i = 0; i < 6; i++) {
		if ((flag & 0x1) == 0x01) {
			memset(buf, 0, 64);
			if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[0].addr, buf) == 0) {
				temp[i] = (buf[6] << 8) | buf[7];
				humi[i] = (buf[8] << 8) | buf[9];
				pres[i] = (buf[10] << 8) | buf[11];
				printf("Sample: i = %d, state = 0x%x\n", i, buf[18]);
				printf("Sample: temp = %d, humi = %d, press = %d\n", temp[i], humi[i], pres[i]);
				if (buf[18] != 0)
					alarm |= (buf[18] & 0x07) << 5;
			}
			if (i == 5) {
				s_data.Air_Temperature = (float)sample_avg(temp, 6) / 10;
				s_data.Humidity = (usint)sample_avg(humi, 6);
				s_data.Air_Pressure = sample_avg(pres, 6);
			}
		}
		if ((flag & 0x10) == 0x10) {
			memset(buf, 0, 64);
			if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[4].addr, buf) == 0) {
				radia[i] = (buf[6] << 8) | buf[7];
				printf("Sample: i = %d, state = 0x%x\n", i, buf[18]);
				printf("Sample: radia = %d\n", radia[i]);
				if (buf[18] != 0)
					alarm |= (1 << 10);
			}
			if (i == 5) {
				s_data.Radiation_Intensity = sample_avg(radia, 6);
			}
		}
		if (i < 5)
			sleep(10);
	}
	if (flag & 0x08) {
		memset(buf, 0, 64);
		if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[3].addr, buf) == 0) {
			s_data.Precipitation = (buf[6] << 8) | buf[7];
			s_data.Precipitation_Intensity = (buf[8] << 8) | buf[9];
			printf("Sample: Precipitation = %d, Intensity = %d\n", s_data.Precipitation, s_data.Precipitation_Intensity);
			if (buf[18] != 0)
				alarm |= ((buf[18] & 0x03) << 8);
		}
	}

	s_data.Alerm_Flag = alarm;

	printf("Leave func: %s\n", __func__);

	return 0;
}

static double slid_avg(int *data, int n, double k, int flag)
{
	double avg = 0.0;
	double pre_avg = 0.0;
	double E = 0.0;

	if (n == 1)
		return (double)data[0];
	pre_avg = slid_avg(data, n-1, k, flag);

	E = data[n - 1] - pre_avg;
	if (flag == 1) {
		if (E > 180)
			E -= 360;
		if (E < (-180))
			E += 360;
	}
	avg = (double)k * E + pre_avg;
	if (flag == 1) {
		if (avg > 360)
			avg -= 360;
		if (avg < 0)
			avg += 360;
	}

	return avg;
}

void *sensor_sample_windSec(void * arg)
{
	int i = 0;
	byte buf[64];
	int speed[3];
	struct record_winsec record;
	int record_len = 0;

	memset(&record, 0, sizeof(struct record_winsec));
	record.tm = rtc_get_time();
	memset(speed, 0, 3);
	for (i = 0; i < 3; i++) {
		memset(buf, 0, 64);
		if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[1].addr, buf) == 0) {
			speed[i] = (buf[6] << 8) | buf[7];
			printf("Sample: i = %d, state = 0x%x\n", i, buf[18]);
			printf("Sample: speed = %d\n", speed[i]);
		}
		if (i == 2) {
			record.speed_sec = (speed[0] + speed[1] + speed[2]) / 3;
		}
		if (i < 2)
			sleep(1);
	}

	record_len = sizeof(struct record_winsec);
	if (File_AppendRecord(RECORD_FILE_WINDSEC, (char *)&record, record_len) < 0) {
		printf("CMD: Recording wind speed data error.\n");
	}

	return 0;
}

void *sensor_sample_windAvg(void * arg)
{
	int i = 0;
	byte buf[64];
	int flag = *((int *)arg);
	int speed[10], speed_d[10];
	struct record_winavg record;
	int record_len = 0;

	memset(&record, 0, sizeof(struct record_winavg));
	record.tm = rtc_get_time();
	memset(speed, 0, 10);
	memset(speed_d, 0, 10);

	for (i = 0; i < 10; i++) {
		if ((flag & 0x2) == 0x02) {
			memset(buf, 0, 64);
			if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[1].addr, buf) == 0) {
				speed[i] = (buf[6] << 8) | buf[7];
				speed_d[i] = (buf[8] << 8) | buf[9];
				printf("Sample: i = %d, state = 0x%x\n", i, buf[18]);
				printf("Sample: speed = %d, speed_d = %d\n", speed[i], speed_d[i]);
			}
			if (i == 9) {
				record.speed_avg = slid_avg(speed, 10, 0.3, 0);
				record.speed_d_avg = slid_avg(speed_d, 10, 0.3, 1);
			}
		}

		if (i < 9)
			sleep(60);
	}

	record_len = sizeof(struct record_winavg);
	if (File_AppendRecord(RECORD_FILE_WINDAVG, (char *)&record, record_len) < 0) {
		printf("CMD: Recording wind average speed data error.\n");
	}

	return 0;
}

void *sensor_qixiang_zigbee(void * arg)
{
	byte buf[16];
	printf("Sensor: Get data from zigbee device.\n");

	memset(buf, 0, 16);
	if (Sensor_Zigbee_ReadData(buf, 13) == 0) {
		s_data.Precipitation = (buf[4] << 8) | buf[5];
		s_data.Precipitation_Intensity = (buf[6] << 8) | buf[7];
		data_qixiang_flag++;
	}
	else {
		fprintf(stderr, "Sensor: read zigbee data error.\n");
	}

	return 0;
}

int Sensor_Sample_Qixiang(void)
{
	int ret = 0;
	int flag;
	pthread_t p1 = -1, p2 = -1;
	int p1_wait = 0;
	time_t now, expect;
	struct tm *tm;
	struct record_qixiang record;
	int record_len = 0;

	data_qixiang_flag = 0;
	memset(&record, 0, sizeof(struct record_qixiang));
	record.tm = rtc_get_time();
	record.data.Time_Stamp = record.tm;

	printf("CMD: sample Weather data start.\n");

	/* Zigbee Sensor Operation */
	ret = pthread_create(&p2, NULL, sensor_qixiang_zigbee, NULL);
	if (ret != 0)
		printf("Sensor: can't create thread.");

	flag = Sensor_Detect_Qixiang();
	printf("flag = %d\n", flag);
	if (flag != 0)
		data_qixiang_flag++;

	memset(&s_data, 0, sizeof(Data_qixiang_t));
	s_data.Time_Stamp = record.tm;
	memcpy(s_data.Component_ID, CMA_Env_Parameter.id, 17);

	if ((flag & 0x0019) != 0) {
		ret = pthread_create(&p1, NULL, sensor_sample_qixiang_1, &flag);
		if (ret != 0)
			printf("Sensor: can't create thread.");
		p1_wait = 1;
	}

	if ((flag & 0x0004) == 0)
		rtc_alarm_del(&wind_avg);
	if ((flag & 0x0002) == 0)
		rtc_alarm_del(&wind_sec);

	if ((flag & 0x0006) != 0) {
		if (rtc_alarm_isActive(&wind_avg) == 0) {
			memset(&wind_avg, 0, sizeof(struct rtc_alarm_dev));
			wind_avg.func = sensor_sample_windAvg;
			wind_avg.repeat = 1;
			wind_avg.interval = 60 * 60;
			now = rtc_get_time();
			tm = gmtime(&now);
			printf("WindAverage: Now, %s", asctime(tm));
			tm->tm_min = 50;
			tm->tm_sec = 0;
			if (now < mktime(tm))
				expect = mktime(tm);
			else
				expect = mktime(tm) + 60 * 60;
			wind_avg.expect = expect;
			tm = gmtime(&expect);
			printf("WindAverage: Expect, %s", asctime(tm));

			rtc_alarm_add(&wind_avg);
			rtc_alarm_update();
		}
		if ((rtc_alarm_isActive(&wind_sec) == 0) && ((flag & 0x0002) != 0)) {
			memset(&wind_sec, 0, sizeof(struct rtc_alarm_dev));
			wind_sec.func = sensor_sample_windSec;
			wind_sec.repeat = 1;
			wind_sec.interval = 2 * 60;
			now = rtc_get_time();
			tm = gmtime(&now);
			printf("WindSpeed: Now, %s", asctime(tm));
			expect = now - tm->tm_sec + (2 - tm->tm_min % 2) * 60;
			wind_sec.expect = expect;
			tm = gmtime(&expect);
			printf("WindSpeed: Expect, %s", asctime(tm));

			rtc_alarm_add(&wind_sec);
			rtc_alarm_update();
		}
	}

	if (p1_wait) {
		ret = pthread_join(p1, NULL);
		if (ret != 0)
			printf("CMD: can't join with p1 thread.");
	}

	ret = pthread_join(p2, NULL);
	if (ret != 0)
		printf("CMD: can't join with p2 thread.");

//	data_qixiang_flag = 1;
	if (data_qixiang_flag) {
		get_wind_data(&s_data);
		record_len = sizeof(struct record_qixiang);
		memcpy(&record.data, &s_data, sizeof(Data_qixiang_t));
		if (File_AppendRecord(RECORD_FILE_QIXIANG, (char *)&record, record_len) < 0) {
			printf("CMD: Recording Qixiang data error.\n");
		}

		printf("Sample Qixiang Data: \n");
		printf("平均风速： %d \n", s_data.Average_WindSpeed_10min);
		printf("平均风向： %d \n", s_data.Average_WindDirection_10min);
		printf("极大风速： %d \n", s_data.Extreme_WindSpeed);
		printf("最大风速： %d \n", s_data.Max_WindSpeed);
		printf("标准风速： %d \n", s_data.Standard_WindSpeed);
		printf("温 度： %f \n", s_data.Air_Temperature);
		printf("湿 度： %d \n", s_data.Humidity);
		printf("大气压： %d \n", s_data.Air_Pressure);
		printf("降雨量： %d \n", s_data.Precipitation);
		printf("降水强度： %d \n", s_data.Precipitation_Intensity);
		printf("光辐射： %d \n", s_data.Radiation_Intensity);
	}

	printf("CMD: sample Weather data finished.\n");

	return ret;
}

int Sensor_Sample_TGQingXie(Data_incline_t *data)
{
	int ret = -1;
	byte buf[64];
	int angle_x, angle_y;
	struct record_incline record;
	int record_len = 0;

	memset(&record, 0, sizeof(struct record_incline));
	record.tm = rtc_get_time();
	record.data.Time_Stamp = record.tm;

	fprintf(stdout, "CMD: Sample Incline data start.\n");

	data->Time_Stamp = record.tm;
	memcpy(data->Component_ID, CMA_Env_Parameter.id, 17);
	memset(buf, 0, 64);
	angle_x = angle_y = 0;
	if (Sensor_Can_ReadData(Sensor_CAN_List_Angle[0].addr, buf) == 0) {
		angle_x = (buf[6] << 8) | buf[7];
		angle_y = (buf[8] << 8) | buf[9];
		printf("Sample: angle_x = %d, angle_y = %d\n", angle_x, angle_y);

		data->Angle_X = (float)angle_x / 100;
		data->Angle_Y = (float)angle_y / 100;
		record_len = sizeof(struct record_incline);
		memcpy(&record.data, data, sizeof(Data_incline_t));
		if (File_AppendRecord(RECORD_FILE_TGQXIE, (char *)&record, record_len) < 0) {
			fprintf(stderr, "CMD: Recording Incline data error.\n");
		}
		printf("Sample Incline Data: \n");
		printf("顺线倾斜角： %f \n", data->Angle_X);
		printf("横向倾斜角： %f \n", data->Angle_Y);

		ret = 0;
	}
	else
		fprintf(stdout, "CMD: Incline Sensor is not Online.\n");

	fprintf(stdout, "CMD: Sample Incline data finished.\n");

	return ret;
}

int Sensor_Sample_FuBing(Data_ice_thickness_t *data)
{
	int ret = -1;
	byte buf[64];
	int force, angle_x, angle_y, wav_cycle, wav_x, wav_y;
	struct record_fubing record;
	int record_len = 0;
	int s_num = sizeof(Sensor_CAN_List_PullForce) / sizeof(struct can_device);
	int i;

	memset(&record, 0, sizeof(struct record_fubing));
	record.tm = rtc_get_time();
	record.data.Time_Stamp = record.tm;

	fprintf(stdout, "CMD: Sample Fu Bing data start.\n");

	data->Time_Stamp = record.tm;
	memcpy(data->Component_ID, CMA_Env_Parameter.id, 17);
	force = angle_x = angle_y = wav_cycle = wav_x = wav_y = 0;

	for (i = 0; i < s_num; i++) {
		memset(buf, 0, 64);
		if (Sensor_Can_ReadData(Sensor_CAN_List_PullForce[i].addr, buf) == 0) {
			force = (buf[6] << 8) | buf[7];
			angle_x = (buf[8] << 8) | buf[9];
			angle_y = (buf[10] << 8) | buf[11];
			wav_cycle = (buf[12] << 8) | buf[13];
			wav_x = (buf[14] << 8) | buf[15];
			wav_y = (buf[16] << 8) | buf[17];

			printf("Sample: force = %d, angle_x = %d, angle_y = %d\n", force, angle_x, angle_y);
			printf("Sample: wav_cycle = %d, wav_x = %d, wav_y = %d\n", wav_cycle, wav_x, wav_y);

			ret = 0;
		}
		else
			fprintf(stdout, "CMD: FuBing %d Sensor is not Online.\n", i);
	}

	if (ret == 0) {
		data->Tension = force;
		data->Windage_Yaw_Angle = (float)angle_x / 10;
		data->Deflection_Angle = (float)angle_y / 10;
		data->Tension_Difference = wav_cycle;
		data->Reserve1 = wav_x;
		data->Reserve2 = wav_y;

		record_len = sizeof(struct record_incline);
		memcpy(&record.data, data, sizeof(Data_ice_thickness_t));
		if (File_AppendRecord(RECORD_FILE_FUBING, (char *)&record, record_len) < 0) {
			fprintf(stderr, "CMD: Recording Incline data error.\n");
		}
		printf("Sample Fubing Data: \n");
		printf("等值覆冰厚度： %f \n", data->Equal_IceThickness);
		printf("综合悬挂载荷： %f \n", data->Tension);
		printf("不均衡张力差： %f \n", data->Tension_Difference);
		printf("绝缘子串风偏角： %f \n", data->Windage_Yaw_Angle);
		printf("绝缘子串倾斜角： %f \n", data->Deflection_Angle);
	}

	fprintf(stdout, "CMD: Sample Fubing data finished.\n");

	return ret;
}

int Sensor_GetData(byte *buf, int type)
{
	byte sensor_buf[MAX_DATA_BUFSIZE];
	int ret = 0;
	
	fprintf(stdout, "Enter func: %s, type = %d \n", __func__, type);

	memset(sensor_buf, 0, MAX_DATA_BUFSIZE);
	
	switch (type) {
	case CMA_MSG_TYPE_DATA_QXENV:
		{
//			Data_qixiang_t *data = (Data_qixiang_t *)sensor_buf;
			ret = Sensor_Sample_Qixiang();
			memcpy(buf, &s_data, sizeof(Data_qixiang_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_TGQXIE:
		{
			Data_incline_t *data = (Data_incline_t *)sensor_buf;
			ret = Sensor_Sample_TGQingXie(data);
			memcpy(buf, data, sizeof(Data_incline_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DDXWFTZ:
		{
			Data_vibration_f_t *data = (Data_vibration_f_t *)sensor_buf;
			
			memcpy(buf, data, sizeof(Data_vibration_f_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DDXWFBX:
		{
			Data_vibration_w_t *data = (Data_vibration_w_t *)sensor_buf;
			
			memcpy(buf, data, sizeof(Data_vibration_w_t));
		}
//		f_head.pack_len = sizeof(Data_vibration_w_t); // ? sample number: n
		break;
	case CMA_MSG_TYPE_DATA_DXHCH:
		{
			Data_conductor_sag_t *data = (Data_conductor_sag_t *)sensor_buf;
			
			memcpy(buf, data, sizeof(Data_conductor_sag_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DXWD:
		{
			Data_line_temperature_t *data = (Data_line_temperature_t *)sensor_buf;
			
			memcpy(buf, data, sizeof(Data_line_temperature_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_FUBING:
		{
			Data_ice_thickness_t *data = (Data_ice_thickness_t *)sensor_buf;
			ret = Sensor_Sample_FuBing(data);
			memcpy(buf, data, sizeof(Data_ice_thickness_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DXFP:
		{
			Data_windage_yaw_t *data = (Data_windage_yaw_t *)sensor_buf;
			
			memcpy(buf, data, sizeof(Data_windage_yaw_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DXWDTZH:
		{
			Data_line_gallop_f_t *data = (Data_line_gallop_f_t *)sensor_buf;
			
			memcpy(buf, data, sizeof(Data_line_gallop_f_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DXWDGJ:
		{
			Data_gallop_w_t *data = (Data_gallop_w_t *)sensor_buf;
			
			memcpy(buf, data, sizeof(Data_gallop_w_t));
		}
//		f_head.pack_len = sizeof(Data_gallop_w_t); // ? sample num: n
		break;
	case CMA_MSG_TYPE_DATA_XCHWS:
		{
			Data_dirty_t *data = (Data_dirty_t *)sensor_buf;
			
			memcpy(buf, data, sizeof(Data_dirty_t));
		}
		break;
	default:
		printf("Invalid Sensor tyep.\n");
		ret = -1;
		break;
	}
	
	return ret;
}

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
		perror("socket");
		return -1;
	}

	addr.can_family = family;
	strcpy(ifr.ifr_name, interface);
	if (ioctl(s, SIOCGIFINDEX, &ifr)) {
		perror("ioctl");
		close(s);
		return -1;
	}
	addr.can_ifindex = ifr.ifr_ifindex;

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(s);
		return -1;
	}

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
		printf("Can Send buf length larger than 8, set to 8.\n");
		len = 8;
	}

	memcpy(frame.data, buf, len);
	frame.can_dlc = len;
	frame.can_id &= CAN_EFF_MASK;
	frame.can_id |= CAN_EFF_FLAG;

	while (loopcount--) {
		ret = write(s, &frame, sizeof(frame));
		if (ret == -1) {
			perror("write");
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
			perror("read");
			return -1;
		}
		memcpy(pbuf, frame.data, frame.can_dlc);
		pbuf += frame.can_dlc;
		printf("nbytes = %d, dlc = %d\n", nbytes, frame.can_dlc);
		for (i = 0; i < 8; i++)
			printf("0x%02x ", frame.data[i]);
		printf("\n");
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

//	printf("--------- Enter func: %s -----------\n", __func__);

	if (buf == NULL)
		return -1;

	cmd[2] = (addr & 0xff00) >> 8;
	cmd[3] = addr & 0x00ff;
	crc16 = RTU_CRC(cmd, 6);
	cmd[6] = (crc16 & 0xff00) >> 8;
	cmd[7] = crc16 & 0x00ff;

#ifdef _DEBUG
	printf("CAN Sensor Send Cmd: ");
	debug_out(cmd, 8);
#endif

	pthread_mutex_lock(&can_mutex);

	if ((s = can_socket_init(CAN_DEVICE_NAME)) < 0) {
		pthread_mutex_unlock(&can_mutex);
		return -1;
	}

	memcpy(frame.data, cmd, 8);
	frame.can_dlc = 8;
	frame.can_id &= CAN_EFF_MASK;
	frame.can_id |= CAN_EFF_FLAG;

	ret = write(s, &frame, sizeof(frame));
	if (ret == -1) {
		perror("write");
		goto err;
	}

	pbuf = buf;
	for (i = 0; i < 3; i++) {
		if ((ret = io_readn(s, &frame, sizeof(struct can_frame), timeout)) < 0) {
			perror("read");
			goto err;
		}
		memcpy(pbuf, frame.data, frame.can_dlc);
		pbuf += frame.can_dlc;
#ifdef _DEBUG
//		printf("nbytes = %d, dlc = %d\n", ret, frame.can_dlc);
		int j;
		for (j = 0; j < 8; j++)
			printf("%02x ", frame.data[j]);
		printf("\n");
#endif
	}

	pbuf = buf + 2;
	crc16 = RTU_CRC(pbuf, 17);
//	printf("crc16 = %x \n", crc16);
	if (crc16 != ((buf[19] << 8) | buf[20])) {
		printf("CRC check Error.\n");
		goto err;
	}

	pthread_mutex_unlock(&can_mutex);

	close(s);
	return 0;

err:
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
	printf("CAN Sensor Send Cmd: ");
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
		perror("write");
		close(s);
		return -1;
	}

	if ((ret = read(s, &frame, sizeof(struct can_frame))) < 0) {
		perror("read");
		close(s);
		return -1;
	}
	if (frame.can_dlc != 8) {
		close(s);
		return -1;
	}

	printf("nbytes = %d, dlc = %d\n", ret, frame.can_dlc);
	for (i = 0; i < 8; i++)
		printf("0x%02x ", frame.data[i]);
	printf("\n");

	memcpy(rbuf, frame.data, 8);
	crc16 = (rbuf[6] << 8) | rbuf[7];
	if (crc16 != RTU_CRC(rbuf, 6)) {
		printf("CRC Check error.\n");
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

int Camera_GetImages(char *ImageName, byte presetting, byte channel)
{
	Ctl_image_device_t par;
	char cmdshell[128];
	int ret;

	Camera_PowerOn(CAMERA_DEVICE_ADDR);
	Camera_CallPreset(CAMERA_DEVICE_ADDR, presetting);

	memset(&par, 0, sizeof(Ctl_image_device_t));
	if (Camera_GetParameter(&par) < 0) {
		fprintf(stderr, "CMD: Can't read image parameters, use default.\n");
		par.Color_Select = 1;
		par.Resolution = 2;
		par.Luminance = 50;
		par.Contrast = 50;
		par.Saturation = 50;
	}

	ret = v4l2_capture_image (ImageName, 720, 576, par.Luminance, par.Contrast, par.Saturation);
	if (ret < 0) {
		fprintf(stderr, "CMD: Capture an Image error.\n");
		return -1;
	}

	if (par.Color_Select ==0 ) {
		memset(cmdshell, 0, 128);
		sprintf(cmdshell, "convert -colorspace gray %s %s", ImageName, ImageName);
		system(cmdshell);
	}

	if (par.Resolution == 1) {
		memset(cmdshell, 0, 128);
		sprintf(cmdshell, "convert -resize 320x240! %s %s", ImageName, ImageName);
		system(cmdshell);
	}

	if (par.Resolution == 2) {
		memset(cmdshell, 0, 128);
		sprintf(cmdshell, "convert -resize 640x480! %s %s", ImageName, ImageName);
		system(cmdshell);
	}

	if (par.Resolution == 3) {
		memset(cmdshell, 0, 128);
		sprintf(cmdshell, "convert -resize 704x576! %s %s", ImageName, ImageName);
		system(cmdshell);
	}

	return 0;
}

int Camera_GetParameter(Ctl_image_device_t *par)
{
	int fd;
	int len = 0;
	int ret = 0;

	printf("Enter func: %s ------\n", __func__);

	if (File_Exist(FILE_IMAGECAPTURE_PAR) == 0)
		return -1;

	fd = File_Open(FILE_IMAGECAPTURE_PAR);
	if (fd < 0)
		return -1;

	len = sizeof(Ctl_image_device_t);
	if (read(fd, par, len) != len)
		ret = -1;

	File_Close(fd);

	return ret;
}

int Camera_SetParameter(Ctl_image_device_t *par)
{
	int fd;
	int len = 0;
	int ret = 0;

	printf("Enter func: %s ------\n", __func__);

	if (File_Exist(FILE_IMAGECAPTURE_PAR))
		File_Delete(FILE_IMAGECAPTURE_PAR);

	fd = File_Open(FILE_IMAGECAPTURE_PAR);
	if (fd < 0)
		return -1;

	len = sizeof(Ctl_image_device_t);
	if (write(fd, par, len) != len)
		ret = -1;

	File_Close(fd);

	return ret;
}

void *camera_caputre_func(void *arg)
{
	int fd;
	usint t1 = 0, t2 = 0;
	Ctl_image_timetable_t data;
	char *recordFile = FILE_IMAGE_TIMETABLE0;
	int len = sizeof(Ctl_image_timetable_t);
	time_t now = rtc_get_time();
	struct tm *tm;
	int ret;
	char imageName[256];
	byte channel = 1;

	memset(imageName, 0, 256);
	printf("Enter func: %s ------\n", __func__);

	if (File_Exist(recordFile) == 0)
		return 0;

	fd = File_Open(recordFile);
	if (fd < 0)
		return 0;

	tm = localtime(&now);
	memset(&data, 0, len);
	while (1) {
		if ((ret = read(fd, &data, len)) <= 0) {
			break;
		}
		if ((tm->tm_hour == data.Hour) && (tm->tm_min == data.Minute)) {
			/* Get an image at this pre-setting */
			if (Camera_StartCapture(imageName, channel, data.Presetting_No) < 0)
				break;

			if (CMA_Image_SendRequest(CMA_Env_Parameter.socket_fd, imageName, channel, data.Presetting_No) < 0)
				break;
		}
		t1 = tm->tm_hour * 60 + tm->tm_min;
		t2 = data.Hour * 60 + data.Minute;
		if (t2 > t1)
			break;
	}

	File_Close(fd);

	Camera_NextTimer();

	return 0;
}

int Camera_NextTimer(void)
{
	int fd;
	usint t1 = 0, t2 = 0;
	char *recordFile = FILE_IMAGE_TIMETABLE0;
	Ctl_image_timetable_t data;
	int len = sizeof(Ctl_image_timetable_t);
	time_t expect;
	time_t now = rtc_get_time();
	struct tm *tm;
	int ret;

	printf("Enter func: %s ------\n", __func__);

	if (File_Exist(recordFile) == 0)
		return 0;

	fd = File_Open(recordFile);
	if (fd < 0)
		return -1;

	tm = localtime(&now);
	memset(&data, 0, len);
	while (1) {
		if ((ret = read(fd, &data, len)) <= 0) {
			break;
		}
		t1 = tm->tm_hour * 60 + tm->tm_min;
		t2 = data.Hour * 60 + data.Minute;
		if (t2 > t1)
			break;
	}

	if (t2 > t1) {
		tm->tm_hour = data.Hour;
		tm->tm_min = data.Minute;
		tm->tm_sec = 0;
		expect = mktime(tm);
	}
	else {
		lseek(fd, 0, SEEK_SET);
		if ((ret = read(fd, &data, len)) <= 0) {
			fprintf(stderr, "CMD: Read Camera Caputer timetable error.\n");
		}
		expect = now + 24 * 60 * 60;
		tm = localtime(&expect);
		tm->tm_hour = data.Hour;
		tm->tm_min = data.Minute;
		tm->tm_sec = 0;
		expect = mktime(tm);
	}

	File_Close(fd);

	if (rtc_alarm_isActive(&camera_dev))
		rtc_alarm_del(&camera_dev);

	memset(&camera_dev, 0, sizeof(struct rtc_alarm_dev));
	camera_dev.func = camera_caputre_func;
	camera_dev.repeat = 0;
	camera_dev.interval = 0;
	camera_dev.expect = expect;

	tm = localtime(&now);
	printf("Camera Capture: Now, %s", asctime(tm));
	tm = localtime(&expect);
	printf("Camera Capture: Expect, %s", asctime(tm));

	rtc_alarm_add(&camera_dev);
	rtc_alarm_update();

	return 0;
}

int Camera_SetTimetable(Ctl_image_timetable_t *tb, byte groups, byte channel)
{
	int fd;
	int t1 = 0, t2 = 0;
	int i, len, ret = 0;
	int size = 0;
	char *fbp = NULL;
	char *to, *from;
	int copy_len = 0;
	Ctl_image_timetable_t *data = NULL;
	char *recordFile = FILE_IMAGE_TIMETABLE0;

	printf("Enter func: %s ------\n", __func__);

	if (File_Exist(recordFile))
		File_Delete(recordFile);

	len = sizeof(Ctl_image_timetable_t);
	size = len * groups;
	if (File_Create(recordFile, size) < 0)
		return -1;

	fd = File_Open(recordFile);
	if (fd < 0)
		return -1;
	fbp = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fbp == MAP_FAILED) {
		printf("file mmap error.\n");
		File_Close(fd);
		return -1;
	}

	memset(fbp, 255, size);
	for (i = 0; i < groups; i++) {
		t1 = (tb[i].Hour * 60 << 8) + (tb[i].Minute << 8) + tb[i].Presetting_No;
		data = (Ctl_image_timetable_t *)fbp;
		while (1) {
			if (data->Hour == 255)
				break;
			if (((char *)data + len) > (fbp + size)) {
				fprintf(stderr, "Never reach here, if here, wrong.\n");
				return -1;
			}
			t2 = (data->Hour * 60 << 8) + (data->Minute << 8) + data->Presetting_No;
			if (t1 < t2) {
				copy_len = fbp + size - (char *)data - len;
				to = (char *)(data + 1);
				from = (char *)data;
				memmove(to, from, copy_len);
				memcpy(data, (tb + i), len);
				break;
			}
			data++;
		}
		if (data->Hour == 255) {
			memcpy(data, (tb + i), len);
		}
	}

	if (munmap(fbp, size) < 0) {
		printf("file munmap error.\n");
	}

	File_Close(fd);

	Camera_NextTimer();

	return ret;
}

int Camera_GetImageName(char *filename, byte channel, byte presetting)
{
	char *folder = "/CMD_Data/images/";
//	char *folder = "data-";
	time_t now;
	struct tm *tm;

	if (filename == NULL)
		return -1;

//	now = rtc_get_time();
	now = time((time_t*)NULL);
	tm = localtime(&now);

	memset(filename, 0, 256);
//	sprintf(filename, "%simages-%d-%d-%d.jpg", folder, (int)now, channel, presetting);
	sprintf(filename, "%simages-%d%02d%02d%02d%02d%02d-%d-%d.jpg", folder, (tm->tm_year + 1900), (tm->tm_mon + 1), tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec, channel, presetting);

	return 0;
}

int Camera_StartCapture(char *filename, byte channel, byte presetting)
{
	if (filename == NULL)
		return -1;

	Camera_GetImageName(filename, channel, presetting);

	if (Camera_GetImages(filename, presetting, channel) < 0)
		return -1;

	printf("Get Images: fileName = %s\n", filename);

	return 0;
}

int Camera_Control(byte action, byte presetting, byte channel)
{
	int ret = 0;

	fprintf(stdout, "Camera Control: action = %d, preSetting = %d\n", action, presetting);

	switch (action) {
	case CAMERA_ACTION_POWERON:
		Camera_PowerOn(CAMERA_DEVICE_ADDR);
		break;
	case CAMERA_ACTION_POWEROFF:
		Camera_PowerOff(CAMERA_DEVICE_ADDR);
		break;
	case CAMERA_ACTION_CALLPRESET:
		Camera_CallPreset(CAMERA_DEVICE_ADDR, presetting);
		break;
	case CAMERA_ACTION_SETPRESET:
		Camera_SetPreset(CAMERA_DEVICE_ADDR, presetting);
		break;
	case CAMERA_ACTION_MOVEUP:
		Camera_MoveUp(CAMERA_DEVICE_ADDR);
		break;
	case CAMERA_ACTION_MOVEDOWN:
		Camera_MoveDown(CAMERA_DEVICE_ADDR);
		break;
	case CAMERA_ACTION_MOVELEFT:
		Camera_MoveLeft(CAMERA_DEVICE_ADDR);
		break;
	case CAMERA_ACTION_MOVERIGHT:
		Camera_MoveRight(CAMERA_DEVICE_ADDR);
		break;
	case CAMERA_ACTION_FOCUSFAR:
		Camera_FocusFar(CAMERA_DEVICE_ADDR);
		break;
	case CAMERA_ACTION_FOCUSNERA:
		Camera_FocusNear(CAMERA_DEVICE_ADDR);
		break;
	default:
		fprintf(stderr, "CMD: Invalid camera control action.\n");
		break;
	}

	return ret;
}

int Camera_Video_Start(byte channel, byte control, usint port)
{
	return 0;
}


