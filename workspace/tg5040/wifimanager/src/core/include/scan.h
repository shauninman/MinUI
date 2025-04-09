#ifndef __NETWORK_MANAGER_H
#define __NETWORK_MANAGER_H
#include "tool.h"

#if __cplusplus
extern "C" {
#endif

#define SCAN_BUF_LEN      4096
#define KEY_NONE_INDEX    0
#define KEY_WPA_PSK_INDEX 1
#define KEY_WEP_INDEX     2
#define KEY_UNKOWN		  3

struct net_scan {
	/* store scan results */
	char results[SCAN_BUF_LEN];
	unsigned int results_len;
	unsigned int try_scan_count;
	bool enable;
};
int direct_get_scan_results_inner(char *results,int *len);
int get_key_mgmt(const char *ssid, int key_mgmt_info[]);
int isScanEnable();


#if __cplusplus
};  // extern "C"
#endif

#endif /* __NETWORK_MANAGER_H */
