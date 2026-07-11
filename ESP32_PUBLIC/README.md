# ESP32 Public Example

This folder contains a redacted version of the ESP32 WS2812B 16x16 MQTT receiver used by the v2 animation-engine baseline.

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

The v2 animation engine maps the 9 note events to 9 LED behaviors:

- Do: center ripple
- Re: double wave
- Mi: triple ripple
- Fa: lightning
- Sol: full-matrix wave
- La: heartbeat pulse
- Si: particle gather
- High Do: fountain
- High Re: firework

The v3 random-color update keeps the 9 animation behaviors, but assigns a fresh vivid color whenever a new note/mode event arrives. The spectrum bars then continue with the same random color until the next event.

PlatformIO / VS Code upload:

```bash
pio run
pio run --target upload
```

Do not commit real Wi-Fi passwords or private MQTT credentials.

