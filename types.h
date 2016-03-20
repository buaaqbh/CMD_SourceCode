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

#include "logcat.h"

#define CMA_NEW_TEST_PLATFORM			1
//#define CMD_SENSOR_AUTO_DETECT			1

#define CMA_FRAME_TYPE_DATA				0x01
#define CMA_FRAME_TYPE_DATA_RES			0x02
#define CMA_FRAME_TYPE_CONTROL			0x03
#define CMA_FRAME_TYPE_CONTROL_RES		0x04
#define CMA_FRAME_TYPE_IMAGE_DATA		0x05
#define CMA_FRAME_TYPE_IMAGE_DATA_RES	0x06
#define CMA_FRAME_TYPE_IMAGE_CTRL		0x07
#define CMA_FRAME_TYPE_IMAGE_CTRL_RES	0x08
#define CMA_FRAME_TYPE_STATUS			0x09
#define CMA_FRAME_TYPE_STATUS_RES		0x0a

#define CMA_MSG_TYPE_DATA_QXENV			0x01
#define CMA_MSG_TYPE_DATA_TGQXIE		0x02
#define CMA_MSG_TYPE_DATA_DDXWFTZ		0x03
#define CMA_MSG_TYPE_DATA_DDXWFBX		0x04
#define CMA_MSG_TYPE_DATA_DXHCH			0x05
#define CMA_MSG_TYPE_DATA_DXWD			0x06
#define CMA_MSG_TYPE_DATA_FUBING		0x07
#define CMA_MSG_TYPE_DATA_DXFP			0x08
#define CMA_MSG_TYPE_DATA_DXWDTZH		0x09
#define CMA_MSG_TYPE_DATA_DXWDGJ		0x0a
#define CMA_MSG_TYPE_DATA_XCHWS			0x0b

#define CMA_MSG_TYPE_CTL_TIME_AD		0xA1
#define CMA_MSG_TYPE_CTL_REQ_DATA		0xA2
#define CMA_MSG_TYPE_CTL_CY_PAR			0xA3
#define CMA_MSG_TYPE_CTL_TOCMA_INFO		0xA4
#define CMA_MSG_TYPE_CTL_DEV_ID			0xA5
#define CMA_MSG_TYPE_CTL_DEV_RESET		0xA6
#define CMA_MSG_TYPE_CTL_MODEL_PAR		0xA7

#define CMA_MSG_TYPE_IMAGE_CAP_PAR		0xB1
#define CMA_MSG_TYPE_IMAGE_CAP_TIME		0xB2
#define CMA_MSG_TYPE_IMAGE_CAP_MANUAL	0xB3
#define CMA_MSG_TYPE_IMAGE_SENDIMG_REQ	0xB4
#define CMA_MSG_TYPE_IMAGE_DATA			0xB5
#define CMA_MSG_TYPE_IMAGE_DATA_END		0xB6
#define CMA_MSG_TYPE_IMAGE_DATA_REP		0xB7
#define CMA_MSG_TYPE_IMAGE_CAM_ADJ		0xB8

#define CMA_MSG_TYPE_STATUS_HEART		0xC1
#define CMA_MSG_TYPE_STATUS_ERROR		0xC2

#define CAMERA_ACTION_CALLPRESET		1
#define CAMERA_ACTION_MOVEUP			2
#define CAMERA_ACTION_MOVEDOWN			3
#define CAMERA_ACTION_MOVELEFT			4
#define CAMERA_ACTION_MOVERIGHT			5
#define CAMERA_ACTION_FOCUSFAR			6
#define CAMERA_ACTION_FOCUSNERA			7
#define CAMERA_ACTION_SETPRESET			8


#define 		CMA_MAX_SENSOR_NUM	20
unsigned char 	Sensor_L1_type[CMA_MAX_SENSOR_NUM];

extern char *config_file;

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
	char	org_id[18];
	int 	l2_type;
	int 	local_port;
	char 	cma_ip[32];
	usint 	cma_port;
	char	cma_domain[64];
	int 	s_protocal;
	int 	socket_fd;
	float	temp;
	int		sensor_type;
	char 	imei[20];
	int 	heart_cycle;
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
	byte 	index;
} frame_head_t;

