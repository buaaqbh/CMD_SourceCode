#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <net/if.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <pthread.h>
#include "sensor_ops.h"
#include "io_util.h"
#include "list.h"

#define  CAN_DEVICE_NAME	"can0"
unsigned int Sensor_Online_flag = 0;
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
		.addr = 0x0014,
	},
	{
		.type = SENSOR_WDIRECTION,
		.addr = 0x0018,
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
				s_data.Air_Temperature = sample_avg(temp, 6);
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

static double slid_avg(int *data, int n, int k)
{
	double avg = 0.0;
	double pre_avg = 0.0;
	if (n == 1)
		return (double)data[0];
	pre_avg = slid_avg(data, n-1, k);
	avg = (double)k * (data[n - 1] - pre_avg) + pre_avg;

	return avg;
}

void *sensor_sample_qixiang_2(void * arg)
{
	int i;
	byte buf[64];
	int flag = *((int *)arg);
	int speed = 0, speed_d = 0;

	if ((flag & 0x2) == 0x02) {
		memset(buf, 0, 64);
		if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[1].addr, buf) == 0) {
			speed = (buf[6] << 8) | buf[7];
			printf("Sample: i = %d, state = 0x%x\n", i, buf[18]);
			printf("Sample: speed = %d\n", speed);
		}
	}

	if ((flag & 0x2) == 0x04) {
		memset(buf, 0, 64);
		if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[2].addr, buf) == 0) {
			speed_d = (buf[6] << 8) | buf[7];
			printf("Sample: i = %d, state = 0x%x\n", i, buf[18]);
			printf("Sample: speed_d = %d\n", speed_d);
		}
	}

	return 0;
}

int Sensor_Sample_Qixiang(void)
{
	int ret = 0;
	int flag;
	pthread_t p1 = -1, p2 = -1;
	int p1_wait = 0, p2_wait = 0;

	flag = Sensor_Detect_Qixiang();
	if (flag == 0)
		return -1;

	if ((flag & 0x0019) != 0) {
		ret = pthread_create(&p1, NULL, sensor_sample_qixiang_1, &flag);
		if (ret != 0)
			printf("Sensor: can't create thread.");
		p1_wait = 1;
	}

	if ((flag & 0x0006) != 0) {
		ret = pthread_create(&p2, NULL, sensor_sample_qixiang_2, &flag);
		if (ret != 0)
			printf("Sensor: can't create thread.");
		p2_wait = 1;
	}

	if (p1_wait) {
		ret = pthread_join(p1, NULL);
		if (ret != 0)
			printf("CMD: can't join with thread.");
	}
	if (p2_wait) {
		ret = pthread_join(p2, NULL);
		if (ret != 0)
			printf("CMD: can't join with thread.");
	}

	printf("Sample Qixiang Data: \n");
	if ((flag & 0x1) == 0x01) {
		printf("温 度： %d \n", s_data.Air_Temperature);
		printf("湿 度： %d \n", s_data.Humidity);
		printf("大气压： %d \n", s_data.Air_Pressure);
	}
	if ((flag & 0x10) == 0x10) {
		printf("光辐射： %d \n", s_data.Radiation_Intensity);
	}
	if (flag & 0x08) {
		printf("降雨量： %d \n", s_data.Precipitation);
		printf("降水强度： %d \n", s_data.Precipitation_Intensity);
	}
	if ((flag & 0x0002) != 0) {
		printf("平均风速： %d \n", s_data.Average_WindSpeed_10min);
		printf("极大风速： %d \n", s_data.Extreme_WindSpeed);
		printf("最大风速： %d \n", s_data.Max_WindSpeed);
		printf("标准风速： %d \n", s_data.Standard_WindSpeed);
	}
	if ((flag & 0x0004) != 0) {
		printf("平均风向： %d \n", s_data.Average_WindDirection_10min);
	}

	return ret;
}

