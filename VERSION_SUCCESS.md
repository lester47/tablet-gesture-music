# v1 Success Baseline

Date: 2026-07-11

This is the first full-chain successful prototype.

## Working Chain

Laptop / iPad / iPhone
-> GitHub Pages
-> hand gesture recognition and Web Audio music
-> MQTTGO
-> ESP32
-> WS2812B 16x16 LED matrix
-> realtime light show

## Web Version

- Public entry file: `index.html`
- Source milestone: `index_mqttgo_spectrum_fixed.html`
- GitHub Pages URL: https://lester47.github.io/tablet-gesture-music/
- MQTT WebSocket URL: `wss://mqttgo.io:8084`
- Topic base: `/user/lester47/bamboo`

## MQTT Topics

- Note: `/user/lester47/bamboo/led/note`
- Spectrum: `/user/lester47/bamboo/led/spectrum`
- Control: `/user/lester47/bamboo/led/control`
- Status: `/user/lester47/bamboo/led/status`

## ESP32 / LED Baseline

- Broker: `mqttgo.io`
- ESP32 MQTT port: `1883`
- LED data GPIO: `18`
- LED type: `WS2812B`
- Matrix: `16x16`
- Mapping: serpentine

## Local Private Backup

The hardware sketch contains Wi-Fi credentials, so the exact ESP32 success sketch is stored only in the local private backup:

`success_versions/esp32_v1_success_private/`

Do not publish private Wi-Fi credentials to GitHub.

