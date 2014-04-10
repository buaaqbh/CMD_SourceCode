/*
 * socket_lib.h
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

#ifndef SOCKET_LIB_H
#define SOCKET_LIB_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ServerEnv {
    int m_hSocket;
}ServerEnv;

typedef void * (*thrFunc)(void * arg);

int start_server(int localport, thrFunc func);
int connect_server( char *destIp, int destPort, int udp, int timeout);
int close_socket(int socket_fd);
int socket_send(int sockfd, unsigned char *buf, int len, int timeout);
int socket_recv(int sockfd, unsigned char *buf, int len, int timeout, int udp);
int socket_send_udp(char *destIp, int destPort, unsigned char *buf, int len);
int socket_recv_udp(int localport, unsigned char *buf, int len, int timeout);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* SOCKET_LIB_H */
