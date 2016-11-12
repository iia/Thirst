#include "main.h"
#include "web_interface.h"

int ICACHE_FLASH_ATTR cgi_root(HttpdConnData *connection_data, char *token, void **arg) {
	os_printf("\n[+] DBG: cgi_root()\n");

	if (token == NULL) {
		return HTTPD_CGI_DONE;
	}

	if (os_strcmp(token, "PLANT_NAME") == 0) {
		httpdSend(connection_data, config_current.plant_name, -1);
	}

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_404(HttpdConnData *connection_data) {
	os_printf("\n[+] DBG: cgi_404()\n");

	return HTTPD_CGI_DONE;
}
