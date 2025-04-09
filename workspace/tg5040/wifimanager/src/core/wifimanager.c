#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/types.h>
#include <poll.h>
#include "wifi.h"
#include "wifi_event.h"
#include "scan.h"
#include "wpa_supplicant_conf.h"
#include "wifi_intf.h"
#include "wmg_debug.h"
#include "wifi_udhcpc.h"

#define WPA_SSID_LENTH  512

#define VERSION "18.10.31"

struct Manager wmg = {
	.StaEvt = {
		.state = DISCONNECTED,
		.event = WSE_UNKNOWN,
	},
	.ssid = NULL,
	.enable = 0,
};

struct Manager *w = &wmg;


static char wpa_scan_ssid[WPA_SSID_LENTH];
static char wpa_conf_ssid[WPA_SSID_LENTH];
static int  ssid_contain_chinese = 0;

static int aw_wifi_disconnect_ap(int event_label);

const char * wmg_state_txt(enum wmgState state)
{
	switch (state) {
	case DISCONNECTED:
		return "DISCONNECTED";
	case CONNECTING:
		return "CONNECTING";
	case CONNECTED:
		return "CONNECTED";
	case OBTAINING_IP:
		return "OBTAINING_IP";
	case NETWORK_CONNECTED:
		return "NETWORK_CONNECTED";
	default:
		return "UNKNOWN";
	}
}

const char* wmg_event_txt(enum wmgEvent event)
{
	switch (event) {
	case WSE_ACTIVE_CONNECT:
		return "WSE_ACTIVE_CONNECT";
	case WSE_WPA_TERMINATING:
		return "WSE_WPA_TERMINATING";
	case WSE_AP_ASSOC_REJECT:
		return "WSE_AP_ASSOC_REJECT";
	case WSE_NETWORK_NOT_EXIST:
		return "WSE_NETWORK_NOT_EXIST";
	case WSE_PASSWORD_INCORRECT:
		return "WSE_PASSWORD_INCORRECT";
	case WSE_OBTAINED_IP_TIMEOUT:
		return "WSE_OBTAINED_IP_TIMEOUT";
	case WSE_CONNECTED_TIMEOUT:
		return "WSE_CONNECTED_TIMEOUT";
	case WSE_DEV_BUSING:
		return "WSE_DEV_BUSING";
	case WSE_CMD_OR_PARAMS_ERROR:
		return "WSE_CMD_OR_PARAMS_ERROR";
	case WSE_KEYMT_NO_SUPPORT:
		return "WSE_KEYMT_NO_SUPPORT";
	case WSE_AUTO_DISCONNECTED:
		return "WSE_AUTO_DISCONNECTED";
	case WSE_ACTIVE_DISCONNECT:
		return "WSE_ACTIVE_DISCONNECT";
	case WSE_STARTUP_AUTO_CONNECT:
		return "WSE_STARTUP_AUTO_CONNECT";
	case WSE_AUTO_CONNECTED:
		return "WSE_AUTO_CONNECTED";
	case WSE_ACTIVE_OBTAINED_IP:
		return "WSE_ACTIVE_OBTAINED_IP";
	case WSE_UNKNOWN:
		return "WSE_UNKNOWN";
	default:
		return "UNKNOWN";
	}
}
int state_event_change(int label)
{
	wmg_printf(MSG_DEBUG,"event_label:%d\n",label);
	wmg_printf(MSG_DEBUG,"--->WMG_EVENT: %s\n",wmg_event_txt(w->StaEvt.event));
	wmg_printf(MSG_DEBUG,"--->WMG_STATE: %s\n",wmg_state_txt(w->StaEvt.state));
	call_state_callback_function(w,label);
}
static int aw_wifi_add_state_callback(tWifi_state_callback pcb)
{
      return add_wifi_state_callback_inner(pcb);
}
enum wmgState aw_wifi_get_wifi_state()
{
	return w->StaEvt.state;
}
enum wmgEvent aw_wifi_get_wifi_event()
{
	return w->StaEvt.event;
}

static int clearManagerdata()
{
	w->StaEvt.state = STATE_UNKNOWN;
	w->StaEvt.event = WSE_UNKNOWN;
	w->ssid = NULL;
	w->enable = false;
}

static int aw_wifi_ssid_is_connected_ap(char *ssid)
{
    int ret = 0;
    struct wpa_status * sta;

    if(! w->enable){
		wmg_printf(MSG_ERROR,"wpa_supplicant is closed\n");
        return -1;
    }

	if((sta = get_wpa_status_info()) != NULL){
		if(sta->wpa_state >= WPA_SCANNING){
			wmg_printf(MSG_INFO,"wpa_supplicant is busing now\n");
			return -1;
		}
		if(sta->wpa_state == WPA_COMPLETED){
			if(!strncmp(sta->ssid,ssid,strlen(ssid)) &&
				sta->ip_address[0] != '\0')
				return 1;
		}
	}else{
		wmg_printf(MSG_INFO,"get wpa status NULL\n");
	}

	return -1;
}


/*
*get wifi connection state with AP
*return value:
*1: connected with AP(connected to network IPv4)
*2: connected with AP(connected to network IPv6)
*0: disconnected with AP
*/
static int aw_wifi_is_ap_connected(char *ssid, int *len)
{
    int ret = 0;
    struct wpa_status * sta;;

    if(! w->enable){
        return -1;
    }

	sta = get_wpa_status_info();
	if(sta->wpa_state >= WPA_SCANNING){
		wmg_printf(MSG_INFO,"wpa_supplicant is busing now\n");
		return -1;
	}

    if( 4 == wpa_conf_is_ap_connected(ssid, len))
	ret = 1;
    else if( 6 == wpa_conf_is_ap_connected(ssid, len))
	ret = 2;
    else
	ret = 0;

    return ret;
}

static int aw_wifi_connection_info(connection_status *connection_info)
{
    int ret = 0;
    struct wpa_status * sta;
	signal_status signal_info;

    if(! w->enable){
		wmg_printf(MSG_ERROR,"wpa_supplicant is closed\n");
        return -1;
	}


	if((sta = get_wpa_status_info()) != NULL){
		if (sta->wpa_state != WPA_COMPLETED) {
			wmg_printf(MSG_INFO,"WIFI isn't connected to AP at current\n");
			return -1;
		} else {
			strncpy(connection_info->ssid, sta->ssid, strlen(sta->ssid));
			strncpy(connection_info->ip_address, sta->ip_address, strlen(sta->ip_address));
			connection_info->freq = sta->freq;

			ret = get_connection_info_inner(&signal_info);
			if (!ret) {
				connection_info->rssi = signal_info.rssi;
				connection_info->link_speed = signal_info.link_speed;
				connection_info->noise = signal_info.noise;
			} else {
				wmg_printf(MSG_INFO,"get signal info NULL\n");
			}
		}
	}else{
		wmg_printf(MSG_INFO,"get wpa status NULL\n");
	}

	return ret;
}

