#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>

#include "wifi_event.h"
#include "scan.h"
#include "wifi_intf.h"
#include "wifi.h"
#include "wmg_debug.h"
#include "tool.h"
#define WAITING_CLK_COUNTS   50
#define SSID_LEN	512
#define TRY_SAN_MAX 6

static struct net_scan scan = {
	.results_len = 0,
	.try_scan_count = 0,
	.enable = 0,
};

int remove_slash_from_scan_results()
{
    char *ptr = NULL;
    char *ptr_s = NULL;
    char *ftr = NULL;

    ptr_s = scan.results;
    while(1)
    {
        ptr = strchr(ptr_s,'\"');
	if(ptr == NULL)
	    break;

        ptr_s = ptr;
        if((*(ptr-1)) == '\\')
	{
            ftr = ptr;
            ptr -= 1;
            while(*ftr != '\0')
                *(ptr++) = *(ftr++);
            *ptr = '\0';
            continue; //restart new search at ptr_s after removing slash
	}
        else
            ptr_s++; //restart new search at ptr_s++
    }

    ptr_s = scan.results;
    while(1)
    {
        ptr = strchr(ptr_s,'\\');
	if(ptr == NULL)
	    break;

        ptr_s = ptr;
        if((*(ptr-1)) == '\\')
	{
            ftr = ptr;
            ptr -= 1;
            while(*ftr != '\0')
                *(ptr++) = *(ftr++);
            *ptr = '\0';
            continue; //restart new search at ptr_s after removing slash
	}
        else
            ptr_s++; //restart new search at ptr_s++
    }

    return 0;
}

int isScanEnable()
{
	if(scan.enable)
		return 1;
	else
		return 0;
}

int direct_get_scan_results_inner(char *results,int *len)
{
	char cmd[16] = {0};
	char reply[16] = {0};
	int ret = 0;
	int i = 0;
	char *ptr = NULL;
	int index;
	int flags;
	enum wpaEvent event;
	while(1) {
		/* clear scan.sockets data before sending scan command*/

		clearEvtSocket();

		scan.enable = true;

		strncpy(cmd,"SCAN",15);
		ret = wifi_command(cmd, reply, sizeof(reply));
		if(ret) {
			wmg_printf(MSG_DEBUG,"wifimanger send scan error:%s\n",reply);
			if(strncmp(reply,"FAIL-BUSY",9) == 0) {
				wmg_printf(MSG_DEBUG,"wpa_supplicant is scanning internally\n");
				event = WPAE_SCAN_RESULTS;
				/*Wpa_supplicant is scanning internally ,so get he scan results directly*/
				goto scan_resluts;
			} else {
				return -1;
			}
		}

read_event:
		ret = evtRead(&event);

		/*read the non-scan event ,try to read event again.*/
		if((event == WPAE_DISCONNECTED) || (event == WPAE_NETWORK_NOT_FOUND)) {
			wmg_printf(MSG_WARNING,"read event again......\n");
			goto read_event;
		}

		/*send scan command failed ,try to scan again.*/
		if(event == WPAE_SCAN_FAILED) {
			wmg_printf(MSG_WARNING,"scan again......\n");
			scan.try_scan_count ++;
			if(scan.try_scan_count > TRY_SAN_MAX){
				wmg_printf(MSG_WARNING,"send scan cmd failed\n");
				return -1;
			}
			sleep(1);
			continue;
		}
scan_resluts:
		if(event == WPAE_SCAN_RESULTS){
			strncpy(cmd,"SCAN_RESULTS",15);
			cmd[15]= '\0';
			ret = wifi_command(cmd,scan.results,sizeof(scan.results));
			if(ret) {
				wmg_printf(MSG_ERROR,"do scan results error!\n");
				return -1;
			}
			remove_slash_from_scan_results();
			scan.results_len =  strlen(scan.results);
		} else {
			wmg_printf(MSG_ERROR,"read scan data is failed\n");
			return -1;
		}

		scan.enable = false;

		if(NULL == results || NULL == len)
			return 0;

		if(*len <= scan.results_len) {
			wmg_printf(MSG_WARNING,"Scan result overflow,%d < %d\n",*len,scan.results_len);
			strncpy(results, scan.results, *len-1);
			index = *len -1;
			results[index] = '\0';
			ptr=strrchr(results, '\n');
			if(ptr != NULL) {
				*ptr = '\0';
			}
		} else {
			strncpy(results, scan.results, scan.results_len);
			results[scan.results_len] = '\0';
			*len = scan.results_len;
		}
		break;
	}
	return 0;
}

