/*
 *   CMA Device Power Control
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>   
#include <unistd.h>  
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <libsocketcan.h>
#include "types.h"
#include "device.h"
#include "iniparser.h"
#include "dictionary.h"

env_data_t CMA_Env_Parameter;

int Device_Env_init(void)
{
	dictionary  *ini;
	char *id;
	char *destip;
	int dest_port, local_port;
	char *s_proto;

	memset(&CMA_Env_Parameter, 0, sizeof(env_data_t));

	ini = iniparser_load(config_file);
	if (ini==NULL) {
		fprintf(stderr, "cannot parse file: %s\n", config_file);
			return -1 ;
	}

	id = iniparser_getstring(ini, "CMA:id", NULL);
	if (strlen(id) != 17)
		goto fail;
	memcpy(CMA_Env_Parameter.id, id, 17);

	CMA_Env_Parameter.l2_type = iniparser_getint(ini, "L2:type", 0);
	destip = iniparser_getstring(ini, "CAG:ip", NULL);
	dest_port = iniparser_getint(ini, "CAG:port", 0);
	local_port = iniparser_getint(ini, "CAG:local_port", 0);
	s_proto = iniparser_getstring(ini, "CAG:s_protocal", NULL);
	if ((destip == NULL) || (dest_port == 0) || (local_port == 0) || (s_proto == NULL))
		goto fail;

	memcpy(CMA_Env_Parameter.cma_ip, destip, strlen(destip));
	CMA_Env_Parameter.cma_port = dest_port;
	CMA_Env_Parameter.local_port = local_port;
	if (memcmp(s_proto, "udp", 3) == 0)
		CMA_Env_Parameter.s_protocal = 0;
	else if (memcmp(s_proto, "tcp", 3) == 0)
		CMA_Env_Parameter.s_protocal = 1;

	iniparser_freedict(ini);
	return 0;

fail:
	iniparser_freedict(ini);
	return -1;
}

int mysystem(char *input, char *output, int maxlen)  
{
	int status;
	int reslen;
	FILE *stream;
  
	if( NULL==input || NULL==output )
		return -1;

	printf("Exec shell command: %s\n", input);
	memset(output, 0, maxlen);
	stream = popen(input, "r");
	reslen = fread(output, sizeof(char), maxlen, stream);
	status = pclose(stream);
	if (-1 == status) {
		printf("pclose error!");
		return -1;
	}
	else {
		printf("exit status value = [0x%x]\n", status);
		if (WIFEXITED(status)) {
			if (0 == WEXITSTATUS(status)) {
				printf("run shell script successfully.\n");
			}
			else {
				printf("run shell script fail, script exit code: [%d]\n", WEXITSTATUS(status));
				return -1;
			}
		}
		else {
			printf("exit status = [%d]\n", WEXITSTATUS(status));
			return -1;
		}
	}

	return reslen;
}
 
int Device_power_ctl(cma_device_t dev, int powerOn)
{
	switch (dev) {
	case DEVICE_3G:
//		if (powerOn)
//			system("echo 1 > /sys/devices/........");
//		else
//			system("echo 0 > /sys/devices/........");
		break;
	case DEVICE_WIFI:
//		if (powerOn)
//			system("echo 1 > /sys/devices/........");
//		else
//			system("echo 0 > /sys/devices/........");
		break;
	}
	
	return 0; 
}

static int create_wpa_conf(void)
{
	dictionary  *ini;
	FILE    *wpa_conf;
	char *ssid, *proto, *key_mgmt, *psk;
	
	ini = iniparser_load(config_file);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", config_file);
        return -1 ;
    }
    
	ssid = iniparser_getstring(ini, "WIFI:ssid", NULL);
	proto = iniparser_getstring(ini, "WIFI:proto", NULL);
	key_mgmt = iniparser_getstring(ini, "WIFI:key_mgmt", NULL);
	psk = iniparser_getstring(ini, "WIFI:psk", NULL);
	
	if ((ssid == NULL) || (proto == NULL) || (key_mgmt == NULL) || (psk == NULL)) {
		printf("Device Init error, wifi configuration error.\n");
		return -1;
	}

    wpa_conf = fopen("wpa_supplicant.conf", "w");
    fprintf(wpa_conf,
    "ctrl_interface=/var/run/wpa_supplicant\n"
    "update_config=1\n"
    "\n"
    "network={\n"
    "ssid=\"%s\"\n"
    "proto=%s\n"
    "key_mgmt=%s\n"
    "psk=\"%s\"\n"
    "}"
    "\n", ssid, proto, key_mgmt, psk);
    
    fclose(wpa_conf);
        
    iniparser_freedict(ini);
    
    return 0;
}

int Device_wifi_init(void)
{
#if 0
	char cmd_shell[SHELL_MAX_BUF_LEN];
	char result[MAX_SHELL_OUTPU_LEN];
	char *cmd_insmod = "/sbin/insmod ";
	char *cmd_wpa = "/sbin/wpa_supplicant -Dwext -c/etc/wpa_supplicant.conf -i";
	
	if (create_wpa_conf() < 0)
		return -1;

	Device_power_ctl(DEVICE_WIFI, 1);
	
	memset(cmd_shell, 0, SHELL_MAX_BUF_LEN);
	sprintf(cmd_shell, "%s%s", cmd_insmod, WLAN_MODULE);
	if (mysystem(cmd_shell, result, MAX_SHELL_OUTPU_LEN) < 0) {
		printf("Insmod wifi module error!\n");
		return -1;
	}
	
	memset(cmd_shell, 0, SHELL_MAX_BUF_LEN);
	sprintf(cmd_shell, "%s%s", cmd_wpa, WLAN_DEVICE);
	if (mysystem(cmd_shell, result, MAX_SHELL_OUTPU_LEN) < 0) {
		printf("Start wpa_supplicant error!\n");
		return -1;
	}
#endif
	
	printf("Wifi Module Init OK.\n");
    
    return 0;
}

int Device_W3G_init(void)
{
	char *cmd_shell = "./start_3G.sh";
	pid_t pid;
	
	pid = fork();
    if(pid == -1) {
        perror("fork error");
        exit(EXIT_FAILURE);
    }
    if(pid == 0){
//		printf("this is child process and child's pid = %d,parent's pid = %d\n",getpid(),getppid());
        system(cmd_shell);
        exit(0);
    }
    if(pid > 0){
        //sleep(1);
//		printf("this is parent process and pid =%d ,child's pid = %d\n",getpid(),pid);
        printf("3G Module Init OK.\n");
        return 0;
    }
    
    return 0;
}

int Device_eth_init(void)
{
	char cmd_shell[SHELL_MAX_BUF_LEN];
	char result[MAX_SHELL_OUTPU_LEN];
	char *cmd_ifconfig = "ifconfig eth0";
	dictionary  *ini;
	char *addr, *broadcast, *netmask, *dns;
	
	ini = iniparser_load(config_file);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", config_file);
        return -1 ;
    }
    
	addr = iniparser_getstring(ini, "ETH:address", NULL);
	broadcast = iniparser_getstring(ini, "ETH:broadcast", NULL);
	netmask = iniparser_getstring(ini, "ETH:netmask", NULL);
	dns = iniparser_getstring(ini, "ETH:dns", NULL);
	
	if ((addr == NULL) || (broadcast == NULL) || (dns == NULL) || (netmask == NULL)) {
		printf("Device Init error, eth configuration error.\n");
		return -1;
	}
	
	memset(cmd_shell, 0, SHELL_MAX_BUF_LEN);
	sprintf(cmd_shell, "%s %s %s %s %s %s", cmd_ifconfig, addr, "broadcast", broadcast, "netmask", netmask);
	
	
	if (mysystem(cmd_shell, result, MAX_SHELL_OUTPU_LEN) < 0) {
		printf("Ethernet module config error!\n");
		iniparser_freedict(ini);
		return -1;
	}
	
	memset(cmd_shell, 0, SHELL_MAX_BUF_LEN);
	sprintf(cmd_shell, "%s %s %s", "echo nameserver", dns, "> /etc/resolv.conf");	
	if (mysystem(cmd_shell, result, MAX_SHELL_OUTPU_LEN) < 0) {
		printf("Ethernet module config error!\n");
		iniparser_freedict(ini);
		return -1;
	}
	
	iniparser_freedict(ini);
	printf("Ethernet Module Init OK.\n");
	return 0;
}

int Device_can_init(void)
{
	dictionary  *ini;
	char *devname = NULL;
	int bitrate = 0;
	int err;
	
	ini = iniparser_load(config_file);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", config_file);
        return -1 ;
    }
    
    devname = iniparser_getstring(ini, "CAN:devname", NULL);
    bitrate = iniparser_getint(ini, "CAN:bitrate", 0);
    if ((devname == NULL) || (bitrate == 0)) {
		printf("Device Init error, CAN interface configuration error.\n");
		return -1;
	}
    
    printf("CAN: set %s bitrate %d .\n", devname, bitrate);
    err = can_set_bitrate(devname, bitrate);
	if (err < 0) {
		fprintf(stderr, "failed to set bitrate of %s to %u\n",
			devname, bitrate);
		return -1;
	}
	
	printf("CAN: start %s .\n", devname);
	if (can_do_start(devname) < 0) {
		fprintf(stderr, "%s: failed to start\n", devname);
		return -1;
	}
    
    iniparser_freedict(ini);
	printf("CAN configration OK.\n");
	return 0;
}


int Device_get_basic_info(status_basic_info_t *dev)
{
	memcpy(dev->SmartEquip_Name, "jiance", 6);
	memcpy(dev->Model, "SY0503", 6);
	memcpy(dev->Essential_Info_Version, "1.01", 4);
	memcpy(dev->Bs_Manufacturer, "buaaa", 5);
	memcpy(dev->Bs_Identifier, "12345678", 8);

	return 0;
}

int Device_get_working_status(status_working_t *status)
{
	return 0;
}

static void get_gateway(const char *net_dev, unsigned long *p)
{
    FILE *fp;
    char buf[1024];
    char iface[16];
    unsigned int dest_addr=0, gate_addr=0;

    fp = fopen("/proc/net/route", "r");
    if(fp == NULL) {
        printf("fopen error \n");
        return;
    }

    fgets(buf, sizeof(buf), fp);

    while(fgets(buf, sizeof(buf), fp)) {
        if((sscanf(buf, "%s\t%X\t%X", iface, &dest_addr, &gate_addr) == 3)
            && (memcmp(net_dev, iface, strlen(net_dev)) == 0)
            && gate_addr != 0)
        {
//				printf("iface: %s, gate_addr = 0x%08x\n", iface, gate_addr);
				*p = gate_addr;
                break;
        }
    }

    fclose(fp);
}

static int get_dns(char *dns)
{
    int fd = -1;
    int size = 0;
    char strBuf[256];
    char tmpBuf[100];
    int buf_size = sizeof(strBuf);
    char *p = NULL;
    char *q = NULL;
    int i = 0;
    int j = 0;
    int count = 0;

    fd = open("/etc/resolv.conf", O_RDONLY);
    if(-1 == fd) {
        printf("%s open error \n", __func__);
        return -1;
    }
    size = read(fd, strBuf, buf_size);
    if(size < 0) {
        printf("%s read file len error \n", __func__);
        close(fd);
        return -1;
    }
    strBuf[buf_size] = '\0';
    close(fd);

//	printf("strBuf: %s\n", strBuf);
    while(i < buf_size) {
        if((p = strstr(&strBuf[i], "nameserver")) != NULL) {
			p += sizeof("nameserver");
            j++;
            count = 0;

            memset(tmpBuf, 0xff, 100);
            memcpy(tmpBuf, p, 100);
            tmpBuf[sizeof(tmpBuf) -1 ] = '\0';

            q = p;
            while(*q != '\n') {
                q++;
                count++;
            }
            i += (sizeof("nameserver") + count);

			memcpy(dns, p, count);
			dns[count]='\0';
        }
        else {
            i++;
        }
    }

    return 0;
}

int Device_getNet_info(Ctl_net_adap_t  *adap)
{
	/*socket参数设置*/
    int sock;
    struct sockaddr_in sin;
    struct ifreq ifr;
    unsigned long gateway;
    char dns[32];
    char *net_dev = "wlan0";

    get_gateway(net_dev, &gateway);
    get_dns(dns);
