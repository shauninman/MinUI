#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "wpa_supplicant_conf.h"
#include "wifi.h"
#include "wmg_debug.h"
#include "wifi_event.h"
#include "wifi_udhcpc.h"

int wpa_conf_network_info_exist()
{
    int ret = -1;
    char cmd[256] = {0}, reply[REPLY_BUF_SIZE] = {0};

    strncpy(cmd, "LIST_NETWORKS", 255);
    cmd[255] = '\0';
    wifi_command(cmd, reply, sizeof(reply));

    if(strchr(reply, '\n') != NULL){
        return 1;
    }else{
        return 0;
    }
}

/*
 * get ap(ssid/key_mgmt) status in wpa_supplicant.conf
 * return
 * -1: not exist
 * 1:  exist but not connected
 * 3:  exist and connected; network id in buffer net_id
*/
int wpa_conf_is_ap_exist(const char *ssid, tKEY_MGMT key_mgmt, char *net_id, int *len)
{
    int ret = -1;
    char cmd[256] = {0};
    char reply[REPLY_BUF_SIZE] = {0}, key_type[128], key_reply[128];
    char *pssid_start=NULL, *pssid_end = NULL, *ptr=NULL;
    int flag = 0;

    if(!ssid || !ssid[0]){
        wmg_printf(MSG_ERROR,"Error: ssid is NULL!\n");
        return -1;
    }

    /* parse key_type */
    if(key_mgmt == WIFIMG_WPA_PSK || key_mgmt == WIFIMG_WPA2_PSK){
        strncpy(key_type, "WPA-PSK", 128);
    } else {
        strncpy(key_type, "NONE", 128);
    }

    strncpy(cmd, "LIST_NETWORKS", 255);
    cmd[255] = '\0';
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do list networks error!\n");
        return -1;
    }

    ptr = reply;
    while((pssid_start=strstr(ptr, ssid)) != NULL){
        char *p_s=NULL, *p_e=NULL, *p=NULL;

        pssid_end = pssid_start + strlen(ssid);
        /* ssid is presuffix of searched network */
        if(*pssid_end != '\t'){
            p_e = strchr(pssid_start, '\n');
            if(p_e != NULL){
                ptr = p_e;
                continue;
            }else{
                break;
            }
        }

        flag = 0;

        p_e = strchr(pssid_start, '\n');
        if(p_e){
            *p_e = '\0';
        }
        p_s = strrchr(ptr, '\n');
        p_s++;

        if(strstr(p_s, "CURRENT")){
            flag = 2;
        }

        p = strtok(p_s, "\t");
        if(p){
            if(net_id != NULL && *len > 0){
                strncpy(net_id, p, *len-1);
                net_id[*len-1] = '\0';
            }
        }

        /* get key_mgmt */
        sprintf(cmd, "GET_NETWORK %s key_mgmt", net_id);
        cmd[255] = '\0';
        ret = wifi_command(cmd, key_reply, sizeof(key_reply));
        if(ret){
            wmg_printf(MSG_ERROR,"do get network %s key_mgmt error!\n", net_id);
            return -1;
        }

        wmg_printf(MSG_DEBUG,"GET_NETWORK %s key_mgmt reply %s\n", net_id, key_reply);
        wmg_printf(MSG_DEBUG,"key type %s\n", key_type);

        if(strcmp(key_reply, key_type) == 0){
            flag += 1;
			*len = strlen(net_id);
            break;
        }

        if(p_e == NULL){
            break;
        }else{
            *p_e = '\n';
            ptr = p_e;
        }
    }

    return flag;
}

/*
 * ssid to netid
*/
int wpa_conf_ssid2netid(char *ssid, tKEY_MGMT key_mgmt, char *net_id, int *len)
{
    int ret = -1;
    char cmd[CMD_LEN + 1] = {0};
    char reply[REPLY_BUF_SIZE] = {0}, key_type[128], key_reply[128];
    char *pssid_start=NULL, *pssid_end=NULL, *ptr=NULL;
    int flag = 0;

    /* parse key_type */
    if(key_mgmt == WIFIMG_WPA_PSK || key_mgmt == WIFIMG_WPA2_PSK){
        strncpy(key_type, "WPA-PSK", 128);
    } else {
        strncpy(key_type, "NONE", 128);
    }

    /* list ap in wpa_supplicant.conf */
    strncpy(cmd, "LIST_NETWORKS", 15);
    cmd[15] = '\0';
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do list networks error!\n");
        return -1;
    }

    ptr = reply;
    while((pssid_start=strstr(ptr, ssid)) != NULL){
        char *p_s=NULL, *p_e=NULL, *p_t=NULL;

        pssid_end = pssid_start + strlen(ssid);
        /* ssid is presuffix of searched network */
        if(*pssid_end != '\t'){
            p_e = strchr(pssid_start, '\n');
            if(p_e != NULL){
                ptr = p_e;
                continue;
            }else{
                break;
            }
        }

       flag |= (0x01 << 0);

        p_e = strchr(pssid_start, '\n');
        if(p_e){
            *p_e = '\0';
        }
        p_s = strrchr(ptr, '\n');
        p_s++;



        p_t = strchr(p_s, '\t');
        if(p_t){
	   int tmp = 0;
            tmp = p_t - p_s;
            if(tmp <= NET_ID_LEN){
            strncpy(net_id, p_s, tmp);
            net_id[tmp] = '\0';
            }
        }

        /* get key_mgmt */
        sprintf(cmd, "GET_NETWORK %s key_mgmt", net_id);
        cmd[CMD_LEN] = '\0';
        ret = wifi_command(cmd, key_reply, sizeof(key_reply));
        if(ret){
            wmg_printf(MSG_ERROR,"do get network %s key_mgmt error!\n", net_id);
            return -1;
        }

        if(strcmp(key_reply, key_type) == 0){
           flag |= (0x01 << 1);
	  *len =  strlen(net_id);
            break;
        }

        if(p_e == NULL){
            break;
        }else{
            *p_e = '\n';
            ptr = p_e;
        }
    }

    return flag;
}

