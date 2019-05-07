#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#define WEB_INTERFACE_JSON_STATUS_OK     1
#define WEB_INTERFACE_JSON_STATUS_FAILED 2

#define WEB_INTERFACE_REQ_JSON_GET_CONFIG           1
#define WEB_INTERFACE_REQ_JSON_SAVE_CONFIG          2
#define WEB_INTERFACE_REQ_JSON_START_WIFI_SCAN      3
#define WEB_INTERFACE_REQ_JSON_GET_SENSOR_READING   4
#define WEB_INTERFACE_REQ_JSON_GET_WIFI_SCAN_RESULT 5

#define WEB_INTERFACE_FORM_POST_BUFFER_SIZE 4096

static const char*
web_interface_fmt_json_resp_req_data = \
"{\
\"request\":%d,\
\"status\":%d,\
\"data\":%s\
}";

static const char*
web_interface_fmt_json_resp_req_data_int = \
"{\
\"request\":%d,\
\"status\":%d,\
\"data\":%d\
}";

static const char*
web_interface_fmt_json_config_data = \
"{\
\"plant_name\":\"%s\", \
\"configuration_password\":\"%s\", \
\"wifi_ap_ssid\":\"%s\", \
\"wifi_ap_bssid\":\"%s\", \
\"wifi_ap_password\":\"%s\", \
\"threshold_percent\":%d, \
\"threshold_lt_gt\":%d, \
\"registered_value\":%d, \
\"notification_email\":\"%s\", \
\"notification_email_subject\":\"%s\", \
\"notification_email_message\":\"%s\"\
}";

static const char*
web_interface_fmt_json_wifi_scan_result_data = \
"{\
\"bssid\":\"%s\",\
\"ssid\":\"%s\",\
\"channel\":%d,\
\"rssi\":%d,\
\"authmode\":%d,\
\"hidden\":%d\
}";

// WiFi scan.
struct web_interface_ap_list_element {
	sint8 rssi;
	uint8 channel;
	uint8 bssid[6];
	uint8 ssid[32];
	uint8 ssid_len;
	uint8 hidden;
	AUTH_MODE authmode;
	SLIST_ENTRY(web_interface_ap_list_element) next;
};

SLIST_HEAD(web_interface_ap_list_head, web_interface_ap_list_element);
typedef struct web_interface_ap_list_head WEB_INTERFACE_AP_LIST_HEAD_t;
typedef struct web_interface_ap_list_element WEB_INTERFACE_AP_LIST_ELEM_t;

WEB_INTERFACE_AP_LIST_HEAD_t web_interface_ap_list_head;

// Functions.

bool ICACHE_FLASH_ATTR
web_interface_init(void);

void ICACHE_FLASH_ATTR
web_interface_cb_wifi_scan_done(
	void* bss_info_list,
	STATUS status
);

bool ICACHE_FLASH_ATTR
web_interface_start_wifi_scan(void);

void ICACHE_FLASH_ATTR
web_interface_cb_timeout_restart(void);

bool ICACHE_FLASH_ATTR
web_interface_save_config_to_flash(void);

void ICACHE_FLASH_ATTR
web_interface_clear_wifi_scan_result(
	WEB_INTERFACE_AP_LIST_HEAD_t* web_interface_ap_list_head
);

void ICACHE_FLASH_ATTR
web_interface_get_wifi_scan_result(char** buffer_response_data);

// CGI functions.

int ICACHE_FLASH_ATTR
web_interface_cgi_root(
	HttpdConnData* connection_data,
	char* token,
	void** arg
);

int ICACHE_FLASH_ATTR
web_interface_cgi_get_config(
	HttpdConnData* connection_data
);

int ICACHE_FLASH_ATTR
web_interface_cgi_save_config(
	HttpdConnData* connection_data
);

int ICACHE_FLASH_ATTR
web_interface_cgi_start_wifi_scan(
	HttpdConnData* connection_data
);

int ICACHE_FLASH_ATTR
web_interface_cgi_get_sensor_reading(
	HttpdConnData* connection_data
);

int ICACHE_FLASH_ATTR
web_interface_cgi_get_wifi_scan_result(
	HttpdConnData* connection_data
);

// Entries are of form: URL ending, Fuction to execute, Function parameter.
static HttpdBuiltInUrl web_interface_httpd_urls[] = {
	{
		"/",
		cgiRedirect,
		"/index.html"
	},
	{
		"/index.html",
		cgiEspFsTemplate,
		web_interface_cgi_root
	},
	{
		"/get_config.cgi",
		web_interface_cgi_get_config,
		NULL
	},
	{
		"/start_wifi_scan.cgi",
		web_interface_cgi_start_wifi_scan,
		NULL
	},
	{
		"/get_wifi_scan_result.cgi",
		web_interface_cgi_get_wifi_scan_result,
		NULL
	},
	{
		"/get_sensor_reading.cgi",
		web_interface_cgi_get_sensor_reading,
		NULL
	},
	{
		"/save_config.cgi",
		web_interface_cgi_save_config,
		NULL
	},
	{
		"*",
		cgiEspFsHook,
		NULL
	},
	{
		NULL,
		NULL,
		NULL
	}
};

#endif // WEB_INTERFACE_H
