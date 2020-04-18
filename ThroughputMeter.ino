#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <U8g2lib.h>
#include <time.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#ifndef STASSID
#define STASSID "MyAP"
#define STAPSK  "passw0rd"
#endif

#define CONNECT_TIMEOUT_MS 300
#define CYCLE_INTERVAL 1000
#define CONNECTION_TEST_INTERVAL_MS 5000
#define CONNECTION_TEST_HOST_V4 "1.1.0.1"
#define CONNECTION_TEST_HOST_V6 "2606:4700:4700::1001"
#define THROUGHPUT_INFO_HOST "2001:db8::1"
#define THROUGHPUT_INFO_MAXVAL_PORT 17460
#define THROUGHPUT_INFO_CURVAL_PORT 17461

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32
#define FONT_HEIGHT 7
#define FONT_WIDTH 5
#define PADDED_BAR_HEIGHT 4
#define LINE_SPACING 1

#define ARROW_DOWN 'L'
#define ARROW_UP 'O'
#define ARROW_FONT_HEIGHT 8
#define ARROW_FONT_WIDTH 6

const char* ssid     = STASSID;
const char* password = STAPSK;

ESP8266WiFiMulti WiFiMulti;
WiFiClient client;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ 16, /* clock=*/ 5, /* data=*/ 4);

bool checkConnectivity4() {
    bool result = true;
    if (!client.connect(CONNECTION_TEST_HOST_V4, 80)) {
        result = false;
    }
    client.stop();
    return result;
}

bool checkConnectivity6() {
    bool result = true;
    if (!client.connect(CONNECTION_TEST_HOST_V6, 80)) {
        result = false;
    }
    client.stop();
    return result;
}

unsigned long long strToULL(String s) {
    unsigned long long result = 0;
    for (int i = 0; i < s.length(); ++i) {
        char c = s[i];
        if (!('0' <= c && c <= '9')) {
            // skip non-digits
            continue;
        }
        result *= 10LL;
        result += c-'0';
    }
    return result;
}

unsigned long calculateThroughput(
        const unsigned long long currentBytecount,
        unsigned long long *previousBytecount,
        const unsigned long currentTimestampMillis,
        const unsigned long previousTimestampMillis) {

    unsigned long throughput = ULONG_MAX;
    unsigned long deltaMillis = currentTimestampMillis - previousTimestampMillis;
    Serial.print("Throughput over ");
    Serial.print(deltaMillis);
    Serial.print(" ms: ");
    if (*previousBytecount != 0) {
        throughput = 1000 * (currentBytecount - *previousBytecount) + (deltaMillis/2) / deltaMillis;
    }
    Serial.println(throughput);

    *previousBytecount = currentBytecount;

    return throughput;
}

void formatThroughputStr(long throughput, char *buffer) {
    if (throughput == ULONG_MAX) {
        // unknown -> ---
        sprintf(buffer, "---");

    } else if (throughput == 0) {
        // 0 -> 0k
        sprintf(buffer, "0k");

    } else if (throughput <= 499) {
        // 1-499 -> +0k
        sprintf(buffer, "+0k");

    } else if (throughput <= 999999) {
        // 500-999'999 - ___k
        throughput = (throughput+1000/2)/1000;
        sprintf(buffer, "%luk", throughput);

    } else if (throughput <= 999999999) {
        // 1000'000-999999'999 - ___.___k
        throughput = (throughput+1000/2)/1000;
        sprintf(buffer, "%lu %03luk", throughput/1000, throughput%1000);

    } else {
        // 1000000-... - _____M
        throughput = (throughput+1000/2)/1000;
        sprintf(buffer, "%luM", throughput/1000);
    }
}