/*
 * Get max priority val in wpa_supplicant.conf
 * return
 *-1: error
 * 0: no network
 * >0: max val
 */
int wpa_conf_get_max_priority()
{
    int  ret = -1;
    int  val = -1, max_val = 0, len = 0;
    char cmd[CMD_LEN + 1] = {0}, reply[REPLY_BUF_SIZE] = {0}, priority[32] = {0};
    char net_id[NET_ID_LEN+1];
    char *p_n = NULL, *p_t = NULL;

    /* list ap in wpa_supplicant.conf */
    strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
    cmd[CMD_LEN] = '\0';
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do list networks error!\n");
        return -1;
    }

    p_n = strchr(reply, '\n');
    while(p_n != NULL){
	  p_n++;
        if((p_t = strchr(p_n, '\t')) != NULL){
            len = p_t - p_n;
            if(len <= NET_ID_LEN){
               strncpy(net_id, p_n, len);
               net_id[len] = '\0';
            }
        }

        sprintf(cmd, "GET_NETWORK %s priority", net_id);
        ret = wifi_command(cmd, priority, sizeof(priority));
        if(ret){
            wmg_printf(MSG_ERROR,"do get network priority error!\n");
            return -1;
        }

        val = atoi(priority);
        if(val >= max_val){
            max_val = val;
        }

        p_n = strchr(p_n, '\n');
    }

    return max_val;
}

int wpa_conf_is_ap_connected(char *ssid, int *len)
{
    int ret = -1;
    char cmd[CMD_LEN+1] = {0};
    char reply[REPLY_BUF_SIZE] = {0};
    char *p_c=NULL, *p_str = NULL;
    char *p_s=NULL, *p_e=NULL, *p=NULL;
    int is_ap_connected = 0;
    int ssid_len = strlen(ssid);

    strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
    cmd[CMD_LEN] = '\0';

    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do list networks error!\n");
        return -1;
    }

    p_str = (char *)reply;
    while((p_c=strstr(p_str, "[CURRENT]")) != NULL){
	if(*(p_c + 9) != '\n' && *(p_c + 9) != '\0')
	{
		p_str = p_c+9;
		continue;
	}

        p_e = strchr(p_c, '\n');
        if(p_e){
            *p_e = '\0';
        }

        p_s = strrchr(p_str, '\n');
        p_s++;
        p = strtok(p_s, "\t");
        p = strtok(NULL, "\t");
	ssid_len = strlen(p);
        if(p){
            if(ssid != NULL && ssid_len > 0){
                strncpy(ssid, p, ssid_len);
                ssid[ssid_len] = '\0';
                *len = strlen(ssid);
		is_ap_connected = 1;
		break;
            }
        }

    }

    /* check ip exist */
    ret = is_ip_exist();
    if(ret > 0 && is_ap_connected == 1){
            return ret;
    }else{
        return is_ap_connected;
    }
}

int wpa_conf_get_netid_connected(char *net_id, int *len)
{
    int ret = -1;
    char cmd[CMD_LEN+1] = {0};
    char reply[REPLY_BUF_SIZE] = {0};
    char *p_c=NULL;

    strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
    cmd[CMD_LEN] = '\0';
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do list networks error!\n");
        return -1;
    }

    if ((p_c=strstr(reply, "CURRENT")) != NULL){
        char *p_s=NULL, *p_e=NULL, *p=NULL;
        p_e = strchr(p_c, '\n');
        if(p_e){
            *p_e = '\0';
        }

        p_s = strrchr(reply, '\n');
        p_s++;
        p = strtok(p_s, "\t");
        if(p){
            if(net_id != NULL && *len > 0){
                strncpy(net_id, p, *len-1);
                net_id[*len-1] = '\0';
                *len = strlen(net_id);
            }
        }

        return 1;
    } else {
        return 0;
    }

}

