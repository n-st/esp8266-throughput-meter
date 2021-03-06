#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <U8g2lib.h>
#include <time.h>

#include "config.h"

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define TIMEDELTA(old__, new__) ( \
        ((new__) < (old__)) \
        ? (ULONG_MAX - (old__) + (new__)) \
        : ((new__) - (old__)) \
        )

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

#define THROUGHPUT_UNKNOWN ULONG_MAX

unsigned long long previousRXBytecount4 = 0;
unsigned long long previousRXBytecount6 = 0;
unsigned long long previousTXBytecount4 = 0;
unsigned long long previousTXBytecount6 = 0;
unsigned long previousTimestampMillis = 0;
unsigned long previousConnCheckMillis = 0;
bool connectivity4 = false;
bool connectivity6 = false;
bool tpMaxGiven = false;
unsigned long tpMaxRX4 = 0;
unsigned long tpMaxRX6 = 0;
unsigned long tpMaxTX4 = 0;
unsigned long tpMaxTX6 = 0;

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

    unsigned long deltaMillis = TIMEDELTA(previousTimestampMillis, currentTimestampMillis);
    Serial.print("Bytes over ");
    Serial.print(deltaMillis);
    Serial.print(" ms: ");

    if (currentBytecount == 0) {
        Serial.println("[no current byte count; skipping]");
        return THROUGHPUT_UNKNOWN;

    } else if (*previousBytecount == 0) {
        Serial.println("[no previous byte count; initialising]");
        *previousBytecount = currentBytecount;
        return THROUGHPUT_UNKNOWN;

    } else if (currentBytecount < *previousBytecount) {
        // we don't know the bit length of the counter, so we can't (easily)
        // calculate the bytes transferred during the overflow
        // (we would have to guess based on the previous counter value, which
        // should be close to the MAX value of the counter datatype)
        Serial.println("[byte count roll-over; re-initialising]");
        *previousBytecount = currentBytecount;
        return THROUGHPUT_UNKNOWN;

    } else {
        unsigned long throughput = (1000ULL * (currentBytecount - *previousBytecount) + (deltaMillis/2)) / deltaMillis;
        Serial.println((unsigned long)(currentBytecount - *previousBytecount));

        Serial.print("Bytes per second: ");
        Serial.println(throughput);

        *previousBytecount = currentBytecount;

        return throughput;
    }
}

unsigned int calculateThroughputBarWidth(unsigned long tp, unsigned long tpMax) {
    if (tp == THROUGHPUT_UNKNOWN || tp == 0) {
        // unknown || 0
        return 0;

    } else {
        unsigned int bar_width;
        bar_width = ((unsigned long long) tp * DISPLAY_WIDTH + tpMax/2) / tpMax;
        if (bar_width == 0) {
            // throughput is not 0, so bar should at least be visible
            bar_width = 1;
        }
        return bar_width;
    }
}

