#ifndef __WIFI_INTF_H
#define __WIFI_INTF_H

#include <stdbool.h>
#include "wifi_udhcpc.h"
#include "status_info.h"

#if __cplusplus
extern "C" {
#endif
#define MAX_CALLBCAKS_COUNT  1024

typedef enum {
	WIFIMG_NONE = 0,
	WIFIMG_WPA_PSK,
	WIFIMG_WPA2_PSK,
	WIFIMG_WEP,
}tKEY_MGMT;

enum wmgState {
	NETWORK_CONNECTED = 0x01,
	CONNECTING,
	OBTAINING_IP,
	DISCONNECTED,
	CONNECTED,
	STATE_UNKNOWN,
};

enum wmgEvent{
	WSE_UNKNOWN = 0x20,

	WSE_STARTUP_AUTO_CONNECT,
	WSE_AUTO_CONNECTED,
	WSE_ACTIVE_CONNECT,
	WSE_ACTIVE_OBTAINED_IP,

	WSE_AUTO_DISCONNECTED,
	WSE_ACTIVE_DISCONNECT,

	WSE_KEYMT_NO_SUPPORT,
	WSE_CMD_OR_PARAMS_ERROR,
	WSE_DEV_BUSING,
	WSE_CONNECTED_TIMEOUT,
	WSE_OBTAINED_IP_TIMEOUT,

	WSE_WPA_TERMINATING,
	WSE_AP_ASSOC_REJECT,
	WSE_NETWORK_NOT_EXIST,
	WSE_PASSWORD_INCORRECT,
};

#define SSID_MAX 64
#define PWD	 48

struct WmgStaEvt {
	enum wmgState state;
	enum wmgEvent event;
};

struct wifi_status {
	enum wmgState state;
	char ssid[SSID_MAX];
};

struct Manager {
	struct WmgStaEvt StaEvt;
	const char *ssid;
	bool enable;
};

typedef struct connection_status {
    char ssid[SSID_MAX];
    char ip_address[32];
    int freq;
	int rssi;
	int link_speed;
	int noise;
} connection_status;

extern struct Manager *w;

typedef void (*tWifi_state_callback)(struct Manager *wmg,int state_label);

typedef struct{
	int (*add_state_callback)(tWifi_state_callback pcb);
	int (*ssid_is_connected_ap)(char *ssid);
	int (*is_ap_connected)(char *ssid, int *len);
	int (*get_connection_info)(connection_status *connection_info);
	int (*get_scan_results)(char *result, int *len);
	int (*connect_ap)(const char *ssid, const char *passwd, int event_label);
	int (*connect_ap_key_mgmt)(const char *ssid, tKEY_MGMT key_mgnt, const char *passwd, int event_label);
	int (*connect_ap_auto)(int event_label);
	int (*connect_ap_with_netid)(const char *net_id, int event_label);
	int (*add_network)(const char *ssid, tKEY_MGMT key_mgnt, const char *passwd, int event_label);
	int (*disconnect_ap)(int event_label);
	int (*remove_network)(char *ssid, tKEY_MGMT key_mgmt);
	int (*remove_all_networks)(void);
	int (*list_networks)(char *reply, size_t reply_len, int event_label);
	int (*get_netid)(const char *ssid, tKEY_MGMT key_mgmt, char *net_id, int *length);
	int (*get_status)(struct wifi_status *s);
	int (*clear_network)(const char *ssid);
	int (*wps_pbc)(int event_label);
}aw_wifi_interface_t;

const aw_wifi_interface_t * aw_wifi_on(tWifi_state_callback pcb,int event_label);
int aw_wifi_off(const aw_wifi_interface_t *p_wifi_interface_t);
int state_event_change(int event_label);
const char *wmg_event_txt(enum wmgEvent event);
const char *wmg_state_txt(enum wmgState state);

void start_udhcpc_thread(void *args);
enum wmgState aw_wifi_get_wifi_state();
enum wmgEvent aw_wifi_get_wifi_event();

#if __cplusplus
};  // extern "C"
#endif

#endif