//   printf("Dns: %s \n", dns);
    inet_aton(dns, &sin.sin_addr);

    adap->Gateway = gateway;
    adap->DNS_Server = sin.sin_addr.s_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket");
        return -1;
    }

    /*这个是网卡的标识符*/
    strncpy(ifr.ifr_name, net_dev, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    /*获取ip*/
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        perror("ioctl");
        return -1;
    }
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
//	sprintf(ip,"%s",inet_ntoa(sin.sin_addr));
	adap->IP = sin.sin_addr.s_addr;

    /*获取子网掩码*/
    if (ioctl(sock, SIOCGIFNETMASK, &ifr)< 0) {
        perror("ioctl");
        return -1;
    }
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
//	sprintf(zwym,"%s\n",inet_ntoa(sin.sin_addr));
	adap->Subnet_mask = sin.sin_addr.s_addr;

	close(sock);

	return 0;
}

int Device_setNet_info(Ctl_net_adap_t  *adap)
{
	return 0;
}

int Device_getId(byte *id, byte *org_id)
{
	if ((id == NULL) || (org_id == NULL))
		return -1;

	return 0;
}

int Device_setId(byte *id, byte *org_id)
{
	if ((id == NULL) || (org_id == NULL))
		return -1;

	return 0;
}

int Device_reset(byte type)
{
	return 0;
}

int Device_setWakeup_time(int revival_time, usint revival_cycle, usint duration_time)
{
	return 0;
}