static int aw_wifi_get_scan_results(char *result, int *len)
{
    if(! w->enable){
        return -1;
    }

    if(direct_get_scan_results_inner(result, len) != 0)
    {
        wmg_printf(MSG_ERROR,"%s: There is a scan or scan_results error, Please try scan again later!\n", __func__);
        return -1;
    } else {
        return 0;
    }
}

/* check wpa/wpa2 passwd is right */
int check_wpa_passwd(const char *passwd)
{
    int result = 0;
    int i=0;

    if(!passwd || *passwd =='\0'){
        return 0;
    }

    for(i=0; passwd[i]!='\0'; i++){
        /* non printable char */
        if((passwd[i]<32) || (passwd[i] > 126)){
            result = 0;
            break;
        }
    }

    if(passwd[i] == '\0'){
        result = 1;
    }

    return result;
}

/* convert app ssid which contain chinese in utf-8 to wpa scan ssid */
static int ssid_app_to_wpa_scan(const char *app_ssid, char *scan_ssid)
{
    unsigned char h_val = 0, l_val = 0;
    int i = 0;
    int chinese_in = 0;

    if(!app_ssid || !app_ssid[0])
    {
        wmg_printf(MSG_ERROR,"Error: app ssid is NULL!\n");
        return -1;
    }

    if(!scan_ssid)
    {
        wmg_printf(MSG_ERROR,"Error: wpa ssid buf is NULL\n");
        return -1;
    }

    i = 0;
    while(app_ssid[i] != '\0')
    {
        /* ascii code */
        if((unsigned char)app_ssid[i] <= 0x7f)
        {
            *(scan_ssid++) = app_ssid[i++];
        }
        else /* covert to wpa ssid for chinese code */
        {
            *(scan_ssid++) = '\\';
            *(scan_ssid++) = 'x';
            h_val = (app_ssid[i] & 0xf0) >> 4;
            if((h_val >= 0) && (h_val <= 9)){
                *(scan_ssid++) = h_val + '0';
            }else if((h_val >= 0x0a) && (h_val <= 0x0f)){
                *(scan_ssid++) = h_val + 'a' - 0xa;
            }

            l_val = app_ssid[i] & 0x0f;
            if((l_val >= 0) && (l_val <= 9)){
                *(scan_ssid++) = l_val + '0';
            }else if((l_val >= 0x0a) && (l_val <= 0x0f)){
                *(scan_ssid++) = l_val + 'a' - 0xa;
            }
            i++;
            chinese_in = 1;
        }
    }
    *scan_ssid = '\0';

    if(chinese_in == 1){
        return 1;
    }

    return 0;
}

/* convert app ssid which contain chinese in utf-8 to wpa conf ssid */
static int ssid_app_to_wpa_conf(const char *app_ssid, char *conf_ssid)
{
    unsigned char h_val = 0, l_val = 0;
    int i = 0;
    int chinese_in = 0;

    if(!app_ssid || !app_ssid[0])
    {
        wmg_printf(MSG_ERROR,"Error: app ssid is NULL!\n");
        return -1;
    }

    if(!conf_ssid)
    {
        wmg_printf(MSG_ERROR,"Error: wpa ssid buf is NULL\n");
        return -1;
    }

    i = 0;
    while(app_ssid[i] != '\0')
    {
        h_val = (app_ssid[i] & 0xf0) >> 4;
        if((h_val >= 0) && (h_val <= 9)){
            *(conf_ssid++) = h_val + '0';
        }else if((h_val >= 0x0a) && (h_val <= 0x0f)){
            *(conf_ssid++) = h_val + 'a' - 0xa;
        }

        l_val = app_ssid[i] & 0x0f;
        if((l_val >= 0) && (l_val <= 9)){
            *(conf_ssid++) = l_val + '0';
        }else if((l_val >= 0x0a) && (l_val <= 0x0f)){
            *(conf_ssid++) = l_val + 'a' - 0xa;
        }
        i++;
    }
    *conf_ssid = '\0';

    return 0;
}

static int connect_command_handle(char *cmd,char *net_id)
{
    int ret;
    char reply[REPLY_BUF_SIZE] = {0};
	wmg_printf(MSG_EXCESSIVE,"connect handle cmd is %s\n",cmd);
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"%s failed,Remove the information just connected!\n",cmd);
        sprintf(cmd, "REMOVE_NETWORK %s", net_id);
        wifi_command(cmd, reply, sizeof(reply));
        sprintf(cmd, "%s", "SAVE_CONFIG");
        wifi_command(cmd, reply, sizeof(reply));
        ret = -1;
		return ret;
    }else{
        wmg_printf(MSG_EXCESSIVE,"%s: %s\n",cmd,reply);
        return 0;
    }
}

void cancel_saved_conf_handle(const char *net_id)
{
    char reply[REPLY_BUF_SIZE] = {0};

	char cmd[CMD_LEN+1] = {0};

	sprintf(cmd, "DISABLE_NETWORK %s", net_id);
	wifi_command(cmd, reply, sizeof(reply));

	sprintf(cmd, "%s", "DISCONNECT");
	wifi_command(cmd, reply, sizeof(reply));

	sprintf(cmd, "REMOVE_NETWORK %s", net_id);
	cmd[CMD_LEN] = '\0';
	wifi_command(cmd, reply, sizeof(reply));

	sprintf(cmd, "%s", "SAVE_CONFIG");
	wifi_command(cmd, reply, sizeof(reply));
}

int check_device_is_busing()
{
	if(w->StaEvt.state == CONNECTING ||
		w->StaEvt.state == OBTAINING_IP){
		return -1;
	}else {
		return 0;
	}
}

