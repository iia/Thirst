/*
	This file is part of Thirst.

	Thirst is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Thirst is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Foobar.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "thirst.h"

extern os_timer_t sys_timer_sw;
extern config_t* config_current;
extern SYS_RTC_DATA_t sys_rtc_data;
extern uint32_t config_flash_sector;

char* web_interface_buffer_post_form; // Allocated after system initialisation.
bool web_interface_waiting_wifi_scan_cb;
uint32_t web_interface_index_buffer_post_form; // Used to process partial arrival of POST data.

extern const unsigned long webpages_espfs_start; // Can not be renamed. Provided by libesphttpd.

// Functions.

bool ICACHE_FLASH_ATTR
web_interface_init(void) {
	web_interface_buffer_post_form = NULL;
	web_interface_index_buffer_post_form = 0;
	web_interface_waiting_wifi_scan_cb = false;

	// Clear WiFi scan result and initialize WiFi scan list.
	web_interface_clear_wifi_scan_result(&web_interface_ap_list_head);
	SLIST_INIT(&web_interface_ap_list_head);

	// Initialise libesphttpd file system.
	if (
		espFsInit(
			(void*)(&webpages_espfs_start)
		) != ESPFS_INIT_RESULT_OK
	)
	{
		#if (PERIPH_ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG :: WEB :: File system initialisation failed for libesphttpd\n"
			);
		#endif

		return false;
	}

	return true;
}

void ICACHE_FLASH_ATTR
web_interface_cb_wifi_scan_done(void* bss_info_list, STATUS status) {
	web_interface_waiting_wifi_scan_cb = false;
	struct bss_info* bss_info_list_element = NULL;
	WEB_INTERFACE_AP_LIST_ELEM_t* ap_list_element = NULL;

	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB :: WiFi scan finished\n");
	#endif

	if (status != OK || bss_info_list == NULL) {
		#if (PERIPH_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: WEB :: WiFi scan failed or nothing found\n");
		#endif

		return;
	}

	web_interface_clear_wifi_scan_result(&web_interface_ap_list_head);

	bss_info_list_element = (struct bss_info*)bss_info_list;

	while (bss_info_list_element) {
		if (
			(bss_info_list_element->authmode != AUTH_OPEN) && \
			(bss_info_list_element->authmode != AUTH_WPA_PSK) && \
			(bss_info_list_element->authmode != AUTH_WPA2_PSK) && \
			(bss_info_list_element->authmode != AUTH_WPA_WPA2_PSK)
		)
		{
			bss_info_list_element = bss_info_list_element->next.stqe_next;

			continue;
		}

		ap_list_element = \
			(WEB_INTERFACE_AP_LIST_ELEM_t*)malloc(sizeof(WEB_INTERFACE_AP_LIST_ELEM_t));

		os_memcpy(
			ap_list_element->bssid,
			bss_info_list_element->bssid,
			sizeof(bss_info_list_element->bssid)
		);

		os_memcpy(
			ap_list_element->ssid,
			bss_info_list_element->ssid,
			bss_info_list_element->ssid_len
		);

		ap_list_element->rssi = bss_info_list_element->rssi;
		ap_list_element->channel = bss_info_list_element->channel;
		ap_list_element->hidden = bss_info_list_element->is_hidden;
		ap_list_element->ssid_len = bss_info_list_element->ssid_len;
		ap_list_element->authmode = bss_info_list_element->authmode;

		SLIST_INSERT_HEAD(
			&web_interface_ap_list_head,
			ap_list_element,
			next
		);

		bss_info_list_element = bss_info_list_element->next.stqe_next;
	}
}

bool ICACHE_FLASH_ATTR
web_interface_start_wifi_scan(void) {
	if (web_interface_waiting_wifi_scan_cb) {
		#if (PERIPH_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: WEB :: WiFi scan in progress\n");
		#endif

		return true;
	}

	if (
		!(
			wifi_station_scan(
				NULL,
				web_interface_cb_wifi_scan_done
			)
		)
	)
	{
		#if (PERIPH_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: WEB :: Failed to start WiFi scan\n");
		#endif

		return false;
	}

	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB :: WiFi scan started\n");
	#endif

	web_interface_waiting_wifi_scan_cb = true;

	return true;
}

void ICACHE_FLASH_ATTR
web_interface_cb_timeout_restart(void) {
	sys_state_transition_with_wifi_mode(
		SYS_STATE_CONFIG,
		SYS_STATE_RESET_AFTER_CONFIG_SAVE,
		NULL_MODE
	);
}

bool ICACHE_FLASH_ATTR
web_interface_save_config_to_flash(void) {
	// Clear RTC data.
	os_bzero((void*)&sys_rtc_data, sizeof(SYS_RTC_DATA_t));

	if (
		!(
			system_rtc_mem_write(
				PERIPH_MEM_ADDR_RTC,
				&sys_rtc_data,
				sizeof(sys_rtc_data)
			)
		)
	)
	{
		#if (PERIPH_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: WEB :: Save config, RTC data write failed\n");
		#endif

		return false;
	}

	// Generate checksum of the current configuration.
	config_current->hash = config_get_hash_pearson((uint8_t*)config_current);

	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB :: Saving following configuration to flash\n");

		os_printf(
			"\nhash = 0x%X",
			config_current->hash
		);

		os_printf(
			"\nplant_name = %s",
			config_current->plant_name
		);

		os_printf(
			"\nconfiguration_password = %s",
			config_current->configuration_password
		);

		os_printf(
			"\nwifi_ap_ssid = %s",
			config_current->wifi_ap_ssid
		);

		os_printf(
			"\nwifi_ap_bssid = %s",
			config_current->wifi_ap_bssid
		);

		os_printf(
			"\nwifi_ap_password = %s",
			config_current->wifi_ap_password
		);

		os_printf(
			"\nthreshold_mode = %d",
			config_current->threshold_mode
		);

		os_printf(
			"\nthreshold_percent = %d",
			config_current->threshold_percent
		);

		os_printf(
			"\nthreshold_lt_gt = %d",
			config_current->threshold_lt_gt
		);

		os_printf(
			"\nregistered_value = %d",
			config_current->registered_value
		);

		os_printf(
			"\nnotification_email = %s",
			config_current->notification_email
		);

		os_printf(
			"\nnotification_email_subject = %s",
			config_current->notification_email_subject
		);

		os_printf(
			"\nnotification_email_message = %s\n",
			config_current->notification_email_message
		);
	#endif

	if (
		(spi_flash_erase_sector(config_flash_sector)) != SPI_FLASH_RESULT_OK
	)
	{
		#if (PERIPH_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: WEB :: Configuration save, flash erase failed\n");
		#endif

		return false;
	}
	else {
		if (
			(
				spi_flash_write(
					(config_flash_sector * 4096),
					(uint32_t*)config_current,
					sizeof(config_t)
				)
			) != SPI_FLASH_RESULT_OK
		)
		{
			#if (PERIPH_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: WEB :: Configuration save, flash write failed\n");
			#endif

			return false;
		}
	}

	return true;
}

void ICACHE_FLASH_ATTR
web_interface_clear_wifi_scan_result(
	WEB_INTERFACE_AP_LIST_HEAD_t* web_interface_ap_list_head
)
{
	WEB_INTERFACE_AP_LIST_ELEM_t* ap_list_element = NULL;

	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB :: Clearing WiFi scan result\n");
	#endif

	while (!SLIST_EMPTY(web_interface_ap_list_head)) {
		ap_list_element = SLIST_FIRST(web_interface_ap_list_head);

		SLIST_REMOVE_HEAD(web_interface_ap_list_head, next);
		free(ap_list_element);
	}
}

void ICACHE_FLASH_ATTR
web_interface_get_wifi_scan_result(char** buffer_response_data) {
	uint32_t ap_count = 0;
	uint32_t ap_total = 0;
	uint16_t ap_info_len = 256;
	char* buffer_ap_element = NULL;
	char* buffer_ap_element_ssid = NULL;
	char* buffer_ap_element_bssid = NULL;
	WEB_INTERFACE_AP_LIST_ELEM_t* ap_list_element = NULL;

	os_free(*buffer_response_data);

	*buffer_response_data = NULL;

	SLIST_FOREACH(
		ap_list_element,
		&web_interface_ap_list_head,
		next
	)
	{
		ap_count++;
	}

	ap_total = ap_count;
	ap_count = 0;

	if (ap_total < 1) {
		*buffer_response_data = (char*)malloc(4);

		os_bzero(*buffer_response_data, 4);
		os_strcat(*buffer_response_data, "[]");

		return;
	}

	*buffer_response_data = (char*)malloc((ap_total * ap_info_len) + 2);

	os_bzero(*buffer_response_data, (ap_total * ap_info_len) + 2);
	os_strcpy(*buffer_response_data, "[\0");

	SLIST_FOREACH(
		ap_list_element,
		&web_interface_ap_list_head,
		next
	)
	{
		ap_count++;

		buffer_ap_element = (char*)malloc(ap_info_len);
		buffer_ap_element_ssid = (char*)malloc(33);
		buffer_ap_element_bssid = (char*)malloc(16);

		os_bzero(buffer_ap_element, ap_info_len);
		os_bzero(buffer_ap_element_ssid, 33);
		os_bzero(buffer_ap_element_bssid, 16);

		os_strncpy(
			buffer_ap_element_ssid,
			ap_list_element->ssid,
			ap_list_element->ssid_len
		);

		os_sprintf(
			buffer_ap_element_bssid,
			"%02X%02X%02X%02X%02X%02X",
			ap_list_element->bssid[0],
			ap_list_element->bssid[1],
			ap_list_element->bssid[2],
			ap_list_element->bssid[3],
			ap_list_element->bssid[4],
			ap_list_element->bssid[5]
		);

		os_sprintf(
			buffer_ap_element,
			web_interface_fmt_json_wifi_scan_result_data,
			buffer_ap_element_bssid,
			buffer_ap_element_ssid,
			ap_list_element->channel,
			ap_list_element->rssi,
			ap_list_element->authmode,
			ap_list_element->hidden
		);

		os_strcat(*buffer_response_data, buffer_ap_element);

		if (ap_count < ap_total) {
			os_strcat(*buffer_response_data, ",\0");
		}

		os_free(buffer_ap_element);
		os_free(buffer_ap_element_ssid);
		os_free(buffer_ap_element_bssid);
	}

	os_strcat(*buffer_response_data, "]\0");
}

// CGI functions.

int ICACHE_FLASH_ATTR
web_interface_cgi_root(
	HttpdConnData* connection_data,
	char* token,
	void** arg
)
{
	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB-CGI :: Serving URL = /index.html\n");
	#endif

	if (token == NULL) {
		return HTTPD_CGI_DONE;
	}

	// Using the template engine providing the plant name for the 'title' tag.
	if (os_strcmp(token, "PLANT_NAME") == 0) {
		httpdSend(connection_data, config_current->plant_name, -1);
	}

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
web_interface_cgi_get_config(HttpdConnData* connection_data) {
	char request[4];
	char* buffer_response = (uint8_t*)os_malloc(8192);
	char* buffer_response_data = (uint8_t*)os_malloc(4096);

	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB-CGI :: Serving URL = /get_config.cgi\n");
	#endif

	os_bzero(request, sizeof(request));
	os_bzero(buffer_response, sizeof(buffer_response));
	os_bzero(buffer_response_data, sizeof(buffer_response_data));

	// HTTP header.
	httpdStartResponse(connection_data, 200);
	httpdHeader(connection_data, "Content-Type", "text/json");
	httpdEndHeaders(connection_data);

	if (
		(
			httpdFindArg(
				connection_data->post->buff,
				"request",
				request,
				sizeof(request)
			)
		) > 0
	)
	{
		// Check request.
		if (
			(strtoul(request, NULL, 10))
			!= WEB_INTERFACE_REQ_JSON_GET_CONFIG
		)
		{
			os_sprintf(
				buffer_response,
				web_interface_fmt_json_resp_req_data,
				WEB_INTERFACE_REQ_JSON_GET_CONFIG,
				WEB_INTERFACE_JSON_STATUS_FAILED,
				"{}"
			);

			httpdSend(connection_data, buffer_response, -1);

			os_free(buffer_response);
			os_free(buffer_response_data);

			return HTTPD_CGI_DONE;
		}
	}
	else {
		os_sprintf(
			buffer_response,
			web_interface_fmt_json_resp_req_data,
			WEB_INTERFACE_REQ_JSON_GET_CONFIG,
			WEB_INTERFACE_JSON_STATUS_FAILED,
			"{}"
		);

		httpdSend(connection_data, buffer_response, -1);

		os_free(buffer_response);
		os_free(buffer_response_data);

		return HTTPD_CGI_DONE;
	}

	os_sprintf(
		buffer_response_data,
		web_interface_fmt_json_config_data,
		config_current->plant_name,
		config_current->configuration_password,
		config_current->wifi_ap_ssid,
		config_current->wifi_ap_bssid,
		config_current->wifi_ap_password,
		config_current->threshold_mode,
		config_current->threshold_percent,
		config_current->threshold_lt_gt,
		periph_read_adc((uint32_t)PERIPH_ADC_SAMPLE_SIZE),
		config_current->notification_email,
		config_current->notification_email_subject,
		config_current->notification_email_message
	);

	os_sprintf(
		buffer_response,
		web_interface_fmt_json_resp_req_data,
		WEB_INTERFACE_REQ_JSON_GET_CONFIG,
		WEB_INTERFACE_JSON_STATUS_OK,
		buffer_response_data
	);

	httpdSend(connection_data, buffer_response, -1);

	os_free(buffer_response);
	os_free(buffer_response_data);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
web_interface_cgi_save_config(HttpdConnData* connection_data) {
	int type_json = 0;
	char* data_json = NULL;
	char buffer_response[64];
	char data_json_len_raw[8];
	uint32_t data_json_len = 0;
	struct jsonparse_state* parser_json = NULL;
	char threshold_mode[2];
	char threshold_lt_gt[2];
	char registered_value[8];
	char threshold_percent[4];
	char plant_name[CONFIG_SSID_LEN + 1];
	char wifi_ap_bssid[CONFIG_BSSID_LEN];
	char wifi_ap_ssid[CONFIG_SSID_LEN + 1];
	char wifi_ap_password[CONFIG_SSID_PASSWORD_LEN + 1];
	char configuration_password[CONFIG_SSID_PASSWORD_LEN + 1];
	char notification_email[CONFIG_NOTIFICATION_EMAIL_LEN + 1];
	char notification_email_subject[CONFIG_NOTIFICATION_SUBJECT_LEN + 1];
	char notification_email_message[CONFIG_NOTIFICATION_MESSAGE_LEN + 1];

	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB-CGI :: Serving URL = /save_config.cgi\n");
	#endif

	// Partial arrival processing.
	os_memcpy(
		&web_interface_buffer_post_form[web_interface_index_buffer_post_form],
		connection_data->post->buff,
		connection_data->post->buffLen
	);

	web_interface_index_buffer_post_form += connection_data->post->buffLen;

	// Wait for pending data.
	if (connection_data->post->len != connection_data->post->received) {
		return HTTPD_CGI_MORE;
	}

	// Reset buffer accumulator index.
	web_interface_index_buffer_post_form = 0;

	// HTTP header.
	httpdStartResponse(connection_data, 200);
	httpdHeader(connection_data, "Content-Type", "text/json");
	httpdEndHeaders(connection_data);

	os_bzero(&data_json_len_raw, 8);

	if (
		(
			httpdFindArg(
				web_interface_buffer_post_form,
				"data_json_len",
				(char*)&data_json_len_raw,
				8
			)
		) <= 0
	)
	{
		#if (PERIPH_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG WEB-CGI :: Can't get JSON data length\n");
		#endif

		return HTTPD_CGI_DONE;
	}

	data_json_len = strtoul((char*)&data_json_len_raw, NULL, 10) + 1;

	data_json = (char*)os_malloc(data_json_len);
	os_bzero(data_json, data_json_len);

	if (
		(
			httpdFindArg(
				web_interface_buffer_post_form,
				"data_json",
				data_json,
				data_json_len
			)
		) <= 0
	)
	{
		#if (PERIPH_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: WEB-CGI :: Can't get JSON data\n");
		#endif

		os_free(data_json);

		return HTTPD_CGI_DONE;
	}

	parser_json = \
		(struct jsonparse_state*)os_malloc(sizeof(struct jsonparse_state));

	jsonparse_setup(parser_json, data_json, os_strlen(data_json));
	os_bzero(config_current, sizeof(config_current));

	while ((type_json = jsonparse_next(parser_json)) != 0) {
		if (type_json == JSON_TYPE_PAIR_NAME) {
			if (jsonparse_strcmp_value(parser_json, "thresholdMode") == 0) {
				os_bzero(&threshold_mode, 2);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);

				jsonparse_copy_value(
					parser_json,
					(char*)&threshold_mode,
					2
				);

				config_current->threshold_mode = \
					(uint8_t)strtoul((char*)&threshold_mode, NULL, 10);
			}
			else if (jsonparse_strcmp_value(parser_json, "plantName") == 0) {
				os_bzero(&plant_name, CONFIG_SSID_LEN + 1);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);

				jsonparse_copy_value(
					parser_json,
					(char*)&plant_name,
					CONFIG_SSID_LEN + 1
				);

				os_memcpy(
					config_current->plant_name,
					&plant_name,
					sizeof(plant_name)
				);
			}
			else if (jsonparse_strcmp_value(parser_json, "configurationPassword") == 0) {
				os_bzero(&configuration_password, CONFIG_SSID_PASSWORD_LEN + 1);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
	
				jsonparse_copy_value(
					parser_json,
					(char*)&configuration_password,
					CONFIG_SSID_PASSWORD_LEN + 1
				);

				os_memcpy(
					config_current->configuration_password,
					&configuration_password,
					sizeof(configuration_password)
				);
			}
			else if (jsonparse_strcmp_value(parser_json, "wifiAPSSID") == 0) {
				os_bzero(&wifi_ap_ssid, CONFIG_SSID_LEN + 1);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);

				jsonparse_copy_value(
					parser_json,
					(char*)&wifi_ap_ssid,
					CONFIG_SSID_LEN + 1
				);

				os_memcpy(
					config_current->wifi_ap_ssid,
					wifi_ap_ssid,
					sizeof(wifi_ap_ssid)
				);
			}
			else if (jsonparse_strcmp_value(parser_json, "wifiAPBSSID") == 0) {
				os_bzero(&wifi_ap_bssid, CONFIG_BSSID_LEN);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);

				jsonparse_copy_value(
					parser_json,
					(char*)&wifi_ap_bssid,
					CONFIG_BSSID_LEN
				);

				os_memcpy(
					config_current->wifi_ap_bssid,
					&wifi_ap_bssid,
					sizeof(wifi_ap_bssid)
				);
			}
			else if (jsonparse_strcmp_value(parser_json, "wifiAPPassword") == 0) {
				os_bzero(&wifi_ap_password, CONFIG_SSID_PASSWORD_LEN + 1);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);

				jsonparse_copy_value(
					parser_json,
					(char*)&wifi_ap_password,
					CONFIG_SSID_PASSWORD_LEN + 1
				);

				os_memcpy(
					config_current->wifi_ap_password,
					&wifi_ap_password,
					sizeof(wifi_ap_password)
				);
			}
			else if (jsonparse_strcmp_value(parser_json, "thresholdPercent") == 0) {
				os_bzero(&threshold_percent, 4);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);
	
				jsonparse_copy_value(
					parser_json,
					(char*)&threshold_percent,
					4
				);

				config_current->threshold_percent = \
					(uint8_t)strtoul((char*)&threshold_percent, NULL, 10);
			}
			else if (jsonparse_strcmp_value(parser_json, "thresholdLTGT") == 0) {
				os_bzero(&threshold_lt_gt, 2);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);

				jsonparse_copy_value(
					parser_json,
					(char*)&threshold_lt_gt,
					2
				);

				config_current->threshold_lt_gt = \
					(uint8_t)strtoul((char*)&threshold_lt_gt, NULL, 10);
			}
			else if (jsonparse_strcmp_value(parser_json, "registeredValue") == 0) {
				os_bzero(&registered_value, 8);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);

				jsonparse_copy_value(
					parser_json,
					(char*)&registered_value,
					8
				);

				config_current->registered_value = \
					(uint32_t)strtoul((char*)&registered_value, NULL, 10);
			}
			else if (jsonparse_strcmp_value(parser_json, "notificationEmail") == 0) {
				os_bzero(&notification_email, CONFIG_NOTIFICATION_EMAIL_LEN + 1);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);

				jsonparse_copy_value(
					parser_json,
					(char*)&notification_email,
					CONFIG_NOTIFICATION_EMAIL_LEN + 1
				);

				os_memcpy(
					config_current->notification_email,
					&notification_email,
					sizeof(notification_email)
				);
			}
			else if (jsonparse_strcmp_value(parser_json, "notificationEmailSubject") == 0) {
				os_bzero(
					&notification_email_subject,
					CONFIG_NOTIFICATION_SUBJECT_LEN + 1
				);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);

				jsonparse_copy_value(
					parser_json,
					(char*)&notification_email_subject,
					CONFIG_NOTIFICATION_SUBJECT_LEN + 1
				);

				os_memcpy(
					config_current->notification_email_subject,
					&notification_email_subject,
					sizeof(notification_email_subject)
				);
			}
			else if (jsonparse_strcmp_value(parser_json, "notificationEmailMessage") == 0) {
				os_bzero(
					&notification_email_message,
					CONFIG_NOTIFICATION_MESSAGE_LEN + 1
				);

				jsonparse_next(parser_json);
				jsonparse_next(parser_json);

				jsonparse_copy_value(
					parser_json,
					(char*)&notification_email_message,
					CONFIG_NOTIFICATION_MESSAGE_LEN + 1
				);

				os_memcpy(
					config_current->notification_email_message,
					&notification_email_message,
					sizeof(notification_email_message)
				);
			}
		}
	}

	os_bzero(&buffer_response, 64);

	if (web_interface_save_config_to_flash()) {
		os_sprintf(
			(char*)&buffer_response,
			web_interface_fmt_json_resp_req_data,
			WEB_INTERFACE_REQ_JSON_SAVE_CONFIG,
			WEB_INTERFACE_JSON_STATUS_OK,
			"{}"
		);

		#if (PERIPH_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: WEB-CGI :: Scheduling system reset with delay\n");
		#endif

		/*
		 * Schedule a system restart but ensure that
		 * before sleeping there is enough time to
		 * send the response.
		 */
		os_timer_setfn(
			&sys_timer_sw,
			(os_timer_func_t*)web_interface_cb_timeout_restart,
			NULL
		);

		os_timer_arm(&sys_timer_sw, 4000, false);
	}
	else {
		os_sprintf(
			(char*)&buffer_response,
			web_interface_fmt_json_resp_req_data,
			WEB_INTERFACE_REQ_JSON_SAVE_CONFIG,
			WEB_INTERFACE_JSON_STATUS_FAILED,
			"{}"
		);
	}

	httpdSend(connection_data, (char*)&buffer_response, -1);

	os_free(data_json);
	os_free(parser_json);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
