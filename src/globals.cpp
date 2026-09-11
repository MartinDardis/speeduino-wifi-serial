// config.cpp — Global variable definitions and debug output sink
//
// Purpose: the single translation unit that *defines* all variables declared
//   as extern in globals.h. Also defines TcpLogger, the concrete Print
//   implementation that Debug_COM points to.
//
// Why TcpLogger lives here (not in uart.cpp or webserver.cpp):
//   TcpLogger directly accesses hwclient[][], which is defined in this same
//   file. Placing TcpLogger here avoids a circular dependency between
//   webserver.cpp and config.cpp.
//
// Why Debug_COM is a Print* (interface pointer, not a concrete type):
//   Using the abstract Print interface lets any module call Debug_COM->print()
//   without knowing whether output goes to Serial, a TCP socket, or a null
//   sink. During testing or production builds, Debug_COM could be set to NULL
//   to suppress all debug output with zero code changes in callers.
#include "globals.h"

UARTParam      COM[NUM_HW_COM];
HardwareSerial* HW_SERIAL[3] = { &Serial0, &Serial1, &Serial2 };

WiFiServer* hwserver[NUM_HW_COM] = { NULL, NULL, NULL };
WiFiClient  hwclient[NUM_HW_COM][MAX_NMEA_CLIENTS];

uint8_t  buf1[NUM_HW_COM][bufferSize];
uint16_t i1[NUM_HW_COM];
uint8_t  buf2[NUM_HW_COM][bufferSize + 2];
uint16_t i2[NUM_HW_COM];

// TcpLogger mirrors every debug write to both the USB-CDC Serial port and all
// TCP clients currently connected to port 0 (COM0). This lets a host connected
// via TCP see the same diagnostic output as the USB monitor, which is useful
// when the device is deployed without a USB cable attached.
class TcpLogger : public Print {
public:
  size_t write(uint8_t c) override {
    Serial.write(c);
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++)
      if (hwclient[0][i] && hwclient[0][i].connected())
        hwclient[0][i].write(c);
    return 1;
  }
  size_t write(const uint8_t* buf, size_t size) override {
    Serial.write(buf, size);
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++)
      if (hwclient[0][i] && hwclient[0][i].connected())
        hwclient[0][i].write(buf, size);
    return size;
  }
};

static TcpLogger tcpLogger;
Print* Debug_COM = &tcpLogger;

AsyncWebServer server(80);
Preferences    preferences;

bool   bLoginEnable    = false;
String login_username  = "admin";
String login_password  = "admin";

String receiverName    = "SpeeduinoWiFi";
bool   openAP          = true;
bool   connectFailed   = false;
String WiFiClientState = "";
String AP_pw           = "12345678";
String WiFi_ssid;
String WiFi_pw;
int    iWiFiSearchTime = 0;
int    iWiFiTxPower    = 20;

bool bSioDebugInit  = true;
bool bSioDebugError = true;

volatile uint32_t ledOffTime    = 0;
volatile uint32_t ledRedOffTime = 0;
