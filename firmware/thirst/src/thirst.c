#include "thirst.h"

// Global variables.

// Configuration.
config_t* config_current;
uint32_t config_flash_sector;

// Gateway.
esp_tcp gateway_sock_tcp;
struct espconn gateway_sock;
ip_addr_t gateway_ip_dns_resolved;

// Web interface.
extern char* web_interface_buffer_post_form;

// System.
SYS_STATE_t sys_state;
os_timer_t sys_timer_sw;
bool sys_state_error_led;
SYS_RTC_DATA_t sys_rtc_data;
os_event_t* sys_task_queue_main;
SYS_DEEP_SLEEP_PARAMS_t sys_deep_sleep_params;

// Pearson hash.
static const uint8_t config_permutation_pearson[CONFIG_PERMUTATION_PEARSON_SIZE] = \
	{
		1, 87, 49, 12, 176, 178, 102, 166, 121, 193, 6, 84, 249, 230, 44, 163,
		14, 197, 213, 181, 161, 85, 218, 80, 64, 239, 24, 226, 236, 142, 38, 200,
		110, 177, 104, 103, 141, 253, 255, 50, 77, 101, 81, 18, 45, 96, 31, 222,
		25, 107, 190, 70, 86, 237, 240, 34, 72, 242, 20, 214, 244, 227, 149, 235,
		97, 234, 57, 22, 60, 250, 82, 175, 208, 5, 127, 199, 111, 62, 135, 248,
		174, 169, 211, 58, 66, 154, 106, 195, 245, 171, 17, 187, 182, 179, 0, 243,
		132, 56, 148, 75, 128, 133, 158, 100, 130, 126, 91, 13, 153, 246, 216, 219,
		119, 68, 223, 78, 83, 88, 201, 99, 122, 11, 92, 32, 136, 114, 52, 10,
		138, 30, 48, 183, 156, 35, 61, 26, 143, 74, 251, 94, 129, 162, 63, 152,
		170, 7, 115, 167, 241, 206, 3, 150, 55, 59, 151, 220, 90, 53, 23, 131,
		125, 173, 15, 238, 79, 95, 89, 16, 105, 137, 225, 224, 217, 160, 37, 123,
		118, 73, 2, 157, 46, 116, 9, 145, 134, 228, 207, 212, 202, 215, 69, 229,
		27, 188, 67, 124, 168, 252, 42, 4, 29, 108, 21, 247, 19, 205, 39, 203,
		233, 40, 186, 147, 198, 192, 155, 33, 164, 191, 98, 204, 165, 180, 117, 76,
		140, 36, 210, 172, 41, 54, 159, 8, 185, 232, 113, 196, 231, 47, 146, 120,
		51, 65, 28, 144, 254, 221, 93, 189, 194, 139, 112, 43, 71, 109, 184, 209
	};

/*
 * Partition table (required since SDK 3.0.0).
 *
 * Entries are of form: Type, Start Address, Size.
 */
static const partition_item_t sys_partition_table[] = {
	{
		SYSTEM_PARTITION_BOOTLOADER,
		SYS_PARTITION_BOOTLOADER_ADDR,
		SYS_PARTITION_BOOTLOADER_SIZE
	},
	{
		SYSTEM_PARTITION_OTA_1,
		SYS_PARTITION_OTA_FW_1_ADDR,
		SYS_PARTITION_OTA_FW_SIZE
	},
	{
		SYSTEM_PARTITION_OTA_2,
		SYS_PARTITION_OTA_FW_2_ADDR,
		SYS_PARTITION_OTA_FW_SIZE
	},
	{
		SYSTEM_PARTITION_RF_CAL,
		SYS_PARTITION_RF_CAL_ADDR,
		SYS_PARTITION_RF_CAL_SIZE
	},
	{
		SYSTEM_PARTITION_PHY_DATA,
		SYS_PARTITION_PHY_DATA_ADDR,
		SYS_PARTITION_PHY_DATA_SIZE
	},
	{
		SYSTEM_PARTITION_SYSTEM_PARAMETER,
		SYS_PARTITION_SYSTEM_PARAMETER_ADDR,
		SYS_PARTITION_SYSTEM_PARAMETER_SIZE
	},
	{
		SYSTEM_PARTITION_CUSTOMER_BEGIN,
		SYS_PARTITION_CONFIG_ADDR,
		SYS_PARTITION_CONFIG_SIZE
	},
};

// Gateway.

