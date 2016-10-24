#include "main.h"
#include "osapi.h"
#include "user_config.h"
#include "espconn.h"

// Socket stuffs.
esp_tcp sock_tcp;
struct espconn sock;
ip_addr_t ip_dns_resolved;

// Generic software timer.
os_timer_t timer_generic_software;

void
cb_timer_generic_software(void) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_timer_generic_software()\n");
		os_printf("\n[+] DBG: Turning off the notification LED\n");
		os_printf("\n[+] DBG: Switching to deep sleep mode\n");
	#endif

	// Turn off the notification LED.
	gpio_output_set(GPIO_NOTIFICATION_LED_BIT, 0, GPIO_NOTIFICATION_LED_BIT, 0);
	system_deep_sleep(DEEP_SLEEP_INTERVAL);
}

void ICACHE_FLASH_ATTR
cb_sock_disconnect(void *arg) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_disconnect()\n");
		os_printf("\n[+] DBG: Arming generic software timer\n");
		os_printf("\n[+] DBG: Turning on the notification LED\n");
	#endif

	// Arm the generic software timer.
	os_timer_disarm(&timer_generic_software);
	os_timer_setfn(&timer_generic_software, cb_timer_generic_software, NULL);
	os_timer_arm(&timer_generic_software, TIMER_DURATION_NOTIFICATION_LED_OFF, 0);

	// Turn on the notification LED.
	gpio_output_set(0, GPIO_NOTIFICATION_LED_BIT, GPIO_NOTIFICATION_LED_BIT, 0);
}

void ICACHE_FLASH_ATTR
cb_sock_recv(void *arg, char *data, unsigned short length) {
	struct espconn *sock = (struct espconn *)arg;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_recv()\n");
		os_printf("\n[+] DBG: Received length = %d, data = %s\n", length, data);
	#endif

	espconn_disconnect(sock);
}

void ICACHE_FLASH_ATTR
cb_sock_sent(void *arg) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_sent()\n");
	#endif
}

void ICACHE_FLASH_ATTR
cb_sock_connect(void *arg) {
	struct espconn *sock = (struct espconn *)arg;
	char buffer_message[POSTMATK_SIZE_SEND_BUFFER];

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_sock_connect()\n");
	#endif

	espconn_regist_sentcb(sock, cb_sock_sent);
	espconn_regist_recvcb(sock, cb_sock_recv);
	espconn_regist_disconcb(sock, cb_sock_disconnect);

	os_sprintf(buffer_message,
		POSTMATK_FMT_HTTP_HEADER,
		os_strlen(POSTMATK_DATA_HTTP_JSON),
		POSTMATK_API_TOKEN,
		POSTMATK_DATA_HTTP_JSON);

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Sending message\n");
	#endif

	espconn_send(sock, buffer_message, os_strlen(buffer_message));
}

void ICACHE_FLASH_ATTR
cb_dns_resolved(const char *hostname, ip_addr_t *ip_resolved, void *arg) {
	char ip_server[4];
	struct espconn *sock = (struct espconn *)arg;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_dns_resolved()\n");
	#endif

	if (ip_resolved != NULL) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: Host = %s, IP = %d.%d.%d.%d\n",
			hostname,
			*((uint8 *)&ip_resolved->addr),
			*((uint8 *)&ip_resolved->addr + 1),
			*((uint8 *)&ip_resolved->addr + 2),
			*((uint8 *)&ip_resolved->addr + 3));
		#endif

		ip_server[0] = *((uint8 *)&ip_resolved->addr);
		ip_server[1] = *((uint8 *)&ip_resolved->addr + 1);
		ip_server[2] = *((uint8 *)&ip_resolved->addr + 2);
		ip_server[3] = *((uint8 *)&ip_resolved->addr + 3);

		// Configure the socket.
		sock->proto.tcp = &sock_tcp;
		sock->type = ESPCONN_TCP;
		sock->state = ESPCONN_NONE;
		sock->proto.tcp->remote_port = POSTMATK_API_PORT;
		sock->proto.tcp->local_port = espconn_port();

		// Connect the socket.
		os_memcpy(sock->proto.tcp->remote_ip, ip_server, 4);
		espconn_regist_connectcb(sock, cb_sock_connect);
		espconn_connect(sock);
	}
}

