#include <stdlib.h>
#include <mem.h>
#include "user_main.h"
#include "espfs.h"
#include "platform.h"
#include "httpdespfs.h"
#include "driver/uart.h"
#include "web_interface.h"
#include "user_interface.h"

// Initialize global variables.
uint32_t data_rtc = 0;
ap_list_head_t ap_list_head;
uint32_t do_notification_next = 0;
bool waiting_wifi_scan_cb = false;
bool state_error_led_state = true;
uint32_t do_notification_current = 0;

/*
 * Already prepared default configuration so that
 * it can simply copied to current configuration
 * when required.
 */
config_t config_default = {
	.config_hash_pearson = 0,

	// Plant configuration.

	// Will be filled with default plant name.
	.the_plant_name = "\0",
	.the_plant_configuration_password = "1234567890",
	.the_plant_wifi_ap = "\0",
	.the_plant_wifi_ap_bssid = "\0",
	.the_plant_wifi_ap_password = "\0",
	.the_plant_threshold_percent = 5,
	.the_plant_threshold_lt_gt = CONFIG_THRESHLOD_LT,
	.registered_value = 0,

	// Notification configuration.
	.notification_email = "\0",
	.notification_email_subject = "\0",
	.notification_email_message = "\0"
};

uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

void ICACHE_FLASH_ATTR
do_notification(void) {
	struct station_config config_st_interface;

	// Clear out WiFi station interface configuration storage.
	os_bzero(&config_st_interface, sizeof(config_st_interface));

	// Enable station mode.
	wifi_set_opmode_current(STATION_MODE);

	// WiFi station settings.
	config_st_interface.bssid_set = 0;
	os_memcpy(&config_st_interface.ssid, config_current->the_plant_wifi_ap,
	os_strlen(config_current->the_plant_wifi_ap));
	os_memcpy(&config_st_interface.password, config_current->the_plant_wifi_ap_password,
	os_strlen(config_current->the_plant_wifi_ap_password));

	// Apply WiFi settings.
	wifi_station_set_config_current(&config_st_interface);

	// Set callback for WiFi events.
	wifi_set_event_handler_cb(cb_wifi_event);

	// Enable station mode reconnect.
	wifi_station_set_reconnect_policy(true);

	// Connect WiFi in station mode.
	if(!wifi_station_connect()) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi station mode connect failed\n");
		#endif

		do_state_error();

		return;
	}
}

void ICACHE_FLASH_ATTR
do_toggle_vcc_probe(bool toggle) {
	PIN_FUNC_SELECT(GPIO_O_VCC_PROBE, GPIO_O_FUNC_VCC_PROBE);

	if(toggle) {
		gpio_output_set(GPIO_O_BIT_VCC_PROBE, 0, GPIO_O_BIT_VCC_PROBE, 0);
	}
	else {
		gpio_output_set(0, GPIO_O_BIT_VCC_PROBE, GPIO_O_BIT_VCC_PROBE, 0);
	}
}

bool ICACHE_FLASH_ATTR
is_battery_low(uint32_t adc_sample_size) {
	uint32_t i = 0;
	uint32_t sum = 0;
	uint32_t battery_voltage_reading = 0;
	uint16_t *adc_samples = (uint16_t *)os_malloc(adc_sample_size);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: is_battery_low()\n");
	#endif

	do_toggle_sesnor(false);
	do_toggle_vcc_probe(true);

	os_delay_us(128);

	for(i = 0; i < adc_sample_size; i++) {
		adc_samples[i] = system_adc_read();
	}

	do_toggle_vcc_probe(false);

	sum = 0;

	for(i = 0; i < adc_sample_size; i++) {
		sum = sum + adc_samples[i];
	}

	battery_voltage_reading = sum / adc_sample_size;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Current battery voltage reading = %d\n",
		          battery_voltage_reading);
	#endif

	os_free(adc_samples);

	if (battery_voltage_reading < LOW_BATTERY_THRESHOLD) {
		return true;
	}
	else {
		return false;
	}
}

void
cb_timer_disconnect_sock(void) {
	os_timer_disarm(&timer_generic_software);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_timer_disconnect_sock()\n");
	#endif

	espconn_disconnect(&sock);
}

