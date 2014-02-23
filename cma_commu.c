#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include "device.h"
#include "cma_commu.h"
#include "types.h"
#include "socket_lib.h"
#include "sensor_ops.h"
#include "rtc_alarm.h"
#include "file_ops.h"

volatile int CMD_Response_data = 0;
extern pthread_mutex_t sndMutex;
extern pthread_mutex_t imgMutex;

typedef struct __reponse_data {
	byte msg_type;
	int res;
}Response_data;

static Response_data resData[15];
static volatile int upgradeLostFlag = 0;
byte imageRbuf[MAX_COMBUF_SIZE];
static volatile int imageRcvLen = 0;

//#define _DEBUG

#ifdef _DEBUG
static void print_message(byte *buf, int len)
{
	int i;
	for(i = 0; i<len; i++) {
		printf("0x%02x ", buf[i]);
		if (((i+1)%16) == 0)
			printf("\n");
	}
	printf("\n");
}
#endif

int Commu_GetPacket_Udp(int fd, byte *rbuf, int len, int timeout)
{
	int ret = 0;
	usint crc16 = 0;
	usint size;

	if ((rbuf == NULL)) {
		printf("Commu_GetPacket: Invalid parameter.\n");
		return -1;
	}

	printf("UDP Socket Begin to receive msg, len = %d\n", len);
	memset(rbuf, 0, len);
	if (fd == -1) {
		return -1;
	}
	else {
		ret = socket_recv(fd, rbuf, len, timeout);
		if (ret < 0)
			return ret;
	}

	memcpy(&size, (rbuf + 2), 2);
	printf("size = %d, ret = %d \n", size, ret);
	if ((size < 0) || (ret < (size + sizeof(frame_head_t) + 2)))
		return -1;

	if ((rbuf[0] != 0xA5) && (rbuf[1] != 0x5A)) {
		printf("Invalid package head.\n");
		return -1;
	}

	memcpy(&crc16, (rbuf + size + sizeof(frame_head_t)), 2);

	if (crc16 != RTU_CRC(rbuf, (size + sizeof(frame_head_t)))) {
		printf("Data packagre crc error!\n");
		return -1;
	}

#ifdef _DEBUG
	printf("Receive MSG, len = %d:\n", ret);
	print_message(rbuf, ret);
#endif

	return ret;
}

int Commu_GetPacket(int fd, byte *rbuf, int len, int timeout)
{
	int ret = 0;
	usint crc16 = 0;
	usint size;

	if ((rbuf == NULL)) {
		printf("Commu_GetPacket: Invalid parameter.\n");
		return -1;
	}

	if (len < (sizeof(frame_head_t) + 3))
		return -1;

	memset(rbuf, 0, len);

	if (CMA_Env_Parameter.s_protocal == 1)
		return Commu_GetPacket_Udp(fd, rbuf, len, timeout);

//	printf("Begin to receive msg, len = %d\n", len);
	
	ret = socket_recv(fd, rbuf, sizeof(frame_head_t), timeout);
	if (ret < 0)
		return ret;

	memcpy(&size, (rbuf + 2), 2);
//	printf("size = %d \n", size);
	if ((size < 0) || (ret != sizeof(frame_head_t))) {
		return -1;
	}
	
	if ((rbuf[0] != 0xA5) && (rbuf[1] != 0x5A)) {
		printf("Invalid package head.\n");
		return -1;
	}

	ret = socket_recv(fd, (rbuf + sizeof(frame_head_t)), (size + 2), timeout);
	if (ret < 0)
		return ret;
	if (ret != (size + 2))
		return -1;

	memcpy(&crc16, (rbuf + size + sizeof(frame_head_t)), 2);

	if (crc16 != RTU_CRC(rbuf, (size + sizeof(frame_head_t)))) {
		printf("Data packagre crc error!\n");
		return -1;
	}

#ifdef _DEBUG
	printf("Receive MSG, len = %d:\n", (size + 25));
	print_message(rbuf, (size + 25));
#endif

	return (size + 25);
}

int Commu_SendPacket(int fd, frame_head_t *head, byte *data)
{
	int ret = 0;
	byte sbuf[MAX_COMBUF_SIZE];
	int timeout = 5;
	usint crc16 = 0;
	int size = 0;

	if ((head == NULL) || (data == NULL)) {
		printf("Commu_SendPacket: Invalid parameter.\n");
		return -1;
	}
	memset(sbuf, 0, MAX_COMBUF_SIZE);

	memcpy(sbuf, head, sizeof(frame_head_t));
	size += sizeof(frame_head_t);
	memcpy((sbuf + size), data, head->pack_len);
	size += head->pack_len;
	crc16 = RTU_CRC(sbuf, size);
	memcpy((sbuf + size), &crc16, 2);
	size += 2;
#ifdef _DEBUG
	printf("Send MSG, len = %d:\n", size);
	print_message(sbuf, size);
#endif

	if (fd == -1) {
		ret = socket_send_udp(CMA_Env_Parameter.cma_ip, CMA_Env_Parameter.cma_port, sbuf, size);
		return ret;
	}

	pthread_mutex_lock(&sndMutex);
	ret = socket_send(fd, sbuf, size, timeout);
	pthread_mutex_unlock(&sndMutex);
	printf("Socket Send Data: ret = %d, size = %d\n", ret, size);
	if (ret <= 0) {
		printf("Send Package: send error.\n");
		return -1;
	}

	return 0;
}

static int CMD_GetMsgTypeIndex(byte type)
{
	int index = 0;

	switch(type) {
	case CMA_MSG_TYPE_DATA_QXENV:
		index = 1;
		break;
	case CMA_MSG_TYPE_DATA_TGQXIE:
		index = 2;
		break;
	case CMA_MSG_TYPE_DATA_DDXWFTZ:
		index = 3;
		break;
	case CMA_MSG_TYPE_DATA_DDXWFBX:
		index = 4;
		break;
	case CMA_MSG_TYPE_DATA_DXHCH:
		index = 5;
		break;
	case CMA_MSG_TYPE_DATA_DXWD:
		index = 6;
		break;
	case CMA_MSG_TYPE_DATA_FUBING:
		index = 7;
		break;
	case CMA_MSG_TYPE_DATA_DXFP:
		index = 8;
		break;
	case CMA_MSG_TYPE_DATA_DXWDTZH:
		index = 9;
		break;
	case CMA_MSG_TYPE_DATA_DXWDGJ:
		index = 10;
		break;
	case CMA_MSG_TYPE_DATA_XCHWS:
		index = 11;
		break;
	case CMA_MSG_TYPE_STATUS_INFO:
		index = 12;
		break;
	case CMA_MSG_TYPE_STATUS_WORK:
		index = 13;
		break;
	case CMA_MSG_TYPE_STATUS_ERROR:
		index = 14;
		break;
	default:
		printf("Invalid Sensor tyep.\n");
		break;
	}

	return index;
}