void ICACHE_FLASH_ATTR
gateway_cb_sock_rx(void* arg, char* data, unsigned short length) {
	int i = 0;
	int code_http = 0;
	char line_first[256];
	char line_http_code[8];

	// Reset the RX timer.
	os_timer_disarm(&sys_timer_sw);

	os_bzero(&line_first, 256);
	os_bzero(&line_http_code, 8);

	#if (SYS_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: GATEWAY :: RX OK\n");

		os_printf(
			"\n[+] DBG :: GATEWAY :: RX, Length = %d, Data = %s\n",
			length,
			data
		);
	#endif

	// Verify the HTTP response code.
	while ((*data != '\r') && (*data != '\n')) {
		line_first[i] = *data;
		data++;
		i++;
	}

	for (i = 0; i < os_strlen(line_first); i++) {
		if (line_first[i] != ' ') {
			continue;
		}

		os_memcpy(
			(void*)&line_http_code,
			(void*)&line_first[i+1],
			3
		);

		break;
	}

	code_http = strtoul(line_http_code, NULL, 10);

	#if (SYS_ENABLE_DEBUG == 1)
		os_printf(
			"\n[+] DBG :: GATEWAY :: RX, HTTP Response code = %d\n",
			code_http
		);
	#endif

	if (code_http != GATEWAY_HTTP_CODE_ACCEPTED) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG :: GATEWAY :: HTTP response validation failed, received = 0x%X, expected = 0x%X\n",
				code_http,
				GATEWAY_HTTP_CODE_ACCEPTED
			);
		#endif

		sys_state_transition(SYS_STATE_SEND_DATA, SYS_STATE_ERROR);

		if (espconn_secure_disconnect(&gateway_sock)) {
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: GATEWAY :: Socket disconnect failed\n");
			#endif

			if (!wifi_station_disconnect()) {
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf(
						"\n[+] DBG :: GATEWAY :: WiFi station mode disconnect failed\n"
					);
				#endif

				sys_state_transition_with_wifi_mode(
					SYS_STATE_SEND_DATA,
					SYS_STATE_ERROR,
					NULL_MODE
				);
			}
		}

		return;
	}

	// Setup deep sleep config.
	if (SYS_DEEP_SLEEP_TIMES == 1) {
		sys_deep_sleep_params.sleep_type = SYS_DEEP_SLEEP_OPTION_SAME_AS_PWRUP;
	}
	else {
		sys_deep_sleep_params.sleep_type = SYS_DEEP_SLEEP_OPTION_NO_RADIO;
	}

	sys_deep_sleep_params.sleep_duration = SYS_DEEP_SLEEP_DURATION_US;

	sys_state_transition(SYS_STATE_SEND_DATA, SYS_STATE_SEND_DATA_DONE);

	if (espconn_secure_disconnect(&gateway_sock)) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: GATEWAY :: Socket disconnect failed\n");
		#endif

		if (!wifi_station_disconnect()) {
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf(
					"\n[+] DBG :: GATEWAY :: WiFi station mode disconnect failed\n"
				);
			#endif

			sys_state_transition_with_wifi_mode(
				SYS_STATE_SEND_DATA,
				SYS_STATE_ERROR,
				NULL_MODE
			);
		}
	}
}

void ICACHE_FLASH_ATTR
gateway_cb_dns_resolved(const char* hostname, ip_addr_t* ip_resolved, void* arg) {
	char ip_server[4];

	os_memcpy(
		(void*)&gateway_sock,
		arg,
		sizeof(struct espconn)
	);

	if (ip_resolved != NULL) {
		ip_server[0] = *((uint8*)&ip_resolved->addr);
		ip_server[1] = *((uint8*)&ip_resolved->addr + 1);
		ip_server[2] = *((uint8*)&ip_resolved->addr + 2);
		ip_server[3] = *((uint8*)&ip_resolved->addr + 3);

		#if (SYS_ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG :: GATEWAY :: DNS resolved, IP = %d.%d.%d.%d\n",
				ip_server[0],
				ip_server[1],
				ip_server[2],
				ip_server[3]
			);
		#endif

		// Configure the socket.
		gateway_sock.type = ESPCONN_TCP;
		gateway_sock.state = ESPCONN_NONE;
		gateway_sock.proto.tcp = &gateway_sock_tcp;
		gateway_sock.proto.tcp->remote_port = GATEWAY_PORT;
		gateway_sock.proto.tcp->local_port = espconn_port();

		os_memcpy(
			gateway_sock.proto.tcp->remote_ip,
			ip_server,
			4
		);

		// Register connect CB.
		espconn_regist_connectcb(
			&gateway_sock,
			gateway_cb_sock_connect
		);

		// Set TLS client mode and allocate memory.
		if (
			!(
				espconn_secure_set_size(
					0x01,
					(uint16_t)GATEWAY_TLS_BUFFER_SIZE
				)
			)
		)
		{
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: GATEWAY :: TLS socket initialisation failed\n");
			#endif

			sys_state_transition(SYS_STATE_SEND_DATA, SYS_STATE_ERROR);

			if (!wifi_station_disconnect()) {
				sys_state_transition_with_wifi_mode(
					SYS_STATE_SEND_DATA,
					SYS_STATE_ERROR,
					NULL_MODE
				);
			}
		}
		else {
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: GATEWAY :: TLS socket initialised\n");
			#endif

			// Connect the socket.
			if (espconn_secure_connect(&gateway_sock)) {
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG :: GATEWAY :: TLS Socket connect failed\n");
				#endif

				sys_state_transition(SYS_STATE_SEND_DATA, SYS_STATE_ERROR);

				if (!wifi_station_disconnect()) {
					sys_state_transition_with_wifi_mode(
						SYS_STATE_SEND_DATA,
						SYS_STATE_ERROR,
						NULL_MODE
					);
				}
			}
			else {
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG :: GATEWAY :: TLS socket connecting\n");
				#endif
			}
		}
	}
	else {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: GATEWAY :: DNS resolve failed\n");
		#endif

		sys_state_transition(SYS_STATE_SEND_DATA, SYS_STATE_ERROR);

		if (!wifi_station_disconnect()) {
			sys_state_transition_with_wifi_mode(
				SYS_STATE_SEND_DATA,
				SYS_STATE_ERROR,
				NULL_MODE
			);
		}
	}
}

void ICACHE_FLASH_ATTR
gateway_cb_timeout_rx(void) {
	// Reset the RX timer.
	os_timer_disarm(&sys_timer_sw);

	#if (SYS_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: GATEWAY :: RX timed out\n");
	#endif

	sys_state_transition(SYS_STATE_SEND_DATA, SYS_STATE_ERROR);

	if (espconn_secure_disconnect(&gateway_sock)) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: GATEWAY :: Socket disconnect failed\n");
		#endif

		if (!wifi_station_disconnect()) {
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf(
					"\n[+] DBG :: GATEWAY :: WiFi station mode disconnect failed\n"
				);
			#endif

			sys_state_transition_with_wifi_mode(
				SYS_STATE_SEND_DATA,
				SYS_STATE_ERROR,
				NULL_MODE
			);
		}
	}
}

void ICACHE_FLASH_ATTR
gateway_cb_sock_tx(void* arg) {
	#if (SYS_ENABLE_DEBUG == 1)
		os_printf(
			"\n[+] DBG :: GATEWAY :: TX OK, waiting for response\n"
		);
	#endif
}

