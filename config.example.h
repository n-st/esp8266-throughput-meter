/* Configuration for ThroughputMeter.ino */

// Credentials for WiFi access point
#define WIFI_SSID "MyAP"
#define WIFI_PSK  "passw0rd"

// Uncomment to only show most significant digit of throughput numbers
// (replacing the rest with _)
//#define LOW_PRECISION_MODE 1

// Timeout for TCP connection attempts (milliseconds).
// Applies both to the info host and the two connectivity checks, so may be
// encountered up to three times per cycle.
#define CONNECT_TIMEOUT_MS 200
// Time between "cycles", i.e. display info refreshes (milliseconds).
#define CYCLE_INTERVAL 500
// Time between internet connection checks (milliseconds).
#define CONNECTION_TEST_INTERVAL_MS 10000
// Test hosts to check internet connection.
// Can be hostnames or IP addresses.
// Will be tested via a TCP connection attempt on port 80.
#define CONNECTION_TEST_HOST_V4 "1.0.0.1"
#define CONNECTION_TEST_HOST_V6 "2606:4700:4700::1001"
// Info host to obtain throughput information from.
// Can be a hostname, IPv4 address or (non-link-local) IPv6 address.
#define THROUGHPUT_INFO_HOST "2001:db8::1"
// Port on the info host for the maximum throughput value (in Bytes/sec).
#define THROUGHPUT_INFO_MAXVAL_PORT 17460
// Port on the info host for the byte counter output (absolute, monotonously
// increasing Byte value).
#define THROUGHPUT_INFO_CURVAL_PORT 17461
