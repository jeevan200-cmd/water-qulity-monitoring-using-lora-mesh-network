#include <SPI.h>
#include <LoRa.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DEVICE_NAME "Sensor Node"

// LoRa pins
#define LORA_SCK  18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 4
#define LED_PIN   15
#define LORA_FREQUENCY 433E6

// Sensor pins
#define ONE_WIRE_PIN 27
#define TDS_PIN      34
#define TURB_PIN     35
#define SAMPLES      30

#define SEND_INTERVAL 5000   // ms between packets

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

float lastTemp = 25.0;
unsigned long seq = 0;

// median of N readings, in volts at the ESP32 pin
float readMedianV(int pin) {
  int buf[SAMPLES];
  for (int i = 0; i < SAMPLES; i++) {
    buf[i] = analogReadMilliVolts(pin);
    delay(10);
  }
  for (int i = 0; i < SAMPLES - 1; i++)
    for (int j = i + 1; j < SAMPLES; j++)
      if (buf[j] < buf[i]) { int t = buf[i]; buf[i] = buf[j]; buf[j] = t; }
  return buf[SAMPLES / 2] / 1000.0;
}

float calcTds(float voltage, float tempC) {
  float comp = 1.0 + 0.02 * (tempC - 25.0);
  float v = voltage / comp;
  float tds = (133.42 * v * v * v - 255.86 * v * v + 857.39 * v) * 0.5;
  return tds < 0 ? 0 : tds;
}

float calcNtu(float v) {
  if (v < 2.5) return 3000;
  if (v > 4.2) return 0;
  float ntu = -1120.4 * v * v + 5742.3 * v - 4352.9;
  return ntu < 0 ? 0 : ntu;
}

void sendMessage(const char *msg) {
  LoRa.idle();
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
  LoRa.receive();
  Serial.print("[Sent] ");
  Serial.println(msg);
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd == "ON") {
    digitalWrite(LED_PIN, HIGH);
  } else if (cmd == "OFF") {
    digitalWrite(LED_PIN, LOW);
  } else if (cmd == "TOGGLE") {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}

void readAndSend() {
  // temperature
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);
  bool tempOk = (t != DEVICE_DISCONNECTED_C);
  if (tempOk) lastTemp = t;

  // TDS (compensated with the last good temperature)
  float tds = calcTds(readMedianV(TDS_PIN), lastTemp);

  // turbidity (x2 undoes the voltage divider)
  float turbV = readMedianV(TURB_PIN) * 2.0;
  float ntu = calcNtu(turbV);

  char msg[64];
  snprintf(msg, sizeof(msg), "ID=%lu,T=%.2f,TDS=%.0f,NTU=%.0f",
           seq++, tempOk ? t : -127.0, tds, ntu);
  sendMessage(msg);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println(DEVICE_NAME);

  sensors.begin();
  pinMode(ONE_WIRE_PIN, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(TDS_PIN, ADC_11db);
  analogSetPinAttenuation(TURB_PIN, ADC_11db);
  Serial.printf("DS18B20 found: %d\n", sensors.getDeviceCount());

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("*** LoRa INIT FAILED! Check wiring and 3.3V supply ***");
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }

  // Must be identical on both boards
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setPreambleLength(8);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  LoRa.receive();
  Serial.println("LoRa ready, sending sensor data...");
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last >= SEND_INTERVAL) {
    last = millis();
    readAndSend();
  }

  // still accepts ON / OFF / TOGGLE from the receiver
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String received = "";
    while (LoRa.available()) received += (char)LoRa.read();
    Serial.print("[Received] ");
    Serial.println(received);
    handleCommand(received);
  }
}