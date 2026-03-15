# Percobaan 1 — Full Duplex Communication
# STM32 ↔ ESP32 ↔ Python GUI

## Arsitektur Sistem

```
┌──────────────┐   UART (2 pin)   ┌──────────────┐   USB Serial   ┌──────────────┐
│   STM32      │◄────────────────►│   ESP32       │◄─────────────►│  Python GUI  │
│  (Blue Pill) │  PA9 TX → RX2   │  (Bridge)     │               │   (PC)       │
│              │  PA10 RX ← TX2  │               │               │              │
│  4 LED       │  GPIO16, GPIO17 │               │               │  Kontrol LED │
│  2 Button    │                 │               │               │  Status BTN  │
└──────────────┘                 └──────────────┘               └──────────────┘
```

## Wiring

### STM32 Blue Pill

| Pin  | Fungsi          |
|------|-----------------|
| PA0  | LED 1 (output)  |
| PA1  | LED 2 (output)  |
| PA2  | LED 3 (output)  |
| PA3  | LED 4 (output)  |
| PB0  | Button 1 (input pull-up, active LOW) |
| PB1  | Button 2 (input pull-up, active LOW) |
| PA9  | UART1 TX → ESP32 GPIO16 (RX2) |
| PA10 | UART1 RX ← ESP32 GPIO17 (TX2) |

### ESP32

| Pin     | Fungsi                      |
|---------|-----------------------------|
| GPIO16  | RX2 ← STM32 PA9 (TX)       |
| GPIO17  | TX2 → STM32 PA10 (RX)      |
| USB     | Ke PC (Python GUI)          |

### Catatan
- **GND STM32 dan ESP32 harus dihubungkan bersama**
- Kedua board beroperasi di 3.3V, tidak perlu level shifter

## Protokol Komunikasi

### PC → STM32 (via ESP32)
| Command       | Fungsi             |
|---------------|--------------------|
| `LED1:ON\n`   | Nyalakan LED 1     |
| `LED1:OFF\n`  | Matikan LED 1      |
| `LED2:ON\n`   | Nyalakan LED 2     |
| ...           | ...                |
| `LED4:OFF\n`  | Matikan LED 4      |
| `STATUS\n`    | Request status     |

### STM32 → PC (via ESP32)
| Response                              | Fungsi              |
|---------------------------------------|---------------------|
| `BTN1:1\n`                            | Button 1 ditekan    |
| `BTN1:0\n`                            | Button 1 dilepas    |
| `BTN2:1\n`                            | Button 2 ditekan    |
| `BTN2:0\n`                            | Button 2 dilepas    |
| `S:L1:0,L2:1,L3:0,L4:0,B1:0,B2:0\n` | Full status reply   |

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
4. Status **BTN 1–2** akan update otomatis saat button ditekan/dilepas
