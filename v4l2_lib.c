/*
 **  V4L2 video capture example
 **
 **  This program can be used and distributed without restrictions.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>		/* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>		/* for videodev2.h */
#include <jpeglib.h>
//#include <libv4l2.h>

#include "v4l2_lib.h"

#define CAMERA_DEVICE 	"/dev/video0"

struct buffer {
	void *start;
	size_t length;
};

struct buffer *buffers = NULL;
static unsigned int n_buffers = 0;
static unsigned char jpegQuality = 90;
v4l2_std_id g_current_std = V4L2_STD_PAL;
int g_fmt = V4L2_PIX_FMT_YUYV;
int g_in_width = 0;
int g_in_height = 0;

#define CLEAR(x) memset (&(x), 0, sizeof (x))

static int xioctl (int fd, int request, void *arg)
{
	int r;

	do {
		r = ioctl (fd, request, arg);
//		r = v4l2_ioctl(fd, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

int v4l2_open_device (char *dev_name)
{
	int fd;
	struct stat st;

	if (-1 == stat (dev_name, &st)) {
		fprintf (stderr, "Cannot identify '%s': %d, %s\n", dev_name, errno, strerror (errno));
		return -1;
	}

	if (!S_ISCHR (st.st_mode)) {
		fprintf (stderr, "%s is no device\n", dev_name);
		return -1;
	}

	fd = open (dev_name, O_RDWR /* required */  | O_NONBLOCK, 0);
//	fd = v4l2_open(dev_name, O_RDWR /* required */  | O_NONBLOCK, 0);

	if (-1 == fd) {
		fprintf (stderr, "Cannot open '%s': %d, %s\n", dev_name, errno, strerror (errno));
		return -1;
	}
	
	return fd;
}

void v4l2_close_device (int fd)
{
	if (-1 == close (fd))
//	if (-1 == v4l2_close(fd))
		perror ("close");

	return;
}

static int v4l2_init_mmap (int fd)
{
	struct v4l2_requestbuffers req;

	CLEAR (req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf (stderr, "Video Device does not support memory mapping\n");
			return -1;
		}
        	else {
			perror ("VIDIOC_REQBUFS");
			return -1;
		}
	}

	if (req.count < 2) {
		fprintf (stderr, "Insufficient buffer memory on Video Device\n");
		return -1;
	}

	buffers = calloc (req.count, sizeof (*buffers));

	if (!buffers) {
		fprintf (stderr, "Out of memory\n");
        return -1;
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;
		CLEAR (buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;

		if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf)) {
			perror ("VIDIOC_QUERYBUF");
			return -1;
		}

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start = mmap (NULL /* start anywhere */ ,
//		buffers[n_buffers].start = v4l2_mmap (NULL /* start anywhere */ ,
						buf.length,
						PROT_READ | PROT_WRITE /* required */ ,
						MAP_SHARED /* recommended */ ,
						fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start) {
			perror ("mmap");
			return -1;
		}
	}
	
	return 0;
}

int v4l2_init_device (int fd, struct v4l2_pix_format pix, int lu, int co, int sa)
{
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_dbg_chip_ident chip;
	struct v4l2_streamparm parm;
	v4l2_std_id id;
	unsigned int min;

	int g_input=1;

	if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf (stderr, "VIDIOC_QUERYCAP: Video Device is no V4L2 device\n");
		}
		else {
			perror("VIDIOC_QUERYCAP");
		}
		return -1;
	}
//	fprintf(stdout, "DriverName:%s\nCard Name:%s\nBus info:%s\nDriverVersion:%u.%u.%u\n",
//				cap.driver,cap.card,cap.bus_info,(cap.version>>16)&0xff,(cap.version>>8)&0xff,cap.version&0xff);
//	fprintf(stdout, "Driver Capability: 0x%08x, V4L2_CAP_VIDEO_CAPTURE = 0x%08x, V4L2_CAP_STREAMING = 0x%08x\n",
//			cap.capabilities, V4L2_CAP_VIDEO_CAPTURE, V4L2_CAP_STREAMING);

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf (stderr, "Video Device is no video capture device\n");
		return -1;
	}


	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf (stderr, "Video Device does not support streaming i/o\n");
		return -1;
	}

	if (ioctl(fd, VIDIOC_DBG_G_CHIP_IDENT, &chip)) {
		printf("VIDIOC_DBG_G_CHIP_IDENT failed.\n");
		return -1;
	}
	printf("TV decoder chip is %s\n", chip.match.name);

	if (ioctl(fd, VIDIOC_S_INPUT, &g_input) < 0) {
		printf("VIDIOC_S_INPUT failed\n");
		return -1;
	}

	if (ioctl(fd, VIDIOC_G_STD, &id) < 0) {
		printf("VIDIOC_G_STD failed\n");
		return -1;
	}