void ICACHE_FLASH_ATTR
cb_sock_recv(void *arg, char *data, unsigned short length) {
	os_timer_disarm(&timer_generic_software);

	char buffer_json_data[1024];
	char buffer_low_battery_subject[128];
	char buffer_low_battery_message[512];
	struct espconn *sock = (struct espconn *)arg;
	char *buffer_message = (char *)malloc(NOTIFIER_SIZE_SEND_BUFFER);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_recv()\n");
		os_printf("\n[+] DBG: Received length = %d, data = %s\n", length, data);
	#endif

	if (do_notification_next != 0) {
		do_notification_next = 0;

		os_sprintf(
			buffer_low_battery_subject,
			FMT_LOW_BATTERY_SUBJECT,
			config_current->the_plant_name
		);

		os_sprintf(
			buffer_low_battery_message,
			FMT_LOW_BATTERY_MESSAGE,
			config_current->the_plant_name
		);

		os_sprintf(
			buffer_json_data,
			FMT_NOTIFIER_DATA_HTTP_JSON,
			config_current->notification_email,
			buffer_low_battery_subject,
			config_current->the_plant_name,
			buffer_low_battery_message
		);

		os_sprintf(
			buffer_message,
			FMT_NOTIFIER_HTTP_HEADER,
			os_strlen(buffer_json_data),
			buffer_json_data
		);

		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Sending request, message = %s\n", buffer_message);
		#endif

		// Send request.
		if(espconn_secure_send(sock, buffer_message, os_strlen(buffer_message))) {
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: Sending data using socket failed\n");
			#endif

			os_free(buffer_message);

			do_state_error();
		}
	}
	else {
		os_free(buffer_message);

		// Setup timer to call disconnect on the socket.
		os_timer_setfn(&timer_generic_software,
		               (os_timer_func_t *)cb_timer_disconnect_sock, NULL);
		os_timer_arm(&timer_generic_software, 2000, false);
	}
}

void ICACHE_FLASH_ATTR
cb_sock_disconnect(void *arg) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_disconnect()\n");
	#endif

	do_set_deep_sleep_mode();

	// Make sure the sensor is powered off.
	do_toggle_sesnor(false);

	// Make sure VCC probe GPIO is on logic low.
	do_toggle_vcc_probe(false);

	system_deep_sleep(DEEP_SLEEP_DURATION_US);
}

void ICACHE_FLASH_ATTR
cb_sock_sent(void *arg) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_sent(), waiting for response\n");
	#endif
}

void ICACHE_FLASH_ATTR
cb_sock_connect(void *arg) {
	char buffer_json_data[1024];
	char buffer_low_battery_subject[128];
	char buffer_low_battery_message[512];
	struct espconn *sock = (struct espconn *)arg;
	char *buffer_message = (char *)malloc(NOTIFIER_SIZE_SEND_BUFFER);

	os_bzero(buffer_message, NOTIFIER_SIZE_SEND_BUFFER);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_connect()\n");
	#endif

	espconn_regist_sentcb(sock, cb_sock_sent);
	espconn_regist_recvcb(sock, cb_sock_recv);
	espconn_regist_disconcb(sock, cb_sock_disconnect);

	if (do_notification_current == DO_NOTIFICATION_THIRST) {
		os_sprintf(
			buffer_json_data,
			FMT_NOTIFIER_DATA_HTTP_JSON,
			config_current->notification_email,
			config_current->notification_email_subject,
			config_current->the_plant_name,
			config_current->notification_email_message
		);

		os_sprintf(
			buffer_message,
			FMT_NOTIFIER_HTTP_HEADER,
			os_strlen(buffer_json_data),
			buffer_json_data
		);

		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Sending request, message = %s\n", buffer_message);
		#endif

		// Send request.
		if(espconn_secure_send(sock, buffer_message, os_strlen(buffer_message))) {
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: Sending data using socket failed\n");
			#endif

			do_state_error();
		}
	}
	else if (do_notification_current == DO_NOTIFICATION_LOW_BATTERY) {
		os_sprintf(
			buffer_low_battery_subject,
			FMT_LOW_BATTERY_SUBJECT,
			config_current->the_plant_name
		);

		os_sprintf(
			buffer_low_battery_message,
			FMT_LOW_BATTERY_MESSAGE,
			config_current->the_plant_name
		);

		os_sprintf(
			buffer_json_data,
			FMT_NOTIFIER_DATA_HTTP_JSON,
			config_current->notification_email,
			buffer_low_battery_subject,
			config_current->the_plant_name,
			buffer_low_battery_message
		);

		os_sprintf(
			buffer_message,
			FMT_NOTIFIER_HTTP_HEADER,
			os_strlen(buffer_json_data),
			buffer_json_data
		);

		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Sending request, message = %s\n", buffer_message);
		#endif

		// Send request.
		if(espconn_secure_send(sock, buffer_message, os_strlen(buffer_message))) {
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: Sending data using socket failed\n");
			#endif

			do_state_error();
		}
	}

	os_free(buffer_message);
}

