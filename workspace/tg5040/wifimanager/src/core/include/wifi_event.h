#ifndef __WIFI_EVENT_H
#define __WIFI_EVENT_H

#include "wifi_intf.h"

#define EVENT_BUF_SIZE 2048

#if __cplusplus
extern "C" {
#endif

#define MAX_ASSOC_REJECT_COUNT  5
#define MAX_RETRIES_ON_AUTHENTICATION_FAILURE 2

enum wpaEvent{
    WPAE_CONNECTED = 1,
    WPAE_DISCONNECTED,
    WPAE_STATE_CHANGE,
    WPAE_SCAN_FAILED,
    WPAE_SCAN_RESULTS,
    WPAE_LINK_SPEED,
    WPAE_TERMINATING,
    WPAE_DRIVER_STATE,
    WPAE_EAP_FAILURE,
    WPAE_ASSOC_REJECT,
    WPAE_NETWORK_NOT_FOUND,
    WPAE_PASSWORD_INCORRECT,
    WPAE_UNKNOWN,
};

#define NET_ID_LEN 10

struct wpaManager{
	enum wpaEvent evt;
	int evtFd[2];
	bool EvtSocketEnable;
	pthread_t evtThreadId;
	unsigned int assocRejectCnt;
	unsigned int netNotFoundCnt;
	unsigned int authFailCnt;
	int label;
	char netIdConnecting[NET_ID_LEN + 1];
};

extern struct wpaManager *a;

void wifi_start_event_loop();
void wifi_stop_event_loop();
void start_wifi_on_check_connect_timeout();
void start_check_connect_timeout();
void set_scan_start_flag();
int  get_scan_status();
int add_wifi_state_callback_inner(tWifi_state_callback pcb);
int call_state_callback_function(struct Manager *w, int state_label);
int reset_wifi_state_callback();
void reset_assoc_reject_count();
void rest_network_not_found_count();
void rest_entication_fail_count();
int evtSocketInit();
void evtSockeExit();
int clearEvtSocket();
int evtSend(enum wpaEvent event);
int evtRead(enum wpaEvent *event);

#if __cplusplus
};  // extern "C"
#endif

#endif /*__WIFI_EVENT_H*/
