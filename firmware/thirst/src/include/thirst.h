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

static bool ICACHE_FLASH_ATTR
thirst_init(void);

static bool ICACHE_FLASH_ATTR
thirst_init_main_task(void);

#endif // THIRST_H
