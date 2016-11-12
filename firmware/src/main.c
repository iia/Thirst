#include <mem.h>
#include "main.h"
#include "espfs.h"
#include "platform.h"
#include "httpdespfs.h"
#include "driver/uart.h"
#include "web_interface.h"

void
cb_timer_led_blue_blink(void *arg) {
	led_blink_parameters_t *params_blink = (led_blink_parameters_t *)arg;

	// Toggle LED.
	if(params_blink->action){
		do_led_blue_turn_on();
	}
	else {
		do_led_blue_turn_off();
	}

	params_blink->times--;
	params_blink->action = !params_blink->action;

	if(params_blink->times == 0) {
		os_timer_disarm(&timer_generic_software);

		// Blinking sequence complete.
		#if (ENABLE_DEBUG == 1)
      os_printf("\n[+] DBG: Blue LED blink sequence done\n");

			if(params_blink->cb_blink_done != NULL) {
				params_blink->cb_blink_done();
			}
    #endif

    system_deep_sleep(60000000); // Deep sleep for a minute.
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
	struct espconn *sock = (struct espconn *)arg;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_recv()\n");
		os_printf("\n[+] DBG: Received length = %d, data = %s\n", length, data);
	#endif

	// Setup timer to call disconnect on the socket.
	os_timer_setfn(&timer_generic_software, (os_timer_func_t *)cb_timer_disconnect_sock, NULL);
	os_timer_arm(&timer_generic_software, 2000, false);
}

void ICACHE_FLASH_ATTR
cb_sock_disconnect(void *arg) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_disconnect()\n");
	#endif

	os_printf("\n[+] DBG: Restarting...\n");

	system_restart();
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
	struct espconn *sock = (struct espconn *)arg;
	char buffer_message[POSTMARK_SIZE_SEND_BUFFER];

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_connect()\n");
	#endif

	espconn_regist_sentcb(sock, cb_sock_sent);
	espconn_regist_recvcb(sock, cb_sock_recv);
	espconn_regist_disconcb(sock, cb_sock_disconnect);

	os_sprintf(buffer_json_data, FMT_POSTMARK_DATA_HTTP_JSON,
		config_current.notification_email, config_current.notification_subject,
		config_current.notification_message);

	os_sprintf(buffer_message, FMT_POSTMARK_HTTP_HEADER,
		os_strlen(buffer_json_data), POSTMARK_API_TOKEN, buffer_json_data);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Sending request, message = %s\n", buffer_message);
	#endif

	// Send request.
	if(espconn_send(sock, buffer_message, os_strlen(buffer_message))) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Sending data using socket failed\n");
		#endif

		do_state_error();
	}
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

		// Configure the socket.
		sock->proto.tcp = &sock_tcp;
		sock->type = ESPCONN_TCP;
		sock->state = ESPCONN_NONE;
		sock->proto.tcp->remote_port = POSTMARK_API_PORT;
		sock->proto.tcp->local_port = espconn_port();
		os_memcpy(sock->proto.tcp->remote_ip, ip_server, 4);

		espconn_regist_connectcb(sock, cb_sock_connect);

		// Connect the socket.
		if(espconn_connect(sock)) {
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: Socket connect failed\n");
			#endif

			do_state_error();
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
cb_wifi_event(System_Event_t *evt) {
	err_t e;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_wifi_event()\n");
	#endif

	if(evt->event == EVENT_STAMODE_GOT_IP) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Got IP\n");
		#endif

		e = espconn_gethostbyname(&sock, POSTMARK_API_HOST, &ip_dns_resolved,
			cb_dns_resolved);

		switch(e) {
			case ESPCONN_OK: {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: DNS lookup started\n");
				#endif
			}

			case ESPCONN_ARG: {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: DNS lookup argument error\n");
				#endif
			}

			case ESPCONN_INPROGRESS: {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: DNS lookup in progress\n");
				#endif
			}
		}
	}
	else if(evt->event == STATION_WRONG_PASSWORD) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi station connect failed, wrong password\n");
		#endif

		do_state_error();
	}
	else if(evt->event == STATION_NO_AP_FOUND) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi station connect failed, no AP found\n");
		#endif

		do_state_error();
	}
	else if(evt->event == STATION_CONNECT_FAIL) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: WiFi station connect failed\n");
		#endif

		do_state_error();
	}
}

