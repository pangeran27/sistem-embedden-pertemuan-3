/**
 * @file main.cpp
 * @brief ESP32 Full Duplex UART Bridge
 *
 * Meneruskan data secara transparan (full duplex) antara:
 *   - USB Serial  (ke Python GUI di PC)
 *   - UART2       (ke STM32 Blue Pill)
 *
 * Wiring ESP32 <-> STM32:
 *   ESP32 GPIO16 (RX2)  <--  STM32 PA9  (TX1)
 *   ESP32 GPIO17 (TX2)  -->  STM32 PA10 (RX1)
 *   GND          <-->  GND
 *
 * Catatan: Pastikan level tegangan cocok (3.3 V kedua sisi).
 */

#include <Arduino.h>

#define STM32_RX  16   // ESP32 RX2 <- STM32 TX
#define STM32_TX  17   // ESP32 TX2 -> STM32 RX

void setup() {
    // USB Serial — terhubung ke PC / Python GUI
    Serial.begin(115200);

    // UART2 — terhubung ke STM32
    Serial2.begin(115200, SERIAL_8N1, STM32_RX, STM32_TX);

    delay(500);
    Serial.println("ESP32_BRIDGE_READY");
}

void loop() {
    // -------- USB -> STM32 --------
    while (Serial.available()) {
        Serial2.write(Serial.read());
    }

    // -------- STM32 -> USB --------
    while (Serial2.available()) {
        Serial.write(Serial2.read());
    }
}