static int wait_event(const char *netIdOld,const char *netIdNew,int isExist)
{
	char reply[REPLY_BUF_SIZE] = {0};
	char cmd[CMD_LEN+1] = {0};
	int ret ;
	enum wpaEvent evt = WPAE_UNKNOWN;

	a->assocRejectCnt = 0;
	a->netNotFoundCnt = 0;
	a->authFailCnt = 0;
        wmg_printf(MSG_DEBUG,"start reading WPA EVENT!\n");
	ret = evtRead(&evt);
        wmg_printf(MSG_DEBUG,"reading WPA EVENT is over!\n");
	wmg_printf(MSG_MSGDUMP,"ret = %d,event = %d\n",ret,evt);
	if(ret >= 0) {
		switch(evt){
			case WPAE_CONNECTED:
				if(isExist == 1 || isExist == 3){
				  //network is exist or connected
					sprintf(cmd, "REMOVE_NETWORK %s", netIdOld);
					cmd[CMD_LEN] = '\0';
					wifi_command(cmd, reply, sizeof(reply));
				}
				if(isExist != -1){
					/* save config */
					sprintf(cmd, "%s", "SAVE_CONFIG");
					wifi_command(cmd, reply, sizeof(reply));
					wmg_printf(MSG_DEBUG,"wifi connected in inner!\n");
				}
				w->StaEvt.state = CONNECTED;
				break;

			case WPAE_PASSWORD_INCORRECT:
				wmg_printf(MSG_DEBUG,"password incorrect!\n");
				w->StaEvt.event = WSE_PASSWORD_INCORRECT;
				break;

			case WPAE_NETWORK_NOT_FOUND:
				wmg_printf(MSG_DEBUG,"network not found!\n");
				w->StaEvt.event = WSE_NETWORK_NOT_EXIST;
				break;

			case WPAE_ASSOC_REJECT:
				wmg_printf(MSG_DEBUG,"assoc reject!\n");
				w->StaEvt.event = WSE_AP_ASSOC_REJECT;
				break;

			case WPAE_TERMINATING:
				wmg_printf(MSG_DEBUG,"wpa_supplicant terminating!\n");
				w->StaEvt.event = WSE_WPA_TERMINATING;
				break;

			default:
				break;
		}
		if(evt != WPAE_CONNECTED){
			if(ret == 0){
				w->StaEvt.event = WSE_CONNECTED_TIMEOUT;
				wmg_printf(MSG_DEBUG,"connected timeout!\n");
			}
			if(netIdNew)
				cancel_saved_conf_handle(netIdNew);
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
		}
	}
	return ret;
}

/* connect visiable network */
static int aw_wifi_add_network(const char *ssid, tKEY_MGMT key_mgmt, const char *passwd, int event_label)
{
    int i=0, ret = -1, len = 0, max_prio = -1;
    char cmd[CMD_LEN+1] = {0};
    char reply[REPLY_BUF_SIZE] = {0}, netid1[NET_ID_LEN+1]={0}, netid2[NET_ID_LEN+1] = {0};
    int is_exist = 0;
    int passwd_len = 0;
    const char *p_ssid = NULL;

    if(! w->enable){
	    wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
	    return -1;
    }

    if(!ssid || !ssid[0]){
        wmg_printf(MSG_ERROR,"Error: ssid is NULL!\n");
        ret = -1;
        w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
        goto end;
    }

	if(check_device_is_busing() < 0){
        w->StaEvt.event = WSE_DEV_BUSING;
		w->StaEvt.state = DISCONNECTED;
		state_event_change(event_label);
        return -1;
	}

	w->StaEvt.state = CONNECTING;
	w->StaEvt.event = WSE_ACTIVE_CONNECT;
	w->ssid = ssid;
	state_event_change(event_label);

	clearEvtSocket();

    /* connecting */
 //   set_wifi_machine_state(CONNECTING_STATE);
	state_event_change(event_label);
    /* set connecting event label */
    a->label= event_label;

    /* convert app ssid to wpa scan ssid */
    ret = ssid_app_to_wpa_scan(ssid, wpa_scan_ssid);
    if(ret < 0){
        ret = -1;
        w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
        goto end;
    }else if(ret > 0){
        ssid_contain_chinese = 1;
    }else {
        ssid_contain_chinese = 0;
    }

    /* has no chinese code */
    if(ssid_contain_chinese == 0){
        p_ssid = ssid;
    }else{
        ssid_app_to_wpa_conf(ssid, wpa_conf_ssid);
        p_ssid = wpa_conf_ssid;
    }

    /* check already exist or connected */
    len = NET_ID_LEN+1;
    is_exist = wpa_conf_is_ap_exist(p_ssid, key_mgmt, netid1, &len);

	/* add network */
	strncpy(cmd, "ADD_NETWORK", CMD_LEN);
	cmd[CMD_LEN] = '\0';
	ret = wifi_command(cmd, netid2, sizeof(netid2));
	if(ret){
		wmg_printf(MSG_ERROR,"do add network results error!\n");
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}
	/* set network ssid */
	if(ssid_contain_chinese == 0){
		sprintf(cmd, "SET_NETWORK %s ssid \"%s\"", netid2, p_ssid);
	}else{
		sprintf(cmd, "SET_NETWORK %s ssid %s", netid2, p_ssid);
	}

	if(connect_command_handle(cmd,netid2)){
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}

	/* no passwd */
	if (key_mgmt == WIFIMG_NONE){
		/* set network no passwd */
		sprintf(cmd, "SET_NETWORK %s key_mgmt NONE", netid2);
		if(connect_command_handle(cmd,netid2)){
			ret = -1;
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			goto end;
		}
	} else if(key_mgmt == WIFIMG_WPA_PSK || key_mgmt == WIFIMG_WPA2_PSK){
		/* set network psk passwd */
		sprintf(cmd,"SET_NETWORK %s key_mgmt WPA-PSK", netid2);
		if(connect_command_handle(cmd,netid2)){
			ret = -1;
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			goto end;
		}

        ret = check_wpa_passwd(passwd);
        if(ret == 0){
            wmg_printf(MSG_ERROR,"check wpa-psk passwd is error!\n");
			cancel_saved_conf_handle(netid2);
            ret = -1;
            w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
            goto end;
        }

	    sprintf(cmd, "SET_NETWORK %s psk \"%s\"", netid2, passwd);
		if(connect_command_handle(cmd,netid2)){
			ret = -1;
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			goto end;
		}

	  } else if(key_mgmt == WIFIMG_WEP){
        /* set network  key_mgmt none */
		sprintf(cmd, "SET_NETWORK %s key_mgmt NONE", netid2);
		if(connect_command_handle(cmd,netid2)){
			ret = -1;
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			goto end;
		}

        /* set network wep_key0 */
		passwd_len = strlen(passwd);
		if((passwd_len == 10) || (passwd_len == 26)) {
		    sprintf(cmd, "SET_NETWORK %s wep_key0 %s", netid2, passwd);
		    wmg_printf(MSG_DEBUG,"The passwd is HEX format!\n");
		} else if((passwd_len == 5) || (passwd_len == 13)) {
		    sprintf(cmd, "SET_NETWORK %s wep_key0 \"%s\"", netid2, passwd);
		    wmg_printf(MSG_DEBUG,"The passwd is ASCII format!\n");
		} else {
		    wmg_printf(MSG_ERROR,"The password does not conform to the specification!\n");
		    /* cancel saved in wpa_supplicant.conf */
		    cancel_saved_conf_handle(netid2);
		    w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		    w->StaEvt.state = DISCONNECTED;
		    ret = -1;
		    goto end;
		}
		if(connect_command_handle(cmd,netid2)){
			ret = -1;
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			goto end;
		}


        /* set network auth_alg */
		sprintf(cmd, "SET_NETWORK %s auth_alg OPEN SHARED", netid2);
		if(connect_command_handle(cmd,netid2)){
			ret = -1;
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			goto end;
		}

	  } else {
	      wmg_printf(MSG_ERROR,"Error: key mgmt not support!\n");

	      /* cancel saved in wpa_supplicant.conf */
	      cancel_saved_conf_handle(netid2);
	      ret = -1;
	      w->StaEvt.event = WSE_KEYMT_NO_SUPPORT;
	      w->StaEvt.state = DISCONNECTED;
	      goto end;
	  }

	/* set scan_ssid to 1 for network */
    sprintf(cmd,"SET_NETWORK %s scan_ssid 1", netid2);
	if(connect_command_handle(cmd,netid2)){
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}


	  /* get max priority in wpa_supplicant.conf */
    max_prio =  wpa_conf_get_max_priority();

    /* set priority for network */
    sprintf(cmd,"SET_NETWORK %s priority %d", netid2, (max_prio+1));
	if(connect_command_handle(cmd,netid2)){
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}

	/* select network */
	sprintf(cmd, "SELECT_NETWORK %s", netid2);
	if(connect_command_handle(cmd,netid2)){
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}
    /* save config */
	  sprintf(cmd, "%s", "SAVE_CONFIG");
	if(connect_command_handle(cmd,netid2)){
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}

    /* save netid */
    strcpy(a->netIdConnecting, netid2);

    /* wait for check status connected/disconnected */
	ret = wait_event(netid1,netid2,is_exist);

end:

	state_event_change(event_label);
	//restore state when call wrong
    return ret;
}

