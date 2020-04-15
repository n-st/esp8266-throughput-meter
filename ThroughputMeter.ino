/*
    This sketch sends a string to a TCP server, and prints a one-line response.
    You must run a TCP server in your local network.
    For example, on Linux you can use this command: nc -v -l 3000
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#ifndef STASSID
#define STASSID "MyAP"
#define STAPSK  "passw0rd"
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;

const char* host = "2001:db8::1";
const uint16_t port = 17460;

ESP8266WiFiMulti WiFiMulti;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ 16, /* clock=*/ 5, /* data=*/ 4);

void setup() {
  Serial.begin(115200);
  
  u8g2.begin();

  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);

  Serial.println();
  Serial.println();
  Serial.print("Wait for WiFi... ");

  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  delay(500);
}

unsigned long long old4 = 0;
unsigned long long old6 = 0;

void loop() {
    u8g2.clearBuffer();          // clear the internal memory
  //u8g2.setFont(u8g2_font_luRS08_tr); // 20 chars
  u8g2.setFont(u8g2_font_finderskeepers_tr); // 27 chars
  //u8g2.setFont(u8g2_font_pxplusibmcgathin_8u); // 16 chars
  
  Serial.print("connecting to ");
  Serial.print(host);
  Serial.print(':');
  Serial.println(port);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;

  client.setTimeout(300);

  if (!client.connect(host, port)) {
    Serial.println("connection failed");
    Serial.println("wait 5 sec...");
    delay(5000);
    return;
  }

    Serial.println("80 connection attempt");
  if (!client.connect("2606:4700:4700::1001", 80)) {
    Serial.println("80 connection failed");
  }

  // This will send the request to the server
  client.println("hello from ESP8266");

  //read back one line from server
  Serial.println("receiving from remote server");
  String line = client.readStringUntil('\n');
  unsigned long long new4 = 0;
  for (int i = 0; i < line.length(); ++i) {
    char c = line[i];
    Serial.print(c);
    new4 *= 10LL;
    new4 += c-'0';
  }
  line = client.readStringUntil('\n');
  line = client.readStringUntil('\n');
  unsigned long long new6 = 0;
  for (int i = 0; i < line.length(); ++i) {
    char c = line[i];
    Serial.print(c);
    new6 *= 10LL;
    new6 += c-'0';
  }
  line = client.readStringUntil('\n');
  //Serial.println(line);
  Serial.println((long int)(new6-old6));

  u8g2.setCursor(0, 7);
  u8g2.print((long int)(new4-old4));  // write something to the internal memory
  u8g2.setCursor(0, 15);
  u8g2.print((long int)(new6-old6));  // write something to the internal memory
  
  u8g2.setCursor(0, 23);
  u8g2.print("v 987.654k ");
  u8g2.setFont(u8g2_font_open_iconic_arrow_1x_t);
  u8g2.print('L');
  u8g2.setFont(u8g2_font_finderskeepers_tr); // 27 chars
  u8g2.print(" 230.654k v");  // write something to the internal memory
  
  u8g2.setCursor(0, 31);
  u8g2.print("4 900 723k ");
  u8g2.setFont(u8g2_font_open_iconic_arrow_1x_t);
  u8g2.print('O');
  u8g2.setFont(u8g2_font_finderskeepers_tr); // 27 chars
  u8g2.print(" 230 654k 6");  // write something to the internal memory

  
  old4 = new4;
  old6 = new6;
  u8g2.sendBuffer();          // transfer internal memory to the display

  Serial.println("closing connection");
  client.stop();

  Serial.println("wait 1 sec...");
  delay(1000);
}
