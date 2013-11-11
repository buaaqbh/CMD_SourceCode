/*
 * v4l2_lib.h
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

#ifndef V4L2_LIB_H
#define V4L2_LIB_H
#ifdef __cplusplus
extern "C" {
#endif

#include <linux/videodev2.h>

int		v4l2_open_device (char *dev_name);
void 	v4l2_close_device (int fd);
int 	v4l2_init_device (int fd, struct v4l2_pix_format pix, int lu, int co, int sa);
void 	v4l2_uninit_device (void);
int 	v4l2_start_capturing (int fd);
int 	v4l2_stop_capturing (int fd);
int 	v4l2_read_frame (int fd, int width, int height, char *jpegFilename);
int 	v4l2_capture_image (char *jpegFilename, int width, int height, int lu, int co, int sa);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* V4L2_LIB_H */
