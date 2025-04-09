#ifndef __WPA_SUPPLICANT_CONF_H
#define __WPA_SUPPLICANT_CONF_H

#if __cplusplus
extern "C" {
#endif

#include "wifi_intf.h"

#define CMD_LEN        255
#define REPLY_BUF_SIZE 4096 // wpa_supplicant's maximum size.


int wpa_conf_network_info_exist();
int wpa_conf_is_ap_exist(const char *ssid, tKEY_MGMT key_mgmt, char *net_id, int *len);
int wpa_conf_ssid2netid(char *ssid, tKEY_MGMT key_mgmt, char *net_id, int *len);
int wpa_conf_get_max_priority();
int wpa_conf_is_ap_connected(char *ssid, int *len);
int wpa_conf_get_netid_connected(char *net_id, int *len);
int wpa_conf_get_ap_connected(char *netid, int *len);
int wpa_conf_enable_all_networks();
int wpa_conf_remove_all_networks();
int wpa_conf_remove_maxnetid_network();

#if __cplusplus
};  // extern "C"
#endif

#endif /* __WPA_SUPPLICANT_CONF_H */
