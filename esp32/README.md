# ESP32 Node

FreeRTOS firmware for the ESP32-WROOM-32E. Receives UART packets from the STM32, queues them, and publishes over MQTT. Manages light sleep between publish cycles.

---

## Repository Structure

```
esp32/
├── CMakeLists.txt
├── components/
│   ├── uart_rx/
│   │   ├── CMakeLists.txt
│   │   ├── uart_rx.h           # Task interface: function, stack, priority
│   │   └── uart_rx.c           # UART receive task + queue push
│   ├── mqtt_pub/
│   │   ├── CMakeLists.txt
│   │   ├── mqtt_pub.h          # Task interface: function, stack, priority
│   │   └── mqtt_pub.c          # WiFi connect + MQTT publish task
│   └── sleep/
│       ├── CMakeLists.txt
│       ├── sleep.h             # Sleep interface
│       └── sleep.c             # Light sleep management
└── main/
    └── main.c                  # app_main: queue creation, task spawn
```

---

## Toolchain

| Tool | Purpose | Install |
|---|---|---|
| ESP-IDF v5.x | SDK + build system | [docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) |

---

## Build + Flash

```bash
idf.py set-target esp32
idf.py menuconfig       # Set WiFi SSID/password, MQTT broker IP
idf.py build flash monitor
```

---

## Technical Details

### FreeRTOS Task Design

| Task | Priority | Stack | Description |
|---|---|---|---|
| `uart_rx_task` | 5 | TBD | Reads UART bytes, assembles packets on `\n`, pushes to queue |
| `mqtt_pub_task` | 3 | TBD | Blocks on queue, connects WiFi, publishes JSON to broker |

- Inter-task queue: `xPacketQueue`, depth 12, item size `sizeof(mqtt_message_t)`
- Light sleep entered when queue is empty and WiFi is idle
- Wake source: UART RX line activity

### MQTT Topic
```
vibration/stm32-01/data
```