void ICACHE_FLASH_ATTR
gateway_cb_sock_connect(void* arg) {
	char buffer_json_data[1024];
	char* buffer_message = (char*)malloc(GATEWAY_SIZE_SEND_BUFFER);

	#if (SYS_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: GATEWAY :: Socket connected\n");
	#endif

	os_memcpy(
		(void*)&gateway_sock,
		arg,
		sizeof(struct espconn)
	);

	os_bzero(buffer_message, GATEWAY_SIZE_SEND_BUFFER);

	// Register socket TX, RX and disconnect callbacks.
	espconn_regist_sentcb(&gateway_sock, gateway_cb_sock_tx);
	espconn_regist_recvcb(&gateway_sock, gateway_cb_sock_rx);
	espconn_regist_disconcb(&gateway_sock, gateway_cb_sock_disconnect);

	os_sprintf(
		buffer_json_data,
		GATEWAY_FMT_NOTIFICATION_HTTP_DATA_JSON,
		config_current->notification_email,
		config_current->notification_email_subject,
		config_current->plant_name,
		config_current->notification_email_message
	);

	os_sprintf(
		buffer_message,
		GATEWAY_FMT_NOTIFICATION_HTTP_HEADER,
		GATEWAY_HOST,
		os_strlen(buffer_json_data),
		GATEWAY_KEY,
		buffer_json_data
	);

	#if (SYS_ENABLE_DEBUG == 1)
		os_printf(
			"\n[+] DBG :: GATEWAY :: Sending notification, JSON data = %s\n",
			buffer_json_data
		);
	#endif

	// Send request.
	if (
		espconn_secure_send(
			&gateway_sock,
			buffer_message,
			os_strlen(buffer_message)
		)
	)
	{
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG :: GATEWAY :: Sending data using socket failed\n"
			);
		#endif

		sys_state_transition(SYS_STATE_SEND_DATA, SYS_STATE_ERROR);

		if (espconn_secure_disconnect(&gateway_sock)) {
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: GATEWAY :: Socket disconnect failed\n");
			#endif

			if (!wifi_station_disconnect()) {
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf(
						"\n[+] DBG :: GATEWAY :: WiFi station mode disconnect failed\n"
					);
				#endif

				sys_state_transition_with_wifi_mode(
					SYS_STATE_SEND_DATA,
					SYS_STATE_ERROR,
					NULL_MODE
				);
			}
		}
	}
	else {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: GATEWAY :: Sent data using socket\n");
		#endif

		// Start the timer to handle RX timeout.
		os_timer_setfn(
			&sys_timer_sw,
			(os_timer_func_t*)gateway_cb_timeout_rx,
			NULL
		);

		// Wait about 30 seconds for a response.
		os_timer_arm(&sys_timer_sw, 30000, false);
	}

	os_free(buffer_message);
}

void ICACHE_FLASH_ATTR
gateway_cb_sock_disconnect(void* arg) {
	#if (SYS_ENABLE_DEBUG == 1)
		os_printf("\n[+] DBG :: GATEWAY :: Socket disconnected\n");
	#endif

	if (!wifi_station_disconnect()) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG :: GATEWAY :: WiFi station mode disconnect failed\n"
			);
		#endif

		sys_state_transition_with_wifi_mode(
			SYS_STATE_SEND_DATA,
			SYS_STATE_ERROR,
			NULL_MODE
		);
	}
}

// Configuration.

bool ICACHE_FLASH_ATTR
config_read(void) {
	#if (SYS_ENABLE_DEBUG == 1)
		os_printf(
			"\n[+] DBG :: CONFIG :: Reading configuration from flash\n"
		);
	#endif

	os_bzero(config_current, sizeof(config_current));

	if (
		(
			spi_flash_read(
				(config_flash_sector * 4096),
				(uint32_t*)config_current,
				sizeof(config_t)
			)
		) != SPI_FLASH_RESULT_OK
	)
	{
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG :: CONFIG :: Reading configuration failed\n"
			);
		#endif

		return false;
	}

	return true;
}

void ICACHE_FLASH_ATTR
config_get_default_plant_name(char* default_plant_name, uint8_t len) {
	os_bzero(default_plant_name, len);

	os_sprintf(
		default_plant_name,
		CONFIG_FMT_DEFAULT_PLANT_NAME,
		system_get_chip_id()
	);
}

void ICACHE_FLASH_ATTR
config_load_default_config(void) {
	#if (SYS_ENABLE_DEBUG == 1)
		os_printf(
			"\n[+] DBG :: CONFIG :: Loading default configuration\n"
		);
	#endif

	// Clear current configuration.
	os_bzero((void*)config_current, sizeof(config_t));

	// Set the deflault plant name.
	config_get_default_plant_name(
		config_current->plant_name,
		sizeof(config_current->plant_name)
	);

	// Set default configuration password.
	os_memcpy(
		(void*)&config_current->configuration_password,
		"1234567890",
		os_strlen("1234567890")
	);

	config_current->threshold_percent = 8;
	config_current->threshold_lt_gt = CONFIG_THRESHLOD_LT;

	// Calculate configuration hash.
	config_current->hash = \
		config_get_hash_pearson((uint8_t*)&config_current);
}

uint8_t ICACHE_FLASH_ATTR
config_get_hash_pearson(uint8_t* bytes) {
	uint32_t i = 0;
	uint8_t hash = 0;

	/*
	 * Start from index 1 as we want to skip the
	 * first byte that is used to store the checksum.
	 */
	for (i = 1; i < sizeof(config_t); i++) {
		hash = config_permutation_pearson[hash ^ bytes[i]];
	}

	return hash;
}

// Peripherals.