static int wifi_connect_ap_inner(const char *ssid, tKEY_MGMT key_mgmt, const char *passwd, int event_label)
{
    int i=0, ret = -1, len = 0, max_prio = -1;
    char cmd[CMD_LEN+1] = {0};
    char reply[REPLY_BUF_SIZE] = {0}, netid1[NET_ID_LEN+1]={0}, netid2[NET_ID_LEN+1] = {'\0'};
    int is_exist = 0;
    int passwd_len = 0;

	w->StaEvt.state = CONNECTING;
	w->StaEvt.event = WSE_ACTIVE_CONNECT;
	w->ssid = ssid;

	state_event_change(event_label);

    /* set connecting event label */
    a->label= event_label;

	/*clear event data in socket*/
	clearEvtSocket();

    /* check already exist or connected */
    len = NET_ID_LEN+1;
    is_exist = wpa_conf_is_ap_exist(ssid, key_mgmt, netid1, &len);

    /* add network */
    strncpy(cmd, "ADD_NETWORK", CMD_LEN);
    cmd[CMD_LEN] = '\0';
    ret = wifi_command(cmd, netid2, sizeof(netid2));
    if(ret){
        wmg_printf(MSG_ERROR,"do add network results error!\n");
        ret = -1;
		w->StaEvt.event= WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
        goto end;
    }
	  /* set network ssid */
    if(ssid_contain_chinese == 0){
        sprintf(cmd, "SET_NETWORK %s ssid \"%s\"", netid2, ssid);
    }else{
        sprintf(cmd, "SET_NETWORK %s ssid %s", netid2, ssid);
    }
	wmg_printf(MSG_EXCESSIVE,"ssid:%s id:%s\n",ssid,netid2);

	if(connect_command_handle(cmd,netid2)){
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}

	  /* no passwd */
	  if (key_mgmt == WIFIMG_NONE){
	      /* set network no passwd */
	  sprintf(cmd, "SET_NETWORK %s key_mgmt NONE", netid2);
	  if(connect_command_handle(cmd,netid2)){
		  w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		  w->StaEvt.state = DISCONNECTED;
		  ret = -1;
		  goto end;
	  }
	  } else if(key_mgmt == WIFIMG_WPA_PSK || key_mgmt == WIFIMG_WPA2_PSK){
	      /* set network psk passwd */
	      sprintf(cmd,"SET_NETWORK %s key_mgmt WPA-PSK", netid2);
		  if(connect_command_handle(cmd,netid2)){
			  w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			  w->StaEvt.state = DISCONNECTED;
			  ret = -1;
			  goto end;
		  }

        ret = check_wpa_passwd(passwd);
        if(ret == 0){
            wmg_printf(MSG_ERROR,"check wpa-psk passwd is error!\n");

            /* cancel saved in wpa_supplicant.conf */
            sprintf(cmd, "REMOVE_NETWORK %s", netid2);
            wifi_command(cmd, reply, sizeof(reply));
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
            ret = -1;
            goto end;
        }

		sprintf(cmd, "SET_NETWORK %s psk \"%s\"", netid2, passwd);
		if(connect_command_handle(cmd,netid2)){
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}
	  } else if(key_mgmt == WIFIMG_WEP){
        /* set network  key_mgmt none */
		sprintf(cmd, "SET_NETWORK %s key_mgmt NONE", netid2);
		if(connect_command_handle(cmd,netid2)){
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}

        /* set network wep_key0 */
		passwd_len = strlen(passwd);
		if((passwd_len == 10) || (passwd_len == 26)) {
		    sprintf(cmd, "SET_NETWORK %s wep_key0 %s", netid2, passwd);
		    wmg_printf(MSG_DEBUG,"The passwd is HEX format!\n");
		} else if((passwd_len == 5) || (passwd_len == 13)) {
		    sprintf(cmd, "SET_NETWORK %s wep_key0 \"%s\"", netid2, passwd);
		    wmg_printf(MSG_DEBUG,"The passwd is ASCII format!\n");
		} else {
		    wmg_printf(MSG_ERROR,"The password does not conform to the specification!\n");
		    /* cancel saved in wpa_supplicant.conf */
		    cancel_saved_conf_handle(netid2);
		    w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		    w->StaEvt.state = DISCONNECTED;
		    ret = -1;
		    goto end;
		}
		if(connect_command_handle(cmd,netid2)){
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}

        /* set network auth_alg */
		sprintf(cmd, "SET_NETWORK %s auth_alg OPEN SHARED", netid2);
		if(connect_command_handle(cmd,netid2)){
			w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
			w->StaEvt.state = DISCONNECTED;
			ret = -1;
			goto end;
		}
	  } else {
		wmg_printf(MSG_ERROR,"Error: key mgmt is not support!\n");

		/* cancel saved in wpa_supplicant.conf */
		cancel_saved_conf_handle(netid2);
		w->StaEvt.event = WSE_KEYMT_NO_SUPPORT;
		w->StaEvt.state = DISCONNECTED;
		ret = -1;
		goto end;
	  }

	/* set scan_ssid to 1 for network */
	sprintf(cmd,"SET_NETWORK %s scan_ssid 1", netid2);
	if(connect_command_handle(cmd,netid2)) {
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}

	  /* get max priority in wpa_supplicant.conf */
    max_prio =  wpa_conf_get_max_priority();

    /* set priority for network */
    sprintf(cmd,"SET_NETWORK %s priority %d", netid2, (max_prio+1));
	if(connect_command_handle(cmd,netid2)){
		ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
		goto end;
	}

	  /* select network */
	  sprintf(cmd, "SELECT_NETWORK %s", netid2);
	  if(connect_command_handle(cmd,netid2)){
		  ret = -1;
		  w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		  w->StaEvt.state = DISCONNECTED;
		  goto end;
	  }

    /* save netid */
    strcpy(a->netIdConnecting, netid2);
	wmg_printf(MSG_DEBUG,"net id connecting %s\n",a->netIdConnecting);

	ret = wait_event(netid1,netid2,is_exist);
end:
    return ret;
}