void ICACHE_FLASH_ATTR
cb_dns_resolved(const char *hostname, ip_addr_t *ip_resolved, void *arg) {
	char ip_server[4];
	struct espconn *sock = (struct espconn *)arg;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_dns_resolved()\n");
	#endif

	if (ip_resolved != NULL) {
		ip_server[0] = *((uint8 *)&ip_resolved->addr);
		ip_server[1] = *((uint8 *)&ip_resolved->addr + 1);
		ip_server[2] = *((uint8 *)&ip_resolved->addr + 2);
		ip_server[3] = *((uint8 *)&ip_resolved->addr + 3);

		#if (ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG: DNS resolved, IP = %d.%d.%d.%d\n",
				ip_server[0],
				ip_server[1],
				ip_server[2],
				ip_server[3]
			);
		#endif

		// Configure the socket.
		sock->proto.tcp = &sock_tcp;
		sock->type = ESPCONN_TCP;
		sock->state = ESPCONN_NONE;
		sock->proto.tcp->remote_port = NOTIFIER_PORT;
		sock->proto.tcp->local_port = espconn_port();
		os_memcpy(sock->proto.tcp->remote_ip, ip_server, 4);

		espconn_regist_connectcb(sock, cb_sock_connect);

		// Set TLS client mode and allocate memory.
		if(!espconn_secure_set_size((uint8_t)0x01, (uint16_t)TLS_BUFFER_SIZE)) {
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: TLS init failed\n");
			#endif

			do_state_error();
		}
		else {
			// Connect the socket.
			if(espconn_secure_connect(sock)) {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: Socket connect failed\n");
				#endif

				do_state_error();
			}
		}
	}
	else {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: DNS resolve failed\n");
		#endif

		do_state_error();
	}
}

void ICACHE_FLASH_ATTR
do_get_default_plant_name(char *default_plant_name, uint8_t len) {
	uint8_t mac_softap[6];

	os_bzero(&mac_softap, sizeof(mac_softap));
	os_bzero(default_plant_name, len);

	wifi_get_macaddr(SOFTAP_IF, mac_softap);

	os_sprintf(default_plant_name,
	           FMT_CONFIG_DEFAULT_PLANT_NAME,
	           mac_softap[3],
	           mac_softap[4],
	           mac_softap[5]);
}

bool ICACHE_FLASH_ATTR
do_save_current_config_to_flash() {
	data_rtc = 0;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: do_save_current_config_to_flash()\n");
	#endif

	if(system_rtc_mem_write(MEM_ADDR_RTC, &data_rtc, 4)) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: RTC counter reset\n");
		#endif
	}
	else {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: RTC counter reset failed\n");
		#endif

		do_state_error();

		return false;
	}

	// Generate checksum of the current config first.
	config_current->config_hash_pearson = do_get_hash_pearson((uint8_t *)config_current);

	#if(ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Saving current configuration to flash memory\n");
		os_printf("\nconfig_current->config_hash_pearson = 0x%x", do_get_hash_pearson((uint8_t *)config_current));
		os_printf("\nconfig_current->the_plant_name = %s", config_current->the_plant_name);
		os_printf("\nconfig_current->the_plant_configuration_password = %s", config_current->the_plant_configuration_password);
		os_printf("\nconfig_current->the_plant_wifi_ap = %s", config_current->the_plant_wifi_ap);
		os_printf("\nconfig_current->the_plant_wifi_ap_bssid = %s", config_current->the_plant_wifi_ap_bssid);
		os_printf("\nconfig_current->the_plant_wifi_ap_password = %s", config_current->the_plant_wifi_ap_password);
		os_printf("\nconfig_current->the_plant_threshold_percent = %d", config_current->the_plant_threshold_percent);
		os_printf("\nconfig_current->the_plant_threshold_lt_gt = %d", config_current->the_plant_threshold_lt_gt);
		os_printf("\nconfig_current->registered_value = %d", config_current->registered_value);
		os_printf("\nconfig_current->notification_email = %s", config_current->notification_email);
		os_printf("\nconfig_current->notification_email_subject = %s", config_current->notification_email_subject);
		os_printf("\nconfig_current->notification_email_message = %s\n", config_current->notification_email_message);
	#endif

	if((spi_flash_erase_sector(CONFIG_SECTOR_FLASH)) != SPI_FLASH_RESULT_OK) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Configuration save to flash failed in erase phase\n");
		#endif

		do_state_error();

		return false;
	}
	else {
		if((spi_flash_write(CONFIG_SECTOR_FLASH*4096, (uint32_t *)config_current, sizeof(config_t))) != SPI_FLASH_RESULT_OK) {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: Configuration save to flash failed in write phase\n");
				#endif

				do_state_error();

				return false;
		}
	}

	return true;
}