void ICACHE_FLASH_ATTR
periph_sesnor_toggle(bool toggle) {
	#if (SYS_ENABLE_DEBUG == 1)
		os_printf(
			"\n[+] DBG :: PERIPH :: Toggle Sensor, toggle = %d\n",
			toggle
		);
	#endif

	PIN_FUNC_SELECT(
		PERIPH_GPIO_O_SENSOR,
		PERIPH_GPIO_O_FUNC_SENSOR
	);

	if (toggle) {
		gpio_output_set(
			PERIPH_GPIO_O_BIT_SENSOR,
			0,
			PERIPH_GPIO_O_BIT_SENSOR,
			0
		);
	}
	else {
		gpio_output_set(
			0,
			PERIPH_GPIO_O_BIT_SENSOR,
			PERIPH_GPIO_O_BIT_SENSOR,
			0
		);
	}
}

void ICACHE_FLASH_ATTR
periph_led_blue_toggle(bool toggle) {
	if (toggle) {
		PIN_FUNC_SELECT(
			PERIPH_GPIO_O_LED_BLUE,
			PERIPH_GPIO_O_FUNC_LED_BLUE
		);
		gpio_output_set(
			0,
			PERIPH_GPIO_O_BIT_LED_BLUE,
			PERIPH_GPIO_O_BIT_LED_BLUE,
			0
		);
	}
	else {
		PIN_FUNC_SELECT(
			PERIPH_GPIO_O_LED_BLUE,
			PERIPH_GPIO_O_FUNC_LED_BLUE
		);
		gpio_output_set(
			PERIPH_GPIO_O_BIT_LED_BLUE,
			0,
			PERIPH_GPIO_O_BIT_LED_BLUE,
			0
		);
	}
}

uint32_t ICACHE_FLASH_ATTR
periph_read_adc(uint32_t adc_sample_size) {
	uint32_t i = 0;
	uint32_t sum_adc_sample = 0;

	periph_sesnor_toggle(true);

	// Disable interrupts.
	ets_intr_lock();

	for (i = 0; i < adc_sample_size; i++) {
		sum_adc_sample += system_adc_read();
	}

	// Enable interrupts.
	ets_intr_unlock();

	periph_sesnor_toggle(false);

	return (sum_adc_sample / adc_sample_size);
}

// System.

static bool ICACHE_FLASH_ATTR
thirst_init(void) {
	config_flash_sector = 0;
	sys_state_error_led = true;
	partition_item_t partition_item;

	gpio_init();

	periph_sesnor_toggle(false);
	periph_led_blue_toggle(false);

	if (
		!system_partition_get_item(
			SYSTEM_PARTITION_CUSTOMER_BEGIN,
			&partition_item
		)
	)
	{
		os_printf(
			"\n[+] DBG :: SYSTEM :: Get partition information failed\n"
		);

		return false;
	}

	config_flash_sector = partition_item.addr / 4096;

	// Initialise web interface.
	if (!web_interface_init()) {
		return false;
	}

	return true;
}

static bool ICACHE_FLASH_ATTR
thirst_init_main_task(void) {
	sys_task_queue_main = \
		(os_event_t*)os_malloc(
			sizeof(os_event_t) * SYS_TASK_QUEUE_MAIN_LEN
		);

	if (sys_task_queue_main == NULL) {
		return false;
	}

	return \
		system_os_task(
			sys_task_handler_main,
			USER_TASK_PRIO_0,
			sys_task_queue_main,
			SYS_TASK_QUEUE_MAIN_LEN
		);
}

void ICACHE_FLASH_ATTR
sys_cb_init_done(void) {
	#if (SYS_ENABLE_DEBUG == 1)
		// Setup the debug UART.
		uart_div_modify(UART0, PERIPH_UART_BIT_RATE);
		system_set_os_print(1);

		os_printf(
			"\n[+] DBG :: SYSTEM :: SDK version = %s\n",
			system_get_sdk_version()
		);
	#endif

	// Register callback for WiFi events.
	wifi_set_event_handler_cb(sys_cb_wifi_event);

	// Initialise the main task.
	if (!thirst_init_main_task()) {
		os_printf("\n[+] DBG :: SYSTEM :: Main task init failed\n");

		sys_state_transition_with_wifi_mode(
			SYS_STATE_INIT,
			SYS_STATE_ERROR_INIT_MAIN_TASK,
			NULL_MODE
		);

		return;
	}

	// Initialise the firmware.
	if (!thirst_init()) {
		os_printf("\n[+] DBG :: SYSTEM :: Firmware init failed\n");

		sys_state_transition_with_wifi_mode(
			SYS_STATE_INIT,
			SYS_STATE_ERROR,
			NULL_MODE
		);

		return;
	}

	os_printf("\n[+] DBG :: SYSTEM :: System init OK\n");

	sys_state_transition_with_wifi_mode(
		SYS_STATE_INIT,
		SYS_STATE_INIT_OK,
		NULL_MODE
	);
}

