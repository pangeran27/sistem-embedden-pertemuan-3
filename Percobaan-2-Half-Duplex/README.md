# Percobaan 2 — Half Duplex Communication
# STM32 ↔ ESP32 ↔ Python GUI

## Arsitektur Sistem

```
┌──────────────┐  1 wire (Half Duplex)  ┌──────────────┐   USB Serial   ┌──────────────┐
│   STM32      │◄──────────────────────►│   ESP32       │◄─────────────►│  Python GUI  │
│  (Blue Pill) │  PA9 ↔ GPIO16         │  (Bridge)     │               │   (PC)       │
│              │  single wire 9600bd   │               │               │              │
│  4 LED       │                       │               │               │  Kontrol LED │
│  2 Button    │                       │               │               │  Status BTN  │
└──────────────┘                       └──────────────┘               └──────────────┘
```

## Perbedaan dengan Percobaan 1 (Full Duplex)

| Aspek | Full Duplex (Percobaan 1) | Half Duplex (Percobaan 2) |
|-------|--------------------------|--------------------------|
| Jumlah kabel data | 2 (TX + RX) | **1 (single wire)** |
| Pin STM32 | PA9 (TX), PA10 (RX) | **PA9 saja** |
| Pin ESP32 | GPIO16 (RX), GPIO17 (TX) | **GPIO16 saja** |
| Baud rate | 115200 | **9600** |
| Arah komunikasi | Bersamaan (simultan) | **Bergantian (request-response)** |
| Button reporting | Event-driven (langsung kirim) | **Poll-based (ESP32 minta status)** |
| UART library ESP32 | HardwareSerial (Serial2) | **EspSoftwareSerial (half-duplex)** |
| Mode UART STM32 | Normal (full duplex) | **HDSEL (half-duplex single wire)** |

## Wiring

### STM32 Blue Pill

| Pin  | Fungsi          |
|------|-----------------|
| PA0  | LED 1 (output, active HIGH)  |
| PA1  | LED 2 (output, active HIGH)  |
| PA2  | LED 3 (output, active HIGH)  |
| PA3  | LED 4 (output, active HIGH)  |
| PB0  | Button 1 (input pull-up, active LOW) |
| PB1  | Button 2 (input pull-up, active LOW) |
| PA9  | UART1 Half-Duplex ↔ ESP32 GPIO16 |

### ESP32

| Pin     | Fungsi                          |
|---------|---------------------------------|
| GPIO16  | SoftwareSerial Half-Duplex ↔ STM32 PA9 |
| USB     | Ke PC (Python GUI)              |

### Wiring Diagram

```
                    4.7kΩ
STM32 PA9 ─────┬───/\/\/──── 3.3V
               │
ESP32 GPIO16 ──┘

STM32 GND ─────────────── ESP32 GND
```

### Komponen
- **LED** (4 buah): `PA0–PA3 → Resistor 220Ω → LED → GND`
- **Push Button** (2 buah): `PB0/PB1 → Button → GND` (pull-up internal)
- **Pull-up resistor** (1 buah): `4.7kΩ` antara jalur data dan 3.3V

### Catatan Penting
- **GND STM32 dan ESP32 harus dihubungkan bersama**
- **Pull-up 4.7kΩ wajib** pada jalur data (PA9/GPIO16) ke 3.3V
- Kedua board beroperasi di 3.3V, tidak perlu level shifter
- Baud rate 9600 untuk reliability half-duplex dengan SoftwareSerial

## Protokol Komunikasi

### Request-Response (Half Duplex)
Semua komunikasi bersifat **request → response**:

| Request (ESP32 → STM32) | Response (STM32 → ESP32) |
|--------------------------|--------------------------|
| `LED1:ON\n` | `S:L1:1,L2:0,L3:0,L4:0,B1:0,B2:0\n` |
| `LED1:OFF\n` | `S:L1:0,L2:0,L3:0,L4:0,B1:0,B2:0\n` |
| `STATUS\n` | `S:L1:0,L2:0,L3:0,L4:0,B1:0,B2:0\n` |

- ESP32 polling `STATUS` setiap 150ms untuk mendapatkan status button terbaru
- STM32 **tidak pernah mengirim data secara spontan** (hanya merespons)

## Cara Menjalankan

### 1. Upload Firmware STM32
```bash
cd STM32
pio run --target upload
```

### 2. Upload Firmware ESP32
```bash
cd ESP32
pio run --target upload
```

### 3. Jalankan Python GUI
```bash
pip install pyserial
cd Python
python gui.py
```

1. Pilih COM port ESP32 di dropdown
2. Klik **Connect**
3. Klik tombol **LED 1–4** untuk toggle
4. Status **BTN 1–2** akan update otomatis (via polling)