void formatThroughputStr(long throughput, char *buffer) {
    if (throughput == THROUGHPUT_UNKNOWN) {
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
        #ifdef LOW_PRECISION_MODE
        for (int i = 1; i < strlen(buffer)-1; ++i) {
            buffer[i] = '_';
        }
        #endif

    } else if (throughput <= 999999999) {
        // 1000'000-999999'999 - ___.___k
        throughput = (throughput+1000/2)/1000;
        #ifdef LOW_PRECISION_MODE
        sprintf(buffer, "%lu ___k", throughput/1000);
        for (int i = 1; i < strlen(buffer)-5; ++i) {
            buffer[i] = '_';
        }
        #else
        sprintf(buffer, "%lu %03luk", throughput/1000, throughput%1000);
        #endif

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
    WiFiMulti.addAP(WIFI_SSID, WIFI_PSK);

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
    u8g2.print("IP: ");
    u8g2.print(WiFi.localIP());
    u8g2.sendBuffer();

    Serial.print("Waiting for info host.");
    while (!client.connect(THROUGHPUT_INFO_HOST, THROUGHPUT_INFO_CURVAL_PORT)) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("");

    u8g2.setCursor(0, 31);
    if (client.connect(THROUGHPUT_INFO_HOST, THROUGHPUT_INFO_MAXVAL_PORT)) {
        tpMaxRX4 = strToULL(client.readStringUntil('\n'));
        tpMaxTX4 = strToULL(client.readStringUntil('\n'));
        tpMaxRX6 = strToULL(client.readStringUntil('\n'));
        tpMaxTX6 = strToULL(client.readStringUntil('\n'));
        tpMaxGiven = true;
        u8g2.print("Max: ");
        u8g2.print(tpMaxRX4/1000000);
        u8g2.print('/');
        u8g2.print(tpMaxTX4/1000000);
        u8g2.print('/');
        u8g2.print(tpMaxRX6/1000000);
        u8g2.print('/');
        u8g2.print(tpMaxTX6/1000000);
        Serial.print("Max throughput (MB/s RX4/TX4/RX6/TX6): ");
        Serial.print(tpMaxRX4/1000000);
        Serial.print('/');
        Serial.print(tpMaxTX4/1000000);
        Serial.print('/');
        Serial.print(tpMaxRX6/1000000);
        Serial.print('/');
        Serial.print(tpMaxTX6/1000000);
        Serial.println();
    } else {
        u8g2.print("Max tp not set");
        Serial.println("Max throughput not set");
    }
    u8g2.sendBuffer();

    delay(1000);
}

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
        tpTX4 = calculateThroughput(currentTXBytecount4, &previousTXBytecount4, currentTimestampMillis, previousTimestampMillis);
        tpRX6 = calculateThroughput(currentRXBytecount6, &previousRXBytecount6, currentTimestampMillis, previousTimestampMillis);
        tpTX6 = calculateThroughput(currentTXBytecount6, &previousTXBytecount6, currentTimestampMillis, previousTimestampMillis);
        unsigned long tpSumRX = tpRX4 + tpRX6;
        if (!tpMaxGiven && tpRX4 != THROUGHPUT_UNKNOWN && tpRX6 != THROUGHPUT_UNKNOWN && (tpSumRX > tpMaxRX4 || tpSumRX > tpMaxRX6)) {
            tpMaxRX4 = tpSumRX;
            tpMaxRX6 = tpSumRX;
        }
        unsigned long tpSumTX = tpTX4 + tpTX6;
        if (!tpMaxGiven && tpTX4 != THROUGHPUT_UNKNOWN && tpTX6 != THROUGHPUT_UNKNOWN && (tpSumTX > tpMaxTX4 || tpSumTX > tpMaxTX6)) {
            tpMaxTX4 = tpSumTX;
            tpMaxTX6 = tpSumTX;
        }
    } else {
        Serial.println("FAIL");
        tpRX4 = THROUGHPUT_UNKNOWN;
        tpRX6 = THROUGHPUT_UNKNOWN;
        tpTX4 = THROUGHPUT_UNKNOWN;
        tpTX6 = THROUGHPUT_UNKNOWN;
    }
    previousTimestampMillis = currentTimestampMillis;

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
    unsigned int bar_width;

    formatThroughputStr(tpRX4, s);
    displayPrintRightAligned(s, DISPLAY_WIDTH/2-ARROW_FONT_WIDTH/2-3, 1+2*PADDED_BAR_HEIGHT);
    bar_width = calculateThroughputBarWidth(tpRX4, tpMaxRX4);
    u8g2.drawBox(0, 1, bar_width, PADDED_BAR_HEIGHT-1);

    formatThroughputStr(tpTX4, s);
    displayPrintRightAligned(s, DISPLAY_WIDTH/2-ARROW_FONT_WIDTH/2-3, 1+2*PADDED_BAR_HEIGHT+FONT_HEIGHT+LINE_SPACING);
    bar_width = calculateThroughputBarWidth(tpTX4, tpMaxTX4);
    u8g2.drawBox(0, DISPLAY_HEIGHT-2*PADDED_BAR_HEIGHT+1, bar_width, PADDED_BAR_HEIGHT-1);

    formatThroughputStr(tpRX6, s);
    displayPrintRightAligned(s, DISPLAY_WIDTH-(FONT_WIDTH+2)-3, 1+2*PADDED_BAR_HEIGHT);
    bar_width = calculateThroughputBarWidth(tpRX6, tpMaxRX6);
    u8g2.drawBox(DISPLAY_WIDTH-bar_width, 1+PADDED_BAR_HEIGHT, bar_width, PADDED_BAR_HEIGHT-1);

    formatThroughputStr(tpTX6, s);
    displayPrintRightAligned(s, DISPLAY_WIDTH-(FONT_WIDTH+2)-3, 1+2*PADDED_BAR_HEIGHT+FONT_HEIGHT+LINE_SPACING);
    bar_width = calculateThroughputBarWidth(tpTX6, tpMaxTX6);
    u8g2.drawBox(DISPLAY_WIDTH-bar_width, DISPLAY_HEIGHT-PADDED_BAR_HEIGHT+1, bar_width, PADDED_BAR_HEIGHT-1);

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

    /* Check internet connectivity (to be displayed next cycle) */
    if (TIMEDELTA(previousConnCheckMillis, currentTimestampMillis) >= CONNECTION_TEST_INTERVAL_MS) {
        Serial.println("Checking connectivity...");

        connectivity4 = checkConnectivity4();
        Serial.print("IPv4: ");
        Serial.println(connectivity4 ? "OK" : "FAIL");

        connectivity6 = checkConnectivity6();
        Serial.print("IPv6: ");
        Serial.println(connectivity6 ? "OK" : "FAIL");

        previousConnCheckMillis = currentTimestampMillis;
    }

    /* Debug output: Obtained max values */
    Serial.print("Max throughput (B/s RX4/TX4/RX6/TX6): ");
    Serial.print(tpMaxRX4);
    Serial.print('/');
    Serial.print(tpMaxTX4);
    Serial.print('/');
    Serial.print(tpMaxRX6);
    Serial.print('/');
    Serial.print(tpMaxTX6);
    Serial.println();

    /* Calculate waiting time until next cycle */
    int waitMillis = TIMEDELTA(millis(), currentTimestampMillis + CYCLE_INTERVAL);
    if (waitMillis < 0) {
        waitMillis = 0;
    }
    Serial.print("Waiting ");
    Serial.print(waitMillis);
    Serial.println(" ms until next cycle...");
    delay(waitMillis);
}
