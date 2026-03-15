/**
 * @file main.cpp
 * @brief Percobaan 1 - Full Duplex Communication STM32 <-> ESP32
 *
 * Hardware:
 *   - 4 LED   : PA0, PA1, PA2, PA3  (active HIGH)
 *   - 2 Button: PB0 (pull-up, active LOW), PB1 (pull-up, active LOW)
 *   - UART1   : PA9 (TX) -> ESP32 RX2 (GPIO16)
 *                PA10 (RX) <- ESP32 TX2 (GPIO17)
 *
 * Protocol (text, newline-terminated):
 *   RX commands  : "LED1:ON\n", "LED1:OFF\n", ... "LED4:OFF\n", "STATUS\n"
 *   TX responses : "BTN1:1\n"  (button pressed)
 *                  "BTN1:0\n"  (button released)
 *                  "S:L1:0,L2:0,L3:0,L4:0,B1:0,B2:0\n"  (full status)
 */

#include <Arduino.h>

// ===================== PIN DEFINITIONS =====================
const uint8_t LED_PIN[4]  = { PA0, PA1, PA2, PA3 };
const uint8_t BTN_PIN[2]  = { PB0, PB1 };

// ===================== STATE VARIABLES =====================
bool ledState[4]       = { false, false, false, false };
bool btnState[2]       = { false, false };
bool lastBtnRead[2]    = { false, false };
unsigned long lastDebounce[2] = { 0, 0 };
const unsigned long DEBOUNCE_MS = 50;

String rxBuffer = "";

// ===================== SEND FULL STATUS =====================
void sendStatus() {
    // Format: S:L1:0,L2:1,L3:0,L4:1,B1:0,B2:1
    String msg = "S:";
    for (int i = 0; i < 4; i++) {
        msg += "L" + String(i + 1) + ":" + String(ledState[i] ? 1 : 0);
        msg += ",";
    }
    for (int i = 0; i < 2; i++) {
        msg += "B" + String(i + 1) + ":" + String(btnState[i] ? 1 : 0);
        if (i < 1) msg += ",";
    }
    Serial.println(msg);
}

// ===================== PROCESS RX COMMAND =====================
void processCommand(String cmd) {
    cmd.trim();

    if (cmd.startsWith("LED") && cmd.length() >= 7) {
        // "LED1:ON" or "LED3:OFF"
        int ledNum = cmd.charAt(3) - '1';           // 0..3
        String action = cmd.substring(5);            // "ON" or "OFF"

        if (ledNum >= 0 && ledNum < 4) {
            bool newState = (action == "ON");
            ledState[ledNum] = newState;
            digitalWrite(LED_PIN[ledNum], newState ? HIGH : LOW);
            sendStatus();   // acknowledge with full status
        }
    }
    else if (cmd == "STATUS") {
        sendStatus();
    }
}

// ===================== SETUP =====================
void setup() {
    // UART1 (PA9/PA10) — connects to ESP32
    Serial.begin(115200);

    // LED outputs
    for (int i = 0; i < 4; i++) {
        pinMode(LED_PIN[i], OUTPUT);
        digitalWrite(LED_PIN[i], LOW);
    }

    // Button inputs (internal pull-up)
    for (int i = 0; i < 2; i++) {
        pinMode(BTN_PIN[i], INPUT_PULLUP);
        btnState[i]    = false;
        lastBtnRead[i] = false;
    }

    // Startup blink — all LEDs flash 3×
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < 4; i++) digitalWrite(LED_PIN[i], HIGH);
        delay(150);
        for (int i = 0; i < 4; i++) digitalWrite(LED_PIN[i], LOW);
        delay(150);
    }

    delay(300);
    Serial.println("STM32_READY");
}

// ===================== LOOP =====================
void loop() {
    // ---------- 1. Read incoming UART commands ----------
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            processCommand(rxBuffer);
            rxBuffer = "";
        } else if (c != '\r') {
            rxBuffer += c;
        }
    }

    // ---------- 2. Debounce & report button changes ----------
    for (int i = 0; i < 2; i++) {
        bool reading = !digitalRead(BTN_PIN[i]);   // active LOW -> true when pressed

        if (reading != lastBtnRead[i]) {
            lastDebounce[i] = millis();
        }

        if ((millis() - lastDebounce[i]) > DEBOUNCE_MS) {
            if (reading != btnState[i]) {
                btnState[i] = reading;
                // Send change notification
                Serial.println("BTN" + String(i + 1) + ":" + String(btnState[i] ? 1 : 0));
            }
        }

        lastBtnRead[i] = reading;
    }
}
