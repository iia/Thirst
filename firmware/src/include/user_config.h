// System related.
#define ENABLE_DEBUG 1
#define GPIO_NOTIFICATION_LED FUNC_GPIO2
#define GPIO_NOTIFICATION_LED_BIT BIT2
#define UART_BIT_RATE UART_CLK_FREQ/BIT_RATE_115200

// Timer related.
#define DEEP_SLEEP_INTERVAL 8000000 // In microseconds.
#define TIMER_DURATION_NOTIFICATION_LED_OFF 8000 // In miliseconds.

// WiFi station related.
#define WIFI_STATION_SSID "LisIsh"
#define WIFI_STATION_SSID_PASSWORD "connect_to_wlan"

// ADC related.
#define ADC_CLK_DIV 8
#define ADC_SAMPLE_SIZE 32
#define ADC_SENSOR_THRESHOLD 650

// Postmark related.
#define POSTMATK_API_PORT 80
#define POSTMATK_SIZE_SEND_BUFFER 4096
#define POSTMATK_API_HOST "api.postmarkapp.com"
#define POSTMATK_API_TOKEN "cd636c0b-9c23-42f1-8c62-6bba620f079c"
#define POSTMATK_FMT_HTTP_HEADER "POST /email HTTP/1.1\n\
Host: api.postmarkapp.com\n\
Accept: application/json; charset=utf-8\n\
Content-Type: application/json; charset=utf-8\n\
Content-Length: %d\n\
X-Postmark-Server-Token: %s\n\
Cache-Control: no-cache\n\
\n\
%s"
#define POSTMATK_DATA_HTTP_JSON "\
{\
\"From\": \"ishraq@tinkerforge.com\",\
\"To\": \"ishraq86@gmail.com\",\
\"Subject\": \"Sensor status\",\
\"HtmlBody\": \"Probes submerged in water!\"\
}"