void ICACHE_FLASH_ATTR toggle_sesnor(bool toggle) {
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
	int i = 0;
	int average = 0;
	uint32_t sum = 0;
	int percent_change = 0;
	bool trigger_notification = false;
	uint16_t adc_samples[ADC_SAMPLE_SIZE];
	struct station_config config_st_interface;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: do_read_adc()\n");
		os_printf("\n[+] DBG: Samplig ADC\n");
	#endif

	// Disable interrupts.
	ets_intr_lock( );

	toggle_sesnor(true);
	os_delay_us(4000); // 4ms.

	system_adc_read_fast(adc_samples, ADC_SAMPLE_SIZE, ADC_CLK_DIV);

	os_delay_us(4000); // 4ms.
	toggle_sesnor(false);

	for(i = 0; i < ADC_SAMPLE_SIZE; i++) {
		sum = sum + adc_samples[i];
	}

	average = sum / ADC_SAMPLE_SIZE;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: ADC sum = %d\n", sum);
		os_printf("\n[+] DBG: ADC sample size = %d\n", ADC_SAMPLE_SIZE);
		os_printf("\n[+] DBG: ADC average = %d\n", average);
		os_printf("\n[+] DBG: Registered value = %d\n", config_current.registered_value);
	#endif

	// Enable interrupts.
	ets_intr_unlock();

	// To avoid division by zero.
	if(config_current.registered_value == 0) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Registered value is 0, will cause division by zero\n");
		#endif

		do_state_error();

		return;
	}

	// Calculate percent change.
	percent_change =
	(((average - config_current.registered_value) * 100) / config_current.registered_value);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Percent change = %d\n", percent_change);
	#endif

	// Positive percent change.
	if(percent_change > 0) {
		if(config_current.plant_threshold_lt_gt == CONFIG_THRESHLOD_GT &&
			(percent_change >= config_current.plant_threshold_percent)) {
				trigger_notification = true;
		}
	}
	// Negative percent change.
	else if(percent_change < 0) {
		if(config_current.plant_threshold_lt_gt == CONFIG_THRESHLOD_LT &&
			((percent_change * -1) >= config_current.plant_threshold_percent)) {
				trigger_notification = true;
		}
	}
	// No percent change.
	else {
		trigger_notification = false;
	}

	if(trigger_notification) {
		// Generate a notification.
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: ADC reading triggered notification\n");
			os_printf("\n[+] DBG: WiFi station connecting\n");
		#endif

		// Clear out WiFi station interface configuration storage.
		os_memset(&config_st_interface, 0, sizeof(config_st_interface));

		// WiFi station settings.
		config_st_interface.bssid_set = 0;
		os_memcpy(&config_st_interface.ssid, config_current.plant_wifi_ssid,
			os_strlen(config_current.plant_wifi_ssid));
		os_memcpy(&config_st_interface.password, config_current.plant_wifi_ssid_password,
			os_strlen(config_current.plant_wifi_ssid_password));

		// Apply WiFi settings.
		wifi_station_set_config_current(&config_st_interface);

		// Set callback for WiFi events.
		wifi_set_event_handler_cb(cb_wifi_event);

		// Enable station mode.
		wifi_set_opmode(STATION_MODE);

		// Enable station mode reconnect.
		wifi_station_set_reconnect_policy(true);

		// FIXME: Sometimes not connecting to WiFi until a reset.
		// Connect WiFi in station mode.
		if(!wifi_station_connect()) {
			#if (ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG: WiFi station mode connect failed\n");
			#endif

			do_state_error();

			return;
		}
	}
	else {
		// Don't generate notification.
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: ADC reading didn't trigger notification\n");
			os_printf("\n[+] DBG: Restarting...\n");
		#endif

		system_restart();
	}
}

uint8_t ICACHE_FLASH_ATTR
do_get_hash_pearson(uint8_t *bytes) {
	int i = 0;
	uint8_t hash = 0;

	for(i = 1; i < sizeof(bytes); i++) {
		hash = permutation_pearson[hash ^ bytes[i]];
	}

	return hash;
}

void ICACHE_FLASH_ATTR
do_state_config(void) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: In configuration state\n");
	#endif

	// Turn off both the blue and the red LEDs.
	do_leds_turn_off();

	// Turn on the blue LED.
	do_led_blue_turn_on();
}

