/*
 * camera_control.h
 *
 *  Created on: 2013年10月28日
 *      Author: qinbh
 */

#ifndef CAMERA_CONTROL_H_
#define CAMERA_CONTROL_H_

#include "types.h"

void Camera_PowerOn(byte addr);
void Camera_PowerOff(byte addr);
void Camera_CallPreset(byte addr, byte index);
void Camera_SetPreset(byte addr, byte index);
void Camera_DelPreset(byte addr, byte index);
void Camera_MoveLeft(byte addr);
void Camera_MoveRight(byte addr);
void Camera_MoveUp(byte addr);
void Camera_MoveDown(byte addr);
void Camera_FocusFar(byte addr);
void Camera_FocusNear(byte addr);
void Camera_CmdStop(byte addr);

#endif /* CAMERA_CONTROL_H_ */
