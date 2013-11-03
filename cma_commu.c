#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include "device.h"
#include "cma_commu.h"
#include "types.h"
#include "socket_lib.h"
#include "sensor_ops.h"
#include "rtc_alarm.h"
#include "file_ops.h"

volatile int CMD_Response_data = 0;

#define _DEBUG

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

int Commu_GetPacket_Udp(int localport, byte *rbuf, int len)
{
	int ret = 0;
	usint crc16 = 0;
	usint size;

	if ((rbuf == NULL)) {
		printf("Commu_GetPacket: Invalid parameter.\n");
		return -1;
	}

	printf("Begin to receive msg, len = %d\n", len);
	memset(rbuf, 0, len);
	ret = socket_recv_udp(localport, rbuf, len);
	if (ret < 0)
		return ret;
	if ((rbuf[0] != 0xA5) && (rbuf[1] != 0x5A)) {
		printf("Invalid package head.\n");
		return -1;
	}

	memcpy(&size, (rbuf + 2), 2);
	printf("size = %d \n", size);
	if ((size <= 0) || (ret < (size + sizeof(frame_head_t) + 2)))
		return -1;

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

	if (fd == -1) {
		return Commu_GetPacket_Udp(CMA_Env_Parameter.local_port, rbuf, len);
	}

//	printf("Begin to receive msg, len = %d\n", len);
	
	ret = socket_recv(fd, rbuf, sizeof(frame_head_t), timeout);
	if (ret < 0)
		return ret;
	if ((rbuf[0] != 0xA5) && (rbuf[1] != 0x5A)) {
		printf("Invalid package head.\n");
		return -1;
	}
	
	memcpy(&size, (rbuf + 2), 2);
//	printf("size = %d \n", size);
	if ((size <= 0) || (ret != sizeof(frame_head_t))) {
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

	ret = socket_send(fd, sbuf, size, timeout);
//	printf("ret = %d, size = %d\n", ret, size);
	if (ret < 0)
		return -1;

	return 0;
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
	if (memcmp(f_head->id, CMA_Env_Parameter.id, 17) != 0) {
		fprintf(stderr, "Device ID: %s, MSG ID: %s, Miss Match.\n", f_head->id, CMA_Env_Parameter.id);
		return -1;
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
		case CMA_MSG_TYPE_CTL_UPGRADE_DATA:
			if (CMA_SoftWare_Update_Response(fd, rbuf) < 0)
				ret = -1;
			break;
		default:
			ret = -1;
			printf("CMA: Invalid MSG type.\n");
		}
	}
	else if (frame_type == CMA_FRAME_TYPE_DATA_RES) {
		status = *(rbuf + sizeof(frame_head_t));
		fprintf(stdout, "CMD: Send data response 0x%x.\n", status);
		CMD_Response_data = status;
	}
	else if (frame_type == CMA_FRAME_TYPE_STATUS_RES) {
		status = *(rbuf + sizeof(frame_head_t));
		fprintf(stdout, "CMD: Send device status response 0x%x.\n", status);
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
		f_head.pack_len = sizeof(Data_incline_t);
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
		f_head.pack_len = sizeof(Data_ice_thickness_t);
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
	
//	if (Sensor_GetData(data_buf, type) < 0) {
//		printf("Get Sensor data error.\n");
//		return -1;
//	}

	if (Commu_SendPacket(fd, &f_head, (byte *)data) < 0)
		return -1;

	return 0;
}

int CMA_Wait_SensorData_Res(int fd, int type)
{
	frame_head_t *p_head = NULL;
	byte rbuf[MAX_COMBUF_SIZE];
	byte status = 0;
	int ret;

	ret = Commu_GetPacket(CMA_Env_Parameter.socket_fd, rbuf, MAX_COMBUF_SIZE, 5);
	if (ret < 0)
		return ret;

	p_head = (frame_head_t *)rbuf;
	if ((type == p_head->msg_type) && (p_head->frame_type == CMA_FRAME_TYPE_DATA_RES)) {
		status = *(rbuf + sizeof(frame_head_t));
		if (status == 0xff)
			return 0;
	}

	return -1;
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

static int CMA_Send_RecordingData(int fd, byte type, time_t start, time_t end)
{
	byte record[256];
	int total = 0;
	int record_len;
	char *filename = NULL;
	int i;
	time_t t_max, t_min, t;

	switch (type) {
	case CMA_MSG_TYPE_DATA_QXENV:
		filename = RECORD_FILE_QIXIANG;
		record_len = sizeof(struct record_qixiang);
		break;
	case CMA_MSG_TYPE_DATA_TGQXIE:
//		filename = RECORD_FILE_TGQXIE;
//		break;
	case CMA_MSG_TYPE_DATA_DDXWFTZ:
//		break;
	case CMA_MSG_TYPE_DATA_DDXWFBX:
//		break;
	case CMA_MSG_TYPE_DATA_DXHCH:
//		break;
	case CMA_MSG_TYPE_DATA_DXWD:
//		break;
	case CMA_MSG_TYPE_DATA_FUBING:
//		break;
	case CMA_MSG_TYPE_DATA_DXFP:
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
				CMA_Send_SensorData(fd, type, (record + sizeof(time_t)));
				if (CMA_Wait_SensorData_Res(fd, type) == 0)
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
		CMA_Send_BasicInfo(fd, CMA_Env_Parameter.id);
	else if (msg_type == 0x02)
		CMA_Send_WorkStatus(fd, CMA_Env_Parameter.id);
	else
		sbuf[0] = 0x00;

	return 0;
}

int CMA_SoftWare_Update_Response(int fd, byte *rbuf)
{
//	frame_head_t *p_head = (frame_head_t *)rbuf;
//	int msg_type = p_head->msg_type;
	unsigned int package_num;
	unsigned int package_index;
	char filename[20];

	memcpy(filename, (rbuf + sizeof(frame_head_t)), 20);
	memcpy(&package_num, (rbuf + sizeof(frame_head_t) + 20), 4);
	memcpy(&package_index, (rbuf + sizeof(frame_head_t) + 24), 4);

	/*
	 *    Process Software Update
	 */

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

int CMA_ManualCapture_Response(int fd, byte *rbuf)
{
//	frame_head_t *p_head = (frame_head_t *)rbuf;
	byte channel = *(rbuf + sizeof(frame_head_t));
	byte presetting = *(rbuf + sizeof(frame_head_t) + 1);

	/*
	 *  Set Camera Parameters
	 */
	if (Camera_StartCapture(channel, presetting) < 0)
		return -1;

	return 0;
}

int CMA_Image_SendRequest(int fd, char *id)
{
	frame_head_t f_head;
	Send_image_req_t req;

	if (strlen(id) != 17) {
		printf("Invalid Device ID.\n");
		return -1;
	}
	memset(&f_head, 0, sizeof(frame_head_t));
	memset(&req, 0, sizeof(Send_image_req_t));

	f_head.head = 0x5aa5;
	f_head.pack_len = sizeof(Send_image_req_t);
	f_head.frame_type = CMA_FRAME_TYPE_IMAGE;
	f_head.msg_type = CMA_MSG_TYPE_IMAGE_SENDIMG_REQ;
	memcpy(f_head.id, id, 17);

	/*
	 *  Capture an image from camera(channel, position),
	 *  return Package Number, Image Data
	 */

	if (Commu_SendPacket(fd, &f_head, (byte *)&req) < 0)
		return -1;

	return 0;
}

int CMA_Image_SendData(int fd, char *id)
{
	frame_head_t f_head;
	byte channel = 0;
	byte pre_setting = 255;
	usint package_num;
	usint subpack_index;
	byte *sub_data = NULL;
	byte sbuf[MAX_DATA_BUFSIZE];
	int len = 0;

	if (strlen(id) != 17) {
		printf("Invalid Device ID.\n");
		return -1;
	}
	memset(&f_head, 0, sizeof(frame_head_t));

	f_head.head = 0x5aa5;
	f_head.frame_type = CMA_FRAME_TYPE_IMAGE;
	f_head.msg_type = CMA_MSG_TYPE_IMAGE_DATA;
	memcpy(f_head.id, id, 17);

	/*
	 *  Get Image Data, sub_data, len
	 */

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = channel;
	sbuf[1] = pre_setting;
	memcpy((sbuf + 2), &package_num, 2);
	memcpy((sbuf + 4), &subpack_index, 2);
	memcpy((sbuf + 6), sub_data, len);

	f_head.pack_len = 6 + len;

	if (Commu_SendPacket(fd, &f_head, sbuf) < 0)
		return -1;

	return 0;
}

int CMA_Image_SendData_End(int fd, char *id)
{
	frame_head_t f_head;
	byte channel = 0;
	byte pre_setting = 255;
	byte sbuf[MAX_DATA_BUFSIZE];
	int cur_time = time((time_t*)NULL);

	if (strlen(id) != 17) {
		printf("Invalid Device ID.\n");
		return -1;
	}
	memset(&f_head, 0, sizeof(frame_head_t));

	f_head.head = 0x5aa5;
	f_head.frame_type = CMA_FRAME_TYPE_IMAGE;
	f_head.msg_type = CMA_MSG_TYPE_IMAGE_DATA;
	memcpy(f_head.id, id, 17);

	/*
	 *  Get Image Data, sub_data, len
	 */

	memset(sbuf, 0, MAX_DATA_BUFSIZE);
	sbuf[0] = channel;
	sbuf[1] = pre_setting;
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
//	byte pre_setting = *(rbuf + sizeof(frame_head_t) + 1);
	byte action = *(rbuf + sizeof(frame_head_t) + 2);

	/*
	 *  Set Camera Parameters
	 */
	if (Camera_Control(action, channel) < 0)
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


int CMA_Send_BasicInfo(int fd, char *id)
{
	frame_head_t f_head;
	status_basic_info_t dev;

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

	if (Commu_SendPacket(fd, &f_head, (byte *)&dev) < 0)
		return -1;

	return 0;
}

int CMA_Send_WorkStatus(int fd, char *id)
{
	frame_head_t f_head;
	status_working_t status;

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

	return 0;
}

int CMA_Send_Fault_Info(int fd, char *id, char *fault_desc)
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
	memcpy((sbuf + 4), fault_desc, strlen(fault_desc));

	f_head.pack_len = 4 + strlen(fault_desc);

	if (Commu_SendPacket(fd, &f_head, sbuf) < 0)
		return -1;

	return 0;
}
