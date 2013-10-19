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

#ifdef CMD_SENSOR_AUTO_DETECT
struct list_head s_head;

static void clean_list(struct list_head *list)
{
	sensor_device_t *dev = NULL;
	struct list_head *plist;

	list_for_each(plist, list) {
			dev = list_entry(plist, sensor_device_t, list);
			free((void *)dev);
	}

	list->next = list;
}

void Sensor_Scanning(void)
{
	sensor_device_t *dev = NULL;
	int i;
	int s_num = sizeof(Sensor_CAN_List_Qixiang) / sizeof(struct can_device);
	byte buf[24] = { 0 };

	printf("CAN Sensor num: %d\n", s_num);

	clean_list(&s_head);
	for (i = 0; i < s_num; i++) {
		if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[i].addr, buf) < 0)
			continue;
		dev = (sensor_device_t *)malloc(sizeof(sensor_device_t));
		dev->interface = 0;
		dev->addr = Sensor_CAN_List_Qixiang[i].addr;
		dev->type = Sensor_CAN_List_Qixiang[i].type;
		list_add(&dev->list, &s_head);
	}

	if (list_empty(&s_head))
		printf("Sensor List is Empty.\n");

	return;
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

int Sensor_Sample_Qixiang(void)
{
	byte buf[64];
	int ret = 0;
	int flag;

	flag = Sensor_Detect_Qixiang();
	if (flag == 0)
		return -1;

	/* Get temperature */
	memset(buf, 0, 64);
	if (Sensor_Can_ReadData(Sensor_CAN_List_Qixiang[0].addr, buf) == 0) {

	}

	return ret;
}

int Sensor_GetData_QiXiang(Data_qixiang_t *data)
{
#ifdef CMD_SENSOR_AUTO_DETECT
	struct list_head *plist;
	sensor_device_t *dev = NULL;
#endif
	int Clocktime_Stamp = time((time_t*)NULL);
	int has_data = 0;
	byte buf[64];
	int ret = 0;
	int flag;

	flag = Sensor_Detect_Qixiang();
	printf("Sensor exist flag = 0x%x \n", flag);

#ifdef CMD_SENSOR_AUTO_DETECT
	list_for_each(plist, &s_head) {
		dev = list_entry(plist, sensor_device_t, list);
		printf("Sensor Device: interface = %d, type = %d, addr = 0x%x\n", dev->interface, dev->type, dev->addr);
		switch (dev->type) {
		case SENSOR_TEMP:
		{
			if (dev->interface == 0) {
				memset(buf, 0, 64);
				if (Sensor_Can_ReadData(dev->addr, buf) < 0)
					break;
			}
			break;
		}
		case SENSOR_WSPEED:
			break;
		case SENSOR_WDIRECTION:
			break;
		case SENSOR_RAIN:
			break;
		case SENSOR_RADIATION:
			break;
		default:
			break;
		}

	}
#else
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
#endif

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
	int s, ret, i, j;
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
			printf("return -1\n");
			perror("read");
			close(s);
			return -1;
		}
		memcpy(pbuf, frame.data, frame.can_dlc);
		pbuf += frame.can_dlc;
#ifdef _DEBUG
//		printf("nbytes = %d, dlc = %d\n", ret, frame.can_dlc);
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


