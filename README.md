# Meridian

A multi-node embedded systems project spanning bare-metal STM32, FreeRTOS on ESP32, and Linux-side orchestration on a Raspberry Pi 5. The system performs real-time vibration analysis on a physical machine or motor, ships frequency-domain data over WiFi/MQTT, and runs anomaly detection with live dashboarding.

Each platform is chosen for a justified reason — not just to use all three.

---

## Architecture

```
┌─────────────────────┐        UART        ┌──────────────────────┐      MQTT/WiFi       ┌─────────────────────┐
│     STM32 Node      │ ─────────────────► │     ESP32 Node       │ ───────────────────► │     RPi 5 Node      │
│  (bare-metal)       │                    │  (FreeRTOS)          │                      │  (Linux)            │
│                     │                    │                      │                      │                     │
│  • SPI+DMA @ 1 kHz  │                    │  • UART RX task      │                      │  • Mosquitto broker │
│  • CMSIS-DSP FFT    │                    │  • FreeRTOS queue    │                      │  • InfluxDB ingest  │
│  • Peak + RMS detect│                    │  • MQTT publish task │                      │  • Anomaly detect   │
│  • UART TX packet   │                    │  • Light sleep mgmt  │                      │  • Grafana dashboard│
└─────────────────────┘                    └──────────────────────┘                      └─────────────────────┘
         ▲
         │ SPI
┌─────────────────────┐
│   ADXL345           │
│   Accelerometer     │
└─────────────────────┘
```

---

## Platform Roles

**STM32 — Hard Real-Time Sensor Engine**
Bare-metal firmware. SPI+DMA streams accelerometer data at exactly 1 kHz into a circular buffer without CPU involvement. A timer interrupt triggers a CMSIS-DSP FFT every 1024 samples. Peak frequency and RMS amplitude are packed into a struct and transmitted over UART.

**ESP32 — Wireless Bridge**
FreeRTOS with two tasks: a UART receive task that queues incoming packets, and a WiFi/MQTT publish task that forwards them to the broker. Light sleep between publish cycles. Reconnection logic runs without blocking the receive task.

**Raspberry Pi 5 — Orchestration Layer**
Mosquitto broker, InfluxDB ingest, z-score anomaly detection, and Grafana dashboarding. General-purpose Linux handles what bare-metal and RTOS nodes shouldn't.

---

## Hardware

| Component | Part |
|---|---|
| STM32 | Nucleo-F446RE |
| ESP32 | ESP32-WROOM-32E DevKit |
| Accelerometer | ADXL345 breakout |
| Linux board | Raspberry Pi 5 |

---

## MQTT Packet Schema

```json
{
  "node_id": "stm32-01",
  "ts": 1718000000,
  "ax": 0.12,
  "ay": -0.04,
  "az": 9.81,
  "peak_hz": 47.3,
  "peak_amp": 0.34,
  "rms": 0.18,
  "alert": false
}
```

| Field | Source | Description |
|---|---|---|
| `node_id` | ESP32 | Identifies the sensor node |
| `ts` | ESP32 (NTP) | Unix timestamp at publish time |
| `ax/ay/az` | STM32 | Raw accelerometer readings in g |
| `peak_hz` | STM32 (FFT) | Dominant vibration frequency |
| `peak_amp` | STM32 (FFT) | Amplitude at peak frequency |
| `rms` | STM32 | RMS of raw acceleration magnitude |
| `alert` | STM32 | Threshold flag set on-device |

---

## Repository Structure

```
meridian/
├── stm32/       # Bare-metal STM32 firmware (Nucleo-F446RE)
├── esp32/       # FreeRTOS ESP32 firmware (WROOM-32E)
├── rpi5/        # Linux orchestration layer (Raspberry Pi 5)
├── docs/
│   ├── wiring_diagram.png
│   └── architecture.png
└── README.md
```

See each subdirectory for setup, build instructions, and technical details.