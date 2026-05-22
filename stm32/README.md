# STM32 Node

Bare-metal firmware for the Nucleo-F446RE. Handles hard real-time accelerometer sampling, FFT computation, and UART packet transmission.

---

## Repository Structure

```
stm32/
├── CMakeLists.txt              # Top-level build file
├── cmake/
│   └── arm-none-eabi.cmake     # Toolchain file (cross-compiler config)
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── accel.h             # ADXL345 driver interface
│   │   ├── fft.h               # CMSIS-DSP FFT wrapper
│   │   └── packet.h            # Packet serialization interface
│   └── Src/
│       ├── main.c              # Entry point, peripheral init
│       ├── accel.c             # SPI+DMA driver for ADXL345
│       ├── fft.c               # CMSIS-DSP FFT wrapper
│       └── packet.c            # Struct serialization + UART TX
├── Drivers/                    # CubeMX generated: STM32F4xx HAL + CMSIS
└── STM32F446RETx_FLASH.ld      # Linker script (CubeMX generated)
```

---

## Toolchain

| Tool | Purpose | Install |
|---|---|---|
| `arm-none-eabi-gcc` | Cross-compiler for ARM Cortex-M | `brew install --cask gcc-arm-embedded` |
| `cmake` + `ninja` | Build system | `brew install cmake ninja` |
| `STM32CubeMX` | Peripheral init codegen (one-time) | [st.com](https://www.st.com/en/development-tools/stm32cubemx.html) |
| `openocd` | Flash + debug via onboard ST-Link | `brew install openocd` |

---

## Build

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -GNinja
cmake --build build
```

### Flash

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/meridian_stm32.elf verify reset exit"
```

---

## Technical Details

### SPI + DMA
- SPI1 in master mode, CPOL=1 CPHA=1 (ADXL345 SPI mode 3), max 5 MHz
- DMA1 Channel 2 (SPI1_RX) in circular mode into a 1024-sample double buffer
- TIM2 drives the 1 kHz sample rate timebase
- DMA half-complete and complete interrupts trigger FFT processing on the inactive half
- No RTOS — all logic driven by interrupts; main loop polls a `data_ready` flag

### FFT
- `arm_rfft_fast_f32` from CMSIS-DSP, optimized for Cortex-M4
- 1024-point real FFT at 1 kHz → 0.97 Hz frequency resolution
- Hanning window applied pre-transform to reduce spectral leakage
- Peak bin search excludes DC (bin 0) and mirror alias (bins > N/2)

### UART Packet
- Packed struct serialized to JSON, terminated with `\n`
- Transmitted to ESP32 at 115200 baud
- Fields: `ax`, `ay`, `az`, `peak_hz`, `peak_amp`, `rms`, `alert`