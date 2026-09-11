// uart.cpp — UART hardware initialization and thread-safe I/O
//
// Purpose: implement the functions declared in uart.h.
//
// Thread-safety model: the bridge task (FreeRTOS, core 1) and the web server
//   task (core 0) can both call COM_read/COM_write. Each UART has a pair of
//   FreeRTOS mutexes (COM[n].ReadSemaphore, COM[n].WriteSemaphore) created in
//   main.cpp setup() before either task starts. Every read and write acquires
//   the relevant mutex with portMAX_DELAY, performs the operation, then
//   releases it.
#include "../globals.h"

void InitSerial(int No) {
  // Build the UART config word expected by HardwareSerial::begin().
  // The base value 0x8000000 selects "no hardware flow control" in the
  // ESP32 Arduino UART driver (UART_HW_FLOWCTRL_DISABLE). Data bits,
  // stop bits, and parity are OR-ed in using the masks from config.h.
  unsigned long ulParam = 0x8000000;

  switch (COM[No].Data) {
    case 5:  ulParam |= UART_5_BIT; break;
    case 6:  ulParam |= UART_6_BIT; break;
    case 7:  ulParam |= UART_7_BIT; break;
    default:
    case 8:  ulParam |= UART_8_BIT; break;
  }

  switch (COM[No].Stop) {
    default:
    case 1:  ulParam |= UART_1_STOPBIT; break;
    case 2:  ulParam |= UART_2_STOPBIT; break;
  }

  switch (COM[No].Parity) {
    default:
    case 'N':  ulParam |= UART_NO_PARITY;   break;
    case 'O':  ulParam |= UART_ODD_PARITY;  break;  // O = Odd
    case 'E':  ulParam |= UART_EVEN_PARITY; break;  // E = Even
  }

  // Pull both pins high before calling begin() so the UART line idles at
  // the correct logic level and does not generate spurious break conditions.
  pinMode(COM[No].RxPin, INPUT_PULLUP);
  pinMode(COM[No].TxPin, INPUT_PULLUP);

  HW_SERIAL[No]->begin(COM[No].Baudrate, ulParam, COM[No].RxPin, COM[No].TxPin);

  if (Debug_COM != NULL)
    Debug_COM->printf("Open COM%u: %uBd %1u%c%1u <==> Port:%u\n",
      No, COM[No].Baudrate, COM[No].Data, COM[No].Parity, COM[No].Stop, COM[No].Port);
}

bool COM_available(uint8_t num) {
  return HW_SERIAL[num]->available();
}

int COM_read(uint8_t num) {
  xSemaphoreTake(COM[num].ReadSemaphore, portMAX_DELAY);
  int n = HW_SERIAL[num]->read();
  xSemaphoreGive(COM[num].ReadSemaphore);
  return n;
}

size_t COM_write(uint8_t num, const uint8_t* buf, uint16_t len) {
  xSemaphoreTake(COM[num].WriteSemaphore, portMAX_DELAY);
  size_t n = HW_SERIAL[num]->write(buf, len);
  xSemaphoreGive(COM[num].WriteSemaphore);
  return n;
}