void ICACHE_FLASH_ATTR
cb_wifi_scan_done(void *bss_info_list, STATUS status) {
	waiting_wifi_scan_cb = false;
	ap_list_element_t *ap_list_element = NULL;
	struct bss_info *bss_info_list_element = NULL;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_wifi_scan_done()\n");
	#endif


	if(status != OK || bss_info_list == NULL) {
		return;
	}

	do_clear_wifi_scan_result(&ap_list_head);

	bss_info_list_element = (struct bss_info*)bss_info_list;

	while(bss_info_list_element) {
		if (bss_info_list_element->authmode != AUTH_OPEN && \
		    bss_info_list_element->authmode != AUTH_WPA_PSK && \
		    bss_info_list_element->authmode != AUTH_WPA2_PSK && \
		    bss_info_list_element->authmode != AUTH_WPA_WPA2_PSK) {
		        bss_info_list_element = bss_info_list_element->next.stqe_next;

		        continue;
		}

		ap_list_element = \
			(ap_list_element_t *)malloc(sizeof(ap_list_element_t));

		os_memcpy(ap_list_element->bssid,
		          bss_info_list_element->bssid,
			  sizeof(bss_info_list_element->bssid));
		os_memcpy(ap_list_element->ssid,
		          bss_info_list_element->ssid,
			  bss_info_list_element->ssid_len);

		ap_list_element->ssid_len = bss_info_list_element->ssid_len;
		ap_list_element->channel = bss_info_list_element->channel;
		ap_list_element->rssi = bss_info_list_element->rssi;
		ap_list_element->authmode = bss_info_list_element->authmode;
		ap_list_element->is_hidden = bss_info_list_element->is_hidden;

		SLIST_INSERT_HEAD(&ap_list_head, ap_list_element, next);

		bss_info_list_element = bss_info_list_element->next.stqe_next;
	}
}

void ICACHE_FLASH_ATTR
cb_wifi_event(System_Event_t *evt) {
	err_t e;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_wifi_event()\n");
	#endif

	if(evt->event == EVENT_STAMODE_CONNECTED) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi STATION connected\n");
		#endif
	}
	else if(evt->event == EVENT_STAMODE_DISCONNECTED) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi STATION disconnected\n");
		#endif
	}
	else if(evt->event == EVENT_STAMODE_AUTHMODE_CHANGE) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi STATION authentication mode changed = %d -> %d\n",
			          evt->event_info.auth_change.old_mode,
			          evt->event_info.auth_change.new_mode);
		#endif
	}
	else if(evt->event == EVENT_STAMODE_GOT_IP) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi STATION got IP\n");
		#endif

		e = espconn_gethostbyname(&sock,
		                          NOTIFIER_HOST,
		                          &ip_dns_resolved,
		                          cb_dns_resolved);

		switch(e) {
			case ESPCONN_OK: {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: DNS lookup started\n");
				#endif

				break;
			}

			case ESPCONN_ARG: {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: DNS lookup argument error\n");
				#endif

				do_state_error();

				break;
			}

			case ESPCONN_INPROGRESS: {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: DNS lookup in progress\n");
				#endif

				break;
			}
		}
	}
	else if(evt->event == EVENT_STAMODE_DHCP_TIMEOUT) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi STATION DHCP timeout\n");
		#endif

		do_state_error();
	}

	else if(evt->event == EVENT_SOFTAPMODE_STACONNECTED) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi SOFTAP station connected\n");
		#endif
	}
	else if(evt->event == EVENT_SOFTAPMODE_STADISCONNECTED) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi SOFTAP station disconnected\n");
		#endif
	}
	else if(evt->event == EVENT_SOFTAPMODE_PROBEREQRECVED) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi SOFTAP probe request received\n");
		#endif
	}
	else if(evt->event == EVENT_OPMODE_CHANGED) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi operating mode changed\n");
		#endif
	}
	else {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi unknown event\n");
		#endif
	}
}

uint32_t ICACHE_FLASH_ATTR
do_get_sensor_reading(uint32_t adc_sample_size) {
	uint32_t i = 0;
	uint32_t sum = 0;
	uint16_t *adc_samples = (uint16_t *)os_malloc(adc_sample_size);

	do_toggle_vcc_probe(false);
	do_toggle_sesnor(true);

	os_delay_us(128);

	for(i = 0; i < adc_sample_size; i++) {
		adc_samples[i] = system_adc_read();
	}

	do_toggle_sesnor(false);

	sum = 0;

	for(i = 0; i < adc_sample_size; i++) {
		sum = sum + adc_samples[i];
	}

	os_free(adc_samples);

	return sum / adc_sample_size;
}

