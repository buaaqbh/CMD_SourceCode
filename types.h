/*
 * types.h
 * 
 * Copyright 2013 qinbh <buaaqbh@gmail.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301  USA.
 * 
 */

#ifndef CMA_TYPES_H
#define CMA_TYPES_H
#ifdef __cplusplus
extern "C" {
#endif


#define CMA_NEW_TEST_PLATFORM			1
//#define CMD_SENSOR_AUTO_DETECT			1

#define CMA_FRAME_TYPE_DATA				0x01
#define CMA_FRAME_TYPE_DATA_RES			0x02
#define CMA_FRAME_TYPE_CONTROL			0x03
#define CMA_FRAME_TYPE_CONTROL_RES		0x04
#define CMA_FRAME_TYPE_IMAGE			0x05
#define CMA_FRAME_TYPE_IMAGE_CTRL		0x06
#define CMA_FRAME_TYPE_STATUS			0x07
#define CMA_FRAME_TYPE_STATUS_RES		0x08
#define CMA_FRAME_TYPE_SYNC_DATA		0x09

#define CMA_MSG_TYPE_DATA_QXENV			0x01
#define CMA_MSG_TYPE_DATA_TGQXIE		0x0c
#define CMA_MSG_TYPE_DATA_DDXWFTZ		0x1e
#define CMA_MSG_TYPE_DATA_DDXWFBX		0x1f
#define CMA_MSG_TYPE_DATA_DXHCH			0x20
#define CMA_MSG_TYPE_DATA_DXWD			0x21
#define CMA_MSG_TYPE_DATA_FUBING		0x22
#define CMA_MSG_TYPE_DATA_DXFP			0x23
#define CMA_MSG_TYPE_DATA_DXWDTZH		0x24
#define CMA_MSG_TYPE_DATA_DXWDGJ		0x25
#define CMA_MSG_TYPE_DATA_XCHWS			0x5c

#define CMA_MSG_TYPE_CTL_TIME_CS		0xA1
#define CMA_MSG_TYPE_CTL_TIME_AD		0xA2
#define CMA_MSG_TYPE_CTL_REQ_DATA		0xA3
#define CMA_MSG_TYPE_CTL_CY_PAR			0xA4
#define CMA_MSG_TYPE_CTL_MODEL_PAR		0xA5
#define CMA_MSG_TYPE_CTL_ALARM			0xA6
#define CMA_MSG_TYPE_CTL_TOCMA_INFO		0xA7
#define CMA_MSG_TYPE_CTL_BASIC_INFO		0xA8
#define CMA_MSG_TYPE_CTL_UPGRADE_DATA	0xA9
#define CMA_MSG_TYPE_CTL_UPGRADE_END	0xAA
#define CMA_MSG_TYPE_CTL_UPGRADE_REP	0xAB
#define CMA_MSG_TYPE_CTL_DEV_ID			0xAC
#define CMA_MSG_TYPE_CTL_DEV_RESET		0xAD
#define CMA_MSG_TYPE_CTL_DEV_WAKE		0xAE
#define CMA_MSG_TYPE_CTL_QX_PAR			0xAF
#define CMA_MSG_TYPE_CTL_TGQX_PAR		0xB0
#define CMA_MSG_TYPE_CTL_DDXWFZD_PAR	0xB1
#define CMA_MSG_TYPE_CTL_DXHCH_PAR		0xB2
#define CMA_MSG_TYPE_CTL_DXWD_PAR		0xB3
#define CMA_MSG_TYPE_CTL_FUBING_PAR		0xB4
#define CMA_MSG_TYPE_CTL_DXFP_PAR		0xB5
#define CMA_MSG_TYPE_CTL_DDXWD_PAR		0xB6
#define CMA_MSG_TYPE_CTL_XCHWS_PAR		0xB7

#define CMA_MSG_TYPE_IMAGE_CAP_PAR		0xC9
#define CMA_MSG_TYPE_IMAGE_CAP_TIME		0xCA
#define CMA_MSG_TYPE_IMAGE_CAP_MANUAL	0xCB
#define CMA_MSG_TYPE_IMAGE_SENDIMG_REQ	0xCC
#define CMA_MSG_TYPE_IMAGE_DATA			0xCD
#define CMA_MSG_TYPE_IMAGE_DATA_END		0xCE
#define CMA_MSG_TYPE_IMAGE_DATA_REP		0xCF
#define CMA_MSG_TYPE_IMAGE_CAM_ADJ		0xD0
#define CMA_MSG_TYPE_IMAGE_VIDEO_SET	0xD1
#define CMA_MSG_TYPE_IMAGE_DEV_SERVER	0xD2
#define CMA_MSG_TYPE_IMAGE_CONNECT_STOP	0xD3
#define CMA_MSG_TYPE_IMAGE_BASIC_INFO	0xD4
#define CMA_MSG_TYPE_IMAGE_GET_PAR		0xD5
#define CMA_MSG_TYPE_IMAGE_GET_TIME		0xD6

#define CMA_MSG_TYPE_STATUS_HEART		0xE6
#define CMA_MSG_TYPE_STATUS_INFO		0xE7
#define CMA_MSG_TYPE_STATUS_WORK		0xE8
#define CMA_MSG_TYPE_STATUS_ERROR		0xE9

#define CAMERA_ACTION_POWERON			1
#define CAMERA_ACTION_CALLPRESET		2
#define CAMERA_ACTION_MOVEUP			3
#define CAMERA_ACTION_MOVEDOWN			4
#define CAMERA_ACTION_MOVELEFT			5
#define CAMERA_ACTION_MOVERIGHT			6
#define CAMERA_ACTION_FOCUSFAR			7
#define CAMERA_ACTION_FOCUSNERA			8
#define CAMERA_ACTION_SETPRESET			9
#define CAMERA_ACTION_POWEROFF			10


#define 		CMA_MAX_SENSOR_NUM	20
extern unsigned int Sensor_Online_flag;
unsigned char 	Sensor_L1_type[CMA_MAX_SENSOR_NUM];

extern char *config_file;
extern volatile int CMD_Response_data;

typedef unsigned long 	ulint;
typedef long 			lint;
typedef unsigned short 	usint;
typedef short 			sint;
typedef short 			boolean;
typedef unsigned char 	byte;

#define CMA_MSG_MAX_LEN		1024
#define MAX_COMBUF_SIZE		2048
#define MAX_DATA_BUFSIZE 	256

typedef struct env_data {
	char 	id[18];
	char	c_id[18];
	usint	org_id;
	int 	l2_type;
	int 	local_port;
	char 	cma_ip[32];
	usint 	cma_port;
	char	cma_domain[64];
	int 	s_protocal;
	int 	socket_fd;
	float	temp;
} env_data_t;

extern env_data_t CMA_Env_Parameter;

#pragma pack(1) 

typedef struct _frame_str_head
{
	usint 	head;
	usint 	pack_len;
	byte 	id[17];
	byte 	frame_type;
	byte 	msg_type;
} frame_head_t;

typedef struct _data_str_qixiang
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	float 	Average_WindSpeed_10min;
	usint 	Average_WindDirection_10min;
	float 	Max_WindSpeed;
	float 	Extreme_WindSpeed;
	float 	Standard_WindSpeed;
	float 	Air_Temperature;
	usint 	Humidity;
	float 	Air_Pressure;
	float 	Precipitation;
	float 	Precipitation_Intensity;
	int 	Radiation_Intensity;
	int 	Reserve1;
	int 	Reserve2;
} Data_qixiang_t;

