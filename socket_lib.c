#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "socket_lib.h"

static thrFunc sever_thr;

int add_new_tcp_process_thr( ServerEnv *envp )
{
    pthread_t tcpThr;

    if(pthread_create( &tcpThr, NULL, sever_thr, envp)) {
        printf("tcp thread create fail!\n");
        return -1;
    }

    printf("tcp thread has been created!\n");

    return 0;
}

int start_server(int localport, thrFunc func)
{
    int result;
    int clientSocket,client_len;
    int serverSocket;
    ServerEnv env;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    // create a socket obj for server
    serverSocket = socket(AF_INET,SOCK_STREAM,0);

    // bind tcp port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(localport);

    result = bind(serverSocket,(struct sockaddr *)&server_addr,sizeof(server_addr));
    if( result != 0 ) {
	       printf("[tcp server] bind error!\n ");    
    	   return -1;
    }

	// begin to listen
	result = listen(serverSocket, 5);
	if( result != 0 ) {
		printf("[tcp server] listen error!\n ");
		return -1;
	}

    sever_thr = func;
	while(1) {
//		printf("Begin to accept client.\n");
		client_len = sizeof(client_addr);
		clientSocket = accept(serverSocket, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);

		if( clientSocket < 0 ) {
			printf("[tcp server] accept error!\n" );
			return -1;
		}
		env.m_hSocket = clientSocket;

		printf("socke fd = %d\n", clientSocket);
//		add new tcp server thread
		add_new_tcp_process_thr(&env);
	}

    return serverSocket;
}

int connect_server(char *destIp, int destPort)
{
    int result;
    struct sockaddr_in address;
    int s_socket;

    s_socket = socket(AF_INET, SOCK_STREAM, 0);
	
    // set server addr
    bzero(&address,sizeof(address)); 
    address.sin_family = AF_INET;
    // use server ip and listen port to connect
    address.sin_addr.s_addr = inet_addr(destIp);
    address.sin_port        = htons(destPort);
    
    // connect tcp server
	result = connect(s_socket, (struct sockaddr *)&address, sizeof(address));
	if( result == -1 ) {
		printf("[tcp client] can't connect server !\n");
		return -1;
	}
	
	return s_socket;
}

int close_socket(int socket_fd)
{
    printf("close connect with server !\n ");

    close(socket_fd);

    return 0;
}

int socket_send(int sockfd, unsigned char *buf, int len, int timeout)
{
	unsigned char *p_buf = buf;
	int sendBytes = 0;
	int total_len = len;
	struct timeval tv;
	int ret;
	
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));
    if (ret < 0) {
		perror("setsockopt");
		return -1;
	}

	while (1)
	{
		sendBytes = send(sockfd, p_buf, total_len, MSG_NOSIGNAL);
//		printf("Finsh Send Message, ret = %d, errno = %d\n", sendBytes, errno);
		if( sendBytes < 0 && errno != EINTR)
		{
			if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
				printf("Send timeout.\n");
			else
				printf("Send errors!\n");
	        return -1;
		}
		else if(sendBytes == 0) {
			printf("Send: disconnected.\n");
			return 0;
		}
		else if (sendBytes > 0) 
		{
			if (sendBytes >= total_len)
			{
//				printf("Send Message OK!\n");
				break;
			}
//			printf("Send Message partially, sendBytes = %d \n", sendBytes);
			p_buf += sendBytes;
			total_len -= sendBytes;
			continue;
		} 
        else
		{
			continue;
		}
	}

	return len;
}

int socket_recv(int sockfd, unsigned char *buf, int len, int timeout)
{
	unsigned char *p_buf = buf;
	int recvBytes = 0;
	int total_len = len;
	struct timeval tv;
	int ret;
	int nRecvBuf = 64*1024;//设置为32K
	
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
	if (ret < 0) {
		perror("setsockopt");
		return -1;
	}

	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&nRecvBuf, sizeof(int));

	while(1) {
		recvBytes = recv(sockfd, p_buf, total_len, 0);
//		printf("recv return: %d, errno = %d\n", recvBytes, errno);
		if( recvBytes < 0 && errno != EINTR) {
			if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
				printf("Receive timeout.\n");
			else {
				printf("Receive: error occur!\n");
				return -2;
			}
			break;
		}
		else if(recvBytes == 0) {
			printf("Receive: disconnected.\n");
			return -2;
		}
		else if( recvBytes > 0 )
		{
			if (recvBytes >= total_len) {
				printf("Receive Message OK, len = %d!\n", recvBytes);
				break;
			}
			printf("Receive Message partially, recvBytes = %d \n", recvBytes);
			total_len -= recvBytes;
			p_buf += recvBytes;
			if (timeout == 0)
				break;
			else
				continue;
		}
		else {
			continue;
		}
	}

	return (len - total_len);
}

int socket_send_udp(char *destIp, int destPort, unsigned char *buf, int len)
{
	int result;
	struct sockaddr_in address;
	int s_socket;
	int count = 5;

	s_socket = socket(AF_INET, SOCK_DGRAM, 0);

	// set server addr
	bzero(&address,sizeof(address));
	address.sin_family 		= AF_INET;
	address.sin_addr.s_addr = inet_addr(destIp);
	address.sin_port        = htons(destPort);

	while (count --) {
		result = sendto(s_socket, buf, len, 0, (struct sockaddr *)&address, sizeof(address));
		if (result == len)
			break;
	}

	close(s_socket);

	if ((count == 0) && (result != len))
		return -1;
	else
		return 0;
}

int socket_recv_udp(int localport, unsigned char *buf, int len)
{
    int ret;
    int serverSocket;
    socklen_t sin_len;

    struct sockaddr_in server_addr;

    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(localport);
    sin_len = sizeof(server_addr);

    ret = bind(serverSocket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if( ret != 0 ) {
	       printf("[tcp server] bind error!\n ");
    	   return -1;
    }

    ret = recvfrom(serverSocket, buf, len, 0, (struct sockaddr *)&server_addr, &sin_len);

    close(serverSocket);

    return ret;
}

