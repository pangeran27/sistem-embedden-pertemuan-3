/**
 * @file main.cpp
 * @brief ESP32 Half Duplex UART Bridge
 *
 * Wiring ESP32 <-> STM32 (semua ke 1 kabel data):
 *   ESP32 TX2 (GPIO17) ──┬── STM32 PA9
 *   ESP32 RX2 (GPIO16) ──┘
 *   Pull-up 10kΩ dari jalur data ke 3.3V
 *   GND <--> GND
 *
 * Half-Duplex Strategy:
 *   TX2 di-set HIGH-Z (input) saat idle → tidak mengganggu jalur
 *   TX2 di-reconnect ke UART hanya saat kirim data
 *   RX2 selalu listen
 *   Echo (TX loopback ke RX) di-discard berdasarkan jumlah byte
 */

#include <Arduino.h>

#define TX_PIN  17
#define RX_PIN  16

const unsigned long POLL_INTERVAL_MS = 300;
unsigned long lastPoll = 0;

String usbRxBuffer = "";
String lastStatus  = "";

// ---- Matikan TX (set HIGH-Z / input) supaya tidak ganggu bus ----
void txDisable() {
    pinMode(TX_PIN, INPUT);  // High-Z, pull-up eksternal jaga bus HIGH
}

// ---- Nyalakan TX (reconnect ke UART2) ----
void txEnable() {
    Serial2.setPins(-1, TX_PIN);  // Reconnect TX pin ke UART2 peripheral
}

// ===================== KIRIM + BACA RESPONSE =====================
String sendAndReceive(const String& cmd, unsigned long timeoutMs = 600) {
    String data = cmd + "\n";

    // 1. Flush RX buffer
    while (Serial2.available()) Serial2.read();

    // 2. Enable TX, kirim data
    txEnable();
    Serial2.print(data);
    Serial2.flush();

    // 3. Disable TX -> High-Z, biarkan STM32 jawab
    txDisable();

    // 4. Discard echo dari TX
    //    (TX2 & RX2 di kabel sama, jadi data kirim masuk ke RX)
    size_t echoBytes = data.length();
    size_t discarded = 0;
    unsigned long echoStart = millis();
    while (discarded < echoBytes && (millis() - echoStart) < 200) {
        if (Serial2.available()) {
            Serial2.read();
            discarded++;
        }
    }

    // 5. Baca response dari STM32
    String response = "";
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        if (Serial2.available()) {
            char c = (char)Serial2.read();
            if (c == '\n') {
                return response;
            } else if (c != '\r') {
                response += c;
            }
        }
    }
    return response;
}

// ===================== FORWARD KE STM32 =====================
void forwardToSTM32(const String& cmd, bool alwaysForward = true) {
    Serial.print("[DBG] TX->STM: ");
    Serial.println(cmd);

    String resp = sendAndReceive(cmd);

    Serial.print("[DBG] RX<-STM: '");
    Serial.print(resp);
    Serial.print("' len=");
    Serial.println(resp.length());

    if (resp.length() > 0) {
        if (alwaysForward) {
            Serial.println(resp);
        } else {
            if (resp != lastStatus) {
                lastStatus = resp;
                Serial.println(resp);
            }
        }
    }
}

void setup() {
    Serial.begin(115200);

    // RX2 selalu aktif, TX2 juga di-init dulu
    Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    // Langsung disable TX -> bus bebas, pull-up jaga HIGH
    txDisable();

    delay(500);
    Serial.println("ESP32_HALFDUPLEX_READY");

    // Tunggu STM32 boot
    delay(2000);
    forwardToSTM32("STATUS", true);
}

void loop() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            usbRxBuffer.trim();
            if (usbRxBuffer.length() > 0) {
                forwardToSTM32(usbRxBuffer, true);
            }
            usbRxBuffer = "";
        } else if (c != '\r') {
            usbRxBuffer += c;
        }
    }

    if (millis() - lastPoll >= POLL_INTERVAL_MS) {
        lastPoll = millis();
        forwardToSTM32("STATUS", false);
    }
}
