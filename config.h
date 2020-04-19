/* Configuration for ThroughputMeter.ino */

// Credentials for WiFi access point
#define WIFI_SSID "MyAP"
#define WIFI_PSK  "passw0rd"

// Uncomment to only show most significant digit of throughput numbers
// (blanking out the rest with _)
//#define LOW_PRECISION_MODE 1

#define CONNECT_TIMEOUT_MS 200
#define CYCLE_INTERVAL 500
#define CONNECTION_TEST_INTERVAL_MS 10000
#define CONNECTION_TEST_HOST_V4 "1.0.0.1"
#define CONNECTION_TEST_HOST_V6 "2606:4700:4700::1001"
#define THROUGHPUT_INFO_HOST "2001:db8::1"
#define THROUGHPUT_INFO_MAXVAL_PORT 17460
#define THROUGHPUT_INFO_CURVAL_PORT 17461