/*
 * 1. link to ap
 * 2. get ip addr
 *
*/
int wpa_conf_get_ap_connected(char *netid, int *len)
{
    int ret = -1;
    char cmd[CMD_LEN+1] = {0};
    char reply[REPLY_BUF_SIZE] = {0};
    char *p_c=NULL;

    strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
    cmd[CMD_LEN] = '\0';

    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do list networks error!\n");
        return -1;
    }

    if ((p_c=strstr(reply, "CURRENT")) != NULL){
        char *p_s=NULL, *p_e=NULL, *p=NULL;
        p_e = strchr(p_c, '\n');
        if(p_e){
            *p_e = '\0';
        }

        p_s = strrchr(reply, '\n');
        p_s++;
        p = strtok(p_s, "\t");
        if(p){
            if(netid != NULL && *len > 0){
                strncpy(netid, p, *len-1);
                netid[*len-1] = '\0';
                *len = strlen(netid);
            }
        }
		wmg_printf(MSG_EXCESSIVE,"net id %s\n",netid);
        return 1;
    } else {
	wmg_printf(MSG_EXCESSIVE,"no CURRENT\n");
        return 0;
    }

}

int wpa_conf_enable_all_networks()
{
    int ret = -1, len = 0;
    char cmd[CMD_LEN+1] = {0};
    char reply[REPLY_BUF_SIZE] = {0};
    char net_id[NET_ID_LEN+1] = {0};
    char *p_n = NULL, *p_t = NULL;

    /* list ap in wpa_supplicant.conf */
    strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
    cmd[CMD_LEN] = '\0';
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do list networks error!\n");
        return -1;
    }

    p_n = strchr(reply, '\n');
    while(p_n != NULL){
	  p_n++;
        if((p_t = strchr(p_n, '\t')) != NULL){
            len = p_t - p_n;
            if(len <= NET_ID_LEN){
               strncpy(net_id, p_n, len);
               net_id[len] = '\0';
            }
        }

        /* cancel saved in wpa_supplicant.conf */
        sprintf(cmd, "ENABLE_NETWORK %s", net_id);
        ret = wifi_command(cmd, reply, sizeof(reply));
        if(ret){
            wmg_printf(MSG_ERROR,"do enable network %s error!\n", net_id);
            return -1;
        }

        p_n = strchr(p_n, '\n');
    }

    /* save config */
	  sprintf(cmd, "%s", "SAVE_CONFIG");
	  ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do save config error!\n");
        return -1;
    }

    return 0;
}

int wpa_conf_remove_all_networks()
{
    int ret = -1, len = 0;
    char cmd[CMD_LEN+1] = {0};
    char reply[REPLY_BUF_SIZE] = {0};
    char net_id[NET_ID_LEN+1] = {0};
    char *p_n = NULL, *p_t = NULL;

    /* list ap in wpa_supplicant.conf */
    strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
    cmd[CMD_LEN] = '\0';
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do list networks error!\n");
        return -1;
    }

    p_n = strchr(reply, '\n');
    while(p_n != NULL){
	  p_n++;
        if((p_t = strchr(p_n, '\t')) != NULL){
            len = p_t - p_n;
            if(len <= NET_ID_LEN){
               strncpy(net_id, p_n, len);
               net_id[len] = '\0';
            }
        }

        /* cancel saved in wpa_supplicant.conf */
        sprintf(cmd, "REMOVE_NETWORK %s", net_id);
        ret = wifi_command(cmd, reply, sizeof(reply));
        if(ret){
            wmg_printf(MSG_ERROR,"do remove network %s error!\n", net_id);
            return -1;
        }

        p_n = strchr(p_n, '\n');
    }

    /* save config */
	  sprintf(cmd, "%s", "SAVE_CONFIG");
	  ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do save config error!\n");
        return -1;
    }

    return 0;
}

int wpa_conf_remove_maxnetid_network()
{
    int ret = -1, len = 0;
    char cmd[CMD_LEN+1] = {0};
    char reply[REPLY_BUF_SIZE] = {0};
    char net_id[NET_ID_LEN+1] = {0};
    char *p_n = NULL, *p_t = NULL;

    /* list ap in wpa_supplicant.conf */
    strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
    cmd[CMD_LEN] = '\0';
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do list networks error!\n");
        return -1;
    }

    p_n = strchr(reply, '\n');
    while(p_n != NULL){
	p_n++;
        if((p_t = strchr(p_n, '\t')) != NULL){
            len = p_t - p_n;
            if(len <= NET_ID_LEN){
               strncpy(net_id, p_n, len);
               net_id[len] = '\0';
            }
        }
        p_n = strchr(p_n, '\n');
    }

    /* cancel saved in wpa_supplicant.conf */
    sprintf(cmd, "REMOVE_NETWORK %s", net_id);
    wmg_printf(MSG_DEBUG,"remove network %s!\n", net_id);
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do remove network %s error!\n", net_id);
        return -1;
    }

    /* save config */
      sprintf(cmd, "%s", "SAVE_CONFIG");
      ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do save config error!\n");
        return -1;
    }

    return 0;
}