void ICACHE_FLASH_ATTR
do_state_error(void) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: In error state\n");
	#endif

	// Turn off both the blue and the red LEDs.
	do_leds_turn_off();

	// Turn on the blue LED.
	do_led_red_turn_on();
}

bool ICACHE_FLASH_ATTR
do_configuration_read(uint32_t *config_buffer) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Reading configuration from flash\n");
	#endif

	if(spi_flash_read((uint32_t)(CONFIG_SECTOR_FLASH * 4096),
	config_buffer, (uint32_t)sizeof(config_t)) != SPI_FLASH_RESULT_OK) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Configuration reading failed\n");
		#endif

		do_state_error();

		return false;
	}

	return true;
}

void ICACHE_FLASH_ATTR
do_led_red_turn_on(void) {
	PIN_FUNC_SELECT(GPIO_O_LED_RED, GPIO_O_FUNC_LED_RED);
	gpio_output_set(0, GPIO_O_BIT_LED_RED, GPIO_O_BIT_LED_RED, 0);
}

void ICACHE_FLASH_ATTR
do_led_blue_turn_on(void) {
	PIN_FUNC_SELECT(GPIO_O_LED_BLUE, GPIO_O_FUNC_LED_BLUE);
	gpio_output_set(0, GPIO_O_BIT_LED_BLUE, GPIO_O_BIT_LED_BLUE, 0);
}

void ICACHE_FLASH_ATTR
do_led_red_turn_off(void) {
	PIN_FUNC_SELECT(GPIO_O_LED_RED, GPIO_O_FUNC_LED_RED);
	gpio_output_set(GPIO_O_BIT_LED_RED, 0, GPIO_O_BIT_LED_RED, 0);
}

void ICACHE_FLASH_ATTR
do_led_blue_turn_off(void) {
	PIN_FUNC_SELECT(GPIO_O_LED_BLUE, GPIO_O_FUNC_LED_BLUE);
	gpio_output_set(GPIO_O_BIT_LED_BLUE, 0, GPIO_O_BIT_LED_BLUE, 0);
}

void ICACHE_FLASH_ATTR
do_leds_turn_off(void) {
	do_led_blue_turn_off();
	do_led_red_turn_off();
}

void ICACHE_FLASH_ATTR
do_led_blue_blink(uint32_t times, uint32_t interval, void (*cb_blink_done)(void)) {
	os_timer_disarm(&timer_generic_software);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: do_led_blue_blink()\n");
	#endif

	// Turn off both the blue and the red LEDs.
	do_leds_turn_off();

	// Clear out timer parameter storage.
	os_memset(&led_blue_params_blink, 0 , sizeof(led_blue_params_blink));

	// Initialise timer parameters.
	led_blue_params_blink.action = true;
	led_blue_params_blink.times = times;
	led_blue_params_blink.interval = interval;
	led_blue_params_blink.cb_blink_done = cb_blink_done;

	os_timer_setfn(&timer_generic_software, (os_timer_func_t *)cb_timer_led_blue_blink,
	(void *)&led_blue_params_blink);

	os_timer_arm(&timer_generic_software, interval, true);
}