int Sensor_GetData_QiXiang(Data_qixiang_t *data)
{
	int Clocktime_Stamp = time((time_t*)NULL);
	int has_data = 0;
	byte buf[64];
	int ret = 0;
	int flag;

	flag = Sensor_Detect_Qixiang();
	printf("Sensor exist flag = 0x%x \n", flag);

	/* Get temperature */
	memset(buf, 0, 64);
	if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[0].addr, buf) == 0) {
		has_data = 1;
		data->Air_Temperature = (buf[6] << 8) | buf[7];
		data->Humidity = (buf[8] << 8) | buf[9];
		data->Air_Pressure = (buf[10] << 8) | buf[11];
		if (buf[18] == 1)
			data->Alerm_Flag |= 0x00e0;
	}

	/* Get Rainfall data */
	memset(buf, 0, 64);
	if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[3].addr, buf) == 0) {
		has_data = 1;
		data->Precipitation = (buf[6] << 8) | buf[7];
		if (buf[18] == 1)
			data->Alerm_Flag |= 0x0100;
	}

	/* Get Radiation Intensity data */
	memset(buf, 0, 64);
	if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[4].addr, buf) == 0) {
		has_data = 1;
		data->Radiation_Intensity = (buf[6] << 8) | buf[7];
		if (buf[18] == 1)
			data->Alerm_Flag |= 0x0400;
	}

	data->Time_Stamp = Clocktime_Stamp;
	if (has_data != 1)
		ret = -1;

	return ret;
}

int Sensor_GetData(byte *buf, int type)
{
	byte sensor_buf[MAX_DATA_BUFSIZE];
	int ret = 0;
	
	memset(sensor_buf, 0, MAX_DATA_BUFSIZE);
	
	switch (type) {
	case CMA_MSG_TYPE_DATA_QXENV:
		{
			Data_qixiang_t *data = (Data_qixiang_t *)sensor_buf;
			ret = Sensor_GetData_QiXiang(data);
			memcpy(buf, &data, sizeof(Data_qixiang_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_TGQXIE:
		{
			Data_incline_t *data = (Data_incline_t *)sensor_buf;

			memcpy(buf, &data, sizeof(Data_incline_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DDXWFTZ:
		{
			Data_vibration_f_t *data = (Data_vibration_f_t *)sensor_buf;
			
			memcpy(buf, &data, sizeof(Data_vibration_f_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DDXWFBX:
		{
			Data_vibration_w_t *data = (Data_vibration_w_t *)sensor_buf;
			
			memcpy(buf, &data, sizeof(Data_vibration_w_t));
		}
//		f_head.pack_len = sizeof(Data_vibration_w_t); // ? sample number: n
		break;
	case CMA_MSG_TYPE_DATA_DXHCH:
		{
			Data_conductor_sag_t *data = (Data_conductor_sag_t *)sensor_buf;
			
			memcpy(buf, &data, sizeof(Data_conductor_sag_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DXWD:
		{
			Data_line_temperature_t *data = (Data_line_temperature_t *)sensor_buf;
			
			memcpy(buf, &data, sizeof(Data_line_temperature_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_FUBING:
		{
			Data_ice_thickness_t *data = (Data_ice_thickness_t *)sensor_buf;
			
			memcpy(buf, &data, sizeof(Data_ice_thickness_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DXFP:
		{
			Data_windage_yaw_t *data = (Data_windage_yaw_t *)sensor_buf;
			
			memcpy(buf, &data, sizeof(Data_windage_yaw_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DXWDTZH:
		{
			Data_line_gallop_f_t *data = (Data_line_gallop_f_t *)sensor_buf;
			
			memcpy(buf, &data, sizeof(Data_line_gallop_f_t));
		}
		break;
	case CMA_MSG_TYPE_DATA_DXWDGJ:
		{
			Data_gallop_w_t *data = (Data_gallop_w_t *)sensor_buf;
			
			memcpy(buf, &data, sizeof(Data_gallop_w_t));
		}
//		f_head.pack_len = sizeof(Data_gallop_w_t); // ? sample num: n
		break;
	case CMA_MSG_TYPE_DATA_XCHWS:
		{
			Data_dirty_t *data = (Data_dirty_t *)sensor_buf;
			
			memcpy(buf, &data, sizeof(Data_dirty_t));
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

	pbuf = buf;
	for (i = 0; i < 3; i++) {
		if ((ret = io_readn(s, &frame, sizeof(struct can_frame), timeout)) < 0) {
			perror("read");
			close(s);
			return -1;
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
		close(s);
		return -1;
	}

	close(s);
	return 0;
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

int Camera_SetParameter(Ctl_image_device_t *par)
{
	return 0;
}

int Camera_SetTimetable(Ctl_image_timetable_t *tb, byte groups, byte channel)
{
	return 0;
}

int Camera_StartCapture(byte channel, byte presetting)
{
	return 0;
}

int Camera_Control(byte action, byte channel)
{
	return 0;
}

int Camera_Video_Start(byte channel, byte control, usint port)
{
	return 0;
}

