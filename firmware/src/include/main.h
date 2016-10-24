#include "driver/uart.h"
#include "user_interface.h"

// Generic iterator for loops and such.
int i;

// Function prototypes.
void
cb_timer_generic_software(void);

void ICACHE_FLASH_ATTR
cb_sock_disconnect(void *arg);

void ICACHE_FLASH_ATTR
cb_sock_recv(void *arg, char *data, unsigned short length);

void ICACHE_FLASH_ATTR
cb_sock_sent(void *arg);

void ICACHE_FLASH_ATTR
cb_sock_connect(void *arg);

void ICACHE_FLASH_ATTR
cb_dns_resolved(const char *hostname, ip_addr_t *ip_resolved, void *arg);

void ICACHE_FLASH_ATTR
cb_wifi_event(System_Event_t *evt);

void  ICACHE_FLASH_ATTR
do_read_ADC(void);

void ICACHE_FLASH_ATTR
cb_system_init_done(void);

void ICACHE_FLASH_ATTR
do_setup_debug_UART();

void ICACHE_FLASH_ATTR
user_init(void);
