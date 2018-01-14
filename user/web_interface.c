#include <stdlib.h>
#include <mem.h>
#include "user_main.h"
#include "web_interface.h"
#include "json/jsonparse.h"

uint32_t index_buffer_post_form = 0;

int ICACHE_FLASH_ATTR
cb_timer_deep_sleep(void) {
	system_restart();
}

int ICACHE_FLASH_ATTR
cgi_root(HttpdConnData *connection_data, char *token, void **arg) {
	#if(ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cgi_root()\n");
	#endif

	if (token == NULL) {
		return HTTPD_CGI_DONE;
	}

	if (os_strcmp(token, "PLANT_NAME") == 0) {
		httpdSend(connection_data, config_current->the_plant_name, -1);
	}

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
cgi_get_settings(HttpdConnData *connection_data) {
	char request[4];
	char *buffer_response = (uint8_t *)os_malloc(8192);
	char *buffer_response_data = (uint8_t *)os_malloc(4096);

	#if(ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cgi_get_settings()\n");
	#endif

	os_bzero(request, sizeof(request));
	os_bzero(buffer_response, sizeof(buffer_response));
	os_bzero(buffer_response_data, sizeof(buffer_response_data));

	// HTTP header.
	httpdStartResponse(connection_data, 200);
	httpdHeader(connection_data, "Content-Type", "text/json");
	httpdEndHeaders(connection_data);

	if((httpdFindArg(connection_data->post->buff,
	                 "request",
	                 request,
	                 sizeof(request))) > 0) {
		// Check request.
		if(strtoul(request, NULL, 10) != REQUEST_JSON_GET_SETTINGS) {
			os_sprintf(buffer_response,
			           fmt_response_json_request_data_json,
			           REQUEST_JSON_GET_SETTINGS,
			           JSON_STATUS_FAILED, "{}");

			httpdSend(connection_data, buffer_response, -1);

			os_free(buffer_response);
			os_free(buffer_response_data);

			return HTTPD_CGI_DONE;
		}
	}
	else {
		os_sprintf(buffer_response,
		           fmt_response_json_request_data_json,
		           REQUEST_JSON_GET_SETTINGS,
		           JSON_STATUS_FAILED,
		           "{}");

		/*
		 * FIXME: If send data size is large then it may require partial sends which
		 * is currently not handled.
		 */
		httpdSend(connection_data, buffer_response, -1);

		os_free(buffer_response);
		os_free(buffer_response_data);

		return HTTPD_CGI_DONE;
	}

	os_sprintf(buffer_response_data,
	           fmt_get_settings_data,
	           config_current->the_plant_name,
	           config_current->the_plant_configuration_password,
	           config_current->the_plant_wifi_ap,
	           config_current->the_plant_wifi_ap_bssid,
	           config_current->the_plant_wifi_ap_password,
	           config_current->the_plant_threshold_percent,
	           config_current->the_plant_threshold_lt_gt,
	           do_get_sensor_reading(ADC_SAMPLE_SIZE),
	           config_current->notification_email,
	           config_current->notification_email_subject,
	           config_current->notification_email_message);

	os_sprintf(buffer_response,
	           fmt_response_json_request_data_json,
	           REQUEST_JSON_GET_SETTINGS,
	           JSON_STATUS_OK,
	           buffer_response_data);

	httpdSend(connection_data, buffer_response, -1);

	os_free(buffer_response);
	os_free(buffer_response_data);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
cgi_get_sensor_reading(HttpdConnData *connection_data) {
	uint32_t sensor_reading = 0;
	char *buffer_response = (uint8_t *)os_malloc(512);

	#if(ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cgi_get_sensor_reading()\n");
	#endif

	os_bzero(buffer_response, sizeof(buffer_response));
	sensor_reading = do_get_sensor_reading(ADC_SAMPLE_SIZE);

	#if(ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Got sensor reading = %d\n", sensor_reading);
	#endif

	// HTTPD header.
	httpdStartResponse(connection_data, 200);
	httpdHeader(connection_data, "Content-Type", "text/json");
	httpdEndHeaders(connection_data);

	os_sprintf(buffer_response,
	           fmt_response_json_request_data_integer,
	           REQUEST_JSON_GET_SENSOR_READING,
	           JSON_STATUS_OK,
	           sensor_reading);

	httpdSend(connection_data, buffer_response, -1);

	os_free(buffer_response);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
cgi_start_wifi_scan(HttpdConnData *connection_data) {
	char *buffer_response = (uint8_t *)os_malloc(64);
	os_bzero(buffer_response, 64);

	#if(ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cgi_start_wifi_scan()\n");
	#endif

	if(do_start_wifi_scan()) {
		#if(ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi scan start OK\n");
		#endif

		os_sprintf(buffer_response,
		           fmt_response_json_request_data_json,
		           REQUEST_JSON_START_WIFI_SCAN,
		           JSON_STATUS_OK,
		           "{}");
	}
	else {
		#if(ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi scan start failed\n");
		#endif

		os_sprintf(buffer_response,
		           fmt_response_json_request_data_json,
		           REQUEST_JSON_START_WIFI_SCAN,
		           JSON_STATUS_FAILED,
		           "{}");
	}

	// HTTPD header.
	httpdStartResponse(connection_data, 200);
	httpdHeader(connection_data, "Content-Type", "text/json");
	httpdEndHeaders(connection_data);
	httpdSend(connection_data, buffer_response, -1);
	os_free(buffer_response);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
cgi_get_wifi_scan_result(HttpdConnData *connection_data) {
	char *buffer_response = NULL;
	char *_buffer_response = NULL;

	#if(ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cgi_get_wifi_scan_result()\n");
	#endif

	do_get_wifi_scan_result(&_buffer_response);

	buffer_response = (char *)os_malloc(os_strlen(_buffer_response) + 40);

	os_sprintf(buffer_response,
	           fmt_response_json_request_data_json,
	           REQUEST_JSON_GET_WIFI_SCAN_RESULT,
	           JSON_STATUS_OK,
	           _buffer_response);

	// HTTPD header.
	httpdStartResponse(connection_data, 200);
	httpdHeader(connection_data, "Content-Type", "text/json");
	httpdEndHeaders(connection_data);
	httpdSend(connection_data, buffer_response, -1);
	os_free(buffer_response);
	os_free(_buffer_response);

	return HTTPD_CGI_DONE;
}

void ICACHE_FLASH_ATTR
do_cgi_save_settings_fail_response(HttpdConnData *connection_data) {
	char *buffer_response = (uint8_t *)os_malloc(64);

	os_sprintf(buffer_response,
	           fmt_response_json_request_data_json,
	           REQUEST_JSON_SAVE_SETTINGS,
	           JSON_STATUS_FAILED,
	           "{}");

	httpdSend(connection_data, buffer_response, -1);

	os_free(buffer_response);
}

int ICACHE_FLASH_ATTR
cgi_save_settings(HttpdConnData *connection_data) {
	char registered_value[8];
	char the_plant_threshold_lt_gt[2];
	char the_plant_threshold_percent[4];
	char the_plant_name[CONFIG_SSID_LEN + 1];
	char the_plant_wifi_ap[CONFIG_SSID_LEN + 1];
	char the_plant_wifi_ap_bssid[CONFIG_BSSID_LEN];
	char notification_email[CONFIG_NOTIFICATION_EMAIL_LEN + 1];
	char the_plant_wifi_ap_password[CONFIG_SSID_PASSWORD_LEN + 1];
	char the_plant_configuration_password[CONFIG_SSID_PASSWORD_LEN + 1];
	char notification_email_subject[CONFIG_NOTIFICATION_SUBJECT_LEN + 1];
	char notification_email_message[CONFIG_NOTIFICATION_MESSAGE_LEN + 1];

	int type_json = 0;
	char *data_json = NULL;
	char buffer_response[64];
	char data_json_len_raw[8];
	uint32_t data_json_len = 0;
	struct jsonparse_state *parser_json = NULL;

	#if(ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cgi_save_settings()\n");
	#endif

	// Partial arrival processing.
	os_memcpy(&buffer_post_form[index_buffer_post_form],
	          connection_data->post->buff,
	          connection_data->post->buffLen);

	index_buffer_post_form += connection_data->post->buffLen;

	// More pending data.
	if(connection_data->post->len != connection_data->post->received) {
		return HTTPD_CGI_MORE;
	}

	// Reset buffer accumulator index.
	index_buffer_post_form = 0;

	// HTTP header.
	httpdStartResponse(connection_data, 200);
	httpdHeader(connection_data, "Content-Type", "text/json");
	httpdEndHeaders(connection_data);

	os_bzero(&data_json_len_raw, 8);

	if((httpdFindArg(buffer_post_form,
		               "data_json_len",
		               (char *)&data_json_len_raw, 8)) <= 0) {
		#if(ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Can't get JSON data length\n");
		#endif

		do_cgi_save_settings_fail_response(connection_data);

		return HTTPD_CGI_DONE;
	}

	// FIXME: Check strtoul() return value for error.
  data_json_len = strtoul((char *)&data_json_len_raw, NULL, 10) + 1;

	data_json = (char *)os_malloc(data_json_len);
	os_bzero(data_json, data_json_len);

	if((httpdFindArg(buffer_post_form,
	                 "data_json",
	                 data_json, data_json_len)) <= 0) {
		#if(ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Can't get JSON data\n");
		#endif

		os_free(data_json);

		do_cgi_save_settings_fail_response(connection_data);

		return HTTPD_CGI_DONE;
	}

	parser_json = (struct jsonparse_state *)os_malloc(sizeof(struct jsonparse_state));

	jsonparse_setup(parser_json, data_json, os_strlen(data_json));
	os_bzero(config_current, sizeof(config_current));

	while((type_json = jsonparse_next(parser_json)) != 0) {
		if (type_json == JSON_TYPE_PAIR_NAME) {
			if (jsonparse_strcmp_value(parser_json, "thePlantName") == 0) {
				os_bzero(&the_plant_name, CONFIG_SSID_LEN + 1);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&the_plant_name,
				                     CONFIG_SSID_LEN + 1);
				os_memcpy(config_current->the_plant_name,
				          &the_plant_name,
				          sizeof(the_plant_name));
			}
			else if (jsonparse_strcmp_value(parser_json, "thePlantConfigurationPassword") == 0) {
				os_bzero(&the_plant_configuration_password, CONFIG_SSID_PASSWORD_LEN + 1);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&the_plant_configuration_password,
				                     CONFIG_SSID_PASSWORD_LEN + 1);
				os_memcpy(config_current->the_plant_configuration_password,
				          &the_plant_configuration_password,
				          sizeof(the_plant_configuration_password));
			}
			else if (jsonparse_strcmp_value(parser_json, "thePlantWifiAP") == 0) {
				os_bzero(&the_plant_wifi_ap, CONFIG_SSID_LEN + 1);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&the_plant_wifi_ap,
				                     CONFIG_SSID_LEN + 1);
				os_memcpy(config_current->the_plant_wifi_ap,
				          the_plant_wifi_ap,
				          sizeof(the_plant_wifi_ap));
			}
			else if (jsonparse_strcmp_value(parser_json, "thePlantWifiAPBSSID") == 0) {
				os_bzero(&the_plant_wifi_ap_bssid, CONFIG_BSSID_LEN);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&the_plant_wifi_ap_bssid,
				                     CONFIG_BSSID_LEN);
				os_memcpy(config_current->the_plant_wifi_ap_bssid,
				          &the_plant_wifi_ap_bssid,
				          sizeof(the_plant_wifi_ap_bssid));
			}
			else if (jsonparse_strcmp_value(parser_json, "thePlantWifiAPPassword") == 0) {
				os_bzero(&the_plant_wifi_ap_password, CONFIG_SSID_PASSWORD_LEN + 1);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&the_plant_wifi_ap_password,
				                     CONFIG_SSID_PASSWORD_LEN + 1);
				os_memcpy(config_current->the_plant_wifi_ap_password,
				          &the_plant_wifi_ap_password,
				          sizeof(the_plant_wifi_ap_password));			}
			else if (jsonparse_strcmp_value(parser_json, "thePlantThresholdPercent") == 0) {
				os_bzero(&the_plant_threshold_percent, 4);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&the_plant_threshold_percent,
				                     4);
				config_current->the_plant_threshold_percent = \
					(uint8_t)strtoul((char *)&the_plant_threshold_percent, NULL, 10);
			}
			else if (jsonparse_strcmp_value(parser_json, "thePlantThresholdLTGT") == 0) {
				os_bzero(&the_plant_threshold_lt_gt, 2);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&the_plant_threshold_lt_gt,
				                     2);
				config_current->the_plant_threshold_lt_gt = \
					(uint8_t)strtoul((char *)&the_plant_threshold_lt_gt, NULL, 10);
			}
			else if (jsonparse_strcmp_value(parser_json, "registeredValue") == 0) {
				os_bzero(&registered_value, 8);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&registered_value,
				                     8);
				config_current->registered_value = \
					(uint32_t)strtoul((char *)&registered_value, NULL, 10);
			}
			else if (jsonparse_strcmp_value(parser_json, "notificationEmail") == 0) {
				os_bzero(&notification_email, CONFIG_NOTIFICATION_EMAIL_LEN + 1);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&notification_email,
				                     CONFIG_NOTIFICATION_EMAIL_LEN + 1);
				os_memcpy(config_current->notification_email,
				          &notification_email,
				          sizeof(notification_email));
			}
			else if (jsonparse_strcmp_value(parser_json, "notificationEmailSubject") == 0) {
				os_bzero(&notification_email_subject,
				         CONFIG_NOTIFICATION_SUBJECT_LEN + 1);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&notification_email_subject,
				                     CONFIG_NOTIFICATION_SUBJECT_LEN + 1);
				os_memcpy(config_current->notification_email_subject,
				          &notification_email_subject,
				          sizeof(notification_email_subject));
			}
			else if (jsonparse_strcmp_value(parser_json, "notificationEmailMessage") == 0) {
				os_bzero(&notification_email_message,
				         CONFIG_NOTIFICATION_MESSAGE_LEN + 1);
				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
				jsonparse_copy_value(parser_json,
				                     (char *)&notification_email_message,
				                     CONFIG_NOTIFICATION_MESSAGE_LEN + 1);
				os_memcpy(config_current->notification_email_message,
				          &notification_email_message,
				          sizeof(notification_email_message));
			}
		}
	}

	os_bzero(&buffer_response, 64);

	if(do_save_current_config_to_flash()) {
		os_sprintf((char *)&buffer_response,
		           fmt_response_json_request_data_json,
		           REQUEST_JSON_SAVE_SETTINGS,
		           JSON_STATUS_OK,
		           "{}");

		/*
		 * Schedule a system restart but ensure that before sleeping there is enough
		 * time to send the response.
		 */
		os_timer_setfn(&timer_generic_software,
		               (os_timer_func_t *)cb_timer_deep_sleep,
		               NULL);

		// Restat after 4 seconds.
		os_timer_arm(&timer_generic_software, 4000, false);
	}
	else {
		os_sprintf((char *)&buffer_response,
		           fmt_response_json_request_data_json,
		           REQUEST_JSON_SAVE_SETTINGS,
		           JSON_STATUS_FAILED, "{}");
	}

	httpdSend(connection_data, (char *)&buffer_response, -1);

	os_free(data_json);
	os_free(parser_json);

	return HTTPD_CGI_DONE;
}