void ICACHE_FLASH_ATTR
sys_deep_sleep_cycle(void) {
	struct rst_info* reset_info = system_get_rst_info();

	os_bzero((void*)&sys_rtc_data, sizeof(sys_rtc_data));

	if (
		!(
			system_rtc_mem_read(
				PERIPH_MEM_ADDR_RTC,
				&sys_rtc_data.sleep_count,
				sizeof(sys_rtc_data.sleep_count)
			)
		)
	)
	{
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: SYSTEM :: RTC memory read failed\n");
		#endif

		sys_state_transition_with_task(
			SYS_STATE_DEEP_SLEEP_CYCLE,
			SYS_STATE_ERROR,
			SYS_TASK_SIGNAL_STATE_ERROR,
			(ETSParam)0
		);

		return;
	}

	if (
		(reset_info->reason == REASON_DEFAULT_RST)\
		||\
		(sys_rtc_data.sleep_count >= SYS_DEEP_SLEEP_TIMES)
	)
	{
		os_bzero((void*)&sys_rtc_data, sizeof(sys_rtc_data));
	}

	if ((sys_rtc_data.sleep_count + 1) < SYS_DEEP_SLEEP_TIMES) {
		sys_rtc_data.sleep_count++;

		if (
			system_rtc_mem_write(
				PERIPH_MEM_ADDR_RTC,
				&sys_rtc_data,
				sizeof(sys_rtc_data)
			)
		)
		{
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf(
					"\n[+] DBG :: SYSTEM :: RTC count up and write, Sleep count = %d\n",
					sys_rtc_data.sleep_count
				);
			#endif

			os_bzero(
				(void*)&sys_deep_sleep_params,
				sizeof(SYS_DEEP_SLEEP_PARAMS_t)
			);

			if (sys_rtc_data.sleep_count >= (SYS_DEEP_SLEEP_TIMES - 1)) {
				sys_deep_sleep_params.sleep_type = SYS_DEEP_SLEEP_OPTION_SAME_AS_PWRUP;
			}
			else {
				sys_deep_sleep_params.sleep_type = SYS_DEEP_SLEEP_OPTION_NO_RADIO;
			}

			sys_deep_sleep_params.sleep_duration = SYS_DEEP_SLEEP_DURATION_US;

			sys_state_transition_with_task(
				SYS_STATE_DEEP_SLEEP_CYCLE,
				SYS_STATE_DEEP_SLEEP,
				SYS_TASK_SIGNAL_DEEP_SLEEP,
				(ETSParam)0
			);
		}
		else {
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: SYSTEM :: RTC memory write failed\n");
			#endif

			sys_state_transition_with_task(
				SYS_STATE_DEEP_SLEEP_CYCLE,
				SYS_STATE_ERROR,
				SYS_TASK_SIGNAL_STATE_ERROR,
				(ETSParam)0
			);
		}
	}
	else {
		sys_rtc_data.sleep_count++;

		// Write the reset condition to RTC memory already.
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
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: SYSTEM :: RTC memory reset condition write failed\n");
			#endif

			sys_state_transition_with_task(
				SYS_STATE_DEEP_SLEEP_CYCLE,
				SYS_STATE_ERROR,
				SYS_TASK_SIGNAL_STATE_ERROR,
				(ETSParam)0
			);
		}
		else {
			sys_state_transition_with_wifi_mode(
				SYS_STATE_DEEP_SLEEP_CYCLE,
				SYS_STATE_PRE_SEND_DATA,
				STATION_MODE
			);
		}
	}
}

void ICACHE_FLASH_ATTR
sys_state_toggle_led_error(void) {
	periph_led_blue_toggle(sys_state_error_led);

	if (sys_state_error_led) {
		sys_state_error_led = false;
	}
	else {
		sys_state_error_led = true;
	}
}

void ICACHE_FLASH_ATTR
sys_state_transition_with_task(
	int state_current,
	int state_next,
	int sys_task_signal,
	ETSParam sys_task_param
)
{
	#if (SYS_ENABLE_DEBUG == 1)
		os_printf(
			"\n[+] DBG :: SYSTEM :: State transition with task, State = %d -> %d, Task = %d\n",
			state_current,
			state_next,
			sys_task_signal
		);
	#endif

	sys_state.state_next = state_next;
	sys_state.state_current = state_current;

	system_os_post(
		USER_TASK_PRIO_0,
		sys_task_signal,
		sys_task_param
	);
}

void ICACHE_FLASH_ATTR
sys_state_transition_with_wifi_mode(
	int state_current,
	int state_next,
	int mode
)
{
	#if (SYS_ENABLE_DEBUG == 1)
		os_printf(
			"\n[+] DBG :: SYSTEM :: State transition with WiFi mode, State = %d -> %d, Mode = %d\n",
			state_current,
			state_next,
			mode
		);
	#endif

	sys_state.state_next = state_next;
	sys_state.state_current = state_current;

	if (mode > -1) {
		wifi_set_opmode_current(mode);
	}
}

