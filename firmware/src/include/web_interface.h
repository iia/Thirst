#include "platform.h"
#include "httpd.h"
#include "httpdespfs.h"

#define JSON_STATUS_OK 1
#define JSON_STATUS_FAILED 2
#define REQUEST_JSON_GET_SETTINGS 1
#define REQUEST_JSON_GET_SENSOR_READING 2
#define REQUEST_JSON_SAVE_SETTINGS 3

static const char *fmt_response_json_request_save_settings = \
"{\
\"request\":%d,\
\"status\":%d,\
\"data\":\"%s\"\
}";

static const char *fmt_response_json_request_get_sensor_reading = \
"{\
\"request\":%d,\
\"status\":%d,\
\"data\":%d\
}";

static const char *fmt_response_json_request_get_settings = \
"{\
\"request\":%d, \
\"status\":%d, \
\"data\":%s \
}";

/*
static const char *fmt_get_settings_data = \
"{\
\"the_plant_name\":\"%s\", \
\"the_plant_configuration_password\":\"%s\", \
\"the_plant_wifi_ap\":\"%s\", \
\"the_plant_wifi_ap_password\":\"%s\", \
\"the_plant_threshold_percent\":%d, \
\"the_plant_threshold_lt_gt\":%d, \
\"registered_value\":%d, \
\"the_plant_check_frequency\":%d, \
\"notification_email\":\"%s\", \
\"notification_email_subject\":\"%s\", \
\"notification_email_message\":\"%s\"\
}";
*/

static const char *fmt_get_settings_data = \
"{\
\"the_plant_name\":\"%s\", \
\"the_plant_configuration_password\":\"%s\", \
\"the_plant_wifi_ap\":\"%s\", \
\"the_plant_wifi_ap_password\":\"%s\", \
\"the_plant_threshold_percent\":%d, \
\"the_plant_threshold_lt_gt\":%d, \
\"registered_value\":%d, \
\"notification_email\":\"%s\", \
\"notification_email_subject\":\"%s\", \
\"notification_email_message\":\"%s\"\
}";

int ICACHE_FLASH_ATTR
cgi_root(HttpdConnData *connection_data, char *token, void **arg);

int ICACHE_FLASH_ATTR
cgi_get_settings(HttpdConnData *connection_data);

int ICACHE_FLASH_ATTR
cgi_get_sensor_reading(HttpdConnData *connection_data);

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
	{"/save_settings.cgi", cgi_save_settings, NULL},
	{"*", cgiEspFsHook, NULL},
	{NULL, NULL, NULL}
};