typedef struct _data_str_qixiang
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
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
} Data_qixiang_t;

typedef struct _data_str_incline
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	float 	Inclination;
	float 	Inclination_X;
	float 	Inclination_Y;
	float 	Angle_X;
	float 	Angle_Y;
} Data_incline_t;

typedef struct _data_str_vibration_f
{
	byte 	Component_ID[17];
	byte	Unit_Sum;
	byte	Unit_Num;
	int 	Time_Stamp;
	usint	Strain_Amplitude;
	float	Bending_Amplitude;
	float	Vibration_Frequence;
} Data_vibration_f_t;

typedef struct _data_str_vibration_w
{
	byte 	Component_ID[17];
	byte	Unit_Sum;
	byte	Unit_Num;
	int 	Time_Stamp;
	byte 	SamplePack_Sum;
	byte	SamplePack_No;
	usint 	*Strain_Data;
	int 	n;
} Data_vibration_w_t;

typedef struct _data_str_conductor_sag
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	float 	Conductor_Sag;
	float 	Toground_Distance;
	float 	Angle;
	byte 	Measure_Flag;
} Data_conductor_sag_t;

typedef struct _data_str_line_temperature
{
	byte 	Component_ID[17];
	byte	Unit_Sum;
	byte	Unit_Num;
	int 	Time_Stamp;
	int 	Line_Temperature;
} Data_line_temperature_t;

typedef struct _data_str_ice_thickness
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	float 	Equal_IceThickness;
	float 	Tension;
	float 	Tension_Difference;
	float 	Windage_Yaw_Angle;
	float 	Deflection_Angle;
	byte	T_Sensor_Num;
	float	*Original_data;
} Data_ice_thickness_t;

typedef struct _data_str_windage_yaw
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	int 	Windage_Yaw_Angle;
	int 	Deflection_Angle;
	int 	Least_Clearance;
} Data_windage_yaw_t;

typedef struct _data_str_line_gallop_f
{
	byte 	Component_ID[17];
	byte	Unit_Sum;
	byte	Unit_Num;
	int 	Time_Stamp;
	int 	U_Gallop_Amplitude;
	int 	U_Vertical_Amplitude;
	int 	U_Horizontal_Amplitude;
	int 	U_AngleToVertica;
	int 	U_Gallop_Frequency;
} Data_line_gallop_f_t;

typedef struct _data_str_gallop_w
{
	byte 	Component_ID[17];
	byte	Unit_Sum;
	byte	Unit_Num;
	int 	Time_Stamp;
	byte 	SamplePack_Sum;
	byte 	SamplePack_No;
	byte 	*Displacement;
	int 	n;
} Data_gallop_w_t;

typedef struct _data_str_dirty
{
	byte 	Component_ID[17];
	int 	Time_Stamp;
	int 	ESDD;
	int 	NSDD;
	int 	Daily_Max_Temperature;
	int 	Daily_Min_Temperature;
	usint 	Daily_Max_Humidity;
	usint 	Daily_Min_Humidity;
} Data_dirty_t;

typedef struct _ctl_response_net_adapter
{
	unsigned long 	IP;
	unsigned long 	Subnet_mask;
	unsigned long 	Gateway;
	byte			PhoneNumber[20];
} Ctl_net_adap_t;

typedef struct _ctl_response_sample
{
	byte	Request_flag;
	byte 	Request_Type;
	usint 	Main_Time;
	byte 	Heartbeat_Time;
} Ctl_sample_par_t;

typedef struct _ctl_response_up_device
{
	byte 	Request_flag;
	unsigned long IP_Address;
	usint 	Port;
} Ctl_up_device_t;

typedef struct _ctl_response_image_par
{
	byte 	Color_Select;
	byte 	Resolution;
	byte 	Luminance;
	byte 	Contrast;
	byte 	Saturation;
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
	uint	Send_Flow;
	uint	Receive_Flow;
	uint	Protocal_Version;
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

#define  UART_PORT_RS485_SENSOR		"/dev/ttymxc4"
#define  UART_PORT_RS485_CAMERA		"/dev/ttymxc3"
#define  UART_RS485_SPEDD 	9600

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