int is_network_exist(const char *ssid, tKEY_MGMT key_mgmt)
{
    int ret = 0, i = 0, key[4] = {0};

    for(i=0; i<4; i++){
        key[i]=0;
    }

    get_key_mgmt(ssid, key);
    if(key_mgmt == WIFIMG_NONE){
        if(key[0] == 1){
            ret = 1;
        }
    }else if(key_mgmt == WIFIMG_WPA_PSK || key_mgmt == WIFIMG_WPA2_PSK){
        if(key[1] == 1){
            ret = 1;
        }
    }else if(key_mgmt == WIFIMG_WEP){
        if(key[2] == 1){
            ret = 1;
        }
    }else{
        ;
    }

    return ret;
}

int get_key_mgmt(const char *ssid, int key_mgmt_info[])
{
    char *ptr = NULL, *pssid_start = NULL, *pssid_end = NULL;
    char *pst = NULL, *pend = NULL;
    char *pflag = NULL;
    char flag[128], pssid[SSID_LEN + 1];
    int  len = 0, i = 0;

    wmg_printf(MSG_DEBUG,"enter get_key_mgmt, ssid %s\n", ssid);

    key_mgmt_info[KEY_NONE_INDEX] = 0;
    key_mgmt_info[KEY_WEP_INDEX] = 0;
    key_mgmt_info[KEY_WPA_PSK_INDEX] = 0;
	key_mgmt_info[KEY_UNKOWN] = 0;
    /* first line end */
	if(direct_get_scan_results_inner(NULL,NULL) != 0) {
		wmg_printf(MSG_WARNING,"get scan result is null\n");
		return 0;
	}

	len = strlen(scan.results);

	if(len <= 48) {
		wmg_printf(MSG_ERROR,"get scan results is null\n");
		return 0;
	}

    ptr = strchr(scan.results, '\n');
	ptr ++;
    while(1){
        /* line end */
        pend = strchr(ptr, '\n');
        if (pend != NULL){
            *pend = '\0';
        }

        /* line start */
        pst = ptr;

        /* abstract ssid */
        pssid_start = strrchr(pst, '\t') + 1;
        strncpy(pssid, pssid_start, SSID_LEN);
        pssid[SSID_LEN] = '\0';

        /* find ssid in scan results */
	if(strcmp(pssid, ssid) == 0){
	    pflag = pst;
            for(i=0; i<3; i++){
                pflag = strchr(pflag, '\t');
                pflag++;
            }

            len = pssid_start - pflag;
            len = len - 1;
            strncpy(flag, pflag, len);
            flag[len] = '\0';
            wmg_printf(MSG_DEBUG,"ssid %s, flag %s\n", ssid, flag);
            if((strstr(flag, "WPA-PSK-") != NULL)
	    || (strstr(flag, "WPA2-PSK-") != NULL)){
                key_mgmt_info[KEY_WPA_PSK_INDEX] = 1;
            }else if(strstr(flag, "WEP") != NULL){
                key_mgmt_info[KEY_WEP_INDEX] = 1;
            }else if((strcmp(flag, "[ESS]") == 0) || (strcmp(flag, "[WPS][ESS]") == 0)){
                key_mgmt_info[KEY_NONE_INDEX] = 1;
            }else{
                key_mgmt_info[KEY_UNKOWN] = 1;
	    }
        }

        if(pend != NULL){
            *pend = '\n';
            //point next line
            ptr = pend+1;
        }else{
            break;
        }
    }
    return 0;


}
