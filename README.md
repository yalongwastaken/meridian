# Meridian — Distributed Vibration & Motion Health Monitor

A multi-node embedded systems project spanning bare-metal STM32, FreeRTOS on ESP32, and Linux-side orchestration on a Raspberry Pi. The system performs real-time vibration analysis on a physical machine or motor, ships frequency-domain data over WiFi/MQTT, and runs anomaly detection with live dashboarding.

Each platform is chosen for a justified reason — not just to use all three.

---

## System Overview

```
┌─────────────────────┐        UART        ┌─────────────────────┐       MQTT/WiFi      ┌─────────────────────┐
│     STM32 Node      │ ─────────────────► │     ESP32 Node      │ ───────────────────► │     Linux Node      │
│  (bare-metal)       │                    │  (FreeRTOS)         │                      │  (Raspberry Pi)     │
│                     │                    │                      │                      │                     │
│  • SPI+DMA @ 1 kHz  │                    │  • UART RX task      │                      │  • Mosquitto broker │
│  • CMSIS-DSP FFT    │                    │  • FreeRTOS queue    │                      │  • InfluxDB ingest  │
│  • Peak + RMS detect│                    │  • MQTT publish task │                      │  • Anomaly detect   │
│  • UART TX packet   │                    │  • Light sleep mgmt  │                      │  • Grafana dashboard│
└─────────────────────┘                    └─────────────────────┘                      └─────────────────────┘
         ▲
         │ SPI
┌─────────────────────┐
│   ADXL345 / MPU-6050│
│   Accelerometer     │
└─────────────────────┘
```

---

## Platform Roles

### STM32 — Hard Real-Time Sensor Engine
The STM32 runs bare-metal (no OS). It configures SPI with DMA to stream raw accelerometer data into a circular buffer at exactly 1 kHz without CPU involvement. A timer interrupt fires every 1024 samples and triggers a CMSIS-DSP FFT. The dominant frequency peak and RMS amplitude are extracted and packed into a struct that is transmitted over UART to the ESP32.

**Why STM32 here:** Any jitter in the sampling rate corrupts frequency-domain results. Linux cannot provide this guarantee. FreeRTOS could, but bare-metal is the cleanest proof of hard real-time capability.

### ESP32 — Wireless Bridge with Power Management
The ESP32 runs FreeRTOS with two tasks: a UART receive task that pulls packets into a queue, and a WiFi/MQTT publish task that forwards them to the Linux broker. Between publish cycles, the modem enters light sleep. Local buffering handles transient WiFi drops. Reconnection logic is handled without blocking the receive task.

**Why ESP32 here:** Wireless belongs on a dedicated radio-capable SoC. The ESP32 handles 802.11 stack complexity so the STM32 doesn't have to, and FreeRTOS task/queue separation mirrors production IoT patterns directly.

### Raspberry Pi — Orchestration and Intelligence Layer
Mosquitto runs as the MQTT broker. A Python subscriber writes incoming packets to InfluxDB. A second Python process queries the DB every 30 seconds, runs a z-score anomaly detector on the `peak_hz` time series, and triggers an alert (GPIO LED, webhook, or Grafana annotation) on significant deviation. Grafana serves the live dashboard.

**Why Linux here:** Time-series databases, dashboards, and ML inference all belong on a general-purpose OS. The RPi coordinates the system without imposing real-time constraints on the nodes that need them.

---

## Hardware

| Component | Part | Notes |
|---|---|---|
| STM32 | STM32F103C8T6 (Blue Pill) or Nucleo-F401RE | Either works; Nucleo is easier for first bring-up |
| ESP32 | ESP32 DevKit v1 | ~$8 |
| Accelerometer | ADXL345 or MPU-6050 breakout | ADXL345 preferred for SPI simplicity |
| Linux board | Raspberry Pi (any model with Ethernet) | Or any SBC running Linux |
| Misc | Logic level shifter, breadboard, jumpers | STM32 and ESP32 are both 3.3V |

Estimated BOM cost: $35–50 (excluding RPi if already owned).

---

## MQTT Packet Schema

All frequency-domain computation happens on the STM32. The ESP32 forwards packets without modification.

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
| `node_id` | ESP32 (injected) | Identifies the physical sensor node |
| `ts` | ESP32 (NTP) | Unix timestamp at publish time |
| `ax/ay/az` | STM32 | Raw accelerometer readings in g |
| `peak_hz` | STM32 (FFT) | Dominant vibration frequency |
| `peak_amp` | STM32 (FFT) | Amplitude at peak frequency |
| `rms` | STM32 | RMS of raw acceleration magnitude |
| `alert` | STM32 | Threshold flag set on-device |

---

## Build Phases

Each phase is scoped to a single weekend session.

| Phase | Goal | Key Deliverable |
|---|---|---|
| 1 | STM32 SPI + DMA bring-up | Raw accelerometer reads; timing verified on scope or logic analyzer |
| 2 | CMSIS-DSP FFT on STM32 | FFT output over UART to terminal; tap a desk to see frequency response |
| 3 | ESP32 UART RX + FreeRTOS queue | Packets decoded and printed to serial monitor |
| 4 | ESP32 WiFi + MQTT publish | Packets visible in MQTT Explorer on laptop |
| 5 | Linux broker + InfluxDB ingest | Data flowing into DB; queryable via CLI |
| 6 | Grafana dashboard | Live time-series charts for `peak_hz`, `rms`, `az` |
| 7 | Python anomaly detection | Alert fires when vibration profile changes (test by tapping vs. not) |
| 8 | Polish | Deep sleep power modes, error handling, wiring diagram, this README |