void ICACHE_FLASH_ATTR
do_clear_wifi_scan_result(ap_list_head_t *ap_list_head) {
	ap_list_element_t *ap_list_element = NULL;

	#if (ENABLE_DEBUG == 1)
	        os_printf("\n[+] DBG: do_clear_wifi_scan_result()\n");
	#endif

	while(!SLIST_EMPTY(ap_list_head)) {
		ap_list_element = SLIST_FIRST(ap_list_head);
		SLIST_REMOVE_HEAD(ap_list_head, next);
		free(ap_list_element);
	}

}

bool ICACHE_FLASH_ATTR
do_start_wifi_scan(void) {
	#if (ENABLE_DEBUG == 1)
	        os_printf("\n[+] DBG: do_start_wifi_scan()\n");
	#endif

	if(waiting_wifi_scan_cb) {
		return true;
	}

	if(!wifi_station_scan(NULL, cb_wifi_scan_done)) {
		return false;
	}

	waiting_wifi_scan_cb = true;

	return true;
}

void ICACHE_FLASH_ATTR
do_get_wifi_scan_result(char **buffer_response_data) {
	uint32_t ap_count = 0;
	uint32_t ap_total = 0;
	char *buffer_ap_element = NULL;
	char *buffer_ap_element_ssid = NULL;
	char *buffer_ap_element_bssid = NULL;
	ap_list_element_t *ap_list_element = NULL;

	#if (ENABLE_DEBUG == 1)
	        os_printf("\n[+] DBG: do_get_wifi_scan_result()\n");
	#endif

	os_free(*buffer_response_data);
	*buffer_response_data = NULL;

	SLIST_FOREACH(ap_list_element, &ap_list_head, next) {
		ap_count++;
	}

	ap_total = ap_count;
	ap_count = 0;

	if(ap_total < 1) {
		*buffer_response_data = (char *)malloc(4);
		os_bzero(*buffer_response_data, 4);
		os_strcat(*buffer_response_data, "[]");

		return;
	}

	*buffer_response_data = (char *)malloc((ap_total*100) + 2);
	os_bzero(*buffer_response_data, (ap_total*100) + 2);
	os_strcpy(*buffer_response_data, "[\0");

	SLIST_FOREACH(ap_list_element, &ap_list_head, next) {
		ap_count++;

		buffer_ap_element = (char *)malloc(100);
		buffer_ap_element_ssid = (char *)malloc(33);
		buffer_ap_element_bssid = (char *)malloc(16);

		os_bzero(buffer_ap_element, 100);
		os_bzero(buffer_ap_element_ssid, 33);
		os_bzero(buffer_ap_element_bssid, 16);

		os_strncpy(buffer_ap_element_ssid,
		           ap_list_element->ssid,
		           ap_list_element->ssid_len);

		os_sprintf(buffer_ap_element_bssid,
		           "%02X%02X%02X%02X%02X%02X",
		           ap_list_element->bssid[0],
		           ap_list_element->bssid[1],
		           ap_list_element->bssid[2],
		           ap_list_element->bssid[3],
		           ap_list_element->bssid[4],
		           ap_list_element->bssid[5]);

		os_sprintf(buffer_ap_element,
		           fmt_get_wifi_scan_result_data_json,
		           buffer_ap_element_bssid,
		           buffer_ap_element_ssid,
		           ap_list_element->channel,
		           ap_list_element->rssi,
		           ap_list_element->authmode,
		           ap_list_element->is_hidden);

		os_strcat(*buffer_response_data, buffer_ap_element);

		if(ap_count < ap_total) {
			os_strcat(*buffer_response_data, ",\0");
		}

		os_free(buffer_ap_element);
		os_free(buffer_ap_element_ssid);
		os_free(buffer_ap_element_bssid);
	}

	os_strcat(*buffer_response_data, "]\0");
}

void ICACHE_FLASH_ATTR
do_toggle_sesnor(bool toggle) {
	PIN_FUNC_SELECT(GPIO_O_VCC_SENSOR, GPIO_O_FUNC_VCC_SENSOR);

	if(toggle) {
		gpio_output_set(GPIO_O_BIT_VCC_SENSOR, 0, GPIO_O_BIT_VCC_SENSOR, 0);
	}
	else {
		gpio_output_set(0, GPIO_O_BIT_VCC_SENSOR, GPIO_O_BIT_VCC_SENSOR, 0);
	}
}

