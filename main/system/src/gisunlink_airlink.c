/*
* _COPYRIGHT_
*
* File Name: gisunlink_airlink.c
* System Environment: JOHAN-PC
* Created Time:2019-02-12
* Author: johan
* E-mail: johaness@qq.com
* Description: 
*
*/

#include "airkiss.h"
#include "gisunlink.h"
#include "gisunlink_print.h"
#include "gisunlink_atomic.h"
#include "gisunlink_config.h"
#include "gisunlink_airlink.h"

#include <unistd.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define WECHAT_APPID "gh_eb34e8b47132"
#define DEVICE_TYPE	"gisunlink_water_"
#define DEFAULT_LAN_PORT 12476  
#define LAN_BUF_SIZE 200  

typedef struct _gisunlink_airlink_ctrl {
	bool task_run;
	int sockfd;
	uint8 broadcast_count; 
} gisunlink_airlink_ctrl;

gisunlink_airlink_ctrl *gisunlink_airlink = NULL;

const airkiss_config_t akconf = {  
	(airkiss_memset_fn)&memset,  
	(airkiss_memcpy_fn)&memcpy,  
	(airkiss_memcmp_fn)&memcmp,  
	(airkiss_printf_fn)&gisunlink_printCb 
};  

static bool gisunlink_airlink_socket_init(gisunlink_airlink_ctrl *airlink) {
	struct sockaddr_in src_addr;
	bzero(&src_addr, sizeof(src_addr));
	src_addr.sin_family = AF_INET;
	src_addr.sin_port   = htons(DEFAULT_LAN_PORT);
	src_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(airlink == NULL) {
		return false;
	}

	if((airlink->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"Create Socket failded!!!");
		return false;
	}

	int err_log = bind(airlink->sockfd, (struct sockaddr*)&src_addr, sizeof(src_addr));
	if(err_log < 0) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"Bind Socket failded!!!");
		close(airlink->sockfd);
		return false;
	}

	int so_broadcast = 1;
	int ret = setsockopt(airlink->sockfd,SOL_SOCKET,SO_BROADCAST,&so_broadcast,sizeof(so_broadcast));
	if(ret != 0) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"Setsockopt Socket failded!!!");
		return false;
	}
	return true;
}

static void gisunlink_airlink_send_packet(gisunlink_airlink_ctrl *airlink,struct sockaddr_in *addr, airkiss_lan_cmdid_t cmd) {
	uint16 lan_buf_len = LAN_BUF_SIZE;  
	uint8 lan_buf[LAN_BUF_SIZE];  
	char *device_sn = NULL;

	if(airlink == NULL) {
		return;
	}

	device_sn = gisunlink_get_mac_with_string(DEVICE_TYPE);
	if(device_sn) {
		airkiss_lan_ret_t packet = airkiss_lan_pack(cmd, WECHAT_APPID, device_sn, 0, 0, lan_buf, &lan_buf_len, &akconf);  

		if(packet != AIRKISS_LAN_PAKE_READY) {  
			gisunlink_print(GISUNLINK_PRINT_ERROR,"Pack lan packet error!:%d\n",packet);
			return;  
		}  

		lan_buf_len = sendto(airlink->sockfd, lan_buf, lan_buf_len, 0, (struct sockaddr *)addr,sizeof(* addr));

		if(lan_buf_len <= 0) {  
			gisunlink_print(GISUNLINK_PRINT_ERROR,"Pack lan packet send error!:%d\n",errno);
		}

		gisunlink_free(device_sn);
		device_sn = NULL;
	}
}  

static void gisunlink_airlink_send_broadcast(gisunlink_airlink_ctrl *airlink) {
	char ip_buf[20]="255.255.255.255";
	struct sockaddr_in dst_addr;
	bzero(&dst_addr,sizeof(dst_addr));
	dst_addr.sin_family = AF_INET;
	dst_addr.sin_port	= htons(DEFAULT_LAN_PORT);
	inet_pton(AF_INET,ip_buf,&dst_addr.sin_addr);
	airkiss_lan_cmdid_t cmd = AIRKISS_LAN_SSDP_NOTIFY_CMD;
	gisunlink_airlink_send_packet(airlink,&dst_addr, cmd);
}