//	g_current_std = id;
	id = g_current_std;

	if (ioctl(fd, VIDIOC_S_STD, &id) < 0) {
		printf("VIDIOC_S_STD failed\n");
		return -1;
	}

	/* Select video input, video standard and tune here. */

	memset(&cropcap, 0, sizeof(cropcap));

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl (fd, VIDIOC_CROPCAP, &cropcap) < 0) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (ioctl (fd, VIDIOC_S_CROP, &crop) < 0) {
			switch (errno) {
			case EINVAL:
				/* Cropping not supported. */
				fprintf (stderr, "/dev/video0  doesn't support crop\n");
				break;
			default:
				/* Errors ignored. */
				break;
			}
		}
	} else {
                /* Errors ignored. */
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = 0;
	parm.parm.capture.capturemode = 0;
	if (ioctl(fd, VIDIOC_S_PARM, &parm) < 0) {
		printf("VIDIOC_S_PARM failed\n");
		return -1;
	}

	memset(&fmt, 0, sizeof(fmt));

	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = 0;
	fmt.fmt.pix.height      = 0;
	fmt.fmt.pix.pixelformat = g_fmt;
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

	if (ioctl (fd, VIDIOC_S_FMT, &fmt) < 0){
		fprintf (stderr, "%s iformat not supported \n", "/dev/video0");
		return -1;
	}

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;

	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
		printf("VIDIOC_G_FMT failed\n");
		return -1;
	}

	g_in_width = fmt.fmt.pix.width;
	g_in_height = fmt.fmt.pix.height;

	printf("VIDIOC_G_FMT: width = %d, height = %d \n", g_in_width, g_in_height);

	struct v4l2_control ctl_set;
	ctl_set.id = V4L2_CID_BRIGHTNESS;
	if ((lu >= 0) && (lu <= 100))
		ctl_set.value = lu * 256 / 100;
	else
		ctl_set.value = 128;
	if (-1 == ioctl(fd, VIDIOC_S_CTRL, &ctl_set)) {
		perror("VIDIOC_QUERYCAP");
	}

	ctl_set.id = V4L2_CID_CONTRAST;
	if ((co >= 0) && (co <= 100))
		ctl_set.value = co * 256 / 100;
	else
		ctl_set.value = 128;
	if (-1 == ioctl(fd, VIDIOC_S_CTRL, &ctl_set)) {
		perror("VIDIOC_QUERYCAP");
	}

	ctl_set.id = V4L2_CID_SATURATION;
	if ((sa >= 0) && (sa <= 100))
		ctl_set.value = sa * 256 / 100;
	else
		ctl_set.value = 128;
	if (-1 == ioctl(fd, VIDIOC_S_CTRL, &ctl_set)) {
		perror("VIDIOC_QUERYCAP");
	}

	if (v4l2_init_mmap(fd) < 0) {
		fprintf(stderr, "Init mmap error!\n");
	}

	return 0;
}

void v4l2_uninit_device (void)
{
	unsigned int i;

	for (i = 0; i < n_buffers; ++i)
		if (-1 == munmap (buffers[i].start, buffers[i].length)) {
			perror ("munmap");	
		}

	free (buffers);
}

int v4l2_start_capturing (int fd)
{
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf)) {
			perror ("VIDIOC_QBUF");
			return -1;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl (fd, VIDIOC_STREAMON, &type)) {
		perror ("VIDIOC_STREAMON");
		return -1;
	}
	
	return 0;
}

int v4l2_stop_capturing (int fd)
{
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type)) {
		perror ("VIDIOC_STREAMOFF");
		return -1;
	}
	
	return 0;
}