void ICACHE_FLASH_ATTR
do_read_adc(void) {
	uint32_t average = 0;
	uint32_t percent_change = 0;

	do_notification_next = 0;
	do_notification_current = 0;

	// To avoid division by zero.
	if(config_current->registered_value == 0) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Registered value is 0, will cause division by zero\n");
		#endif

		do_state_error();

		return;
	}

	average = do_get_sensor_reading(ADC_SAMPLE_SIZE);
	percent_change = \
		(config_current->registered_value * config_current->the_plant_threshold_percent) / 100;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: ADC sample size = %d\n", ADC_SAMPLE_SIZE);
		os_printf("\n[+] DBG: ADC average = %d\n", average);
		os_printf("\n[+] DBG: Registered value = %d\n", config_current->registered_value);
		os_printf("\n[+] DBG: Percent change = %d\n", percent_change);
	#endif

	if(config_current->the_plant_threshold_lt_gt == CONFIG_THRESHLOD_LT) {
		if(average <= (config_current->registered_value - percent_change)) {
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: ADC reading triggered thirst notification\n");
				os_printf("\n[+] DBG: WiFi station connecting\n");
			#endif

			do_notification_current = DO_NOTIFICATION_THIRST;
			do_notification_next = 0;
		}
	}
	else if(config_current->the_plant_threshold_lt_gt == CONFIG_THRESHLOD_GT) {
		if(average >= (config_current->registered_value + percent_change)) {
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: ADC reading triggered thirst notification\n");
				os_printf("\n[+] DBG: WiFi station connecting\n");
			#endif

			do_notification_current = DO_NOTIFICATION_THIRST;
			do_notification_next = 0;
		}
	}
	else {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Wrong threshold LT/GT information in current configuration\n");
		#endif

		do_state_error();

		return;
	}

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Sampling ADC for battery voltage\n");
	#endif

	if(is_battery_low(ADC_SAMPLE_SIZE)) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: ADC reading triggered low battery notification\n");
			os_printf("\n[+] DBG: WiFi station connecting\n");
		#endif

		if (do_notification_current != 0) {
			do_notification_next = DO_NOTIFICATION_LOW_BATTERY;
		}
		else {
			do_notification_current = DO_NOTIFICATION_LOW_BATTERY;
			do_notification_next = 0;
		}
	}

	if(do_notification_next == 0 && do_notification_current == 0) {
		// Don't generate notification.
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: ADC reading didn't trigger notification\n");
			os_printf("\n[+] DBG: Going to deep sleep mode\n");
		#endif

		do_set_deep_sleep_mode();

		// Make sure the sensor is powered off.
		do_toggle_sesnor(false);

		// Make sure VCC probe GPIO is on logic low.
		do_toggle_vcc_probe(false);

		do_set_deep_sleep_mode();
		system_deep_sleep(DEEP_SLEEP_DURATION_US);
	}
	else {
		do_notification();
	}
}

uint8_t ICACHE_FLASH_ATTR
do_get_hash_pearson(uint8_t *bytes) {
	uint32_t i = 0;
	uint8_t hash = 0;

	/*
	 * Start from index 1 as we want to skip the first byte that is used to store
	 * the checksum.
	 */
	for(i = 1; i < sizeof(config_t); i++) {
		hash = permutation_pearson[hash ^ bytes[i]];
	}

	return hash;
}

void ICACHE_FLASH_ATTR
do_state_config(void) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: In configuration state\n");
	#endif

	do_led_blue_turn_on();
}

void ICACHE_FLASH_ATTR
do_state_error_toggle_led(void) {
	if(state_error_led_state) {
		do_led_blue_turn_on();
		state_error_led_state = false;
	}
	else {
		do_led_blue_turn_off();
		state_error_led_state = true;
	}
}

void ICACHE_FLASH_ATTR
do_state_error(void) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: In error state\n");
	#endif

	// Turn off WiFi.
	wifi_set_opmode_current(NULL_MODE);

	os_timer_disarm(&timer_generic_software);
	os_timer_setfn(&timer_generic_software, (os_timer_func_t *)do_state_error_toggle_led, NULL);
	os_timer_arm(&timer_generic_software, 1000, true);
}

bool ICACHE_FLASH_ATTR
do_configuration_read(void) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Reading configuration from flash\n");
	#endif

	os_bzero(config_current, sizeof(config_current));

	if(spi_flash_read(CONFIG_SECTOR_FLASH * 4096, (uint32_t *)config_current, sizeof(config_t)) != SPI_FLASH_RESULT_OK) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Configuration reading failed\n");
		#endif

		do_state_error();

		return false;
	}

	return true;
}

void ICACHE_FLASH_ATTR
do_led_blue_turn_on(void) {
	PIN_FUNC_SELECT(GPIO_O_LED_BLUE, GPIO_O_FUNC_LED_BLUE);
	gpio_output_set(0, GPIO_O_BIT_LED_BLUE, GPIO_O_BIT_LED_BLUE, 0);
}

void ICACHE_FLASH_ATTR
do_led_blue_turn_off(void) {
	PIN_FUNC_SELECT(GPIO_O_LED_BLUE, GPIO_O_FUNC_LED_BLUE);
	gpio_output_set(GPIO_O_BIT_LED_BLUE, 0, GPIO_O_BIT_LED_BLUE, 0);
}

