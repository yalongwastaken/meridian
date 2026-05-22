# RPi 5 Node

Linux orchestration layer running on Raspberry Pi 5. Hosts the MQTT broker, ingests vibration data into InfluxDB, runs anomaly detection, drives a GPIO alert LED, and serves a live alert dashboard.

Built in two phases: Python first to get the full pipeline working end-to-end, then a C rewrite for the ingest and anomaly components to demonstrate production-grade embedded Linux development.

---

## Repository Structure

```
rpi5/
├── broker/
│   └── mosquitto.conf              # Mosquitto broker config
├── ingest/
│   └── subscriber.py               # MQTT → InfluxDB writer
├── anomaly/
│   └── detector.py                 # Z-score anomaly detector
├── alert/
│   ├── gpio_alert.py               # GPIO LED driver
│   └── web/
│       ├── app.py                  # Flask backend
│       ├── templates/
│       │   └── index.html          # Alert dashboard UI
│       └── static/
│           └── main.js             # Frontend logic
├── grafana/
│   └── dashboard.json              # Importable Grafana dashboard
├── systemd/
│   ├── meridian-ingest.service     # systemd unit for ingest
│   └── meridian-anomaly.service    # systemd unit for anomaly detector
├── legacy/                         # Previous implementations preserved for reference
├── requirements.txt
└── README.md
```

---

## Implementation Phases

**Phase 1 — Python (active)**
Get the full pipeline working end-to-end: broker → ingest → anomaly detection → GPIO alert → Grafana dashboard.

**Phase 2 — C rewrite**
Rewrite `ingest/` and `anomaly/` as C daemons. Add systemd service management. GPIO alert rewritten using the Linux character device API. Python implementations moved to `legacy/python/`.

**Phase 3 — Fullstack alert dashboard**
Web UI showing live alert state and history. Flask backend querying InfluxDB, frontend polling or using WebSockets.

---

## Setup

```bash
# System dependencies
sudo apt install mosquitto mosquitto-clients influxdb grafana python3-pip

# Python dependencies
pip install -r requirements.txt

# Configure and restart Mosquitto
sudo cp broker/mosquitto.conf /etc/mosquitto/mosquitto.conf
sudo systemctl restart mosquitto

# Run ingest subscriber
python ingest/subscriber.py --broker localhost --db vibration

# Run anomaly detector
python anomaly/detector.py --window 120 --threshold 3.0

# Run GPIO alert + web dashboard
python alert/gpio_alert.py --pin 17
python alert/web/app.py
```

---

## Technical Details

### Anomaly Detection
- Sliding window of last 120 `peak_hz` samples (60 minutes at 30s poll interval)
- Z-score computed against window mean and standard deviation
- Alert fires when `|z| > 3.0` for two consecutive readings
- Alert triggers GPIO LED and updates the web dashboard

### GPIO Alert
- Phase 1: Python via `RPi.GPIO` or `gpiozero`
- Phase 2: C via Linux GPIO character device API (`/dev/gpiochip0`)

### Grafana Dashboard
- Live time-series panels: `peak_hz`, `rms`, `az`
- Anomaly annotations rendered inline on the `peak_hz` panel
- Refresh interval: 5s