typedef struct _data_str_incline
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	float 	Inclination;
	float 	Inclination_X;
	float 	Inclination_Y;
	float 	Angle_X;
	float 	Angle_Y;
	int 	Reserve1;
	int 	Reserve2;
} Data_incline_t;

typedef struct _data_str_vibration_f
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	usint 	Vibration_Amplitude;
	int 	Vibration_Frequency;
	int 	Reserve1;
	int 	Reserve2;
} Data_vibration_f_t;

typedef struct _data_str_vibration_w
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	byte 	SamplePack_Sum;
	byte	SamplePack_No;
	usint 	*Strain_Data;
	int 	n;
} Data_vibration_w_t;

typedef struct _data_str_conductor_sag
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	int 	Conductor_Sag;
	int 	Toground_Distance;
	int 	Angle;
	byte 	Measure_Flag;
	int 	Reserve1;
	int 	Reserve2;
} Data_conductor_sag_t;

typedef struct _data_str_line_temperature
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	int 	Line_Temperature1;
	int 	Line_Temperature2;
	int 	Reserve1;
	int 	Reserve2;
} Data_line_temperature_t;

typedef struct _data_str_ice_thickness
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	float 	Equal_IceThickness;
	float 	Tension;
	float 	Tension_Difference;
	float 	Windage_Yaw_Angle;
	float 	Deflection_Angle;
	int 	Reserve1;
	int 	Reserve2;
} Data_ice_thickness_t;