void ICACHE_FLASH_ATTR
do_read_counter_value_from_rtc_mem(void) {
	uint32_t time_duration, data_rtc = 0;

	if(config_current.frequency_type == CONFIG_FREQUENCY_DAY) {
		time_duration = config_current.frequency * 1441;
	}
	else if(config_current.frequency_type == CONFIG_FREQUENCY_MINUTE) {
		time_duration = config_current.frequency;
	}

	// RTC memory operations.
	if(system_rtc_mem_read(MEM_ADDR_RTC, &data_rtc, 4)) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: RTC memory read = %d\n", data_rtc);
		#endif

		if(data_rtc < time_duration) {
			data_rtc++;

			if(system_rtc_mem_write(MEM_ADDR_RTC, &data_rtc, 4)) {
				#if (ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG: RTC count up and write = %d\n", data_rtc);
				#endif
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
cb_system_init_done(void) {
	struct ip_info ip;
	uint8_t mac_softap[6];
	uint8_t config_hash = 0;
	bool config_none = false;
	uint32_t gpio_i_config_mode_value = 1;
	struct softap_config config_ap_interface;
	config_t *ptr_config_current = (config_t *)os_malloc(sizeof(config_t));

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_system_init_done()\n");
		os_printf("\n[+] DBG: SDK version = %s\n", system_get_sdk_version());
	#endif

	if(!do_configuration_read((uint32_t *)ptr_config_current)) {
		return;
	}

	// Clear out current configuration storage.
	os_memset(&config_current, 0, sizeof(config_t));
	// Copy configuration read from the flash to the current configuration storage.
	os_memcpy(&config_current, ptr_config_current, sizeof(config_t));
	// Deallocate memory which was used to read the configuration from the flash.
	os_free(ptr_config_current);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Configuration reading successful\n");
	#endif

	// Generate pearson hash for the configuration that is read from the flash.
	config_hash = do_get_hash_pearson((uint8_t *)&config_current);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Pearson hash of the read configuration = 0x%x\n", config_hash);
	#endif

	if(config_hash != config_current.config_hash_pearson) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Configuration hash mismatch, calcualted = 0x%x, found = 0x%x\n",
			config_hash,
			config_current.config_hash_pearson);
		#endif

		config_none = true;

		/*
		 * Since no valid configuration was found on the flash the current
		 * configuration will be initialised with default values. This default
		 * configuration in only applicable to be used in the cvonfiguration mode.
		 * Normal operation can not continue with this configuration.
		 */

		// Clear out the storage for current configuration.
		os_memset(&config_current, 0, sizeof(config_t));

		// Get MAC address of the AP interface.
		os_memset(mac_softap, 0, sizeof(mac_softap));
		wifi_get_macaddr(SOFTAP_IF, (uint8_t *)mac_softap);

		/*
		 * Populate current configuration fields with default values. Not all fields
		 * are populated here. Since they are expected to be zero in default configuration.
		 */
		os_sprintf(config_current.plant_name, FMT_CONFIG_DEFAULT_PLANT_NAME,
			mac_softap[0], mac_softap[1], mac_softap[2], mac_softap[3], mac_softap[4],
			mac_softap[5]);
		os_memcpy(config_current.plant_ap_password, CONFIG_DEFAULT_PLANT_AP_PASSWORD,
			os_strlen(CONFIG_DEFAULT_PLANT_AP_PASSWORD));

		config_current.plant_threshold_percent = CONFIG_DEFAULT_THRESHLOD_PERCENT;
		config_current.plant_threshold_lt_gt = CONFIG_THRESHLOD_LT;

		config_current.frequency = CONFIG_DEFAULT_FREQUENCY;
		config_current.frequency_type = CONFIG_FREQUENCY_DAY;
	}

	// Configure configuration mode selector GPIO pin as input.
	gpio_output_set(0, GPIO_I_BIT_CONFIG_MODE, 0, GPIO_I_BIT_CONFIG_MODE);
	PIN_PULLUP_EN(GPIO_I_CONFIG_MODE);

	/*
	 * Check whether to switch to configuration mode or not.
	 * If grounded then 0 (configuration mode button is pressed) otherwise 1.
	 */
	gpio_i_config_mode_value = GPIO_INPUT_GET(4);

	if (gpio_i_config_mode_value == 0) {
		// config_t mode.
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: In configuration mode\n");
		#endif

		// Clear out configuration storage.
		os_memset(&ip, 0, sizeof(ip));
		os_memset(&config_ap_interface, 0 , sizeof(config_ap_interface));

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
		os_memcpy(config_ap_interface.ssid, config_current.plant_name,
			os_strlen(config_current.plant_name));
		os_memcpy(config_ap_interface.password, config_current.plant_ap_password,
			os_strlen(config_current.plant_ap_password));

		wifi_softap_dhcps_start();

		// Enable AP mode.
		wifi_set_opmode(SOFTAP_MODE);

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
	else {
		if(config_none) {
			do_state_error();

			return;
		}

		/*
		 * Blink sequence of 16 seconds with 125ms interval and then call function
		 * to check timing state.
		 */
		do_led_blue_blink(32, 125, &do_read_counter_value_from_rtc_mem);
	}
}

void ICACHE_FLASH_ATTR
do_setup_debug_UART() {
	uart_div_modify(UART0, UART_BIT_RATE);

	system_set_os_print(1);
}

void ICACHE_FLASH_ATTR
user_init(void) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: user_init()\n");

		// Setup debug UART.
		do_setup_debug_UART();
	#endif

	gpio_init();

	do_leds_turn_off();

	// Register system initialisation done callback.
	system_init_done_cb(cb_system_init_done);
}
