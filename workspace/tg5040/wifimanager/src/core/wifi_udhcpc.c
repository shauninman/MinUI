#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>

#include "wifi_event.h"
#include "wifi.h"
#include "wmg_debug.h"

extern void cancel_saved_conf_handle(const char *net_id);

static int get_net_ip(const char *if_name, int family, char *ip, int len)
{
	struct sockaddr_in *addr4;
	struct sockaddr_in6 *addr6;
	char buf[NI_MAXHOST];
	struct ifaddrs *if_dev, *if_inter;
    if(getifaddrs(&if_dev)) {
		return -1;
	}
	for(if_inter = if_dev; if_inter != NULL;if_inter=if_inter->ifa_next) {
		if(strcmp(if_name,if_inter->ifa_name) != 0)
			continue;

		if(NULL == if_inter->ifa_addr)
			continue;

		if(family != if_inter->ifa_addr->sa_family)
			continue;

		if (if_inter->ifa_addr->sa_family==AF_INET6) { // check it is IP6
			addr6 = (struct sockaddr_in6 *)if_inter->ifa_addr;
			if(IN6_IS_ADDR_MULTICAST(&addr6->sin6_addr)){
				continue;
			}
			if(IN6_IS_ADDR_LINKLOCAL(&addr6->sin6_addr)){
				continue;
			}
			if(IN6_IS_ADDR_LOOPBACK(&addr6->sin6_addr)){
				continue;
			}
			if(IN6_IS_ADDR_UNSPECIFIED(&addr6->sin6_addr)){
				continue;
			}
			if(IN6_IS_ADDR_SITELOCAL(&addr6->sin6_addr)){
				continue;
			}
			if ( NULL != inet_ntop(if_inter->ifa_addr->sa_family,
					(void *)&(addr6->sin6_addr), buf, NI_MAXHOST)){
				if(len <= strlen(buf) )
					break;
				strcpy(ip,buf);
			}
        } else if (if_inter->ifa_addr->sa_family==AF_INET) { // check it is IP4
			addr4 = (struct sockaddr_in *)if_inter->ifa_addr;
			if ( NULL != inet_ntop(if_inter->ifa_addr->sa_family,
						(void *)&(addr4->sin_addr), buf, NI_MAXHOST)) {
				if(len <= strlen(buf))
					break;
				strcpy(ip, buf);
			}
        }
    }

    if(if_dev != NULL){
        freeifaddrs(if_dev);
    }
    return 0;
}


int is_ip_exist()
{
    char ipaddr[NI_MAXHOST] = {0};

    get_net_ip("wlan0", AF_INET, ipaddr, NI_MAXHOST);

	if(ipaddr[0] == 0)
		return 0;
	else
		return 1;
}

int  udhcpc_v4(void)
{
    int times = 0;
    char ipaddr[NI_MAXHOST] = {0};
    char cmd[256] = {0}, reply[8] = {0};

	wmg_printf(MSG_DEBUG,"OBTAINING IPv4......\n");
    /* restart udhcpc */
    system("/etc/wifi/udhcpc_wlan0 start >/dev/null");

    /* check ip exist */
	ms_sleep(1000);
    times = 0;
    do{
		ms_sleep(1000);
        get_net_ip("wlan0", AF_INET, ipaddr, NI_MAXHOST);
        times++;
    }while((ipaddr[0] == 0) && (times < 30));

	if(ipaddr[0] != 0)
		return 0;
	else
		return -1;
}
#ifdef DHCPV6_THRREAD
struct dhcpv6_context {
	pthread_t thread;
	bool thread_enable;
};

struct dhcpv6_context odhcp6_instance = {
	.thread_enable = false,
};

static void *odhcp6_thread(void *arg)
{
    int len = 0, vflag = 0, times = 0;
    char ipaddr[NI_MAXHOST] = {0};

	odhcp6_instance.thread_enable = true;

	pthread_detach(pthread_self());

	wmg_printf(MSG_DEBUG,"OBTAINING IPv6......\n");
	if(get_process_state("odhcp6c",7) > 0) {
		system("killall -9 odhcp6c");
	}

	system("odhcp6c wlan0 -v -e -d &");

	do {
		ms_sleep(1000);
		get_net_ip("wlan0",AF_INET6,ipaddr,NI_MAXHOST);
        times++;
    }while((ipaddr[0] == 0) && (times < 30));

	if(ipaddr[0] != 0) {
		wmg_printf(MSG_DEBUG,"IPv6 address:%s\n",ipaddr);
	}else {
		wmg_printf(MSG_WARNING,"Got IPv4 timeout\n");
	}
	odhcp6_instance.thread_enable = false;

	pthread_exit(NULL);
}

int odhcp6_start(void)
{
	int ret = -1;

	if(odhcp6_instance.thread_enable == true) {
		wmg_printf(MSG_WARNING,"odhcp6 thread already start.\n");
	}

	if((ret = pthread_create(&odhcp6_instance.thread, NULL, odhcp6_thread, NULL)) != 0) {
		wmg_printf(MSG_ERROR,"create odhcp6 thread failed\n");
		return -1;
	}
}
#else
int odhcp6_start(void)
{
	wmg_printf(MSG_DEBUG,"OBTAINING IPv6......\n");

	if(get_process_state("odhcp6c",7) > 0) {
		system("killall -9 odhcp6c");
	}

	/* Consider that not all target network support IPv6 and the time
	 * of network connection, and do not check whether IPv6 is obtained.
	 */

	system("odhcp6c wlan0 -v -e -d &");

	return 0;
}
#endif
void start_udhcpc()
{
	int ret_ipv4 = -1;

	w->StaEvt.event = WSE_ACTIVE_OBTAINED_IP;
	w->StaEvt.state = OBTAINING_IP;
	state_event_change(a->label);
	ret_ipv4 = udhcpc_v4();

	if(ret_ipv4 != 0) {
		wmg_print(MSG_ERROR,"Got IPv4 failed.\n");
	}

#ifdef CONFIG_IPV6
	odhcp6_start();
#endif
	/*For protocol stacks where IPv4 and IPv6 coexist, Only check whether IPv4 is obtained*/
	if(ret_ipv4 == 0) {
		w->StaEvt.state = NETWORK_CONNECTED;
		state_event_change(a->label);
    }else{
        wmg_printf(MSG_ERROR,"udhcpc wlan0 timeout\n");
        if(w->StaEvt.state != CONNECTED){

			w->StaEvt.state = DISCONNECTED;
			w->StaEvt.event = WSE_OBTAINED_IP_TIMEOUT;
			cancel_saved_conf_handle(a->netIdConnecting);

			state_event_change(a->label);
        }
    }
}
