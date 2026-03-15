/**
 * @file main.cpp
 * @brief Percobaan 2 - Half Duplex Communication STM32 <-> ESP32
 *
 * Hardware:
 *   - 4 LED   : PA0, PA1, PA2, PA3  (active HIGH)
 *   - 2 Button: PB0 (pull-up, active LOW), PB1 (pull-up, active LOW)
 *   - UART1 Half-Duplex: PA9 (single wire) <-> ESP32 GPIO16
 *     Resistor pull-up 4.7kΩ pada jalur data ke 3.3V
 *
 * Protocol (text, newline-terminated, request-response):
 *   RX commands  : "LED1:ON\n", "LED1:OFF\n", ... "LED4:OFF\n", "STATUS\n"
 *   TX responses : "S:L1:0,L2:0,L3:0,L4:0,B1:0,B2:0\n"  (always full status)
 *
 * Half-Duplex:
 *   STM32 USART1 dikonfigurasi dalam mode half-duplex (single wire).
 *   Hanya pin PA9 (TX) yang digunakan — pin ini berfungsi sebagai TX dan RX
 *   secara bergantian. Hardware USART otomatis mengatur pergantian arah.
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

// ===================== ENABLE HALF DUPLEX =====================
void enableHalfDuplex() {
    // 1. Disable USART1
    USART1->CR1 &= ~USART_CR1_UE;

    // 2. Enable half-duplex selection (HDSEL bit in CR3)
    USART1->CR3 |= USART_CR3_HDSEL;

    // 3. Configure PA9 as Alternate Function Open-Drain
    //    CRH register: PA9 is bits [7:4]
    //    Mode=11 (50MHz), CNF=11 (AF open-drain) -> 0xF
    GPIOA->CRH = (GPIOA->CRH & ~(0xFUL << 4)) | (0xFUL << 4);

    // 4. Re-enable USART1
    USART1->CR1 |= USART_CR1_UE;
}

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
    delay(2);           // Beri jeda agar jalur stabil sebelum TX
    Serial.println(msg);
    Serial.flush();     // Tunggu TX selesai sebelum lepas jalur

    // Buang echo — dalam half-duplex mode, data yang dikirim
    // juga masuk ke RX (karena TX=RX di pin yang sama)
    delay(2);
    while (Serial.available()) Serial.read();
}

// ===================== PROCESS RX COMMAND =====================
void processCommand(String cmd) {
    cmd.trim();

    // DEBUG: kedipkan LED PA3 setiap terima command (bukti data masuk)
    digitalWrite(LED_PIN[3], HIGH);
    delay(50);
    digitalWrite(LED_PIN[3], LOW);

    if (cmd.startsWith("LED") && cmd.length() >= 7) {
        // "LED1:ON" or "LED3:OFF"
        int ledNum = cmd.charAt(3) - '1';           // 0..3
        String action = cmd.substring(5);            // "ON" or "OFF"

        if (ledNum >= 0 && ledNum < 4) {
            bool newState = (action == "ON");
            ledState[ledNum] = newState;
            digitalWrite(LED_PIN[ledNum], newState ? HIGH : LOW);
        }
    }

    // Selalu respond dengan full status untuk command yang valid
    if (cmd == "STATUS" || cmd.startsWith("LED")) {
        sendStatus();
    }
}

// ===================== SETUP =====================
void setup() {
    // USART1 (PA9/PA10) — baud 9600 untuk half-duplex reliability
    Serial.begin(9600);

    // Switch ke half-duplex mode (hanya PA9 yang dipakai)
    enableHalfDuplex();

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

    // ---------- 2. Debounce buttons (state dilaporkan saat di-poll) ----------
    for (int i = 0; i < 2; i++) {
        bool reading = !digitalRead(BTN_PIN[i]);   // active LOW -> true when pressed

        if (reading != lastBtnRead[i]) {
            lastDebounce[i] = millis();
        }

        if ((millis() - lastDebounce[i]) > DEBOUNCE_MS) {
            btnState[i] = reading;
        }

        lastBtnRead[i] = reading;
    }
}
