#include "osapi.h"
#include "user_interface.h"
#include "espconn.h"
#include "gpio.h"

#define ENABLE_DEBUG 0
#define MEM_ADDR_RTC 0x40
#define ADC_SAMPLE_SIZE 4 // Sampling ADC takes time increasing this causes WDT to reset.
//#define DEEP_SLEEP_1_SEC 1000000
#define HALF_HOURS_IN_A_DAY 48
#define DEEP_SLEEP_HALF_HOUR 1800000000
#define UART_BIT_RATE UART_CLK_FREQ/BIT_RATE_115200
#define GPIO_O_BIT_LED_BLUE BIT2
#define GPIO_O_FUNC_LED_BLUE FUNC_GPIO2
#define GPIO_O_LED_BLUE PERIPHS_IO_MUX_GPIO2_U
#define GPIO_I_BIT_CONFIG_MODE BIT0
#define GPIO_I_FUNC_CONFIG_MODE FUNC_GPIO0
#define GPIO_I_CONFIG_MODE PERIPHS_IO_MUX_GPIO0_U
#define GPIO_O_BIT_VCC_SENSOR BIT15
#define GPIO_O_FUNC_VCC_SENSOR FUNC_GPIO15
#define GPIO_O_VCC_SENSOR PERIPHS_IO_MUX_MTDO_U
#define NOTIFIER_PORT 4200
#define NOTIFIER_SIZE_SEND_BUFFER 4096
#define NOTIFIER_HOST "vultr-debian9.duckdns.org"
#define FMT_NOTIFIER_HTTP_HEADER "POST /notifier/email/send HTTP/1.1\r\n\
Host: vultr-debian9.duckdns.org\r\n\
Accept: application/json; charset=utf-8\r\n\
Content-Type: application/json; charset=utf-8\r\n\
Content-Length: %d\r\n\
Cache-Control: no-cache\r\n\
\r\n\
%s"
#define FMT_NOTIFIER_DATA_HTTP_JSON "\
{\
\"from\": \"thirst.the.project@gmail.com\",\
\"to\": \"%s\",\
\"subject\": \"%s\",\
\"message\": \"%s\"\
}"

#define CONFIG_SECTOR_FLASH 0x3FA // 5th sector from the last sector of 4MB flash.
#define CONFIG_SSID_LEN 32
#define CONFIG_THRESHLOD_LT 1
#define CONFIG_THRESHLOD_GT 2
#define CONFIG_SSID_PASSWORD_LEN 64
#define CONFIG_NOTIFICATION_SUBJECT_LEN 64
#define CONFIG_NOTIFICATION_MESSAGE_LEN 2048
#define CONFIG_NOTIFICATION_EMAIL_LEN 254
#define FMT_CONFIG_DEFAULT_PLANT_NAME "Thirst-%X%X%X"
#define PERMUTATION_PEARSON_SIZE 256

#define DEEP_SLEEP_OPTION_108 0
#define DEEP_SLEEP_OPTION_SAME_AS_PWRUP 1
#define DEEP_SLEEP_OPTION_NO_RF_CAL 2
#define DEEP_SLEEP_OPTION_NO_RADIO 4

typedef struct {
  uint8_t config_hash_pearson;

	// Plant configuration.
	char the_plant_name[CONFIG_SSID_LEN];
	char the_plant_configuration_password[CONFIG_SSID_PASSWORD_LEN];
	char the_plant_wifi_ap[CONFIG_SSID_LEN];
  char the_plant_wifi_ap_password[CONFIG_SSID_PASSWORD_LEN];
  uint8_t the_plant_threshold_percent;
  uint8_t the_plant_threshold_lt_gt;
  uint32_t registered_value;
  uint32_t the_plant_check_frequency;

	// Notification configuration.
	char notification_email[CONFIG_NOTIFICATION_EMAIL_LEN];
	char notification_email_subject[CONFIG_NOTIFICATION_SUBJECT_LEN];
	char notification_email_message[CONFIG_NOTIFICATION_MESSAGE_LEN];
} config_t;

// Pearson hash.
static const uint8_t permutation_pearson[PERMUTATION_PEARSON_SIZE] = {
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

esp_tcp sock_tcp;
struct espconn sock;

/*
 * Flash R/W works from RAM. So instead of normal global variable for current
 * configuration a malloced pointer is used so the current configuration can be
 * flashed without extra operations.
 */
config_t *config_current;

// Used by web interface module to process partial arrival of POST data.
char *buffer_post_form;

ip_addr_t ip_dns_resolved;
bool state_error_led_state;
os_timer_t timer_generic_software;
extern const unsigned long webpages_espfs_start;

void
cb_timer_disconnect_sock(void);

void ICACHE_FLASH_ATTR
cb_sock_recv(void *arg, char *data, unsigned short length);

void ICACHE_FLASH_ATTR
cb_sock_disconnect(void *arg);

void ICACHE_FLASH_ATTR
cb_sock_sent(void *arg);

void ICACHE_FLASH_ATTR
cb_sock_connect(void *arg);

void ICACHE_FLASH_ATTR
cb_dns_resolved(const char *hostname, ip_addr_t *ip_resolved, void *arg);

void ICACHE_FLASH_ATTR
cb_wifi_event(System_Event_t *evt);

void ICACHE_FLASH_ATTR
cb_system_init_done(void);

void ICACHE_FLASH_ATTR
do_get_default_plant_name(char *default_plant_name, uint8_t len);

bool ICACHE_FLASH_ATTR
do_save_current_config_to_flash(/*bool do_timer_reset*/);

uint32_t ICACHE_FLASH_ATTR
do_get_sensor_reading(uint32_t adc_sample_size);

void ICACHE_FLASH_ATTR
do_toggle_sesnor(bool toggle);

void ICACHE_FLASH_ATTR
do_read_adc(void);

uint8_t ICACHE_FLASH_ATTR
do_get_hash_pearson(uint8_t *bytes);

void ICACHE_FLASH_ATTR
do_state_config(void);

void ICACHE_FLASH_ATTR
do_state_error_toggle_led(void);

void ICACHE_FLASH_ATTR
do_state_error(void);

bool ICACHE_FLASH_ATTR
do_configuration_read(void);

void ICACHE_FLASH_ATTR
do_led_blue_turn_on(void);

void ICACHE_FLASH_ATTR
do_led_blue_turn_off(void);

void ICACHE_FLASH_ATTR
do_read_counter_value_from_rtc_mem(void);

void ICACHE_FLASH_ATTR
do_setup_debug_UART(void);

void ICACHE_FLASH_ATTR
user_init(void);