/* connect visiable network */
static int aw_wifi_connect_ap_key_mgmt(const char *ssid, tKEY_MGMT key_mgmt, const char *passwd, int event_label)
{
	int ret = -1, key[4] = {0};

	const char *p_ssid = NULL;

	if(! w->enable){
	  wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
	  return -1;
	}

	if(!ssid || !ssid[0]){
	  wmg_printf(MSG_ERROR,"Error: ssid is NULL!\n");
	  ret = -1;
	  w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
	  w->StaEvt.state = DISCONNECTED;
	  goto end;
	}
	if(check_device_is_busing() < 0){
	  w->StaEvt.event = WSE_DEV_BUSING;
	  w->StaEvt.state = DISCONNECTED;
	  state_event_change(event_label);
	  return -1;
	}

    /* convert app ssid to wpa scan ssid */
    ret = ssid_app_to_wpa_scan(ssid, wpa_scan_ssid);
    if(ret < 0){
        ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
        goto end;
    }else if(ret > 0){
        ssid_contain_chinese = 1;
    }else {
        ssid_contain_chinese = 0;
    }

    /* has no chinese code */
    if(ssid_contain_chinese == 0){
        p_ssid = ssid;
    }else{
        ssid_app_to_wpa_conf(ssid, wpa_conf_ssid);
        p_ssid = wpa_conf_ssid;
    }

    /* checking network exist at first time */
    get_key_mgmt(wpa_scan_ssid, key);

	  /* no password */
    if (key_mgmt == WIFIMG_NONE){
        if(key[0] == 0){
            get_key_mgmt(wpa_scan_ssid, key);
            if(key[0] == 0){
                ret = -1;
				w->StaEvt.event = WSE_NETWORK_NOT_EXIST;
				w->StaEvt.state = DISCONNECTED;
                goto end;
            }
        }
	  }else if(key_mgmt == WIFIMG_WPA_PSK || key_mgmt == WIFIMG_WPA2_PSK){
        if(key[1] == 0){
            get_key_mgmt(wpa_scan_ssid, key);
            if(key[1] == 0){
                ret = -1;
				w->StaEvt.event = WSE_NETWORK_NOT_EXIST;
				w->StaEvt.state = DISCONNECTED;
                goto end;
            }
        }
    }else if(key_mgmt == WIFIMG_WEP){
        if(key[2] == 0){
            get_key_mgmt(wpa_scan_ssid, key);
            if(key[2] == 0){
                ret = -1;
				w->StaEvt.event = WSE_NETWORK_NOT_EXIST;
				w->StaEvt.state = DISCONNECTED;
                goto end;
            }
        }
    }else{
        ret = -1;
		w->StaEvt.event = WSE_KEYMT_NO_SUPPORT;
		w->StaEvt.state = DISCONNECTED;
        goto end;
    }

    ret = wifi_connect_ap_inner(p_ssid, key_mgmt, passwd, event_label);

end:
    //enable all networks in wpa_supplicant.conf
	wpa_conf_enable_all_networks();
	state_event_change(event_label);
    return ret;
}