void displayPrintRightAligned(char *s, unsigned int rightX, unsigned int topY) {
    int width = u8g2.getStrWidth(s);
    u8g2.setCursor(rightX-width, topY+FONT_HEIGHT);
    u8g2.print(s);
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println();
    Serial.println("Initialising...");

    u8g2.begin();
    u8g2.setFont(u8g2_font_finderskeepers_tr);
    u8g2.clearBuffer();

    u8g2.setCursor(0, 7);
    u8g2.print("Initialising...");
    u8g2.sendBuffer();

    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP(ssid, password);

    client.setTimeout(CONNECT_TIMEOUT_MS);

    Serial.print("Waiting for WiFi.");
    u8g2.setCursor(0, 15);
    u8g2.print("Waiting for WiFi...");
    u8g2.sendBuffer();

    while (WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    u8g2.setCursor(0, 23);
    u8g2.print("IP address:");
    u8g2.setCursor(0, 31);
    u8g2.print(WiFi.localIP());
    u8g2.sendBuffer();

    delay(1000);
}

unsigned long long previousRXBytecount4 = 0;
unsigned long long previousRXBytecount6 = 0;
unsigned long long previousTXBytecount4 = 0;
unsigned long long previousTXBytecount6 = 0;
unsigned long previousTimestampMillis = 0;
unsigned long previousConnCheckMillis = 0;
bool connectivity4 = false;
bool connectivity6 = false;

void loop() {
    /* Get byte counters, compute throughput */

    unsigned long tpRX4;
    unsigned long tpRX6;
    unsigned long tpTX4;
    unsigned long tpTX6;

    Serial.print("Getting byte counters... ");
    unsigned long currentTimestampMillis = millis();
    if (client.connect(THROUGHPUT_INFO_HOST, THROUGHPUT_INFO_CURVAL_PORT)) {
        unsigned long long currentRXBytecount4 = strToULL(client.readStringUntil('\n'));
        unsigned long long currentTXBytecount4 = strToULL(client.readStringUntil('\n'));
        unsigned long long currentRXBytecount6 = strToULL(client.readStringUntil('\n'));
        unsigned long long currentTXBytecount6 = strToULL(client.readStringUntil('\n'));
        Serial.println("OK");
        client.stop();
        tpRX4 = calculateThroughput(currentRXBytecount4, &previousRXBytecount4, currentTimestampMillis, previousTimestampMillis);
        tpRX6 = calculateThroughput(currentRXBytecount6, &previousRXBytecount6, currentTimestampMillis, previousTimestampMillis);
        tpTX4 = calculateThroughput(currentTXBytecount4, &previousTXBytecount4, currentTimestampMillis, previousTimestampMillis);
        tpTX6 = calculateThroughput(currentTXBytecount6, &previousTXBytecount6, currentTimestampMillis, previousTimestampMillis);
    } else {
        Serial.println("FAIL");
        tpRX4 = ULONG_MAX;
        tpRX6 = ULONG_MAX;
        tpTX4 = ULONG_MAX;
        tpTX6 = ULONG_MAX;
    }
    previousTimestampMillis = currentTimestampMillis;

    /* Check internet connectivity */
    currentTimestampMillis = millis();
    if (currentTimestampMillis >= previousConnCheckMillis + CONNECTION_TEST_INTERVAL_MS) {
        Serial.println("Checking connectivity...");

        connectivity4 = checkConnectivity4();
        Serial.print("IPv4: ");
        Serial.println(connectivity4 ? "OK" : "FAIL");

        connectivity6 = checkConnectivity6();
        Serial.print("IPv6: ");
        Serial.println(connectivity6 ? "OK" : "FAIL");

        previousConnCheckMillis = currentTimestampMillis;
    }

    /* Fill display */
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_open_iconic_arrow_1x_t);
    u8g2.setCursor(DISPLAY_WIDTH/2-ARROW_FONT_WIDTH/2, 1+2*PADDED_BAR_HEIGHT+ARROW_FONT_HEIGHT);
    u8g2.print(ARROW_DOWN);
    u8g2.setCursor(DISPLAY_WIDTH/2-ARROW_FONT_WIDTH/2, 1+2*PADDED_BAR_HEIGHT+2*ARROW_FONT_HEIGHT+LINE_SPACING-2);
    u8g2.print(ARROW_UP);

    u8g2.setFont(u8g2_font_finderskeepers_tr);

    char s[30];
    int w;

    formatThroughputStr(tpRX4, s);
    displayPrintRightAligned(s, DISPLAY_WIDTH/2-ARROW_FONT_WIDTH/2-3, 1+2*PADDED_BAR_HEIGHT);

    formatThroughputStr(tpTX4, s);
    displayPrintRightAligned(s, DISPLAY_WIDTH/2-ARROW_FONT_WIDTH/2-3, 1+2*PADDED_BAR_HEIGHT+FONT_HEIGHT+LINE_SPACING);

    formatThroughputStr(tpRX6, s);
    displayPrintRightAligned(s, DISPLAY_WIDTH-(FONT_WIDTH+2)-3, 1+2*PADDED_BAR_HEIGHT);

    formatThroughputStr(tpTX6, s);
    displayPrintRightAligned(s, DISPLAY_WIDTH-(FONT_WIDTH+2)-3, 1+2*PADDED_BAR_HEIGHT+FONT_HEIGHT+LINE_SPACING);

    if (!connectivity4) {
        u8g2.drawBox(0, 1+4+4, FONT_WIDTH+2, 15);
        u8g2.setDrawColor(0);
    }
    u8g2.setCursor(1, 1+4+4+4+FONT_HEIGHT);
    u8g2.print('4');
    u8g2.setDrawColor(1);

    if (!connectivity6) {
        u8g2.drawBox(DISPLAY_WIDTH-(FONT_WIDTH+2), 1+4+4, FONT_WIDTH+2, 15);
        u8g2.setDrawColor(0);
    }
    u8g2.setCursor(DISPLAY_WIDTH-1-FONT_WIDTH, 1+4+4+4+FONT_HEIGHT);
    u8g2.print('6');
    u8g2.setDrawColor(1);

    u8g2.sendBuffer();

    /* Calculate waiting time until next cycle */
    int waitMillis = (currentTimestampMillis + CYCLE_INTERVAL) - millis();
    if (waitMillis < 0) {
        waitMillis = 0;
    }
    Serial.print("Waiting ");
    Serial.print(waitMillis);
    Serial.println(" ms until next cycle...");
    delay(waitMillis);
}
