ESP8266 Network Throughput Meter
================================

Queries the current network throughput from an "info host" via a raw TCP
protocol and displays it on a 128x32px display.

Developed and tested for a "WiFi Kit 8"-style ESP8266 + OLED integrated board.

Notes
-----

- Byte counters are stored as 64-bit unsigned integers. The Arduino environment
  does not provide native conversion functions for this, even on the ESP8266,
  so conversion is done manually.
  Non-digit characters are silently ignored.
- Throughput (bytes/sec) is internally stored as 32-bit unsigned integers, so
  it tops out at 4294967295 B/s, i.e. 34.4 Gbit/s. This is currently deemed
  sufficient.
- To align everything symmetrically, the top 1px row is not used.
- The internet connectivity check uses Cloudflare's Public DNS website by
  default, using 1.0.0.1 for IPv4 as it is less commonly mis-configured in
  local networks than 1.1.1.1.
  Using an anycasted target gives the best chance that it will be online; using
  a HTTP-enabled one allows us to resort to TCP requests rather than
  implementing ICMP on the ESP8266.