void ICACHE_FLASH_ATTR
sys_cb_wifi_event(System_Event_t* evt) {
	err_t err;
	uint32_t adc = 0;
	struct ip_info ip;
	uint8_t config_hash = 0;
	uint32_t percentage = 0;
	bool config_none = false;
	bool send_notification = false;
	uint32_t gpio_i_config_mode_value = 1;
	struct softap_config config_ap_interface;
	struct station_config config_station_interface;

	// Events: Station mode.
	if (evt->event == EVENT_STAMODE_CONNECTED) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: SYSTEM :: WiFi station mode connected\n");
		#endif
	}
	else if (evt->event == EVENT_STAMODE_DISCONNECTED) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: SYSTEM :: WiFi station mode disconnected\n");
		#endif

		/*
		 * Error occured in one of the following states:
		 *
		 * 1. Before getting IP in send data state.
		 * 2. Send data state.
		 * 3. Send data done state.
		 */
		if (sys_state.state_next == SYS_STATE_PRE_SEND_DATA) {
			sys_state_transition(
				SYS_STATE_PRE_SEND_DATA,
				SYS_STATE_ERROR
			);
		}

		wifi_set_opmode_current(NULL_MODE);
	}
	else if (evt->event == EVENT_STAMODE_AUTHMODE_CHANGE) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG :: SYSTEM :: WiFi station mode auth mode changed = %d -> %d\n",
				evt->event_info.auth_change.old_mode,
				evt->event_info.auth_change.new_mode
			);
		#endif
	}
	else if (evt->event == EVENT_STAMODE_GOT_IP) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: SYSTEM :: WiFi station mode got IP\n");
		#endif

		sys_state_transition(SYS_STATE_PRE_SEND_DATA, SYS_STATE_SEND_DATA);

		/*
		 * It is required to read the ADC sensor in this state to get a
		 * consistent sensor reading.
		 *
		 * The configured value from the web interface is acquired when
		 * the WiFi radio is enabled. So this reading must also be acquired
		 * with WiFi enabled to get a consistent sensor reading.
		 */
		adc = periph_read_adc((uint32_t)PERIPH_ADC_SAMPLE_SIZE);
		percentage = \
			(config_current->registered_value * config_current->threshold_percent) / 100;

		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: SYSTEM :: ADC reading = %d\n", adc);
		#endif

		if (config_current->threshold_lt_gt == (uint8_t)CONFIG_THRESHLOD_LT) {
			if (adc <= (config_current->registered_value - percentage)) {
				send_notification = true;
			}
		}
		else if (config_current->threshold_lt_gt == (uint8_t)CONFIG_THRESHLOD_GT) {
			if (adc >= (config_current->registered_value + percentage)) {
				send_notification = true;
			}
		}

		if (send_notification) {
			os_printf("\n[+] DBG :: SYSTEM :: Sending notification e-mail\n");

			err = \
				espconn_gethostbyname(
					&gateway_sock,
					GATEWAY_HOST,
					&gateway_ip_dns_resolved,
					gateway_cb_dns_resolved
				);

			switch(err) {
				case ESPCONN_OK:
					#if (SYS_ENABLE_DEBUG == 1)
						os_printf("\n[+] DBG :: SYSTEM :: DNS lookup started\n");
					#endif

					break;

				case ESPCONN_INPROGRESS:
					#if (SYS_ENABLE_DEBUG == 1)
						os_printf("\n[+] DBG :: SYSTEM :: DNS lookup in progress\n");
					#endif

					break;

				default:
					#if (SYS_ENABLE_DEBUG == 1)
						os_printf("\n[+] DBG :: SYSTEM :: DNS lookup error\n");
					#endif

					sys_state_transition(SYS_STATE_SEND_DATA, SYS_STATE_ERROR);

					if (!wifi_station_disconnect()) {
						sys_state_transition_with_wifi_mode(
							SYS_STATE_SEND_DATA,
							SYS_STATE_ERROR,
							NULL_MODE
						);
					}

					break;
			}
		}
		else {
			// Setup deep sleep config.
			if (SYS_DEEP_SLEEP_TIMES == 1) {
				sys_deep_sleep_params.sleep_type = SYS_DEEP_SLEEP_OPTION_SAME_AS_PWRUP;
			}
			else {
				sys_deep_sleep_params.sleep_type = SYS_DEEP_SLEEP_OPTION_NO_RADIO;
			}

			sys_deep_sleep_params.sleep_duration = SYS_DEEP_SLEEP_DURATION_US;

			sys_state_transition(SYS_STATE_SEND_DATA, SYS_STATE_DEEP_SLEEP);

			if (!wifi_station_disconnect()) {
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf(
						"\n[+] DBG :: SYSTEM :: WiFi station mode disconnect failed\n"
					);
				#endif

				sys_state_transition_with_wifi_mode(
					SYS_STATE_SEND_DATA,
					SYS_STATE_ERROR,
					NULL_MODE
				);
			}
		}
	}
	else if (evt->event == EVENT_STAMODE_DHCP_TIMEOUT) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: SYSTEM :: WiFi station mode DHCP timeout\n");
		#endif

		sys_state_transition(SYS_STATE_SEND_DATA, SYS_STATE_ERROR);

		if (!wifi_station_disconnect()) {
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: SYSTEM :: WiFi station mode disconnect failed\n");
			#endif

			sys_state_transition_with_wifi_mode(
				SYS_STATE_SEND_DATA,
				SYS_STATE_ERROR,
				NULL_MODE
			);
		}
	}
	// Events: SoftAP mode.
	else if (evt->event == EVENT_SOFTAPMODE_STACONNECTED) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG :: SYSTEM :: WiFi SoftAP station connected\n"
			);
		#endif
	}
	else if (evt->event == EVENT_SOFTAPMODE_STADISCONNECTED) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG :: SYSTEM :: WiFi SoftAP station disconnected\n"
			);
		#endif
	}
	else if (evt->event == EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf(
				"\n[+] DBG :: SYSTEM :: WiFi SoftAP leased DHCP IP\n"
			);
		#endif
	}
	else if (evt->event == EVENT_SOFTAPMODE_PROBEREQRECVED) {
		// Do not report this event on debug console as it crowds up the console.
	}
	// Operating mode changed event.
	else if (evt->event == EVENT_OPMODE_CHANGED) {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: SYSTEM :: WiFi operating mode changed\n");
		#endif

		// Operating mode switched: Null mode.
		if (wifi_get_opmode() == NULL_MODE) {
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: SYSTEM :: WiFi NULL mode\n");
			#endif

			if (\
				(sys_state.state_current == SYS_STATE_INIT) \
				&& \
				(sys_state.state_next == SYS_STATE_ERROR_INIT_MAIN_TASK)\
			)\
			{
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG :: SYSTEM :: Failed to initialise the main task\n");
				#endif

				/*
				 * Initialisation of the main task failed.
				 *
				 * In this state simply the error state LED
				 * blinking is started since the task can't be
				 * posted.
				 */
				os_timer_disarm(&sys_timer_sw);

				os_timer_setfn(
					&sys_timer_sw,
					(os_timer_func_t*)sys_state_toggle_led_error,
					NULL
				);

				os_timer_arm(&sys_timer_sw, 1000, true);
			}
			else if (\
				(sys_state.state_current == SYS_STATE_INIT) \
				&& \
				(sys_state.state_next == SYS_STATE_ERROR)\
			)\
			{
				system_os_post(
					USER_TASK_PRIO_0,
					SYS_TASK_SIGNAL_STATE_ERROR,
					(ETSParam)0
				);
			}
			else if (\
				(sys_state.state_current == SYS_STATE_INIT) \
				&& \
				(sys_state.state_next == SYS_STATE_INIT_OK)\
			)\
			{
				/*
				 * Allocating memory for configuration and web
				 * interface form post.
				 *
				 * Must not be de-allocated.
				 */
				config_current = (config_t*)os_malloc(sizeof(config_t));
				web_interface_buffer_post_form = \
					(char*)os_malloc(WEB_INTERFACE_FORM_POST_BUFFER_SIZE); // Used by the web interface.

				if ((config_current == NULL) || (web_interface_buffer_post_form == NULL)) {
					#if (SYS_ENABLE_DEBUG == 1)
						os_printf(
							"\n[+] DBG :: SYSTEM :: Memory allocation failed for configuration and web interface form POST buffer\n"
						);
					#endif

					sys_state_transition_with_task(
						SYS_STATE_INIT_OK,
						SYS_STATE_ERROR,
						SYS_TASK_SIGNAL_STATE_ERROR,
						(ETSParam)0
					);
				}

				// Read configuration from the flash memory.
				if (!config_read()) {
					sys_state_transition_with_task(
						SYS_STATE_INIT_OK,
						SYS_STATE_ERROR,
						SYS_TASK_SIGNAL_STATE_ERROR,
						(ETSParam)0
					);
				}
				else {
					#if (SYS_ENABLE_DEBUG == 1)
						os_printf(
							"\n[+] DBG :: SYSTEM :: Configuration read OK\n"
						);

						os_printf(
							"\n[+] DBG :: SYSTEM :: Reading configuration mode pin state change\n"
						);
					#endif

					// Configure configuration mode pin as GPIO input.
					PIN_FUNC_SELECT(
						PERIPH_GPIO_I_CONFIG_MODE,
						PERIPH_GPIO_I_FUNC_CONFIG_MODE
					);

					gpio_output_set(0, 0, 0, PERIPH_GPIO_I_BIT_CONFIG_MODE);

					PIN_PULLUP_EN(PERIPH_GPIO_I_CONFIG_MODE);

					// Wait for 4 seconds.
					system_soft_wdt_stop(); // Stop the WDT.
					os_delay_us(4000000);
					system_soft_wdt_restart(); // Start the WDT.

					/*
					 * Check whether to switch to configuration mode or not.
					 *
					 * 0 = Grounded (configuration mode button is pressed).
					 * 1 = Not pressed.
					 */
					gpio_i_config_mode_value = GPIO_INPUT_GET(0);

					/*
					 * Generate pearson hash for the configuration
					 * that is read from the flash.
					 */
					config_hash = \
						config_get_hash_pearson((uint8_t*)config_current);

					#if (SYS_ENABLE_DEBUG == 1)
						os_printf(
							"\n[+] DBG :: SYSTEM :: Pearson hash of the read configuration = 0x%X\n",
							config_hash
						);
					#endif

					if (config_hash != config_current->hash) {
						#if (SYS_ENABLE_DEBUG == 1)
							os_printf(
								"\n[+] DBG :: SYSTEM :: Configuration hash mismatch, calcualted = 0x%X, expected = 0x%X\n",
								config_hash,
								config_current->hash
							);
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
						config_load_default_config();
					}

					if (config_none || (gpio_i_config_mode_value == 0)) {
						sys_state_transition_with_task(
							SYS_STATE_INIT_OK,
							SYS_STATE_CONFIG,
							SYS_TASK_SIGNAL_CONFIG_MODE,
							(ETSParam)0
						);
					}
					else {
						sys_state_transition_with_task(
							SYS_STATE_INIT_OK,
							SYS_STATE_DEEP_SLEEP_CYCLE,
							SYS_TASK_SIGNAL_DEEP_SLEEP_CYCLE,
							(ETSParam)0
						);
					}
				}
			}
			else if (\
				(sys_state.state_current == SYS_STATE_CONFIG) \
				&& \
				(sys_state.state_next == SYS_STATE_RESET_AFTER_CONFIG_SAVE)\
			)\
			{
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf(
						"\n[+] DBG :: SYSTEM :: System reset after config save\n"
					);
				#endif

				system_os_post(
					USER_TASK_PRIO_0,
					SYS_TASK_SIGNAL_RESET_AFTER_CONFIG_SAVE,
					(ETSParam)0
				);
			}
			else if (\
				(\
					(sys_state.state_current == SYS_STATE_SEND_DATA) && \
					(sys_state.state_next == SYS_STATE_DEEP_SLEEP)\
				)
			)
			{
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG :: SYSTEM :: Going to deep sleep mode\n");
				#endif

				system_os_post(
					USER_TASK_PRIO_0,
					SYS_TASK_SIGNAL_DEEP_SLEEP,
					(ETSParam)0
				);
			}
			else if (\
				(\
					(sys_state.state_current == SYS_STATE_SEND_DATA) && \
					(sys_state.state_next == SYS_STATE_SEND_DATA_DONE)\
				)
			)
			{
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf(
						"\n[+] DBG :: SYSTEM :: Send data done, going to deep sleep mode\n"
					);
				#endif

				system_os_post(
					USER_TASK_PRIO_0,
					SYS_TASK_SIGNAL_SEND_DATA_DONE,
					(ETSParam)0
				);
			}
			else if (sys_state.state_next == SYS_STATE_ERROR) {
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf(
						"\n[+] DBG :: SYSTEM :: Error state in WiFi null mode\n"
					);
				#endif

				system_os_post(
					USER_TASK_PRIO_0,
					SYS_TASK_SIGNAL_STATE_ERROR,
					(ETSParam)0
				);
			}
		}
		// Operating mode switched: Station mode.
		else if (wifi_get_opmode() == STATION_MODE) {
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: SYSTEM :: WiFi station mode\n");
			#endif

			// Clear out WiFi station interface configuration storage.
			os_bzero(
				&config_station_interface,
				sizeof(config_station_interface)
			);

			// WiFi station config.
			config_station_interface.bssid_set = 0;

			os_memcpy(
				&config_station_interface.ssid,
				config_current->wifi_ap_ssid,
				os_strlen(config_current->wifi_ap_ssid)
			);

			os_memcpy(
				&config_station_interface.password,
				config_current->wifi_ap_password,
				os_strlen(config_current->wifi_ap_password)
			);

			// Apply WiFi config.
			wifi_station_set_config_current(&config_station_interface);

			// Enable station mode reconnect.
			wifi_station_set_reconnect_policy(true);

			if (
				!(wifi_station_set_hostname(config_current->plant_name))\
				||\
				!(wifi_station_connect())
			)
			{
				#if (SYS_ENABLE_DEBUG == 1)
					os_printf("\n[+] DBG :: SYSTEM :: WiFi station mode connect failed\n");
				#endif

				sys_state_transition_with_wifi_mode(
					SYS_STATE_PRE_SEND_DATA,
					SYS_STATE_ERROR,
					NULL_MODE
				);
			}
		}
		/*
		 * Operating mode switched: Station + AP mode.
		 *
		 * Only SoftAP mode is not enough because we must be able to scan
		 * to report available WiFi APs around for which station mode is
		 * required.
		 */
		else if (wifi_get_opmode() == STATIONAP_MODE) {
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: SYSTEM :: WiFi Station+AP mode\n");
			#endif

			os_bzero(&ip, sizeof(ip));
			os_bzero(&config_ap_interface, sizeof(config_ap_interface));

			wifi_softap_dhcps_stop();

			// Set AP configuration parameters.
			IP4_ADDR(&ip.ip, 192, 168, 42, 1);
			IP4_ADDR(&ip.gw, 192, 168, 42, 1);
			IP4_ADDR(&ip.netmask, 255, 255, 255, 0);

			wifi_set_ip_info(SOFTAP_IF, &ip);

			config_ap_interface.channel = 7;
			config_ap_interface.max_connection = 1;
			config_ap_interface.beacon_interval = 50;
			config_ap_interface.authmode = AUTH_WPA_WPA2_PSK;

			os_memcpy(
				config_ap_interface.ssid,
				config_current->plant_name,
				os_strlen(config_current->plant_name)
			);

			os_memcpy(
				config_ap_interface.password,
				config_current->configuration_password,
				os_strlen(config_current->configuration_password)
			);

			wifi_softap_dhcps_start();

			// Apply AP configuration.
			wifi_softap_set_config_current(&config_ap_interface);

			// Start the web server.
			httpdInit(web_interface_httpd_urls, 80);
		}
	}
	else {
		#if (SYS_ENABLE_DEBUG == 1)
			os_printf("\n[+] DBG :: SYSTEM :: WiFi unknown event\n");
		#endif
	}
}