static int aw_wifi_connect_ap(const char *ssid, const char *passwd, int event_label)
{
    int  i = 0, ret = 0;
    int  key[4] = {0};
    const char *p_ssid = NULL;

    if(! w->enable){
        wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
        return -1;
    }

    if(!ssid || !ssid[0]){
        wmg_printf(MSG_ERROR,"Error: ssid is NULL!\n");
        ret = -1;
		w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
		w->StaEvt.state = DISCONNECTED;
        goto end;
    }
	if(check_device_is_busing() < 0){
        w->StaEvt.event = WSE_DEV_BUSING;
		w->StaEvt.state = DISCONNECTED;
		state_event_change(event_label);
        return -1;
	}

     /* convert app ssid to wpa scan ssid */
    ret = ssid_app_to_wpa_scan(ssid, wpa_scan_ssid);
    if(ret < 0){
        ret = -1;
	w->StaEvt.event = WSE_CMD_OR_PARAMS_ERROR;
	w->StaEvt.state = DISCONNECTED;
        goto end;
    }else if(ret > 0){
        ssid_contain_chinese = 1;
    }else {
        ssid_contain_chinese = 0;
    }

    /* has no chinese code */
    if(ssid_contain_chinese == 0){
        p_ssid = ssid;
    }else{
        ssid_app_to_wpa_conf(ssid, wpa_conf_ssid);
        p_ssid = wpa_conf_ssid;
    }

    /* try connecting*/
    if(!passwd || !passwd[0]){
	    ret = wifi_connect_ap_inner(p_ssid, WIFIMG_NONE, passwd, event_label);
    } else {
	    ret = wifi_connect_ap_inner(p_ssid, WIFIMG_WPA_PSK, passwd, event_label);
    }

    if(ret  >= 0 || w->StaEvt.event == WSE_PASSWORD_INCORRECT
	    || w->StaEvt.event == WSE_WPA_TERMINATING)
	    goto end;

    wmg_printf(MSG_DEBUG,"The first connection failed,scan it and connect again\n");
    /*If the connection fails, scan it  and connect again. */



    /* checking network exist at first time */
    get_key_mgmt(p_ssid, key);

    /* no password */
    if(!passwd || !passwd[0]){
        if(key[0] == 0){
            ret = -1;
			w->StaEvt.event = WSE_NETWORK_NOT_EXIST;
			w->StaEvt.state = DISCONNECTED;
            goto end;
        }

        ret = wifi_connect_ap_inner(p_ssid, WIFIMG_NONE, passwd, event_label);
    }else{
        if((key[1] == 0) && (key[2] == 0) && (key[3] == 0)){
            ret = -1;
			w->StaEvt.event = WSE_NETWORK_NOT_EXIST;
			w->StaEvt.state = DISCONNECTED;
            goto end;
        }
        /* wpa-psk */
        if(key[1] == 1 || key[3] == 1){
            /* try WPA-PSK */
            ret = wifi_connect_ap_inner(p_ssid, WIFIMG_WPA_PSK, passwd, event_label);
        }

        /* wep */
        if(key[2] == 1){
        /* try WEP */
            ret = wifi_connect_ap_inner(p_ssid, WIFIMG_WEP, passwd, event_label);
        }
    }
end:
    //enable all networks in wpa_supplicant.conf
	wpa_conf_enable_all_networks();
	state_event_change(event_label);
    return ret;
}


static int aw_wifi_connect_ap_with_netid(const char *net_id, int event_label)
{

	int i=0, ret = -1, len = 0;
	char cmd[CMD_LEN+1] = {0};
	char reply[REPLY_BUF_SIZE] = {0};
	const char *p_ssid = NULL;
	struct wpa_status *sta;
	if(! w->enable){
		wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
		return -1;
	}

	if(check_device_is_busing() < 0){
        w->StaEvt.event = WSE_DEV_BUSING;
		w->StaEvt.state = DISCONNECTED;
		state_event_change(event_label);
        return -1;
	}

	if((sta = get_wpa_status_info()) != NULL){
		if(sta->wpa_state == WPA_COMPLETED){
			aw_wifi_disconnect_ap(0x7fffffff);
		}
	}else{
		wmg_printf(MSG_ERROR,"sta->wpa_state is NULL\n");
		ret = -1;
		goto end;
	}
	w->StaEvt.state = CONNECTING;
	w->ssid = net_id;
	w->StaEvt.event = WSE_ACTIVE_CONNECT;
	/* connecting */
	state_event_change(event_label);
	clearEvtSocket();

	/* selected_network */
	sprintf(cmd, "SELECT_NETWORK %s", net_id);
	ret = wifi_command(cmd, reply, sizeof(reply));
	if(ret){
		wmg_printf(MSG_ERROR,"do selected network error!\n");
		ret = -1;
		goto end;
	}

	 /* save netid */
	 strcpy(a->netIdConnecting, net_id);

     /*reconnect*/
	strncpy(cmd, "RECONNECT", CMD_LEN);
       cmd[CMD_LEN] = '\0';
	ret = wifi_command(cmd, reply, sizeof(reply));
	if(ret){
		wmg_printf(MSG_ERROR,"do reconnect error!\n");
		ret = -1;
	}
	ret = wait_event(net_id,NULL,-1);
end:

	state_event_change(event_label);
	return ret;
}

static int aw_wifi_clear_network(const char *ssid)
{
    int ret = 0;
	int len = 0;
    char cmd[CMD_LEN+1] = {0};
    char reply[REPLY_BUF_SIZE] = {0};
    char net_id[NET_ID_LEN+1] = {0};
	struct wpa_status *staInfo;
	char *ptr = NULL;
	char *ptr_id =NULL;
    if(! w->enable){
		wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
        return -1;
    }

    if(!ssid || !ssid[0]){
         wmg_printf(MSG_ERROR,"Error: ssid is null!\n");
        return -1;
    }

	if(check_device_is_busing() < 0){
		return -1;
	}

	/* cancel saved in wpa_supplicant.conf */
    strncpy(cmd, "LIST_NETWORKS", 15);
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do remove network %s error!\n", net_id);
		ret = -1;
		goto end;

    }

	ptr = strstr(reply, ssid);
	if(ptr == NULL)
		goto end;
	while(ptr != NULL) {
		ptr--;
		if(*ptr == '\n'){
			break;
		}
	}
	while(ptr != NULL){
		ptr++;
		if(*ptr >= '0' && *ptr <= '9'){
			ptr_id = ptr;
			break;
		}
	}

	if(NULL == ptr_id)
		return -1;

	while(*ptr >= '0' && *ptr <= '9'){
		len++;
		ptr++;
	}
	if(NULL == ptr_id)
		return -1;
	strncpy(net_id,ptr_id,len);
	net_id[len+1] = '\0';
	wmg_printf(MSG_DEBUG,"net id == %s\n",net_id);

    /* cancel saved in wpa_supplicant.conf */
    sprintf(cmd, "REMOVE_NETWORK %s", net_id);
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do remove network %s error!\n", net_id);
		ret = -1;
		goto end;

    }

    /* save config */
	sprintf(cmd, "%s", "SAVE_CONFIG");
	ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do save config error!\n");
		ret = -1;
		goto end;
    }
end:
    return ret;

}



