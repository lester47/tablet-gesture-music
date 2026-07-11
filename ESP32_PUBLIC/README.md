# ESP32 Public Example

This folder contains a redacted version of the ESP32 WS2812B 16x16 MQTT receiver used by the v1 success baseline.

Before flashing the sketch, replace these placeholders:

- `YOUR_WIFI_SSID`
- `YOUR_WIFI_PASSWORD`
- `YOUR_MQTTGO_ACCOUNT`

The web page default uses:

- MQTT WebSocket URL: `wss://mqttgo.io:8084`
- Topic base: `/user/YOUR_MQTTGO_ACCOUNT/bamboo`

The ESP32 sketch uses:

- MQTT host: `mqttgo.io`
- MQTT port: `1883`
- LED GPIO: `18`
- LED type: `WS2812B`
- Matrix: `16x16`
- Mapping: serpentine

Do not commit real Wi-Fi passwords or private MQTT credentials.