void ICACHE_FLASH_ATTR
do_set_deep_sleep_mode(void) {
	if(data_rtc == 0) {
		// Timer unit wrapped.
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Handling timer unit wrapper event\n");
		#endif

		data_rtc++;

		if(system_rtc_mem_write(MEM_ADDR_RTC, &data_rtc, 4)) {
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: RTC count up and write = %d\n", data_rtc);
			#endif

			// Make sure the sensor is powered off.
			do_toggle_sesnor(false);

			// Make sure VCC probe GPIO is on logic low.
			do_toggle_vcc_probe(false);

			if(data_rtc >= HOURS_IN_A_DAY) {
				system_deep_sleep_set_option(DEEP_SLEEP_OPTION_SAME_AS_PWRUP);
			}
			else {
				system_deep_sleep_set_option(DEEP_SLEEP_OPTION_NO_RADIO);
			}

			system_deep_sleep(DEEP_SLEEP_DURATION_US);
		}
		else {
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: RTC memory write failed\n");
			#endif

			do_state_error();
		}
	}
	else if(data_rtc >= HOURS_IN_A_DAY) {
		// Do RF calibration after next wakeup.
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Setting deep sleep option = DEEP_SLEEP_OPTION_SAME_AS_PWRUP\n");
		#endif

		system_deep_sleep_set_option(DEEP_SLEEP_OPTION_SAME_AS_PWRUP);
	}
	else {
		// Keep radio turned off after next wakeup.
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Setting deep sleep option = DEEP_SLEEP_OPTION_NO_RADIO\n");
		#endif

		system_deep_sleep_set_option(DEEP_SLEEP_OPTION_NO_RADIO);
	}
}

void ICACHE_FLASH_ATTR
do_read_counter_value_from_rtc_mem(void) {
	data_rtc = 0;

	// RTC memory operations.
	if(system_rtc_mem_read(MEM_ADDR_RTC, &data_rtc, 4)) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: RTC memory read = %d\n", data_rtc);
		#endif

		if(data_rtc < HOURS_IN_A_DAY) {
			data_rtc++;

			if(system_rtc_mem_write(MEM_ADDR_RTC, &data_rtc, 4)) {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: RTC count up and write = %d\n", data_rtc);
				#endif

				// Make sure the sensor is powered off.
				do_toggle_sesnor(false);

				// Make sure VCC probe GPIO is on logic low.
				do_toggle_vcc_probe(false);

				do_set_deep_sleep_mode();
				system_deep_sleep(DEEP_SLEEP_DURATION_US);
			}
			else {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: RTC memory write failed\n");
				#endif

				do_state_error();
			}
		}
		else {
			// Reset the data on the RTC memory location.
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: Reset RTC memory data\n");
			#endif

			data_rtc = 0;

			if(system_rtc_mem_write(MEM_ADDR_RTC, &data_rtc, 4)) {
				// Time to do ADC check.
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: RTC write reset data\n");
				#endif

				do_read_adc();
			}
			else {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: RTC memory write failed\n");
				#endif

				do_state_error();
			}
		}
	}
	else {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: RTC memory read failed\n");
		#endif

		do_state_error();
	}
}

void ICACHE_FLASH_ATTR
do_config_mode(void) {
	struct ip_info ip;
	struct softap_config config_ap_interface;

	// Configuration mode.
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: In configuration mode\n");
	#endif

	// Clear out configuration storage.
	os_bzero(&ip, sizeof(ip));
	os_bzero(&config_ap_interface, sizeof(config_ap_interface));

	// Enable Station + AP mode.
	wifi_set_opmode_current(STATIONAP_MODE);
	wifi_softap_dhcps_stop();

	// Set AP mode configuration parameters.
	IP4_ADDR(&ip.ip, 192, 168, 7, 1);
	IP4_ADDR(&ip.gw, 192, 168, 7, 1);
	IP4_ADDR(&ip.netmask, 255, 255, 255, 0);
	wifi_set_ip_info(SOFTAP_IF, &ip);

	config_ap_interface.channel = 7;
	config_ap_interface.max_connection = 4;
	config_ap_interface.beacon_interval = 50;
	config_ap_interface.authmode = AUTH_WPA_WPA2_PSK;

	os_memcpy(config_ap_interface.ssid,
		  config_current->the_plant_name,
		  os_strlen(config_current->the_plant_name));
	os_memcpy(config_ap_interface.password,
		  config_current->the_plant_configuration_password,
		  os_strlen(config_current->the_plant_configuration_password));

	wifi_softap_dhcps_start();

	// Apply AP configuration.
	wifi_softap_set_config_current(&config_ap_interface);

	// Initialise ESPHTTPD web pages.
	if(espFsInit((void*)(&webpages_espfs_start)) != ESPFS_INIT_RESULT_OK) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: ESPFS initialisation for HTTPD failed\n");
		#endif

		do_state_error();

		return;
	}

	// Start ESPHTTPD.
	httpdInit(httpd_urls, 80);

	do_state_config();
}