/* cancel saved AP in wpa_supplicant.conf */
static int aw_wifi_remove_network(char *ssid, tKEY_MGMT key_mgmt)
{
    int ret = -1, len = 0;
    char cmd[CMD_LEN+1] = {0};
    char reply[REPLY_BUF_SIZE] = {0};
    char net_id[NET_ID_LEN+1] = {0};

    if(! w->enable){
		wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
        return -1;
    }

    if(!ssid || !ssid[0]){
         wmg_printf(MSG_ERROR,"Error: ssid is null!\n");
        return -1;
    }

	if(check_device_is_busing() < 0){
		return -1;
	}

    /* check AP is exist in wpa_supplicant.conf */
    len = NET_ID_LEN+1;
    ret = wpa_conf_ssid2netid(ssid, key_mgmt, net_id, &len);
    if(ret <= 0){
       wmg_printf(MSG_WARNING,"Warning: %s is not in wpa_supplicant.conf!\n", ssid);
	   ret = -1;
	   goto end;
    }else if(!(ret & (0x01<<1) ))
    {
		wmg_printf(MSG_WARNING,"Warning: %s exists in wpa_supplicant.conf, but the key_mgmt is not accordant!\n", ssid);
		ret = -1;
		goto end;
    }

    /* cancel saved in wpa_supplicant.conf */
    sprintf(cmd, "REMOVE_NETWORK %s", net_id);
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do remove network %s error!\n", net_id);
		ret = -1;
		goto end;

    }

    /* save config */
	  sprintf(cmd, "%s", "SAVE_CONFIG");
	  ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do save config error!\n");
		ret = -1;
		goto end;
    }
end:

    return ret;
}

static int aw_wifi_remove_all_networks()
{
    int ret = -1;
    struct wpa_status * sta;

    if(! w->enable){
		wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
        return -1;
    }

	if(check_device_is_busing() < 0){
		return -1;
	}

    ret = wpa_conf_remove_all_networks();

end:

    return ret;
}

static int aw_wifi_connect_ap_auto(int event_label)
{
    int i=0, ret = -1, len = 0;
    char cmd[CMD_LEN+1] = {0}, reply[REPLY_BUF_SIZE] = {0};
	struct wpa_status * sta;

    if(! w->enable){
		wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
        return -1;
    }

	if(check_device_is_busing() < 0){
		return -1;
	}

	if((sta = get_wpa_status_info()) != NULL){
		if(sta->wpa_state == WPA_COMPLETED){
			wmg_printf(MSG_INFO,"wpa_supplicant already connected\n");
			w->StaEvt.state = CONNECTED;
			w->StaEvt.event = WSE_ACTIVE_CONNECT;
			state_event_change(event_label);
			return 0;
		}
	}else{
		wmg_printf(MSG_ERROR,"sta->wpa_state is NULL\n");
		return -1;
	}

    /* check network exist in wpa_supplicant.conf */
    if(wpa_conf_network_info_exist() == 0){
        ret = -1;
        wmg_printf(MSG_INFO,"wpa_supplicant no history network information\n");
        goto end;
    }

    /* connecting */
	w->StaEvt.state = CONNECTING;
	w->StaEvt.event = WSE_ACTIVE_CONNECT;
	state_event_change(event_label);


    a->label= event_label;

    /* reconnected */
	sprintf(cmd, "%s", "RECONNECT");
    ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do reconnect error!\n");
        ret = -1;
		goto end;
    }

    /* wait for check status connected/disconnected */
	ret = wait_event(NULL,NULL,-1);

end:
	state_event_change(event_label);
    return ret;
}

static int aw_wifi_disconnect_ap(int event_label)
{
    int i=0, ret = -1, len = 0;
    char cmd[CMD_LEN+1] = {0}, reply[REPLY_BUF_SIZE] = {0};
    char netid[NET_ID_LEN+1]={0};
	struct wpa_status * sta;

    if(! w->enable){
		wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
        return -1;
    }

	if(check_device_is_busing() < 0){
		return -1;
	}

	if(w->StaEvt.state == DISCONNECTED){
		wmg_printf(MSG_WARNING,"The network has been disconnected\n");
		ret = -1;
		goto end;
	}

    /* set disconnect event label */
    a->label= event_label;

    /* disconnected */
	  sprintf(cmd, "%s", "DISCONNECT");
	  ret = wifi_command(cmd, reply, sizeof(reply));
    if(ret){
        wmg_printf(MSG_ERROR,"do disconnect network error!\n");
        ret = -1;
        goto end;
    }
    i=0;
    do{
        usleep(200000);
		if(w->StaEvt.state == DISCONNECTED){
			ret =0;
            break;
        }
        i++;
    }while(i<15);
	if(i >=15){
		wmg_printf(MSG_ERROR,"wait disconnect time out\n");
	}
end:

    return ret;
}

static int aw_wifi_list_networks(char *reply, size_t reply_len, int event_label)
{
	int i=0, ret = -1;
	char cmd[CMD_LEN+1] = {0};

	if(! w->enable){
		wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
		return -1;
	}

	if(check_device_is_busing() < 0){
		return -1;
	}

	/*there is no network information in the supplicant.conf file now!*/
	if(wpa_conf_network_info_exist() == 0){
		ret = 0;
		goto end;
	}

	strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
       cmd[CMD_LEN] = '\0';
	ret = wifi_command(cmd, reply, reply_len);
	if(ret){
		wmg_printf(MSG_ERROR,"do list_networks error!\n");
		ret = -1;
	}
end:

	return ret;

}

/*
*Ap with certain key_mgmt exists in the .conf file:return is 0, get the *net_id as expectation;
*else:return -1
*/
static int aw_wifi_get_netid(const char *ssid, tKEY_MGMT key_mgmt, char *net_id, int *length)
{
	int ret = -1, len = NET_ID_LEN+1;

	if(*length > (NET_ID_LEN+1))
	    len = NET_ID_LEN+1;
	else
	    len = *length;

	ret = wpa_conf_is_ap_exist(ssid, key_mgmt, net_id, &len);

	if(ret == 1 || ret == 3){
		*length = len;
		return 0;
	}else{
		return -1;
	}
}
static int aw_wifi_get_status(struct wifi_status *s)
{
	struct wpa_status *staInfo;

	if(! w->enable){
		wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
		return -1;
	}

	if(check_device_is_busing() < 0){
		return -1;
	}

	if((staInfo = get_wpa_status_info()) != NULL){
		/*wpa_supplicant state == connected*/
		if(staInfo->wpa_state == WPA_COMPLETED){
			/*not ip address*/
			if(staInfo->ip_address[0] == '\0'){
				wmg_printf(MSG_DEBUG,"connected AP,not ip\n");
				s->state = CONNECTED;
			} else {
				if(strlen(staInfo->ssid) > 64)
				{
					wmg_printf(MSG_ERROR,"===ssid name is too long===\n");
					return -1;
				}
				strncpy(s->ssid,staInfo->ssid,strlen(staInfo->ssid));
				wmg_printf(MSG_DEBUG,"connected AP:%s\n",s->ssid);
				s->state = NETWORK_CONNECTED;
			}
		} else {
			s->state = DISCONNECTED;
		}
	}
	return 0;
}