static void gisunlink_airlink_recv(gisunlink_airlink_ctrl *airlink) {
	struct sockaddr_in client_addr;
	socklen_t cliaddr_len = sizeof(client_addr);
	airkiss_lan_cmdid_t cmd = AIRKISS_LAN_SSDP_RESP_CMD;
	char cli_ip[INET_ADDRSTRLEN] = "";
	uint16 lan_buf_len = LAN_BUF_SIZE;  
	uint8 lan_buf[LAN_BUF_SIZE];  

	if((lan_buf_len = recvfrom(airlink->sockfd, lan_buf, sizeof(lan_buf), 0, (struct sockaddr*)&client_addr, &cliaddr_len)) <= 0) {
		gisunlink_print(GISUNLINK_PRINT_ERROR,"recvform errror:%d\n",errno);
		return;
	}

	airkiss_lan_ret_t ret = airkiss_lan_recv(lan_buf,lan_buf_len, &akconf);
	inet_ntop(AF_INET, &client_addr.sin_addr, cli_ip, sizeof(cli_ip));

	switch (ret) {
		case AIRKISS_LAN_SSDP_REQ:
			//		gisunlink_print(GISUNLINK_PRINT_ERROR,"recvform 0x%x message from:%s\n",ret,cli_ip);
			client_addr.sin_port = htons(DEFAULT_LAN_PORT);
			gisunlink_airlink_send_packet(airlink,&client_addr,cmd);
			break;
		default:
			//		gisunlink_print(GISUNLINK_PRINT_ERROR,"Pack is not ssdq req! recv error : %d\n",ret);
			break;
	}
}

static void gisunlink_airlink_task(void * param) {
	gisunlink_airlink_ctrl *airlink = (gisunlink_airlink_ctrl *)param; 

	struct timeval tv = {
		.tv_sec = 1,
		.tv_usec = 0,
	};

	while(airlink->task_run) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(airlink->sockfd, &rfds);

		int s = select(airlink->sockfd + 1, &rfds, NULL, NULL, &tv);
		if(s < 0) {
			airlink->broadcast_count = 0;
			break;
		} else if(s > 0) {
			if(FD_ISSET(airlink->sockfd, &rfds)) {
				gisunlink_airlink_recv(airlink);
			}
		} else {
			if((airlink->broadcast_count++) > 30) {
				airlink->broadcast_count = 0;
				break;
			}
			gisunlink_airlink_send_broadcast(airlink);
		}
	}
	airlink->task_run = false;
	close(airlink->sockfd);
	gisunlink_free(airlink);
	vTaskDelete(NULL);
}

void gisunlink_airlink_init(void) {
	if(gisunlink_airlink) {
		gisunlink_airlink->broadcast_count = 0;
		return;
	}

	gisunlink_airlink = (gisunlink_airlink_ctrl *)gisunlink_malloc(sizeof(gisunlink_airlink_ctrl));

	if(gisunlink_airlink) {
		gisunlink_airlink->broadcast_count = 0;
		if(gisunlink_airlink_socket_init(gisunlink_airlink)) {
			gisunlink_airlink->task_run = true;
			gisunlink_create_task_with_Priority(gisunlink_airlink_task,"airlink_task",gisunlink_airlink,1024,3);
		} else {
			gisunlink_free(gisunlink_airlink);
		}
	}
}

uint8 gisunlink_airlink_is_run(void) {
	return (gisunlink_airlink != NULL && gisunlink_airlink->task_run == true);
}

void gisunlink_airlink_deinit(void) {
	if(gisunlink_airlink && gisunlink_airlink->task_run) {
		gisunlink_airlink->task_run = false;
	}
}
