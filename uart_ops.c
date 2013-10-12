#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>  
#include <sys/stat.h>   
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

int uart_open_dev(char *dev)
{
	int fd = open(dev, O_RDWR);  //| O_NOCTTY | O_NDELAY
	if (-1 == fd) { 			
		perror("Can't Open Serial Port");
		return -1;		
	}
	else {
		printf("Serial: Open %s Sucessful.\n", dev);
		return fd;
	}
}

void uart_close_dev(int fd)
{
	close(fd);
}

int speed_arr[] = { B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300 };
int name_arr[] = { 115200, 57600, 38400,  19200,  9600,  4800,  2400,  1200,  300 };
void uart_set_speed(int fd, int speed)
{
	int   i; 
	int   status; 
	struct termios   Opt;

	printf("Set bitrate to %d\n", speed);

	tcgetattr(fd, &Opt); 

	for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++) { 
		if (speed == name_arr[i]) {     
			tcflush(fd, TCIOFLUSH);     
			cfsetispeed(&Opt, B38400);
			cfsetospeed(&Opt, B38400);
			Opt.c_cflag |= (CLOCAL | CREAD);
			status = tcsetattr(fd, TCSANOW, &Opt);  
			if  (status != 0) {        
				perror("tcsetattr fd1");  
				return;
			}    
			tcflush(fd,TCIOFLUSH);   
		} 
	}
	return;
}

int uart_set_parity(int fd,int databits,int stopbits,int parity)
{
	struct termios options; 
	if (tcgetattr(fd, &options)  !=  0) {
		perror("SetupSerial 1");
		return -1;  
	}

	options.c_cflag &= ~CSIZE;
 
	switch (databits) {   
	case 7:		
		options.c_cflag |= CS7; 
		break;
	case 8:     
		options.c_cflag |= CS8;
		break;   
	default:    
		fprintf(stderr,"Unsupported data size\n"); 
		return -1;
	}

	switch (parity) {   
	case 'n':
	case 'N':    
		options.c_cflag &= ~PARENB;   /* Clear parity enable */
		options.c_iflag &= ~INPCK;     /* Enable parity checking */ 
		break;  
	case 'o':   
	case 'O':     
		options.c_cflag |= (PARODD | PARENB); /* 设置为奇效验*/  
		options.c_iflag |= INPCK;             /* Disnable parity checking */ 
		break;  
	case 'e':  
	case 'E':   
		options.c_cflag |= PARENB;     /* Enable parity */    
		options.c_cflag &= ~PARODD;   /* 转换为偶效验*/     
		options.c_iflag |= INPCK;       /* Disnable parity checking */
		break;
	case 'S': 
	case 's':  /*as no parity*/
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		break;
	default:   
		fprintf(stderr,"Unsupported parity\n");    
		return -1;
	}
  
	/* 设置停止位*/  
	switch (stopbits) {
	case 1:    
		options.c_cflag &= ~CSTOPB;  
		break;  
	case 2:    
		options.c_cflag |= CSTOPB;  
	   break;
	default:    
		 fprintf(stderr,"Unsupported stop bits\n");  
		 return -1;
	}

	/* Set input parity option */ 
	if (parity != 'n')   
		options.c_iflag |= INPCK; 

	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	tcflush(fd,TCIFLUSH);
	options.c_cc[VTIME] = 150; /* 设置超时15 seconds*/   
	options.c_cc[VMIN] = 0; /* Update the options and do it NOW */
	if (tcsetattr(fd,TCSANOW,&options) != 0) { 
		perror("SetupSerial 3");
		return -1;  
	} 
	
	return 0;  
}

