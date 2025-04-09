#ifndef __STATUS_INFO_H
#define __STATUS_INFO_H
#include "tool.h"

#if __cplusplus
extern "C" {
#endif

#define WPA_STA_MAX_SSID	 32
/*for compatibility of chinese ssid*/
#define WPA_STA_MAX_PSSID	 512
#define WPA_STA_MAX_BSSID	 18
#define WPA_STA_MAX_IP_ADDR  16
#define WPA_STA_MAX_KEY_MGMT 16
#define WPA_STA_MAX_MAC_ADDR 18

enum wpa_states {
	WPA_UNKNOWN = 1024,
	WPA_COMPLETED,
	WPA_DISCONNECTED,
	WPA_INTERFACE_DISABLED,
	WPA_INACTIVE,
	WPA_SCANNING,
	WPA_AUTHENTICATING,
	WPA_ASSOCIATING,
	WPA_ASSOCIATED,
	WPA_4WAY_HANDSHAKE,
	WPA_GROUP_HANDSHAKE,
};

struct wpa_status{
    int id;
    char bssid[WPA_STA_MAX_BSSID];
    int freq;
    char ssid[WPA_STA_MAX_PSSID];
    enum wpa_states wpa_state;
    char ip_address[WPA_STA_MAX_IP_ADDR];
    char key_mgmt[WPA_STA_MAX_KEY_MGMT];
    char mac_address[WPA_STA_MAX_MAC_ADDR];
};

typedef struct signal_status {
	int rssi;
	int link_speed;
	int noise;
	int frequency;
} signal_status;


struct wpa_status *get_wpa_status_info();
int get_connection_info_inner(signal_status *signal_info);
void print_wpa_status();
void wpa_status_info_free();

#if __cplusplus
};  // extern "C"
#endif

#endif /* __STATUS_INFO_H */