void ICACHE_FLASH_ATTR
sys_task_handler_main(os_event_t* task_event) {
	switch (task_event->sig) {
		case SYS_TASK_SIGNAL_CONFIG_MODE:
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf(
					"\n[+] DBG :: SYSTEM :: Main task handler, Task = SYS_TASK_SIGNAL_CONFIG_MODE\n"
				);
			#endif

			periph_led_blue_toggle(true);

			/*
			 * Enable STATION + AP mode.
			 *
			 * We need the station mode to scan
			 * for available WiFi networks around.
			 */
			wifi_set_opmode_current(STATIONAP_MODE);

			break;

		case SYS_TASK_SIGNAL_DEEP_SLEEP_CYCLE:
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf(
					"\n[+] DBG :: SYSTEM :: Main task handler, Task = SYS_TASK_SIGNAL_DEEP_SLEEP_CYCLE\n"
				);
			#endif

			sys_deep_sleep_cycle();

			break;

		case SYS_TASK_SIGNAL_STATE_ERROR:
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf(
					"\n[+] DBG :: SYSTEM :: Main task handler, Task = SYS_TASK_SIGNAL_STATE_ERROR\n"
				);
			#endif

			os_timer_disarm(&sys_timer_sw);

			os_timer_setfn(
				&sys_timer_sw,
				(os_timer_func_t*)sys_state_toggle_led_error,
				NULL
			);

			os_timer_arm(&sys_timer_sw, 1000, true);

			break;

		case SYS_TASK_SIGNAL_SEND_DATA:
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf(
					"\n[+] DBG :: SYSTEM :: Main task handler, Task = SYS_TASK_SIGNAL_SEND_DATA\n"
				);
			#endif

			wifi_set_opmode_current(STATION_MODE);

			break;

		case SYS_TASK_SIGNAL_DEEP_SLEEP:
		case SYS_TASK_SIGNAL_SEND_DATA_DONE:
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf(
					"\n[+] DBG :: SYSTEM :: Main task handler, Task = SYS_TASK_SIGNAL_DEEP_SLEEP\n"
				);
			#endif

			system_deep_sleep_set_option(sys_deep_sleep_params.sleep_type);
			system_deep_sleep(sys_deep_sleep_params.sleep_duration);

			break;

		case SYS_TASK_SIGNAL_RESET_AFTER_CONFIG_SAVE:
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf(
					"\n[+] DBG :: SYSTEM :: Main task handler, Task = SYS_TASK_SIGNAL_RESET_AFTER_CONFIG_SAVE\n"
				);
			#endif

			system_restart();

			break;

		default:
			#if (SYS_ENABLE_DEBUG == 1)
				os_printf("\n[+] DBG :: SYSTEM :: Main task handler, Task = UNKNOWN\n");
			#endif
	}
}

void ICACHE_FLASH_ATTR
sys_state_transition(int state_current, int state_next) {
	#if (SYS_ENABLE_DEBUG == 1)
		os_printf(
			"\n[+] DBG :: SYSTEM :: State transition, State = %d -> %d\n",
			state_current,
			state_next
		);
	#endif

	sys_state.state_next = state_next;
	sys_state.state_current = state_current;
}

void ICACHE_FLASH_ATTR
user_init(void) {
	// Register system initialisation done callback.
	system_init_done_cb(sys_cb_init_done);
}

void ICACHE_FLASH_ATTR
user_pre_init(void) {
	if (
		!(
			system_partition_table_regist(
				sys_partition_table,
				sizeof(sys_partition_table) / sizeof(sys_partition_table[0]),
				SPI_FLASH_SIZE_MAP
			)
		)
	)
	{
		while (1);
	}
}