---

## Repository Structure

```
meridian/
├── stm32/
│   ├── core/
│   │   ├── src/
│   │   │   ├── main.c              # Entry, peripheral init
│   │   │   ├── accel.c             # SPI+DMA driver for ADXL345
│   │   │   ├── fft.c               # CMSIS-DSP FFT wrapper
│   │   │   └── packet.c            # Struct serialization + UART TX
│   │   └── inc/
│   ├── drivers/                    # STM32 HAL + CMSIS-DSP
│   └── STM32F103C8TX_FLASH.ld
├── esp32/
│   ├── main/
│   │   ├── main.c                  # FreeRTOS task init
│   │   ├── uart_rx.c               # UART receive task + queue
│   │   ├── mqtt_pub.c              # WiFi connect + MQTT publish task
│   │   └── sleep.c                 # Light sleep management
│   ├── CMakeLists.txt
│   └── sdkconfig
├── linux/
│   ├── broker/
│   │   └── mosquitto.conf
│   ├── ingest/
│   │   └── subscriber.py           # MQTT → InfluxDB writer
│   ├── anomaly/
│   │   └── detector.py             # Z-score detector + alert webhook
│   ├── grafana/
│   │   └── dashboard.json          # Exportable Grafana dashboard
│   └── requirements.txt
├── docs/
│   ├── wiring_diagram.png
│   └── architecture.png
└── README.md
```

---

## Key Technical Details

### STM32: SPI + DMA Configuration
- SPI1 in master mode, CPOL=1 CPHA=1 (ADXL345 SPI mode 3)
- DMA1 Channel 2 (SPI1_RX) in circular mode into a 1024-sample double buffer
- TIM2 used as sample-rate timebase; DMA half-complete and complete interrupts trigger FFT processing on the inactive half
- No FreeRTOS — all logic driven by interrupts and the main loop polling a `data_ready` flag

### STM32: FFT
- `arm_rfft_fast_f32` from CMSIS-DSP (optimized for Cortex-M)
- 1024-point real FFT at 1 kHz sample rate → 0.97 Hz frequency resolution
- Hanning window applied before transform to reduce spectral leakage
- Peak bin search excludes DC (bin 0) and the alias mirror (bins > N/2)

### ESP32: FreeRTOS Task Design
- `uart_rx_task` (priority 5): reads UART bytes into a ring buffer, assembles packets on newline delimiter, pushes to `xPacketQueue` (depth 16)
- `mqtt_pub_task` (priority 3): blocks on `xPacketQueue`, connects/reconnects WiFi, publishes JSON to `vibration/stm32-01/data`
- Light sleep entered when queue is empty and WiFi is idle; wake triggered by UART RX line activity

### Linux: Anomaly Detection
- Sliding window of last 120 `peak_hz` samples (60 minutes at 30s intervals)
- Z-score computed against window mean and std
- Alert fires when `|z| > 3.0` for two consecutive readings
- Alerts POST to a configurable webhook URL (Slack, Home Assistant, etc.) and write an annotation to Grafana via its HTTP API

---

## Getting Started

### Prerequisites
- STM32CubeIDE or CLion + OpenOCD for STM32 flashing
- ESP-IDF v5.x for ESP32
- Raspberry Pi running Raspberry Pi OS Lite (64-bit recommended)
- Python 3.10+ on the RPi

### Quickstart

```bash
# 1. Flash STM32 (from stm32/ directory)
# Open in STM32CubeIDE, build, flash via ST-Link

# 2. Flash ESP32 (from esp32/ directory)
idf.py set-target esp32
idf.py menuconfig   # set WiFi SSID/password and MQTT broker IP
idf.py build flash monitor

# 3. Set up Linux node (on RPi)
sudo apt install mosquitto mosquitto-clients influxdb grafana
pip install -r linux/requirements.txt

# Configure Mosquitto
sudo cp linux/broker/mosquitto.conf /etc/mosquitto/mosquitto.conf
sudo systemctl restart mosquitto

# Start ingest subscriber
python linux/ingest/subscriber.py --broker localhost --db vibration

# Start anomaly detector
python linux/anomaly/detector.py --window 120 --threshold 3.0 --webhook $WEBHOOK_URL

# 4. Import Grafana dashboard
# Navigate to Grafana → Dashboards → Import → upload linux/grafana/dashboard.json
```

---

## Skills Demonstrated

- **Bare-metal STM32:** SPI peripheral configuration, DMA circular buffers, timer-driven interrupt design, no-OS firmware architecture
- **Signal processing:** Real FFT on Cortex-M using CMSIS-DSP, windowing, spectral peak detection
- **FreeRTOS:** Task design, inter-task queues, priority assignment, low-power idle strategy
- **IoT protocols:** MQTT pub/sub, broker configuration, topic design, JSON serialization at the edge
- **Linux systems:** Process-level service architecture, time-series database ingest, REST API integration
- **Observability:** Grafana dashboard design, anomaly detection, alerting pipeline