typedef struct _data_str_windage_yaw
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	int 	Windage_Yaw_Angle;
	int 	Deflection_Angle;
	int 	Least_Clearance;
	int 	Reserve1;
	int 	Reserve2;
} Data_windage_yaw_t;

typedef struct _data_str_line_gallop_f
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	int 	U_Gallop_Amplitude;
	int 	U_Vertical_Amplitude;
	int 	U_Horizontal_Amplitude;
	int 	U_AngleToVertica;
	int 	U_Gallop_Frequency;
	int 	Reserve1;
	int 	Reserve2;
} Data_line_gallop_f_t;

typedef struct _data_str_gallop_w
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	byte 	SamplePack_Sum;
	byte 	SamplePack_No;
	byte 	*Displacement;
	int 	n;
} Data_gallop_w_t;

typedef struct _data_str_dirty
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	usint 	Alerm_Flag;
	int 	ESDD;
	int 	NSDD;
	int 	Daily_Max_Temperature;
	int 	Daily_Min_Temperature;
	usint 	Daily_Max_Humidity;
	usint 	Daily_Min_Humidity;
	int 	Reserve1;
	int 	Reserve2;
} Data_dirty_t;

typedef struct _ctl_response_net_adapter
{
	unsigned long 	IP;
	unsigned long 	Subnet_mask;
	unsigned long 	Gateway;
	unsigned long 	DNS_Server;
	byte 	reserve[16];
} Ctl_net_adap_t;

typedef struct _ctl_response_sample
{
	byte 	Request_Type;
	usint 	Main_Time;
	usint 	Sample_Count;
	usint 	Sample_Frequency;
	byte 	reserve[4];
} Ctl_sample_par_t;

typedef struct _ctl_response_up_device
{
	unsigned long IP_Address;
	usint 	Port;
	byte 	Domain_Name[64];
	byte 	reserve[12];
} Ctl_up_device_t;

typedef struct _ctl_response_image_par
{
	byte 	Color_Select;
	byte 	Resolution;
	byte 	Luminance;
	byte 	Contrast;
	byte 	Saturation;
	byte 	Reserve[8];
} Ctl_image_device_t;

typedef struct _ctl_image_timetable
{
	byte 	Hour;
	byte 	Minute;
	byte 	Presetting_No;
} Ctl_image_timetable_t;

typedef struct _send_image_req
{
	byte 	Channel_No;
	byte 	Presetting_No;
	usint 	Packet_Num;
	byte 	reserve[8];
} Send_image_req_t;

typedef struct _status_str_basic_info
{
	byte	SmartEquip_Name[50];
	byte	Model[10];
	byte	Essential_Info_Version[4];
	byte	Bs_Manufacturer[50];
	int		Bs_Production_Date;
	byte	Bs_Identifier[20];
	byte	reserve[30];
} status_basic_info_t;

typedef struct _status_str_working
{
	int 	Time_Stamp;
	float 	Battery_Voltage;
	float 	Operation_Temperature;
	float 	Battery_Capacity;
	byte 	FloatingCharge;
	float 	Total_Working_Time;
	float 	Working_Time;
	byte 	Connection_State;
	byte	reserve[30];
} status_working_t;

typedef struct _alarm_value {
	int type;
	byte alarm_par[6];
	int  alarm_value;
} alarm_value_t;


#pragma pack() 

#define FILE_IMAGECAPTURE_PAR 	"/CMD_Data/.image_capture_par.cfg"
#define FILE_IMAGE_TIMETABLE0 	"/CMD_Data/.image_capture_timetable0.cfg"
#define FILE_IMAGE_TIMETABLE1 	"/CMD_Data/.image_capture_timetable1.cfg"

#define FILE_ALARM_PAR 			"/CMD_Data/.sensor_alarm_par.cfg"

#define RECORD_FILE_QIXIANG 	"/CMD_Data/record_qixiang.dat"
#define RECORD_FILE_TGQXIE 		"/CMD_Data/record_tgqxie.dat"
#define RECORD_FILE_FUBING 		"/CMD_Data/record_fubing.dat"

struct record_qixiang {
	time_t tm;
	Data_qixiang_t data;
	int send_flag;
};

struct record_incline {
	time_t tm;
	Data_incline_t data;
	int send_flag;
};

struct record_fubing {
	time_t tm;
	Data_ice_thickness_t data;
	int send_flag;
};


extern unsigned short RTU_CRC(unsigned char *puchMsg, unsigned short usDataLen);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* CMA_TYPES_H */
