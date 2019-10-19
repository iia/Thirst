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

#ifndef THIRST_H
#define THIRST_H

#include <mem.h>
#include <stdlib.h>

// Main interface provided by the Espressif SDK.
#include "user_interface.h"

#include "gpio.h"
#include "osapi.h"
#include "espfs.h"
#include "espconn.h"
#include "platform.h"
#include "driver/uart.h"
#include "json/jsonparse.h"

// Required for web interface.
#include "httpd.h"
#include "httpdespfs.h"

#include "system.h"
#include "gateway.h"
#include "peripherals.h"
#include "configuration.h"
#include "web_interface.h"

// Firmware version.
#define THIRST_FIRMWARE_VERSION "v5"

static bool ICACHE_FLASH_ATTR
thirst_init(void);

static bool ICACHE_FLASH_ATTR
thirst_init_main_task(void);

#endif // THIRST_H