web_interface_cgi_start_wifi_scan(HttpdConnData* connection_data) {
	char* buffer_response = (uint8_t*)os_malloc(64);

	os_bzero(buffer_response, 64);

	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB-CGI :: Serving URL = /start_wifi_scan.cgi\n");
	#endif

	if (web_interface_start_wifi_scan()) {
		os_sprintf(
			buffer_response,
			web_interface_fmt_json_resp_req_data,
			WEB_INTERFACE_REQ_JSON_START_WIFI_SCAN,
			WEB_INTERFACE_JSON_STATUS_OK,
			"{}"
		);
	}
	else {
		os_sprintf(
			buffer_response,
			web_interface_fmt_json_resp_req_data,
			WEB_INTERFACE_REQ_JSON_START_WIFI_SCAN,
			WEB_INTERFACE_JSON_STATUS_FAILED,
			"{}"
		);
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
web_interface_cgi_get_sensor_reading(HttpdConnData* connection_data) {
	uint32_t sensor_reading = 0;
	char* buffer_response = (uint8_t*)os_malloc(512);

	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB-CGI :: Serving URL = /get_sensor_reading.cgi\n");
	#endif

	os_bzero(buffer_response, sizeof(buffer_response));
	sensor_reading = periph_read_adc((uint32_t)PERIPH_ADC_SAMPLE_SIZE);

	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB-CGI :: Sensor reading = %d\n", sensor_reading);
	#endif

	// HTTPD header.
	httpdStartResponse(connection_data, 200);
	httpdHeader(connection_data, "Content-Type", "text/json");
	httpdEndHeaders(connection_data);

	os_sprintf(
		buffer_response,
		web_interface_fmt_json_resp_req_data_int,
		WEB_INTERFACE_REQ_JSON_GET_SENSOR_READING,
		WEB_INTERFACE_JSON_STATUS_OK,
		sensor_reading
	);

	httpdSend(connection_data, buffer_response, -1);

	os_free(buffer_response);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
web_interface_cgi_get_wifi_scan_result(HttpdConnData* connection_data) {
	char* buffer_response = NULL;
	char* _buffer_response = NULL;

	#if (PERIPH_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: WEB-CGI :: Serving URL = /get_wifi_scan_result.cgi\n");
	#endif

	web_interface_get_wifi_scan_result(&_buffer_response);

	buffer_response = (char*)os_malloc(os_strlen(_buffer_response) + 64);

	os_sprintf(
		buffer_response,
		web_interface_fmt_json_resp_req_data,
		WEB_INTERFACE_REQ_JSON_GET_WIFI_SCAN_RESULT,
		WEB_INTERFACE_JSON_STATUS_OK,
		_buffer_response
	);

	// HTTPD header.
	httpdStartResponse(connection_data, 200);
	httpdHeader(connection_data, "Content-Type", "text/json");
	httpdEndHeaders(connection_data);
	httpdSend(connection_data, buffer_response, -1);

	os_free(buffer_response);
	os_free(_buffer_response);

	return HTTPD_CGI_DONE;
}