void ICACHE_FLASH_ATTR
cb_wifi_event(System_Event_t *evt) {
	err_t e;

	if(evt->event == EVENT_STAMODE_GOT_IP) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: cb_wifi_event()\n");
			os_printf("\n[+] DBG: Got IP\n");
		#endif

		e = espconn_gethostbyname(&sock,
			POSTMATK_API_HOST,
			&ip_dns_resolved,
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
}

void  ICACHE_FLASH_ATTR
do_read_ADC(void) {
	// WiFi station related
	struct station_config st_config;
	uint32_t summation = 0, average = 0;
	uint16_t adc_samples[ADC_SAMPLE_SIZE];
	char wifi_station_ssid[32] = WIFI_STATION_SSID;
	char wifi_station_ssid_password[64] = WIFI_STATION_SSID_PASSWORD;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: do_read_ADC()\n");
	#endif

	// Disable interrupts.
	ets_intr_lock( );

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: Samplig ADC\n");
	#endif

	system_adc_read_fast(adc_samples, ADC_SAMPLE_SIZE, ADC_CLK_DIV);

	for(i = 0; i < ADC_SAMPLE_SIZE; i++) {
		summation = summation + adc_samples[i];
	}

	average = summation/ADC_SAMPLE_SIZE;

	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: ADC summation = %d\n", summation);
		os_printf("\n[+] DBG: ADC sample size = %d\n", ADC_SAMPLE_SIZE);
		os_printf("\n[+] DBG: ADC average = %d\n", average);
	#endif

	// Enable interrupts.
	ets_intr_unlock();

	if(average > ADC_SENSOR_THRESHOLD) {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: ADC reading above threshold\n");
			os_printf("\n[+] DBG: WiFi station connecting\n");
		#endif

		// Update WiFi station settings.
		os_memcpy(&st_config.ssid, wifi_station_ssid, 32);
		os_memcpy(&st_config.password, wifi_station_ssid_password, 64);

		// Apply WiFi settings.
		wifi_station_set_config(&st_config);

		// Set callback for WiFi events.
		wifi_set_event_handler_cb(cb_wifi_event);

		// Enable station mode.
		wifi_set_opmode(STATION_MODE);

		// Enable station mode reconnect.
		wifi_station_set_reconnect_policy(true);

		// Connect WiFi in station mode.
		wifi_station_connect();
	}
	else {
		#if (ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG: ADC reading below threshold\n");
			os_printf("\n[+] DBG: Switching to deep sleep mode\n");
		#endif

		system_deep_sleep(DEEP_SLEEP_INTERVAL);
	}
}

void ICACHE_FLASH_ATTR
cb_system_init_done(void) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: cb_system_init_done()\n");
		os_printf("\n[+] DBG: SDK version = %s\n", system_get_sdk_version());
	#endif

	// Setup and turn off the notification LED.
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, GPIO_NOTIFICATION_LED);
	gpio_output_set(GPIO_NOTIFICATION_LED_BIT, 0, GPIO_NOTIFICATION_LED_BIT, 0);

	// Read ADC.
	do_read_ADC();
}

void ICACHE_FLASH_ATTR
do_setup_debug_UART() {
	uart_div_modify(UART0, UART_BIT_RATE);
	uart_div_modify(UART1, UART_BIT_RATE);
	system_set_os_print(1);
}

void ICACHE_FLASH_ATTR
user_init(void) {
	#if (ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG: user_init()\n");
		// Setup debug UART.
		do_setup_debug_UART();
	#endif

	// Register system initialisation done callback.
	system_init_done_cb(cb_system_init_done);
}
