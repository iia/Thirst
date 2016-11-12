#include "platform.h"
#include "httpd.h"
#include "httpdespfs.h"

int ICACHE_FLASH_ATTR
cgi_root(HttpdConnData *connection_data, char *token, void **arg);

int ICACHE_FLASH_ATTR
cgi_404(HttpdConnData *connection_data);

static HttpdBuiltInUrl httpd_urls[] = {
	{"/", cgiRedirect, "/index.html"},
	{"/index.html", cgiEspFsTemplate, cgi_root},
	{"*", cgiEspFsHook, NULL},
	{NULL, NULL, NULL}
};
