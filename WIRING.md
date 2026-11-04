# ESP32 LoRa Water-Quality Monitor Wiring

This project uses two ESP32 boards:

- **Transmitter / sensor node**: reads the DS18B20 temperature sensor, TDS sensor, and turbidity sensor, then sends measurements over a 433 MHz LoRa module.
- **Receiver**: receives the measurements and hosts the `WaterQuality-Monitor` Wi-Fi dashboard at `http://192.168.4.1`.

The transmitter and receiver use the same ESP32-to-LoRa wiring and the same LoRa radio settings. The receiver does not need the water-quality sensors.

## ESP32 to SX127x LoRa module

Use a 3.3 V-compatible SX1278/SX1276 module. Connect the module on **both** ESP32 boards as follows:

| LoRa module pin | ESP32 pin | Purpose |
|---|---:|---|
| VCC | 3V3 | 3.3 V power only |
| GND | GND | Common ground |
| SCK | GPIO18 | SPI clock |
| MISO | GPIO19 | SPI data from LoRa module |
| MOSI | GPIO23 | SPI data to LoRa module |
| NSS / CS | GPIO5 | SPI chip select |
| RESET | GPIO14 | LoRa reset |
| DIO0 | GPIO4 | Receive interrupt/status |
| ANT | 433 MHz antenna | Connect an appropriate antenna before transmitting |

Do not connect a LoRa module's 5 V output or 5 V logic to the ESP32. Keep wires short, use a stable 3.3 V supply, and connect the antenna before initializing or transmitting the radio.

## Transmitter sensor wiring

Connect these sensors only to the **transmitter ESP32**:

### DS18B20 temperature sensor

| DS18B20 wire/pin | ESP32 |
|---|---:|
| VDD | 3V3 |
| GND | GND |
| DATA | GPIO27 |

Add a **4.7 kOhm pull-up resistor** between DATA (GPIO27) and 3V3. For a waterproof three-wire probe, red is normally VDD, black is GND, and yellow/white is DATA; verify the sensor's datasheet before powering it.

### TDS sensor

| TDS module output | ESP32 |
|---|---:|
| VCC | Sensor-rated supply |
| GND | GND |
| Analog output | GPIO34 |

The sketch reads GPIO34 with 11 dB ADC attenuation and converts the measured voltage to TDS. Confirm that the module's analog output never exceeds the ESP32 ADC input limit. Use a voltage divider or level shifter if required by the module.

### Turbidity sensor

| Turbidity module output | ESP32 |
|---|---:|
| VCC | Sensor-rated supply |
| GND | GND |
| Analog output | GPIO35 |

The sketch reads GPIO35 and multiplies the ADC voltage by 2 because it expects a 1:2 voltage divider on this input. The divider must keep the ESP32 pin within its safe input range.

## Status LED

The built-in/external status LED control is connected to **GPIO15** on both sketches:

| LED connection | ESP32 |
|---|---:|
| GPIO15 through 220-330 Ohm resistor | LED anode |
| LED cathode | GND |

The receiver can send `ON`, `OFF`, or `TOGGLE` commands from its Serial Monitor to control the transmitter LED.

## Power and grounding

1. Connect all grounds on a board together: ESP32, LoRa module, and sensors.
2. Power the LoRa module from 3.3 V, not 5 V.
3. Use a separate, adequately rated sensor supply when a sensor module requires more current or a different voltage.
4. Ensure every analog signal reaching GPIO34 or GPIO35 is within the ESP32 ADC input range.
5. Keep the LoRa antenna attached and away from metal or the ESP32 power wiring.

## Software setup

Install the ESP32 board package in Arduino IDE and install these libraries:

- `LoRa` by Sandeep Mistry
- `OneWire`
- `DallasTemperature`

Upload `transmitter_lora/Transmitter_lora.ino` to the sensor-node ESP32 and `reciver_lora/reciver_lora.ino` to the receiver ESP32. Select an ESP32 board matching the hardware and use a 115200 baud Serial Monitor.

The two sketches must retain matching LoRa settings:

- Frequency: 433 MHz
- Spreading factor: 7
- Bandwidth: 125 kHz
- Coding rate: 4/5
- Preamble length: 8
- Sync word: `0x12`
- CRC: enabled

After uploading:

1. Power the transmitter and receiver with their antennas connected.
2. Confirm `LoRa ready` in both Serial Monitors.
3. Connect a phone or laptop to the receiver Wi-Fi network `WaterQuality-Monitor`.
4. Open `http://192.168.4.1` if the captive portal does not open automatically.
