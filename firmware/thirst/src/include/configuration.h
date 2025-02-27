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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#define CONFIG_SSID_LEN                 32
#define CONFIG_BSSID_LEN                16
#define CONFIG_SSID_PASSWORD_LEN        64
#define CONFIG_THRESHOLD_MODE_SIMPLE    1
#define CONFIG_THRESHOLD_MODE_ADVANCED  2
#define CONFIG_NOTIFICATION_EMAIL_LEN   64
#define CONFIG_NOTIFICATION_SUBJECT_LEN 64
#define CONFIG_NOTIFICATION_MESSAGE_LEN 512
#define CONFIG_PERMUTATION_PEARSON_SIZE 256

#define CONFIG_FMT_DEFAULT_PLANT_NAME "Thirst-%d"

#define CONFIG_THRESHLOD_LT 1
#define CONFIG_THRESHLOD_GT 2

typedef struct {
	// Configuration checksum.
	uint8_t hash;

	// Plant configuration.
	uint8_t threshold_mode;
	uint8_t threshold_lt_gt;
	uint8_t threshold_percent;
	uint32_t registered_value;
	char plant_name[CONFIG_SSID_LEN + 1];
	char wifi_ap_bssid[CONFIG_BSSID_LEN];
	char wifi_ap_ssid[CONFIG_SSID_LEN + 1];
	char wifi_ap_password[CONFIG_SSID_PASSWORD_LEN + 1];
	char configuration_password[CONFIG_SSID_PASSWORD_LEN + 1];

	// Notification configuration.
	char notification_email[CONFIG_NOTIFICATION_EMAIL_LEN + 1];
	char notification_email_subject[CONFIG_NOTIFICATION_SUBJECT_LEN + 1];
	char notification_email_message[CONFIG_NOTIFICATION_MESSAGE_LEN + 1];
} config_t;

bool ICACHE_FLASH_ATTR
config_read(void);

void ICACHE_FLASH_ATTR
config_get_default_plant_name(
	char *default_plant_name,
	uint8_t len
);

void ICACHE_FLASH_ATTR
config_load_default_config(void);

uint8_t ICACHE_FLASH_ATTR
config_get_hash_pearson(uint8_t *bytes);

#endif // CONFIGURATION_H
