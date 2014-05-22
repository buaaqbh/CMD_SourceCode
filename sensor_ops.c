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
#include "camera_control.h"
#include "rtc_alarm.h"
#include "file_ops.h"
#include "io_util.h"
#include "list.h"
#include "v4l2_lib.h"
#include "cma_commu.h"
#include "device.h"

struct rtc_alarm_dev camera_dev;

pthread_mutex_t can_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rs485_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile unsigned int sensor_status = 0xff;
volatile unsigned int sensor_status_pre = 0xff;
static volatile int zigbee_fault_count = 0;

volatile unsigned int data_qixiang_flag = 0;

int Sensor_Get_AlarmValue(byte type, byte index, void *value)
{
	int i;
	int total = 0;
	int record_len = sizeof(alarm_value_t);
	alarm_value_t record;

//	logcat("Sensor_Get_AlarmValue: type = 0x%x, index = %d \n", type, index);
	total = File_GetNumberOfRecords(FILE_ALARM_PAR, record_len);
	if (total == 0) {
		return -1;
	}

	for (i = 0; i < total; i++) {
		memset(&record, 0, record_len);
		if (File_GetRecordByIndex(FILE_ALARM_PAR, &record, record_len, i) == record_len) {
			if ((record.type == type) && (record.alarm_par[0] == index)) {
				memcpy(value, &record.alarm_value, 4);
				return 0;
			}
		}

	}

	return -1;
}

int sample_avg(int *data, int size)
{
	int i;
	int max, min, total;
	float avg = 0.0;
	if ((data == NULL) | (size <= 2))
		return 0;

	max = min = total = data[0];
	for (i = 1; i < size; i++) {
		total += data[i];
		if (data[i] > max)
			max = data[i];
		if (data[i] < min)
			min = data[i];
	}

//	logcat("total = %d, max = %d, min = %d\n", total, max, min);
	avg = (double)(total - max - min) / (double)(size - 2);

	return (int)(avg + 0.5);
}

void *sensor_qixiang_zigbee(void * arg)
{
	byte buf[16];
	float f_threshold = 0.0;
	Data_qixiang_t *pdata = (Data_qixiang_t *)arg;

	logcat("Sensor: Get data from ZIGBEE device.\n");

	memset(buf, 0, 16);
	if (Sensor_Zigbee_ReadData(buf, 13) == 0) {
		pdata->Precipitation = (buf[4] << 8) | buf[5];
		pdata->Precipitation_Intensity = (buf[6] << 8) | buf[7];
		data_qixiang_flag++;

		if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 9, &f_threshold) == 0) {
			if (pdata->Precipitation > f_threshold)
				pdata->Alerm_Flag |= (1 << 8);
		}

		if (Sensor_Get_AlarmValue(CMA_MSG_TYPE_CTL_QX_PAR, 10, &f_threshold) == 0) {
			if (pdata->Precipitation_Intensity > f_threshold)
				pdata->Alerm_Flag |= (1 << 9);
		}

		sensor_status |= (1 << 3);
	}
	else {
		logcat("Sensor: read ZIGBEE data error.\n");
	}

	return 0;
}

int Sensor_Sample_Qixiang(Data_qixiang_t *data)
{
	int ret = -1;

	if (CMA_Env_Parameter.sensor_type == 1) {
		Device_power_ctl(DEVICE_CAN_12V, 1);
		sleep(8);

		ret = CAN_Sample_Qixiang(data);

		Device_power_ctl(DEVICE_CAN_12V, 0);
	}
	else {
		Device_power_ctl(DEVICE_RS485_CHIP, 1);
		Device_power_ctl(DEVICE_RS485, 1);
		sleep(8);

		ret = RS485_Sample_Qixiang(data);

		Device_power_ctl(DEVICE_RS485, 0);
		Device_power_ctl(DEVICE_RS485_CHIP, 0);
	}

	return ret;
}

int Sensor_Sample_TGQingXie(Data_incline_t *data)
{
	int ret = -1;

	if (CMA_Env_Parameter.sensor_type == 1) {
		Device_power_ctl(DEVICE_CAN_12V, 1);
		sleep(8);

		ret = CAN_Sample_TGQingXie(data);

		Device_power_ctl(DEVICE_CAN_12V, 0);
	}
	else {
		Device_power_ctl(DEVICE_RS485_CHIP, 1);
		Device_power_ctl(DEVICE_RS485, 1);
		sleep(8);

		ret = RS485_Sample_TGQingXie(data);

		Device_power_ctl(DEVICE_RS485, 0);
		Device_power_ctl(DEVICE_RS485_CHIP, 0);
	}

	return ret;
}

