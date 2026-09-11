// main.cpp — Application entry point
//
// Purpose: Arduino setup() and loop() plus the FreeRTOS bridge task.
//
// Dual-core architecture:
//   Core 0 (Arduino default): loop(), OTA processing, WebSocket cleanup, LED updates.
//     The ESP32 WiFi/TCP stack also runs on core 0 via system tasks.
//   Core 1 (pinned task):    serialBridgeTask() — continuously polls all UART
//     ports and routes data. Running the bridge on a dedicated core prevents the
//     network stack from starving UART polling during heavy WiFi activity.
//
// Initialization order in setup():
//   1. Semaphores and buffer indices (must come before any task or ISR).
//   2. LED — early visual confirmation that the MCU is alive.
//   3. Serial / debug output.
//   4. NVS config load — populates COM[], WiFi credentials, etc.
//   5. UART pin assignment and initialization.
//   6. WiFiServer creation — static locals avoid the global constructor order
//      problem; they are guaranteed to be constructed before setup() returns.
//   7. SPIFFS mount, WiFi, web server.
//   8. TCP server start.
//   9. Bridge task creation on core 1.
#include <Arduino.h>
#include <SPIFFS.h>
#include "globals.h"
#include <ElegantOTA.h>
#include <Adafruit_NeoPixel.h>
#include "nvs/nvs.h"
#include "uart/uart.h"
#include "wifi_setup/wifi_setup.h"
#include "webserver/webserver.h"
#include "bridge/bridge.h"

static Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);

static int setup_spiffs() {
  SPIFFS.begin(true); // true = format on mount failure
  return 0;
}

// Bridge task: runs forever on core 1, cycling through all ports each iteration.
// vTaskDelay(1) yields to the FreeRTOS scheduler for at least one tick (~1 ms),
// preventing the watchdog timer from triggering on a tight loop.
static void serialBridgeTask(void*) {
  for (;;) {
    for (int i = 0; i < NUM_HW_COM; i++) {
      PortHandler(i);
      COMHandler(i);
    }
    vTaskDelay(1);
  }
}

void setup() {
  for (int i = 0; i < NUM_HW_COM; i++) {
    hwserver[i] = NULL;
    i1[i] = 0;
    i2[i] = 0;
    COM[i].WriteSemaphore = xSemaphoreCreateMutex();
    COM[i].ReadSemaphore  = xSemaphoreCreateMutex();
  }

  // Two short blue pulses: visual "alive" indication before Serial is ready.
  led.begin();
  led.setBrightness(LED_BRIGHTNESS);
  for (int i = 0; i < 2; i++) {
    led.setPixelColor(0, led.Color(0, 0, 80));
    led.show();
    delay(150);
    led.clear();
    led.show();
    delay(150);
  }

  Serial.begin(115200);
  delay(100);

  if (Debug_COM != NULL) Debug_COM->println("");
  if (Debug_COM != NULL) Debug_COM->println("************************************");
  if (Debug_COM != NULL) {
    esp_reset_reason_t reason = esp_reset_reason();
    Debug_COM->print("Reset reason: ");
    switch (reason) {
      case ESP_RST_POWERON:   Debug_COM->println("Power on"); break;
      case ESP_RST_BROWNOUT:  Debug_COM->println("BROWNOUT - insufficient power"); break;
      case ESP_RST_PANIC:     Debug_COM->println("Panic / crash"); break;
      case ESP_RST_INT_WDT:   Debug_COM->println("Interrupt watchdog"); break;
      case ESP_RST_TASK_WDT:  Debug_COM->println("Task watchdog"); break;
      case ESP_RST_WDT:       Debug_COM->println("Watchdog"); break;
      case ESP_RST_DEEPSLEEP: Debug_COM->println("Deep sleep wakeup"); break;
      case ESP_RST_SW:        Debug_COM->println("Software reset"); break;
      default:                Debug_COM->printf("Unknown (%d)\n", reason); break;
    }
  }
  if (Debug_COM != NULL) Debug_COM->print("ESP32SerialBridge V");
  if (Debug_COM != NULL) Debug_COM->println(VERSION);
  if (Debug_COM != NULL) Debug_COM->println("************************************");

  loadConfig();

  COM[0].RxPin = SERIAL0_RXPIN;
  COM[0].TxPin = SERIAL0_TXPIN;
  COM[1].RxPin = SERIAL1_RXPIN;
  COM[1].TxPin = SERIAL1_TXPIN;
  COM[2].RxPin = SERIAL2_RXPIN;
  COM[2].TxPin = SERIAL2_TXPIN;

  delay(100);
  InitSerial(0);
  InitSerial(1);
  InitSerial(2);

  // Declare WiFiServer as static locals so that their constructors run here,
  // inside setup(), rather than at program startup (before main() / setup()
  // is called), which would happen for global objects and can cause crashes
  // on ESP32 due to uninitialized WiFi state.
  static WiFiServer server_0(COM[0].Port);
  static WiFiServer server_1(COM[1].Port);
  static WiFiServer server_2(COM[2].Port);
  hwserver[0] = &server_0;
  hwserver[1] = &server_1;
  hwserver[2] = &server_2;

  setup_spiffs();
  setup_wifi(NULL);
  setup_webserver(NULL);

  if (Debug_COM != NULL) Debug_COM->println("\n\n\n=====================");
  if (Debug_COM != NULL) Debug_COM->print("LOGIN USER: ");
  if (Debug_COM != NULL) Debug_COM->println(login_username);
  if (Debug_COM != NULL) Debug_COM->print("LOGIN PASS: ");
  if (Debug_COM != NULL) Debug_COM->println(login_password);
  delay(100);

  if (Debug_COM != NULL) Debug_COM->println("\n=====================");
  for (int num = 0; num < NUM_HW_COM; num++) {
    if (Debug_COM != NULL) Debug_COM->print("Starting TCP Server ");
    if (Debug_COM != NULL) Debug_COM->println(num);
    if (hwserver[num] != NULL) hwserver[num]->begin();
    delay(100);
  }
  if (Debug_COM != NULL) Debug_COM->println("=====================\n");

  // Priority 2, stack 4096 bytes, pinned to core 1.
  xTaskCreatePinnedToCore(serialBridgeTask, "SerialBridge", 4096, NULL, 2, NULL, 1);
}

void loop() {
  ElegantOTA.loop();
  wsCleanupClients();

  // Update the onboard NeoPixel based on LED timing requests set by the bridge task.
  // Blue  = COM → TCP data received (ledOffTime).
  // Red   = TCP → COM data on port 1 / ECU port (ledRedOffTime).
  // Mixed = both directions active simultaneously (OR of both colors).
  static uint32_t lastColor = 0;
  uint32_t now   = millis();
  uint32_t color = 0;
  if (now < ledOffTime)    color |= led.Color(0, 0, 80);
  if (now < ledRedOffTime) color |= led.Color(80, 0, 0);
  if (color != lastColor) {
    lastColor = color;
    led.setPixelColor(0, color);
    led.show();
  }
}
