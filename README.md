# ESP32 LoRa Water Quality Monitor

An open-source water-quality monitoring system built with two ESP32 boards and 433 MHz LoRa radios. A sensor node measures temperature, total dissolved solids (TDS), and turbidity, then transmits the readings wirelessly to a receiver. The receiver provides a live dashboard over its own Wi-Fi access point.

## Features

- Wireless sensor telemetry over 433 MHz LoRa
- DS18B20 temperature measurement
- Analog TDS and turbidity measurement
- Median filtering across 30 analog samples
- Temperature-compensated TDS calculation
- Packet sequence tracking, lost-packet count, RSSI, and SNR
- Receiver-hosted dashboard with live charts
- Captive-portal-style Wi-Fi access at `192.168.4.1`
- Remote transmitter LED control using `ON`, `OFF`, and `TOGGLE`
- No cloud service or internet connection required

## System architecture

```text
DS18B20 + TDS + turbidity sensors
                |
                v
       ESP32 sensor transmitter
                |
          SX127x / LoRa 433 MHz
                |
                v
       ESP32 LoRa receiver
                |
        Wi-Fi access point
                |
       Browser dashboard
```

## Repository layout

| Path | Description |
|---|---|
| [`transmitter_lora/Transmitter_lora.ino`](./transmitter_lora/Transmitter_lora.ino) | ESP32 sensor-node firmware |
| [`reciver_lora/reciver_lora.ino`](./reciver_lora/reciver_lora.ino) | ESP32 receiver and dashboard firmware |
| [`WIRING.md`](./WIRING.md) | Complete wiring, power, and installation guide |

> The `reciver_lora` directory name is retained for compatibility with the existing project and upload instructions.

## Hardware

### Required

- 2x ESP32 development boards
- 2x 433 MHz SX127x LoRa modules
- 1x DS18B20 temperature sensor
- 1x TDS sensor module
- 1x turbidity sensor module
- 433 MHz antennas
- 4.7 kOhm resistor for the DS18B20 data pull-up
- Voltage-divider components where required by the analog sensor outputs

### Important electrical notes

- The LoRa modules use 3.3 V power and logic. Do not connect them directly to 5 V.
- All components on each ESP32 must share a common ground.
- Never exceed the ESP32 ADC input range on GPIO34 or GPIO35.
- Connect the antenna before powering or transmitting with a LoRa module.
- Sensor power requirements vary by module; follow each sensor's datasheet.

See [`WIRING.md`](./WIRING.md) for the pin tables and detailed wiring instructions.

## Software requirements

- Arduino IDE
- ESP32 board support package
- `LoRa` by Sandeep Mistry
- `OneWire`
- `DallasTemperature`

## Installation

1. Install the ESP32 board package and the libraries listed above in Arduino IDE.
2. Wire both LoRa modules according to [`WIRING.md`](./WIRING.md).
3. Upload `transmitter_lora/Transmitter_lora.ino` to the sensor-node ESP32.
4. Upload `reciver_lora/reciver_lora.ino` to the receiver ESP32.
5. Open both Serial Monitors at **115200 baud**.
6. Confirm that both boards report `LoRa ready`.
7. Connect a phone or laptop to the receiver's `WaterQuality-Monitor` Wi-Fi network.
8. Open [http://192.168.4.1](http://192.168.4.1) in a browser.

## LoRa configuration

Both sketches must use identical radio settings:

| Setting | Value |
|---|---|
| Frequency | 433 MHz |
| Spreading factor | 7 |
| Signal bandwidth | 125 kHz |
| Coding rate | 4/5 |
| Preamble length | 8 |
| Sync word | `0x12` |
| CRC | Enabled |
| Sensor update interval | 5 seconds |

## Data protocol

The transmitter sends plain-text packets in this format:

```text
ID=42,T=25.50,TDS=318,NTU=12
```

- `ID`: incrementing packet sequence number
- `T`: temperature in degrees Celsius
- `TDS`: total dissolved solids in parts per million
- `NTU`: turbidity estimate in nephelometric turbidity units

The receiver validates the packet format, stores the most recent 60 samples, and displays the readings and radio link statistics on the dashboard.

## Receiver controls

Commands can be entered in the receiver Serial Monitor and sent to the transmitter:

```text
ON
OFF
TOGGLE
```

These commands control the transmitter status LED on GPIO15. The dashboard also exposes the same remote-control action.

## Calibration and limitations

The TDS and turbidity formulas are intended as a starting point and should be calibrated against the specific sensor modules and water samples used in deployment. Sensor voltage ranges, voltage-divider ratios, temperature compensation, and ADC behavior can vary between hardware models.

This project is suitable for monitoring and prototyping. It should not be used as the sole basis for drinking-water safety decisions without validated calibration, maintenance, and independent laboratory verification.

## Troubleshooting

### `LoRa INIT FAILED`

- Confirm 3.3 V power and common ground.
- Check SCK, MISO, MOSI, NSS, RESET, and DIO0 wiring.
- Confirm that the antenna is connected.
- Verify that the selected LoRa module is compatible with the 433 MHz frequency.

### No readings on the dashboard

- Confirm both boards use the same LoRa settings.
- Check that the transmitter Serial Monitor prints `[Sent]`.
- Check the receiver Serial Monitor for incoming packets.
- Keep the boards within a short range during initial testing.

### Incorrect analog readings

- Confirm the sensor output does not exceed the ESP32 ADC input range.
- Verify the turbidity voltage-divider ratio; the transmitter currently compensates for a 1:2 divider.
- Calibrate the sensor formulas using known reference samples.

## License

This project is provided for educational and prototyping purposes. Add the license that matches your intended distribution before publishing derived hardware or software.