void ICACHE_FLASH_ATTR
cb_system_init_done(void) {
	uint32_t i = 0;
	struct ip_info ip;
	uint8_t config_hash = 0;
	bool config_none = false;
	uint32_t gpio_i_config_mode_value = 1;
	struct softap_config config_ap_interface;
	ap_list_element_t *ap_list_element = NULL;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_system_init_done()\n");
		os_printf("\n[+] DBG: SDK version = %s\n", system_get_sdk_version());
	#endif

	/*
	 * We enable Station + AP mode instead of NULL mode here because we must
	 * be ready to handle AP mode anytime.
	 */
	wifi_set_opmode_current(STATIONAP_MODE);
	system_deep_sleep_set_option(DEEP_SLEEP_OPTION_SAME_AS_PWRUP);

	// Make sure the sensor is powered off.
	do_toggle_sesnor(false);

	// Make sure VCC probe GPIO is on logic low.
	do_toggle_vcc_probe(false);

	// Clear WiFi scan result and initialize WiFi scan list.
	do_clear_wifi_scan_result(&ap_list_head);
	SLIST_INIT(&ap_list_head);

	// Must not be de-allocated.
	buffer_post_form = (char *)os_malloc(4096); // Used by web interface module.
	config_current = (config_t *)os_malloc(sizeof(config_t));

	if(!do_configuration_read()) {
		return;
	}

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Configuration reading successful\n");
		os_printf("\n[+] DBG: Waiting for configuration mode pin state change\n");
	#endif

	// Configure configuration mode selector GPIO pin as input.
	PIN_FUNC_SELECT(GPIO_I_CONFIG_MODE, GPIO_I_FUNC_CONFIG_MODE);
	gpio_output_set(0, 0, 0, GPIO_I_BIT_CONFIG_MODE);
	PIN_PULLUP_EN(GPIO_I_CONFIG_MODE);

	// Wait for 2 seconds.
	for(i = 0; i < 15; i++) {
		/*
		 * In firmware version v1 the argument of os_delay_us() was of
		 * type uint32_t. From firmware version v2 it is uint16_t. So to
		 * achieve a delay of 2 seconds (1000000us) here, we iterate the
		 * delay with the highest possible value for uint16_t type.
		 */
		os_delay_us(65535);
	}

	/*
	 * Check whether to switch to configuration mode or not.
	 * If grounded then 0 (configuration mode button is pressed) otherwise 1.
	 */
	gpio_i_config_mode_value = GPIO_INPUT_GET(0);

	// Generate pearson hash for the configuration that is read from the flash.
	config_hash = do_get_hash_pearson((uint8_t *)config_current);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Pearson hash of the read configuration = 0x%x\n", config_hash);
	#endif

	if(config_hash != config_current->config_hash_pearson) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Configuration hash mismatch, calcualted = 0x%x, found = 0x%x\n",
			config_hash,
			config_current->config_hash_pearson);
		#endif

		config_none = true;

		/*
		 * Since no valid configuration was found on the flash the current
		 * configuration will be initialised with default values.
		 *
		 * This default configuration in only applicable to be used in the
		 * configuration mode. Normal operation can not continue with this
		 * configuration.
		 */

		// Set the deflault plant name on default configuration.
		do_get_default_plant_name(config_default.the_plant_name,
		                          sizeof(config_default.the_plant_name));

		// Set default configuration hash.
		config_default.config_hash_pearson = do_get_hash_pearson((uint8_t *)&config_default);

		// Set default configuration as the current configuration.
		os_bzero(config_current, sizeof(config_t));
		os_memcpy(config_current, &config_default, sizeof(config_default));
	}

	if (gpio_i_config_mode_value == 0) {
		do_config_mode();
	}
	else {
		if(config_none) {
			do_config_mode();

			return;
		}

		do_read_counter_value_from_rtc_mem();
	}
}

void ICACHE_FLASH_ATTR
do_setup_debug_UART(void) {
	uart_div_modify(UART0, UART_BIT_RATE);

	system_set_os_print(1);
}

void ICACHE_FLASH_ATTR
user_init(void) {
	#if (ENABLE_DEBUG == 1)
		// Setup debug UART.
		do_setup_debug_UART();
		os_printf("\n[+] DBG: user_init()\n");
	#endif

	gpio_init();
	do_led_blue_turn_off();

	// Register system initialisation done callback.
	system_init_done_cb(cb_system_init_done);
}