int Sensor_Sample_FuBing(Data_ice_thickness_t *data)
{
	int ret = -1;

	if (CMA_Env_Parameter.sensor_type == 1) {
		Device_power_ctl(DEVICE_CAN_12V, 1);
		sleep(8);

		ret = CAN_Sample_FuBing(data);

		Device_power_ctl(DEVICE_CAN_12V, 0);
	}
	else {
		Device_power_ctl(DEVICE_RS485_CHIP, 1);
		Device_power_ctl(DEVICE_RS485, 1);
		sleep(8);

		ret = RS485_Sample_FuBing(data);

		Device_power_ctl(DEVICE_RS485, 0);
		Device_power_ctl(DEVICE_RS485_CHIP, 0);
	}

	return ret;
}

int Sensor_GetData(byte *buf, int type)
{
	byte sensor_buf[MAX_DATA_BUFSIZE];
	int ret = 0;
	
	logcat("Enter func: %s, type = %d \n", __func__, type);

	memset(sensor_buf, 0, MAX_DATA_BUFSIZE);
	
	switch (type) {
	case CMA_MSG_TYPE_DATA_QXENV:
		{
			Data_qixiang_t *data = (Data_qixiang_t *)sensor_buf;
			ret = Sensor_Sample_Qixiang(data);
			memcpy(buf, data, sizeof(Data_qixiang_t));
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
		logcat("Invalid Sensor tyep.\n");
		ret = -1;
		break;
	}
	
	return ret;
}

static char Sensor_Fault_0[] = {0xff, 0x05, 0x02, 0x06, 0x02, 0x0b, 0x02};
static char Sensor_Fault[][3] = {
		{0xff, 0x05, 0x02}, /* Temperature Sensor */
		{0xff, 0x07, 0x02}, /* Wind Speed Sensor */
		{0xff, 0x08, 0x02}, /* Wind Direction Sensor */
		{0xff, 0x09, 0x02}, /* Rain force Sensor */
		{0xff, 0x0a, 0x02}, /* Radiation Sensor */
		{0xff, 0x60, 0x02}, /* TGQX Angle Sensor */
		{0xff, 0x10, 0x02}, /* Tension Sensor */
		{0xff, 0x04, 0x02}, /* Camera Sensor */
};

int Sensor_FaultStatus(void)
{
	int i = 0, fault = 0;
	unsigned int status = sensor_status_pre ^ sensor_status;
	char *p = NULL;
	int len = 0;

//	status = status & sensor_status_pre;
	status &= 0xff;
	logcat("Func: %s, sensor_status = 0x%08x, sensor_status_pre = 0x%08x\n", __func__, sensor_status, sensor_status_pre);
	logcat("Func: %s, status = 0x%08x\n", __func__, status);
	for (i = 0; i < 8; i++) {
		fault = (status >> i) & 0x01;
		if (fault) {
			logcat("Fault Message: i = %d \n", i);
			if (i == 0) {
				p = Sensor_Fault_0;
				len = 7;
			}
			else {
				p = Sensor_Fault[i];
				len = 3;
			}

			p[0] = ((sensor_status >> i) & 0x01) ? 0x00 : 0xff;
			if (i == 3) {
				if (p[0] == 0xff)
					zigbee_fault_count++;
				else
					zigbee_fault_count = 0;
				if (zigbee_fault_count < 5)
					continue;
			}
			if (CMA_Send_Fault_Info(CMA_Env_Parameter.socket_fd, CMA_Env_Parameter.id, p, len) < 0)
				logcat("CMD: Send Fault Message error.\n");
		}
	}

	sensor_status_pre = sensor_status;

	return 0;
}

int Camera_GetImages(char *ImageName, byte presetting, byte channel)
{
	Ctl_image_device_t par;
	char cmdshell[128];
	int ret;

	Camera_PowerOn(CAMERA_DEVICE_ADDR);
	usleep(500 * 1000);
	Camera_CallPreset(CAMERA_DEVICE_ADDR, presetting);

	memset(&par, 0, sizeof(Ctl_image_device_t));
	if (Camera_GetParameter(&par) < 0) {
		logcat("CMD: Can't read image parameters, use default.\n");
		par.Color_Select = 1;
		par.Resolution = 2;
		par.Luminance = 50;
		par.Contrast = 50;
		par.Saturation = 50;
	}

	ret = v4l2_capture_image (ImageName, 720, 576, par.Luminance, par.Contrast, par.Saturation);
	Camera_PowerOff(CAMERA_DEVICE_ADDR);
	if (ret < 0) {
		sensor_status &= (~(1 << 7));
		logcat("CMD: Capture an Image error.\n");
		return -1;
	}
	else
		sensor_status |= (1 << 7);

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

	logcat("Enter func: %s ------\n", __func__);

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

	logcat("Enter func: %s ------\n", __func__);

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

int Camera_GetTimeTable(byte *buf, int *num)
{
	int fd;
	int len = sizeof(Ctl_image_timetable_t);
	int ret = 0;
	char *recordFile = FILE_IMAGE_TIMETABLE0;
	int count = 0;

	logcat("Enter func: %s ------\n", __func__);

	if (File_Exist(recordFile) == 0)
		return -1;

	if (buf == NULL)
		return -1;

	count = File_GetNumberOfRecords(recordFile, len);
	if (count == 0) {
		*num = 0;
		return 0;
	}

	*num = count;

	fd = File_Open(recordFile);
	if (fd < 0)
		return -1;

	ret = read(fd, buf, len * count);
//	if (read(fd, buf, len * count) != (len * count))
//		ret = -1;

//	logcat("len = %d, count = %d\n", len, count);

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
	logcat("Enter func: %s ------\n", __func__);

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

	logcat("Enter func: %s ------\n", __func__);

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
			logcat("CMD: Read Camera Caputer timetable error.\n");
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
	logcat("Camera Capture: Now, %s", asctime(tm));
	tm = localtime(&expect);
	logcat("Camera Capture: Expect, %s", asctime(tm));

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

	logcat("Enter func: %s ------\n", __func__);

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
		logcat("file mmap error.\n");
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
				logcat("Never reach here, if here, wrong.\n");
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
		logcat("file munmap error.\n");
	}

	File_Close(fd);

	Camera_NextTimer();

	return ret;
}

static char *Image_folder = "/CMD_Data/images/";

static void Camera_ImageClear(void)
{
	char *folder = Image_folder;
	char cmd[256] = { 0 };
	time_t now;
	struct tm *tm;

	now = time((time_t*)NULL);
	now = now - 60 * 60 * 24 * 60; /* 60 days ago */
	if (now < 0) {
		logcat("ImageClear: date error.\n");
		return;
	}
	tm = localtime(&now);

	memset(cmd, 0, 256);
	sprintf(cmd, "rm -rf %simages-%d%02d%02d*", folder, (tm->tm_year + 1900), (tm->tm_mon + 1), tm->tm_mday);
	logcat("ImageClear: command shell = %s \n", cmd);
	logcat("ImageClear: Clear Date %s", asctime(tm));
	system(cmd);

	return;
}

int Camera_GetImageName(char *filename, byte channel, byte presetting)
{
	char *folder = Image_folder;
	char cmd[256] = { 0 };
	time_t now;
	struct tm *tm;

	if (access(folder, F_OK) < 0) {
		logcat("CMD: Image Folder %s does't exist.\n", folder);
		sprintf(cmd, "mkdir -p %s", folder);
		system(cmd);
	}

	if (filename == NULL)
		return -1;

//	now = rtc_get_time();
	now = time((time_t*)NULL);
	tm = localtime(&now);

	memset(filename, 0, 256);
	sprintf(filename, "%simages-%d%02d%02d%02d%02d%02d-%d-%d.jpg", folder, (tm->tm_year + 1900), (tm->tm_mon + 1), tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec, channel, presetting);

	Camera_ImageClear();

	return 0;
}

int Camera_StartCapture(char *filename, byte channel, byte presetting)
{
	if (filename == NULL)
		return -1;

	Camera_GetImageName(filename, channel, presetting);

	if (Camera_GetImages(filename, presetting, channel) < 0)
		return -1;

	logcat("Get Images: fileName = %s\n", filename);

	return 0;
}

int Camera_Control(byte action, byte presetting, byte channel)
{
	int ret = 0;

	logcat("Camera Control: action = %d, preSetting = %d\n", action, presetting);

	Device_power_ctl(DEVICE_RS485_CHIP, 1);
	Device_power_ctl(DEVICE_AV, 1);
	sleep(1);

//	pthread_mutex_lock(&rs485_mutex);

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
		logcat("CMD: Invalid camera control action.\n");
		break;
	}

//	pthread_mutex_unlock(&rs485_mutex);

	Device_power_ctl(DEVICE_AV, 0);
	Device_power_ctl(DEVICE_RS485_CHIP, 0);

	return ret;
}

int Camera_Video_Start(byte channel, byte control, usint port)
{
	return 0;
}


