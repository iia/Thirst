#include "platform.h"
#include "httpd.h"
#include "httpdespfs.h"

#define JSON_STATUS_OK 1
#define JSON_STATUS_FAILED 2
#define REQUEST_JSON_GET_SETTINGS 1
#define REQUEST_JSON_GET_SENSOR_READING 2
#define REQUEST_JSON_SAVE_SETTINGS 3
#define REQUEST_JSON_START_WIFI_SCAN 4
#define REQUEST_JSON_GET_WIFI_SCAN_RESULT 5

static const char *fmt_response_json_request_data_json = \
"{\
\"request\":%d,\
\"status\":%d,\
\"data\":%s\
}";

static const char *fmt_response_json_request_data_integer = \
"{\
\"request\":%d,\
\"status\":%d,\
\"data\":%d\
}";

static const char *fmt_get_settings_data = \
"{\
\"the_plant_name\":\"%s\", \
\"the_plant_configuration_password\":\"%s\", \
\"the_plant_wifi_ap\":\"%s\", \
\"the_plant_wifi_ap_bssid\":\"%s\", \
\"the_plant_wifi_ap_password\":\"%s\", \
\"the_plant_threshold_percent\":%d, \
\"the_plant_threshold_lt_gt\":%d, \
\"registered_value\":%d, \
\"notification_email\":\"%s\", \
\"notification_email_subject\":\"%s\", \
\"notification_email_message\":\"%s\"\
}";

static const char *fmt_get_wifi_scan_result_data_json = \
"{\
\"b\":\"%s\",\
\"s\":\"%s\",\
\"c\":%d,\
\"r\":%d,\
\"a\":%d,\
\"h\":%d\
}";

int ICACHE_FLASH_ATTR
cgi_root(HttpdConnData *connection_data, char *token, void **arg);

int ICACHE_FLASH_ATTR
cgi_get_settings(HttpdConnData *connection_data);

int ICACHE_FLASH_ATTR
cgi_get_sensor_reading(HttpdConnData *connection_data);

int ICACHE_FLASH_ATTR
cgi_start_wifi_scan(HttpdConnData *connection_data);

int ICACHE_FLASH_ATTR
cgi_get_wifi_scan_result(HttpdConnData *connection_data);

void ICACHE_FLASH_ATTR
do_cgi_save_settings_fail_response_cleanup(HttpdConnData *connection_data,
	char *data, char *buffer_response);

int ICACHE_FLASH_ATTR
cgi_save_settings(HttpdConnData *connection_data);

static HttpdBuiltInUrl httpd_urls[] = {
	{"/", cgiRedirect, "/index.html"},
	{"/index.html", cgiEspFsTemplate, cgi_root},
	{"/get_settings.cgi", cgi_get_settings, NULL},
	{"/get_sensor_reading.cgi", cgi_get_sensor_reading, NULL},
	{"/start_wifi_scan.cgi", cgi_start_wifi_scan, NULL},
	{"/get_wifi_scan_result.cgi", cgi_get_wifi_scan_result, NULL},
	{"/save_settings.cgi", cgi_save_settings, NULL},
	{"*", cgiEspFsHook, NULL},
	{NULL, NULL, NULL}
};