static int aw_wifi_wps_pbc(int event_label)
{
	int i=0, ret = -1;
	char cmd[CMD_LEN+1] = {0};
	char reply[REPLY_BUF_SIZE] = {0};

	if(! w->enable){
		wmg_printf(MSG_ERROR,"Not connected to wpa_supplicant\n");
		return -1;
	}

	if(check_device_is_busing() < 0){
		w->StaEvt.event = WSE_DEV_BUSING;
		w->StaEvt.state = DISCONNECTED;
		state_event_change(event_label);
		return -1;
	}

	w->StaEvt.state = CONNECTING;
	w->StaEvt.event = WSE_ACTIVE_CONNECT;
	state_event_change(event_label);

	/* set connecting event label */
	a->label= event_label;

	/*clear event data in socket*/
	clearEvtSocket();

	sprintf(cmd, "%s", "WPS_PBC");
	ret = wifi_command(cmd, reply, sizeof(reply));
	if(ret){
		wmg_printf(MSG_ERROR,"do wps_pbc error!\n");
		ret = -1;
		goto end;
	}

	/* save config */
	sprintf(cmd, "%s", "SAVE_CONFIG");
	ret = wifi_command(cmd, reply, sizeof(reply));
	if(ret){
		wmg_printf(MSG_ERROR,"do save config error!\n");
		ret = -1;
		goto end;
	}

	/* wait for check status connected*/
	ret = wait_event(NULL,NULL,-1);
end:
	state_event_change(event_label);
	if(aw_wifi_get_wifi_state() == NETWORK_CONNECTED) {
	    wmg_printf(MSG_INFO,"Wifi WPS connection: Success!\n");
	} else {
	    wmg_printf(MSG_ERROR,"Wifi WPS connection: Failure!\n");
	    wpa_conf_remove_maxnetid_network();
	}
	wpa_conf_enable_all_networks();
	return ret;
}

static const aw_wifi_interface_t aw_wifi_interface = {
    aw_wifi_add_state_callback,
    aw_wifi_ssid_is_connected_ap,
    aw_wifi_is_ap_connected,
	aw_wifi_connection_info,
    aw_wifi_get_scan_results,
    aw_wifi_connect_ap,
    aw_wifi_connect_ap_key_mgmt,
    aw_wifi_connect_ap_auto,
    aw_wifi_connect_ap_with_netid,
    aw_wifi_add_network,
    aw_wifi_disconnect_ap,
    aw_wifi_remove_network,
    aw_wifi_remove_all_networks,
    aw_wifi_list_networks,
    aw_wifi_get_netid,
    aw_wifi_get_status,
    aw_wifi_clear_network,
    aw_wifi_wps_pbc
};
const aw_wifi_interface_t * aw_wifi_on(tWifi_state_callback pcb,int event_label)
{
    int i = 0, ret = -1, connected = 0, len = 64;
    char ssid[64];
    struct wpa_status *staInfo;

    wmg_printf(MSG_DEBUG,"wifimanager Version: %s\n",VERSION);
    if(w->enable){
		wmg_printf(MSG_ERROR,"ERROR,Has been opened once!\n");
        return NULL;
    }

    w->StaEvt.state = CONNECTING;
    w->StaEvt.event = WSE_STARTUP_AUTO_CONNECT;

    ret = wifi_connect_to_supplicant();
#if 0
    if(ret) {
		wmg_printf(MSG_DEBUG,"wpa_suppplicant not running!\n");
		wifi_start_supplicant(0);

		do{
		    usleep(300000);
		    ret = wifi_connect_to_supplicant();
		    if(!ret){
			wmg_printf(MSG_DEBUG,"Connected to wpa_supplicant!\n");
			break;
		    }
		    i++;
		} while(ret && i<10);

		if(ret < 0){
		    wmg_printf(MSG_ERROR,"connect wpa_supplicant failed,please check wifi driver!\n");
		    return NULL;
		}
    }
#else
	if(ret < 0){
	    wmg_printf(MSG_ERROR,"connect wpa_supplicant failed,please check wifi driver!\n");
	    return NULL;
	}
#endif
    w->enable = true;

    aw_wifi_add_state_callback(pcb);

    wifi_start_event_loop();

    evtSocketInit();
    clearEvtSocket();

	staInfo = get_wpa_status_info();

    if(staInfo != NULL) {
		if(staInfo->wpa_state == WPA_INTERFACE_DISABLED) {
			system("ifconfig wlan0 up");
			staInfo = get_wpa_status_info();
			if(staInfo == NULL) {
				return NULL;
			}
		}
		w->ssid = staInfo->ssid;
		if(staInfo->wpa_state == WPA_4WAY_HANDSHAKE){
		    /* wpa_supplicant already run by other process and connected an ap */
//		    wait_event(NULL,NULL,-1);
			w->StaEvt.state = CONNECTING;
			w->ssid = staInfo->ssid;
		    state_event_change(event_label);
		}else if(staInfo->wpa_state == WPA_COMPLETED){
		    w->StaEvt.state = CONNECTED;
		    if(is_ip_exist() == 0){
				wmg_printf(MSG_DEBUG,"Wifi connected but not get ip!\n");
				state_event_change(event_label);
		    } else {
				wmg_printf(MSG_DEBUG,"Wifi already connect to %s\n",staInfo->ssid);
		    }
		}else {
			w->ssid = NULL;
		    w->StaEvt.state = DISCONNECTED;
		}
	}else{
		wmg_printf(MSG_ERROR,"sta->wpa_state is NULL\n");
		return NULL;
    }
    wmg_printf(MSG_DEBUG,"aw wifi on success!\n");
    return &aw_wifi_interface;
}

int aw_wifi_off(const aw_wifi_interface_t *p_wifi_interface)
{
    const aw_wifi_interface_t *p_aw_wifi_intf = &aw_wifi_interface;

    if(p_aw_wifi_intf != p_wifi_interface) {
	wmg_printf(MSG_ERROR,"aw wifi of failed !\n");
        return -1;
    }

    if(! w->enable) {
        return 0;
    }
    wpa_status_info_free();

    evtSockeExit();

    wifi_stop_event_loop();

    wifi_close_supplicant_connection();

//    wifi_stop_supplicant(0);
	system("ifconfig wlan0 down");

    clearManagerdata();

    system("/etc/wifi/udhcpc_wlan0 stop >/dev/null");

    reset_wifi_state_callback();

    wmg_printf(MSG_INFO,"aw wifi off success!\n");
    return 0;
}