static int CMD_WaitStatus_Res(byte msg_type, int timeout)
{
	while ((resData[CMD_GetMsgTypeIndex(msg_type)].res == -1) && timeout) {
		sleep(1);
		timeout--;
	}

	if (timeout == 0)
		return -1;
	else
		return resData[CMD_GetMsgTypeIndex(msg_type)].res;
}

int CMA_Server_Process(int fd, byte *rbuf)
{
	frame_head_t *f_head = (frame_head_t *)rbuf;
	byte frame_type = f_head->frame_type;
	byte msg_type = f_head->msg_type;
	byte id[18] = {0};
	byte status = 0;
	int ret = 0;

	printf("Enter func: %s \n", __func__);
	memcpy(id, f_head->id, 17);
	fprintf(stdout, "CMD: Receive Message, id = %s, frame type = 0x%x, msg type = 0x%x\n",
						id, f_head->frame_type, f_head->msg_type);
	if (CMA_MSG_TYPE_CTL_DEV_ID != msg_type) {
		if (memcmp(f_head->id, CMA_Env_Parameter.id, 17) != 0) {
			fprintf(stderr, "Device ID: %s, MSG ID: %s, Miss Match.\n", f_head->id, CMA_Env_Parameter.id);
			return -1;
		}
	}

	if (frame_type == CMA_FRAME_TYPE_CONTROL) {
		switch (msg_type) {
		case CMA_MSG_TYPE_CTL_TIME_CS:
			if (CMA_Time_SetReq_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_CY_PAR:
			if (CMA_SamplePar_SetReq_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_TIME_AD:
			if (CMA_NetAdapter_SetReq_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_MODEL_PAR:
			if (CMA_ModelPar_SetReq_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_ALARM:
			if (CMA_Alarm_SetReq_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_TOCMA_INFO:
			if (CMA_UpDevice_SetReq_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_DEV_ID:
			if (CMA_DeviceId_SetReq_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_BASIC_INFO:
			if (CMA_BasicInfo_SetReq_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_DEV_RESET:
			if (CMA_DeviceRset_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_REQ_DATA:
			if (CMA_RequestData_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_UPGRADE_DATA:
		case CMA_MSG_TYPE_CTL_UPGRADE_END:
			if (CMA_SoftWare_Update_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_CTL_DEV_WAKE:
			if (CMA_WakeupTime_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		default:
			ret = -1;
			printf("CMA: Invalid MSG type.\n");
		}
	}
	else if (frame_type == CMA_FRAME_TYPE_IMAGE_CTRL) {
		switch (msg_type) {
		case CMA_MSG_TYPE_IMAGE_VIDEO_SET:
			if (CMA_Video_StopStart_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_IMAGE_CAP_MANUAL:
			if (CMA_ManualCapture_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_IMAGE_CAP_TIME:
			if (CMA_CaptureTimetable_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_IMAGE_CAP_PAR:
			if (CMA_SetImagePar_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_IMAGE_CAM_ADJ:
			if (CMA_CameraControl_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_IMAGE_DATA_REP:
			pthread_mutex_lock(&imgMutex);
			memcpy(imageRbuf, rbuf, MAX_COMBUF_SIZE);
			imageRcvLen = f_head->pack_len;
			pthread_mutex_unlock(&imgMutex);
			break;
		case CMA_MSG_TYPE_IMAGE_GET_PAR:
			if (CMA_GetImagePar_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		case CMA_MSG_TYPE_IMAGE_GET_TIME:
			if (CMA_GetImageTimeTable_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		default:
			ret = -1;
			printf("CMA: Invalid MSG type.\n");
		}
	}
	else if (frame_type == CMA_FRAME_TYPE_IMAGE) {
		pthread_mutex_lock(&imgMutex);
		memcpy(imageRbuf, rbuf, MAX_COMBUF_SIZE);
		imageRcvLen = f_head->pack_len;
		pthread_mutex_unlock(&imgMutex);
	}
	else if ((frame_type == CMA_FRAME_TYPE_DATA_RES) || (frame_type == CMA_FRAME_TYPE_STATUS_RES)) {
		status = *(rbuf + sizeof(frame_head_t));
		fprintf(stdout, "CMD: Send data response 0x%x.\n", status);
		resData[CMD_GetMsgTypeIndex(msg_type)].msg_type = msg_type;
		resData[CMD_GetMsgTypeIndex(msg_type)].res = status;
		CMD_Response_data = status;
	}

	return ret;
}

int CMA_Send_SensorData(int fd, int type, void *data)
{
	char *id = CMA_Env_Parameter.id;
	frame_head_t f_head;
	byte data_buf[MAX_DATA_BUFSIZE];

	printf("CMA Send Sensor Data.\n");
	memset(&f_head, 0, sizeof(frame_head_t));

	f_head.head = 0x5aa5;
	f_head.frame_type = CMA_FRAME_TYPE_DATA;
	memcpy(f_head.id, id, 17);
	memset(data_buf, 0, MAX_DATA_BUFSIZE);

	switch (type) {
	case CMA_MSG_TYPE_DATA_QXENV:
	case CMA_MSG_TYPE_CTL_QX_PAR:
		f_head.pack_len = sizeof(Data_qixiang_t);
		f_head.msg_type = CMA_MSG_TYPE_DATA_QXENV;
		break;
	case CMA_MSG_TYPE_DATA_TGQXIE:
	case CMA_MSG_TYPE_CTL_TGQX_PAR:
		f_head.pack_len = sizeof(Data_incline_t);
		f_head.msg_type = CMA_MSG_TYPE_DATA_TGQXIE;
		break;
	case CMA_MSG_TYPE_DATA_DDXWFTZ:
		f_head.pack_len = sizeof(Data_vibration_f_t);
		break;
	case CMA_MSG_TYPE_DATA_DDXWFBX:
//		f_head.pack_len = sizeof(Data_vibration_w_t); // ? sample number: n
		break;
	case CMA_MSG_TYPE_DATA_DXHCH:
		f_head.pack_len = sizeof(Data_conductor_sag_t);
		break;
	case CMA_MSG_TYPE_DATA_DXWD:
		f_head.pack_len = sizeof(Data_line_temperature_t);
		break;
	case CMA_MSG_TYPE_DATA_FUBING:
	case CMA_MSG_TYPE_CTL_FUBING_PAR:
		f_head.pack_len = sizeof(Data_ice_thickness_t);
		f_head.msg_type = CMA_MSG_TYPE_DATA_FUBING;
		break;
	case CMA_MSG_TYPE_DATA_DXFP:
		f_head.pack_len = sizeof(Data_windage_yaw_t);
		break;
	case CMA_MSG_TYPE_DATA_DXWDTZH:
		f_head.pack_len = sizeof(Data_line_gallop_f_t);
		break;
	case CMA_MSG_TYPE_DATA_DXWDGJ:
		f_head.pack_len = sizeof(Data_gallop_w_t);
		break;
	case CMA_MSG_TYPE_DATA_XCHWS:
//		f_head.pack_len = sizeof(Data_dirty_t); // ? sample num: n
		break;
	default:
		printf("Invalid Sensor tyep.\n");
		break;
	}

	if (Commu_SendPacket(fd, &f_head, (byte *)data) < 0) {
		fprintf(stderr, "CMD: Socket Send Data error.\n");
		return -1;
	}

	resData[CMD_GetMsgTypeIndex(type)].res = -1;

	return CMD_WaitStatus_Res(type, 10);
}

int CMA_Check_Send_SensorData(int fd, int type)
{
	byte record[256];
	int total = 0;
	int record_len;
	char *filename = NULL;
	int i, flag = 0;
	time_t now, t;
	int ret = 0;

	switch (type) {
	case CMA_MSG_TYPE_DATA_QXENV:
	case CMA_MSG_TYPE_CTL_QX_PAR:
		filename = RECORD_FILE_QIXIANG;
		record_len = sizeof(struct record_qixiang);
		break;
	case CMA_MSG_TYPE_DATA_TGQXIE:
	case CMA_MSG_TYPE_CTL_TGQX_PAR:
		filename = RECORD_FILE_TGQXIE;
		record_len = sizeof(struct record_incline);
		break;
	case CMA_MSG_TYPE_DATA_FUBING:
	case CMA_MSG_TYPE_CTL_FUBING_PAR:
		filename = RECORD_FILE_FUBING;
		record_len = sizeof(struct record_fubing);
		break;
	case CMA_MSG_TYPE_DATA_DDXWFTZ:
//		break;
	case CMA_MSG_TYPE_DATA_DDXWFBX:
//		break;
	case CMA_MSG_TYPE_DATA_DXHCH:
//		break;
	case CMA_MSG_TYPE_DATA_DXWD:
//		break;
	case CMA_MSG_TYPE_DATA_DXFP:
	case CMA_MSG_TYPE_CTL_DXFP_PAR:
//		break;
	case CMA_MSG_TYPE_DATA_DXWDTZH:
//		break;
	case CMA_MSG_TYPE_DATA_DXWDGJ:
//		break;
	case CMA_MSG_TYPE_DATA_XCHWS:
//		break;
	default:
		printf("Invalid Sensor tyep.\n");
		return -1;
	}

	total = File_GetNumberOfRecords(filename, record_len);
	now = rtc_get_time();
	while (total > 0) {
		memset(&record, 0, record_len);
		memset(&t, 0, sizeof(time_t));
		if (File_GetRecordByIndex(filename, &record, record_len, 0) == record_len) {
			memcpy(&t, &record, sizeof(time_t));
			if ((now - t) > 50*24*60*60) {
				File_DeleteRecordByIndex(filename, record_len, 0);
				total = total -1;
			}
			else
				break;
		}
		else
			break;
	}

	if (total == 0) {
		return 0;
	}

	for (i = (total - 1); i >= 0; i--) {
//		printf("total = %d, i = %d\n", total, i);
		memset(&record, 0, record_len);
		if (File_GetRecordByIndex(filename, &record, record_len, i) == record_len) {
//			printf("filename = %s, record_len = %d\n", filename, record_len);
			memcpy(&flag, ((byte *)&record + (record_len - 4)), 4);
			if (flag == 0) {
				ret = CMA_Send_SensorData(fd, type, (record + sizeof(time_t)));
				if (ret < 0)
					continue;
				else if (ret == 0xff) {
					fprintf(stdout, "CMD: Send Data reponse OK.\n");
					flag = 0xff;
					memcpy(((byte *)&record + record_len - 4), &flag, 4);
					File_UpdateRecordByIndex(filename, &record, record_len, i);
					/*
					File_GetRecordByIndex(filename, &record, record_len, i);
					if (type == CMA_MSG_TYPE_DATA_TGQXIE) {
						struct record_incline *p = (struct record_incline *)&record;
						printf("After Send flag = %d \n", p->send_flag);
					}
					*/
				}
			}
//			else
//				break;
		}
	}

	return 0;
}

int CMA_Time_SetReq_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte  config_type = *(rbuf + sizeof(frame_head_t));
	time_t  cur_time;
	int Clocktime_Stamp;
	byte sbuf[MAX_DATA_BUFSIZE];
	struct timeval tv;
	struct timezone tz;
	
	memcpy(&cur_time, (rbuf + sizeof(frame_head_t) + 1), sizeof(int));
	
	if (config_type == 0x01) {
		gettimeofday (&tv , &tz);
//		printf("Now time: %d, %s \n", (int)tv.tv_sec, asctime(gmtime(&tv.tv_sec)));
		tv.tv_sec = mktime(gmtime(&cur_time));
//		printf("Set time: %d, %s \n", (int)tv.tv_sec, asctime(gmtime(&cur_time)));
		if (settimeofday(&tv, &tz) < 0)
			printf("CMA: Set time error.\n");
		rtc_set_time(gmtime(&cur_time));
	}
	
	cur_time = time((time_t*)NULL);
	Clocktime_Stamp = mktime_k(localtime(&cur_time));
	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = 5;
	
	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;
	memcpy((sbuf + 1), &Clocktime_Stamp, sizeof(int));
	
	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_NetAdapter_SetReq_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte  config_type = *(rbuf + sizeof(frame_head_t));
	Ctl_net_adap_t  *adap = (Ctl_net_adap_t  *)(rbuf + sizeof(frame_head_t) + 1);
	byte sbuf[MAX_DATA_BUFSIZE];
	
	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;
	
	if (config_type == 0x00) {
		Device_getNet_info(adap);
	}
	else if (config_type == 0x01) {
		Device_setNet_info(adap);
	}
	else
		sbuf[0] = 0x00;
		
	
	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = sizeof(Ctl_net_adap_t) + 1;
	
	memcpy((sbuf + 1), adap, sizeof(Ctl_net_adap_t));
	
	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	return 0;
}

static int Request_data_type[] = {
	CMA_MSG_TYPE_DATA_QXENV,
	CMA_MSG_TYPE_DATA_TGQXIE,
	CMA_MSG_TYPE_DATA_DDXWFTZ,
	CMA_MSG_TYPE_DATA_DDXWFBX,
	CMA_MSG_TYPE_DATA_DXHCH,
	CMA_MSG_TYPE_DATA_DXWD,
	CMA_MSG_TYPE_DATA_FUBING,
	CMA_MSG_TYPE_DATA_DXFP,
	CMA_MSG_TYPE_DATA_DXWDTZH,
	CMA_MSG_TYPE_DATA_DXWDGJ,
	CMA_MSG_TYPE_DATA_XCHWS,
};

static int CMA_Send_RecordingData(int fd, byte type, time_t start, time_t end)
{
	byte record[256];
	int total = 0;
	int record_len;
	char *filename = NULL;
	int i;
	time_t t_max, t_min, t;
	int ret = 0;

	switch (type) {
	case CMA_MSG_TYPE_DATA_QXENV:
	case CMA_MSG_TYPE_CTL_QX_PAR:
		filename = RECORD_FILE_QIXIANG;
		record_len = sizeof(struct record_qixiang);
		break;
	case CMA_MSG_TYPE_DATA_TGQXIE:
	case CMA_MSG_TYPE_CTL_TGQX_PAR:
		filename = RECORD_FILE_TGQXIE;
		record_len = sizeof(struct record_incline);
		break;
	case CMA_MSG_TYPE_DATA_FUBING:
	case CMA_MSG_TYPE_CTL_FUBING_PAR:
		filename = RECORD_FILE_FUBING;
		record_len = sizeof(struct record_fubing);
		break;
	case CMA_MSG_TYPE_DATA_DDXWFTZ:
//		break;
	case CMA_MSG_TYPE_DATA_DDXWFBX:
//		break;
	case CMA_MSG_TYPE_DATA_DXHCH:
//		break;
	case CMA_MSG_TYPE_DATA_DXWD:
//		break;
	case CMA_MSG_TYPE_DATA_DXFP:
	case CMA_MSG_TYPE_CTL_DXFP_PAR:
//		break;
	case CMA_MSG_TYPE_DATA_DXWDTZH:
//		break;
	case CMA_MSG_TYPE_DATA_DXWDGJ:
//		break;
	case CMA_MSG_TYPE_DATA_XCHWS:
//		break;
	default:
		printf("Invalid Sensor tyep.\n");
		return -1;
	}

	total = File_GetNumberOfRecords(filename, record_len);
	if (total == 0) {
		return 0;
	}

	if (start == end) {
		memset(&record, 0, record_len);
		if (File_GetRecordByIndex(filename, &record, record_len, (total - 1)) == record_len) {
			ret = CMA_Send_SensorData(fd, type, (record + sizeof(time_t)));
			if (ret == 0xff)
				fprintf(stdout, "CMD: Send Data reponse OK.\n");
		}
		return 0;
	}

	if (start > end) {
		t_max = start;
		t_min = end;
	}
	else {
		t_max = end;
		t_min = start;
	}

	for (i = 0; i < total; i++) {
		memset(&record, 0, record_len);
		memset(&t, 0, sizeof(time_t));
		if (File_GetRecordByIndex(filename, &record, record_len, i) == record_len) {
			memcpy(&t, record, sizeof(time_t));
			if ((t <= t_max) && (t >= t_min)) {
				ret = CMA_Send_SensorData(fd, type, (record + sizeof(time_t)));
				if (ret == 0xff)
					fprintf(stdout, "CMD: Send Data reponse OK.\n");
			}
		}
	}

	return 0;
}

int CMA_RequestData_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte  req_type;
	byte sbuf[MAX_DATA_BUFSIZE];
	int time_start = 0, time_end = 0;
	
	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;
	
	req_type = *(rbuf + sizeof(frame_head_t));
	memcpy(&time_start, (rbuf + sizeof(frame_head_t) + 1), sizeof(int));
	memcpy(&time_end, (rbuf + sizeof(frame_head_t) + 1 + sizeof(int)), sizeof(int));
	fprintf(stdout, "Request Data from: %s", ctime((time_t *)&time_start));
	fprintf(stdout, "to: %s", ctime((time_t *)&time_end));
	
	/*
	 *    Response Process
	 */
	
	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = 2;
	
	*(sbuf) = 0xff;
	*(sbuf + 1) = req_type;
	
	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	if (req_type ==  0xff) {
		fprintf(stdout, "CMD: Send all Sensor Data.\n");
		int i;
		int num = sizeof(Request_data_type) / sizeof(int);
		for (i = 0; i < num; i++) {
			req_type = Request_data_type[i];
			CMA_Send_RecordingData(fd, req_type, time_start, time_end);
		}
	}
	else
		CMA_Send_RecordingData(fd, req_type, time_start, time_end);

	return 0;
}

int CMA_SamplePar_SetReq_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	Ctl_sample_par_t  *sample_par = (Ctl_sample_par_t  *)(rbuf + sizeof(frame_head_t) + 1);
	byte sbuf[MAX_DATA_BUFSIZE];
	byte  set_type = *(rbuf + sizeof(frame_head_t));
	usint cycle = 0;

	printf("Enter func: %s\n", __func__);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;

	if (set_type == 0x00) {
		 /* Get Sample Parameter from Sensor */
		switch (sample_par->Request_Type) {
		case CMA_MSG_TYPE_CTL_QX_PAR:
			cycle = Device_getSampling_Cycle("qixiang:samp_period");
			if (cycle > 0)
				sample_par->Main_Time = cycle;
			break;
		case CMA_MSG_TYPE_CTL_TGQX_PAR:
			cycle = Device_getSampling_Cycle("tgqingxie:samp_period");
			if (cycle > 0)
				sample_par->Main_Time = cycle;
			break;
		case CMA_MSG_TYPE_CTL_FUBING_PAR:
			cycle = Device_getSampling_Cycle("fubing:samp_period");
			if (cycle > 0)
				sample_par->Main_Time = cycle;
			break;
		default:
			sbuf[0] = 0x00;
			return -1;
		}
	}
	else if (set_type == 0x01) {
		; /* Set Sample Parameter from Sensor */
		switch (sample_par->Request_Type) {
		case CMA_MSG_TYPE_CTL_QX_PAR:
			cycle = sample_par->Main_Time;
			if (cycle > 0)
				Device_setSampling_Cycle("qixiang:samp_period", cycle);
			sample_dev.interval = cycle * 60;
			break;
		case CMA_MSG_TYPE_CTL_TGQX_PAR:
			cycle = sample_par->Main_Time;
			if (cycle > 0)
				Device_setSampling_Cycle("tgqingxie:samp_period", cycle);
			sample_dev_1.interval = cycle * 60;
			break;
		case CMA_MSG_TYPE_CTL_FUBING_PAR:
			cycle = sample_par->Main_Time;
			if (cycle > 0)
				Device_setSampling_Cycle("fubing:samp_period", cycle);
			sample_dev_2.interval = cycle * 60;
			break;
		default:
			sbuf[0] = 0x00;
			return -1;
		}
	}
	else
		sbuf[0] = 0x00;

	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = sizeof(Ctl_sample_par_t) + 1;

	memcpy((sbuf + 1), sample_par, sizeof(Ctl_sample_par_t));

	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_ModelPar_SetReq_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte sbuf[MAX_DATA_BUFSIZE];
	
	byte set_type = *(rbuf + sizeof(frame_head_t));
	byte req_type = *(rbuf + sizeof(frame_head_t) + 1);
	byte config_num = *(rbuf + sizeof(frame_head_t) + 2);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;
	
	if (set_type == 0x00) {
		; /* Get Model Parameter from Sensor, return config_num and Model parameter*/
	}
	else if (set_type == 0x01) {
		; /* Set Model Parameter from Sensor */
	}
	else
		sbuf[0] = 0x00;
		
	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = 3 + config_num * 10;
	
	sbuf[1] = req_type;
	sbuf[2] = config_num;
//	memcpy((sbuf + 3), (rbuf + sizeof(frame_head_t) + 3), config_num * 10);
	
	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_Alarm_SetReq_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte sbuf[MAX_DATA_BUFSIZE];

	byte set_type = *(rbuf + sizeof(frame_head_t));
	byte req_type = *(rbuf + sizeof(frame_head_t) + 1);
	byte config_num = *(rbuf + sizeof(frame_head_t) + 2);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;

	if (set_type == 0x00) {
		/* Get Sensor Alarm Setting from Sensor, return config_num and Alarm value*/
		Device_GetAlarm_Threshold(req_type, (sbuf + 3), &config_num);

	}
	else if (set_type == 0x01) {
		; /* Set Alarm Value to Sensor */
		Device_SetAlarm_Threshold(req_type, (rbuf + sizeof(frame_head_t) + 3), config_num);
	}
	else
		sbuf[0] = 0x00;

	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = 3 + config_num * 10;

	sbuf[1] = req_type;
	sbuf[2] = config_num;
	if (set_type == 0x01)
		memcpy((sbuf + 3), (rbuf + sizeof(frame_head_t) + 3), config_num * 10);

	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_UpDevice_SetReq_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	Ctl_up_device_t  *up_device = (Ctl_up_device_t  *)(rbuf + sizeof(frame_head_t) + 1);
	byte sbuf[MAX_DATA_BUFSIZE];

	byte  set_type = *(rbuf + sizeof(frame_head_t));

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;

	if (set_type == 0x00) {
		/* Get up device info, ip addr and port */
		if (Device_getServerInfo(up_device) < 0)
			sbuf[0] = 0x00;
	}
	else if (set_type == 0x01) {
		Device_setServerInfo(up_device); /* Set up device info, ip addr and port */
	}
	else
		sbuf[0] = 0x00;

	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = sizeof(Ctl_up_device_t) + 1;

	memcpy((sbuf + 1), up_device, sizeof(Ctl_up_device_t));

	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_BasicInfo_SetReq_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte sbuf[MAX_DATA_BUFSIZE];

	byte set_type = *(rbuf + sizeof(frame_head_t));
	byte req_type = *(rbuf + sizeof(frame_head_t) + 1);
	byte msg_type = *(rbuf + sizeof(frame_head_t) + 2);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;

	if (set_type == 0x00) {
		/* Get Sensor basic information */
	}
	else if (set_type == 0x01) {
		; /* Set Basic information to Sensor */
	}
	else
		sbuf[0] = 0x00;

	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = 3;

	sbuf[1] = req_type;
	sbuf[2] = msg_type;

	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	if (msg_type == 0x01)
		CMA_Send_BasicInfo(fd, CMA_Env_Parameter.id, 1);
	else if (msg_type == 0x02)
		CMA_Send_WorkStatus(fd, CMA_Env_Parameter.id);
	else
		sbuf[0] = 0x00;

	return 0;
}

int CMA_Request_LostPackage(int fd, byte *rbuf, const char *bitmap)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte sbuf[MAX_COMBUF_SIZE];
	unsigned int package_num;
	int *pdata = NULL;
	int num = 0;
	int i;
	int timeout = 5;

	memcpy(&package_num, (rbuf + sizeof(frame_head_t) + 20), 4);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);

	pdata = (int *)(sbuf + 4);

	num = File_UpdateGetLost(bitmap, package_num, pdata);

	if (num == 0) return 0;

	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->msg_type = CMA_MSG_TYPE_CTL_UPGRADE_REP;
	p_head->pack_len = 4 * (num + 1);
	pdata = (int *)(sbuf);
	*pdata = num;
	printf("Lost package number: %d \n", *pdata++);
	while(*pdata != 0) {
		printf("%d\n", *pdata++);
	}

	for (i = 0; i < 5; i++) {
		upgradeLostFlag = 0;
		if (Commu_SendPacket(fd, p_head, sbuf) < 0)
			return -1;

		printf("---------- Get Software Upgrade Lost Packages --------------\n");
		while ((upgradeLostFlag == 0) && timeout) {
			sleep(1);
			--timeout;
		}
		if (upgradeLostFlag == 1)
			break;
	}

	if (i == 5)
		return -1;
	else
		return 0;
}

static int software_update_end(int fd, byte *rbuf)
{
	unsigned int package_num;
	int time_stamp;
	char filename[20];
	char tmp_file[32];
	char file_bitmap[64];

	memcpy(filename, (rbuf + sizeof(frame_head_t)), 20);
	memcpy(&package_num, (rbuf + sizeof(frame_head_t) + 20), 4);
	memcpy(&time_stamp, (rbuf + sizeof(frame_head_t) + 24), 4);
	fprintf(stdout, "Data End, Filename: %s, total num = %d, time = %d\n", filename, package_num, time_stamp);

	memset(tmp_file, 0, 32);
	memset(file_bitmap, 0, 64);
	sprintf(tmp_file, "%s.tmp", filename);
	sprintf(file_bitmap, "%s.bitmap", filename);

	if (CMA_Request_LostPackage(fd, rbuf, file_bitmap) < 0) {
		fprintf(stderr, "CMD: Send Lost Packages Request error.\n");
		goto clear;
	}

	File_Delete(filename);
	File_UpgradeConstruct(tmp_file, filename);

	Device_SoftwareUpgrade(filename);

clear:
	File_Delete(tmp_file);
	File_Delete(file_bitmap);

	return 0;
}

int CMA_SoftWare_Update_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	int msg_type = p_head->msg_type;
	unsigned int package_num;
	unsigned int package_index;
	char filename[20];
	char tmp_file[32];
	char file_bitmap[64];
	int data_len = 0;

	if (msg_type == CMA_MSG_TYPE_CTL_UPGRADE_DATA) {
		upgradeLostFlag = 1;
		data_len = p_head->pack_len - 28;
		memcpy(filename, (rbuf + sizeof(frame_head_t)), 20);
		memcpy(&package_num, (rbuf + sizeof(frame_head_t) + 20), 4);
		memcpy(&package_index, (rbuf + sizeof(frame_head_t) + 24), 4);
		fprintf(stdout, "Filename: %s, total num = %d, index = %d\n", filename, package_num, package_index);

		memset(tmp_file, 0, 32);
		memset(file_bitmap, 0, 64);
		sprintf(tmp_file, "%s.tmp", filename);
		sprintf(file_bitmap, "%s.bitmap", filename);

		if (File_UpgradeWrite_mmap(tmp_file, package_index, data_len, (rbuf + sizeof(frame_head_t) + 28)) < 0)
			return -1;

		if (File_UpdateBitmap(file_bitmap, package_index, package_num) < 0)
			return -1;
	}

	if (msg_type == CMA_MSG_TYPE_CTL_UPGRADE_END) {
		software_update_end(fd, rbuf);
	}

	return 0;
}

int CMA_DeviceId_SetReq_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte sbuf[MAX_DATA_BUFSIZE];
	byte set_type = *(rbuf + sizeof(frame_head_t));
	byte CMD_ID[18];
	byte Component_ID[18];
	usint Original_ID = 0;

	memset(CMD_ID, 0, sizeof(CMD_ID));
	memset(Component_ID, 0, sizeof(Component_ID));
	memcpy(CMD_ID, p_head->id, 17);
	memcpy(Component_ID, (rbuf + sizeof(frame_head_t) + 1), 17);
	memcpy(&Original_ID, (rbuf + sizeof(frame_head_t) + 18), 2);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;

	if (set_type == 0x00) {
		Device_getId(Component_ID, &Original_ID); /* Get device ID */
		memcpy(p_head->id, CMA_Env_Parameter.id, 17);
	}
	else if (set_type == 0x01) {
		if (Device_setId(CMD_ID, Component_ID, Original_ID) < 0) /* Set device ID */
			sbuf[0] = 0x00;
	}
	else
		sbuf[0] = 0x00;

	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = 20;

	memcpy((sbuf + 1), Component_ID, 17);
	memcpy((sbuf + 18), &Original_ID, 2);

	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_DeviceRset_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte sbuf[MAX_DATA_BUFSIZE];
	usint set_type = 0;

	memcpy(&set_type, (rbuf + sizeof(frame_head_t)), 2);
	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;

	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = 1;

	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	if (Device_reset(set_type) < 0) {
		sbuf[0] = 0x00;
	}

	return 0;
}

int CMA_WakeupTime_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte sbuf[MAX_DATA_BUFSIZE];
	int revival_time;
	usint revival_cycle;
	usint duration_time;

	memcpy(&revival_time, (rbuf + sizeof(frame_head_t)), 4);
	memcpy(&revival_cycle, (rbuf + sizeof(frame_head_t) + 4), 2);
	memcpy(&duration_time, (rbuf + sizeof(frame_head_t) + 4 + 2), 2);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = 0xff;

	if (Device_setWakeup_time(revival_time, revival_cycle, duration_time) < 0) {
		sbuf[0] = 0x00;
	}

	p_head->frame_type = CMA_FRAME_TYPE_CONTROL_RES;
	p_head->pack_len = 1;

	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_SetImagePar_Response(int fd, byte *rbuf)
{
//	frame_head_t *p_head = (frame_head_t *)rbuf;
	Ctl_image_device_t  *image_par = (Ctl_image_device_t  *)(rbuf + sizeof(frame_head_t));

	/*
	 *  Set Camera Parameters
	 */
	if (Camera_SetParameter(image_par) < 0)
		return -1;

	return 0;
}

int CMA_GetImagePar_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte sbuf[MAX_DATA_BUFSIZE];
	Ctl_image_device_t par;

	memset(sbuf, 0, MAX_DATA_BUFSIZE);

	p_head->frame_type = CMA_FRAME_TYPE_IMAGE_CTRL;
	p_head->pack_len = sizeof(Ctl_image_device_t);

	memset(&par, 0, sizeof(Ctl_image_device_t));
	if (Camera_GetParameter(&par) < 0) {
		return -1;
	}
	memcpy(sbuf, &par, sizeof(Ctl_image_device_t));

	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_CaptureTimetable_Response(int fd, byte *rbuf)
{
//	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte channel = *(rbuf + sizeof(frame_head_t));
	byte groups = *(rbuf + sizeof(frame_head_t) + 1);
	Ctl_image_timetable_t  *image_tb = (Ctl_image_timetable_t  *)(rbuf + sizeof(frame_head_t) + 2);

	/*
	 *  Set Camera Time Table
	 */
	if (Camera_SetTimetable(image_tb, groups, channel) < 0)
		return -1;

	return 0;
}

int CMA_GetImageTimeTable_Response(int fd, byte *rbuf)
{
	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte sbuf[MAX_DATA_BUFSIZE];
	int num = 0;

	memset(sbuf, 0, MAX_DATA_BUFSIZE);

	p_head->frame_type = CMA_FRAME_TYPE_IMAGE_CTRL;

	if (Camera_GetTimeTable((sbuf + 2), &num) < 0)
		return -1;

	sbuf[0] = 1;
	sbuf[1] = num;

	p_head->pack_len = sizeof(Ctl_image_timetable_t) * num + 2;

	if (Commu_SendPacket(fd, p_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_ManualCapture_Response(int fd, byte *rbuf)
{
//	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte channel = *(rbuf + sizeof(frame_head_t));
	byte presetting = *(rbuf + sizeof(frame_head_t) + 1);
	char filename[256];

	memset(filename, 0, 256);

	/*
	 *  Set Camera Parameters
	 */
	if (Camera_StartCapture(filename, channel, presetting) < 0)
		return -1;

	if (CMA_Image_SendRequest(fd, filename, channel, presetting) < 0)
		return -1;

	return 0;
}

#define IMAGE_SUBDATA_LEN	512

int CMA_Image_SendRequest(int fd, char *imageName, byte channel, byte presetting)
{
	frame_head_t f_head;
	Send_image_req_t req;
	int size = 0;
	usint pkg_num = 0;
	int i, timeout;

	memset(&f_head, 0, sizeof(frame_head_t));
	memset(&req, 0, sizeof(Send_image_req_t));

	f_head.head = 0x5aa5;
	f_head.pack_len = sizeof(Send_image_req_t);
	f_head.frame_type = CMA_FRAME_TYPE_IMAGE;
	f_head.msg_type = CMA_MSG_TYPE_IMAGE_SENDIMG_REQ;
	memcpy(f_head.id, CMA_Env_Parameter.id, 17);

	req.Channel_No = channel;
	req.Presetting_No = presetting;

	if (File_Exist(imageName) == 0)
		return -1;

	size = get_file_size(imageName);
	if (size == 0)
		return -1;

	pkg_num = (size + IMAGE_SUBDATA_LEN - 1) / IMAGE_SUBDATA_LEN;
	printf("Send Image: package num = %d, size = %d\n", pkg_num, size);
	req.Packet_Num = ((pkg_num & 0xff00) >> 8) | ((pkg_num & 0xff) << 8);

	pthread_mutex_lock(&imgMutex);
	memset(imageRbuf, 0, MAX_COMBUF_SIZE);
	imageRcvLen = 0;
	pthread_mutex_unlock(&imgMutex);

	for (i = 0; i < 5; i++) {
		if (Commu_SendPacket(fd, &f_head, (byte *)&req) < 0)
			return -1;

		printf("---------- Get Image Request Packages --------------\n");

		timeout = 5 * 100;
		while ((imageRcvLen == 0) && (timeout)) {
			usleep(10*1000);
			timeout--;
		}
		if (imageRcvLen > 0) {
			if (memcmp(imageRbuf, &f_head, sizeof(frame_head_t)) == 0) {
				if (memcmp((imageRbuf +  sizeof(frame_head_t)), &req, sizeof(Send_image_req_t)) == 0)
					break;
			}
		}
	}
	if (i == 5) {
		fprintf(stderr, "CMD: Receive Image Request Package error.\n");
		return -1;
	}

	fprintf(stdout, "CMD: Start to Send Image Data.\n");
	if (CMA_Image_SendImageFile(fd, imageName, channel, presetting) < 0)
		return -1;

	return 0;
}

int CMA_Image_SendImageFile(int fd, char *ImageFile, byte channel, byte presetting)
{
	frame_head_t f_head;
	frame_head_t *p_head;
	byte sbuf[MAX_COMBUF_SIZE];
	int image_fd;
	int size = 0;
	usint pkg_num = 0;
	byte data[IMAGE_SUBDATA_LEN];
	int i, timeout;
	int ret = -1;
	usint lost_num = 0;

	if (File_Exist(ImageFile) == 0)
		return -1;

	size = get_file_size(ImageFile);
	if (size == 0)
		return -1;

	if ((image_fd = File_Open(ImageFile)) < 0)
		return -1;

	pkg_num = (size + IMAGE_SUBDATA_LEN - 1) / IMAGE_SUBDATA_LEN;
	printf("Send Image: file = %s, package num = %d, size = %d\n", ImageFile, pkg_num, size);

	memset(&f_head, 0, sizeof(frame_head_t));

	f_head.head = 0x5aa5;
	f_head.frame_type = CMA_FRAME_TYPE_IMAGE;
	f_head.msg_type = CMA_MSG_TYPE_IMAGE_DATA;
	memcpy(f_head.id, CMA_Env_Parameter.id, 17);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = channel;
	sbuf[1] = presetting;
	memcpy((sbuf + 2), &pkg_num, 2);

	for (i = 1; i <= pkg_num; i++) {
		memset(data, 0, IMAGE_SUBDATA_LEN);
		if ((ret = read(image_fd, &data, IMAGE_SUBDATA_LEN)) <= 0) {
			fprintf(stderr, "CMD: Read Image file: %s error.\n", ImageFile);
			break;
		}

		memcpy((sbuf + 4), &i, 2);
		memmove((void *)(sbuf + 6), (void *)data, ret);

		f_head.pack_len = 6 + ret;

		printf("Send Image: i = %d, pkg_num = %d, len = %d, fd = %d\n", i, pkg_num, ret, fd);
		if (Commu_SendPacket(fd, &f_head, sbuf) < 0)
			return -1;
		usleep(400000);
	}

	sleep(2);
	if (CMA_Image_SendData_End(fd, channel, presetting) < 0)
		return -1;

	for (i = 0; i < 5; i++) {
		printf("---------- Wait Image Lost Packages Request, i = %d --------------\n", i);
		pthread_mutex_lock(&imgMutex);
		memset(imageRbuf, 0, MAX_COMBUF_SIZE);
		imageRcvLen = 0;
		pthread_mutex_unlock(&imgMutex);
		timeout = 8 * 100;
		while ((imageRcvLen == 0) && (timeout)) {
			usleep(10*1000);
			timeout--;
		}
		if (imageRcvLen > 0) {
			p_head = (frame_head_t *)imageRbuf;
			if (p_head->msg_type == CMA_MSG_TYPE_IMAGE_DATA_REP) {
				memcpy(&lost_num, (imageRbuf + sizeof(frame_head_t) + 2), 2);
				printf("Lost Image Data: total = %d \n", lost_num);
				if (lost_num > 0) {
					CMA_Image_SendImageLost(fd, ImageFile, imageRbuf);
					i = 0;
				}
			}
		}
	}

	File_Close(image_fd);

	return 0;
}

int CMA_Image_SendImageLost(int fd, char *ImageFile, byte *rbuf)
{
	frame_head_t f_head;
	byte sbuf[MAX_COMBUF_SIZE];
	int image_fd;
	int size = 0;
	usint pkg_num = 0;
	byte data[IMAGE_SUBDATA_LEN];
	int i, ret;
	usint index;
	byte channel = *(rbuf + sizeof(frame_head_t));
	byte presetting = *(rbuf + sizeof(frame_head_t) + 1);
	byte *index_buf = rbuf + sizeof(frame_head_t) + 4;
	usint total = 0;

	memcpy(&total, (rbuf + sizeof(frame_head_t) + 2), 2);

	if (File_Exist(ImageFile) == 0)
		return -1;

	size = get_file_size(ImageFile);
	if (size == 0)
		return -1;

	if ((image_fd = File_Open(ImageFile)) < 0)
		return -1;

	pkg_num = (size + IMAGE_SUBDATA_LEN - 1) / IMAGE_SUBDATA_LEN;
	memset(&f_head, 0, sizeof(frame_head_t));

	f_head.head = 0x5aa5;
	f_head.frame_type = CMA_FRAME_TYPE_IMAGE;
	f_head.msg_type = CMA_MSG_TYPE_IMAGE_DATA;
	memcpy(f_head.id, CMA_Env_Parameter.id, 17);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = channel;
	sbuf[1] = presetting;
	memcpy((sbuf + 2), &pkg_num, 2);

	for (i = 0; i < total; i++) {
		memset(data, 0, IMAGE_SUBDATA_LEN);
//		index = (index_buf[0] << 8) | index_buf[1];
		index = (index_buf[1] << 8) | index_buf[0];
		printf("Lost index = %d, pkg_num = %d\n", index, pkg_num);
		lseek(image_fd, ((index - 1) * IMAGE_SUBDATA_LEN), SEEK_SET);
		if ((ret = read(image_fd, &data, IMAGE_SUBDATA_LEN)) <= 0) {
			fprintf(stderr, "CMD: Read Image file error, file = %s.\n", ImageFile);
			break;
		}

		memcpy((sbuf + 4), &index, 2);
		memcpy((sbuf + 6), data, ret);

		f_head.pack_len = 6 + ret;

		printf("Send Lost Image: total = %d, index = %d, len = %d\n", total, index, ret);
		if (Commu_SendPacket(fd, &f_head, sbuf) < 0)
			return -1;
		usleep(400000);

		index_buf += 2;
	}

	sleep(2);
	if (CMA_Image_SendData_End(fd, channel, presetting) < 0)
		return -1;

	File_Close(image_fd);

	return 0;
}

int CMA_Image_SendData_End(int fd, byte channel, byte presetting)
{
	frame_head_t f_head;
	byte sbuf[MAX_DATA_BUFSIZE];
	int cur_time = time((time_t*)NULL);

	memset(&f_head, 0, sizeof(frame_head_t));

	f_head.head = 0x5aa5;
	f_head.frame_type = CMA_FRAME_TYPE_IMAGE;
	f_head.msg_type = CMA_MSG_TYPE_IMAGE_DATA_END;
	memcpy(f_head.id, CMA_Env_Parameter.id, 17);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = channel;
	sbuf[1] = presetting;
	memcpy((sbuf + 2), &cur_time, 4);

	f_head.pack_len = 14;

	if (Commu_SendPacket(fd, &f_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_CameraControl_Response(int fd, byte *rbuf)
{
//	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte channel = *(rbuf + sizeof(frame_head_t));
	byte pre_setting = *(rbuf + sizeof(frame_head_t) + 1);
	byte action = *(rbuf + sizeof(frame_head_t) + 2);

	/*
	 *  Set Camera Parameters
	 */
	if (Camera_Control(action, pre_setting, channel) < 0)
		return -1;

	return 0;
}

int CMA_Video_StopStart_Response(int fd, byte *rbuf)
{
//	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte channel = *(rbuf + sizeof(frame_head_t));
	byte control = *(rbuf + sizeof(frame_head_t) + 1);
	usint port;

	memcpy(&port, (rbuf + 2), 2);

	/*
	 *  Set Camera Parameters
	 */
	if (Camera_Video_Start(channel, control, port) < 0)
		return -1;

	return 0;
}


int CMA_Send_HeartBeat(int fd, char *id)
{
	frame_head_t f_head;
	int cur_time;

	if (strlen(id) != 17) {
		printf("Invalid Device ID.\n");
		return -1;
	}

	memset(&f_head, 0, sizeof(frame_head_t));
	cur_time = time((time_t*)NULL);

	f_head.head = 0x5aa5;
	f_head.pack_len = 4;
	f_head.frame_type = CMA_FRAME_TYPE_STATUS;
	f_head.msg_type = CMA_MSG_TYPE_STATUS_HEART;
	memcpy(f_head.id, id, 17);

	if (Commu_SendPacket(fd, &f_head, (byte *)&cur_time) < 0)
		return -1;

	return 0;
}

int CMA_Send_BasicInfo(int fd, char *id, int wait)
{
	frame_head_t f_head;
	status_basic_info_t dev;
	int ret = 0;

	if (strlen(id) != 17) {
		printf("Invalid Device ID.\n");
		return -1;
	}

	memset(&f_head, 0, sizeof(frame_head_t));
	memset(&dev, 0, sizeof(status_basic_info_t));

	f_head.head = 0x5aa5;
	f_head.pack_len = sizeof(status_basic_info_t);
	f_head.frame_type = CMA_FRAME_TYPE_STATUS;
	f_head.msg_type = CMA_MSG_TYPE_STATUS_INFO;
	memcpy(f_head.id, id, 17);

	if (Device_get_basic_info(&dev) < 0)
			return -1;

	CMD_Response_data = -1;

	if (Commu_SendPacket(fd, &f_head, (byte *)&dev) < 0)
		return -1;

	if (wait) {
		ret = CMD_WaitStatus_Res(CMA_MSG_TYPE_STATUS_INFO, 5);
		if (ret != 0xff) {
			fprintf(stdout, "CMD: Send Basic Info Error.\n");
			return -1;
		}
	}

	return 0;
}

int CMA_Send_WorkStatus(int fd, char *id)
{
	frame_head_t f_head;
	status_working_t status;
	int ret = 0;

	if (strlen(id) != 17) {
		printf("Invalid Device ID.\n");
		return -1;
	}

	memset(&f_head, 0, sizeof(frame_head_t));
	memset(&status, 0, sizeof(status_working_t));

	f_head.head = 0x5aa5;
	f_head.pack_len = sizeof(status_working_t);
	f_head.frame_type = CMA_FRAME_TYPE_STATUS;
	f_head.msg_type = CMA_MSG_TYPE_STATUS_WORK;
	memcpy(f_head.id, id, 17);

	if (Device_get_working_status(&status) < 0)
		return -1;

	if (Commu_SendPacket(fd, &f_head, (byte *)&status) < 0)
		return -1;

	ret = CMD_WaitStatus_Res(CMA_MSG_TYPE_STATUS_WORK, 5);
	if (ret == 0xff) {
		fprintf(stdout, "CMD: Send work status OK.\n");
	}

	return 0;
}

int CMA_Send_Fault_Info(int fd, char *id, char *fault_desc, int len)
{
	frame_head_t f_head;
	int cur_time = time((time_t*)NULL);
	byte sbuf[MAX_DATA_BUFSIZE];

	if (strlen(id) != 17) {
		printf("Invalid Device ID.\n");
		return -1;
	}

	memset(&f_head, 0, sizeof(frame_head_t));


	f_head.head = 0x5aa5;
	f_head.frame_type = CMA_FRAME_TYPE_STATUS;
	f_head.msg_type = CMA_MSG_TYPE_STATUS_ERROR;
	memcpy(f_head.id, id, 17);

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	memcpy(sbuf, &cur_time, 4);
	memcpy((sbuf + 4), fault_desc, len);

	f_head.pack_len = 4 + len;

	if (Commu_SendPacket(fd, &f_head, sbuf) < 0)
		return -1;

	return 0;
}