/**
  Convert from YUV422 format to RGB888. Formulae are described on http://en.wikipedia.org/wiki/YUV

  \param width width of image
  \param height height of image
  \param src source
  \param dst destination
*/
static void YUV422toRGB888(int width, int height, unsigned char *src, unsigned char *dst)
{
	int line, column;
	unsigned char *py, *pu, *pv;
	unsigned char *tmp = dst;

	/* In this format each four bytes is two pixels. Each four bytes is two Y's, a Cb and a Cr. 
		Each Y goes to one of the pixels, and the Cb and Cr belong to both pixels. */
	py = src;
	pu = src + 1;
	pv = src + 3;

	#define CLIP(x) ( (x)>=0xFF ? 0xFF : ( (x) <= 0x00 ? 0x00 : (x) ) )

	for (line = 0; line < height; ++line) {
		for (column = 0; column < width; ++column) {
			*tmp++ = CLIP((double)*py + 1.402*((double)*pv-128.0));
			*tmp++ = CLIP((double)*py - 0.344*((double)*pu-128.0) - 0.714*((double)*pv-128.0));      
			*tmp++ = CLIP((double)*py + 1.772*((double)*pu-128.0));

			// increase py every time
			py += 2;
			// increase pu,pv every second time
			if ((column & 1)==1) {
				pu += 4;
				pv += 4;
			}
		}
	}
}

/**
  Write image to jpeg file.

  \param img image to write
*/
static void jpegWrite(unsigned char* img, int width, int height, char *jpegFilename)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	
	JSAMPROW row_pointer[1];
	FILE *outfile = fopen( jpegFilename, "wb" );

	// try to open file for saving
	if (!outfile) {
		perror("jpeg");
		return;
  }

	// create jpeg data
	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, outfile);

	// set image parameters
	cinfo.image_width = width;	
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	// set jpeg compression parameters to default
	jpeg_set_defaults(&cinfo);
	// and then adjust quality setting
	jpeg_set_quality(&cinfo, jpegQuality, TRUE);

	// start compress 
	jpeg_start_compress(&cinfo, TRUE);

	// feed data
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &img[cinfo.next_scanline * cinfo.image_width *  cinfo.input_components];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	// finish compression
	jpeg_finish_compress(&cinfo);

	// destroy jpeg data
	jpeg_destroy_compress(&cinfo);

	// close output file
	fclose(outfile);
}

/**
	process image read
*/
static void process_image(const void* p, int width, int height, char *jpegFilename)
{
	unsigned char* src = (unsigned char*)p;
	unsigned char* dst = malloc(width*height*3*sizeof(char));

//	printf("Enter func: %s\n", __func__);
	YUV422toRGB888(width, height, src, dst);

	// write jpeg
	jpegWrite(dst, width, height, jpegFilename);

	// free temporary image
	free(dst);
}

int v4l2_read_frame (int fd, int width, int height, char *jpegFilename)
{
	struct v4l2_buffer buf;

	CLEAR (buf);

//	printf("Enter func: %s\n", __func__);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		case EAGAIN:
			return EAGAIN;
		case EIO:
		/* Could ignore EIO, see spec. */
		/* fall through */
		default:
			perror ("VIDIOC_DQBUF");
			return -1;
		}
	}
	assert (buf.index < n_buffers);
	process_image (buffers[buf.index].start, width, height, jpegFilename);

	if (-1 == xioctl (fd, VIDIOC_QBUF, &buf)) {
		perror ("VIDIOC_QBUF");
		return -1;
	}

	return 0;
}

int v4l2_capture_image (char *jpegFilename, int width, int height, int lu, int co, int sa)
{
	char *dev_name = CAMERA_DEVICE;
	int camera_fd;
	struct v4l2_pix_format pix;
	fd_set fds;
	struct timeval tv;
	int ret;

	camera_fd = v4l2_open_device(dev_name);
	if (camera_fd < 0)
		return -1;

	memset(&pix, 0, sizeof(struct v4l2_pix_format));
	pix.width = width;
	pix.height = height;
	pix.pixelformat = V4L2_PIX_FMT_YUYV;
	pix.field = V4L2_FIELD_INTERLACED;
	ret = v4l2_init_device(camera_fd, pix, lu, co, sa);
	if(ret < 0)
		return -1;

	ret = v4l2_start_capturing(camera_fd);
	if (ret < 0)
		return -1;

	while (1) {
			FD_ZERO (&fds);
			FD_SET (camera_fd, &fds);

			/* Timeout. */
			tv.tv_sec = 5;
			tv.tv_usec = 0;

			ret = select (camera_fd + 1, &fds, NULL, NULL, &tv);
			if (-1 == ret) {
				if (EINTR == errno)
					continue;
				perror ("select");
				break;
			}
			if (0 == ret) {
				fprintf (stderr, "select timeout\n");
				break;
			}
			if (v4l2_read_frame(camera_fd, width, height, jpegFilename) == 0)
				break;
		  /* EAGAIN - continue select loop. */
	}

	v4l2_stop_capturing(camera_fd);
	v4l2_uninit_device();
	v4l2_close_device(camera_fd);

	return 0;
}



