#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>

// ============================================================
// 蝖祇?閮剖?
// ============================================================
#define DATA_PIN 18
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

#define BRIGHTNESS 55
#define MAX_MILLIAMPS 3000

#define SERPENTINE true
#define ORIGIN_TOP_LEFT true

CRGB leds[NUM_LEDS];

// ============================================================
// Wi-Fi 閮剖?
// ============================================================
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ============================================================
// MQTTGO 閮剖?
// ESP32 雿輻銝??MQTT TCP Port 1883
// ============================================================
const char* MQTT_HOST = "mqttgo.io";
const uint16_t MQTT_PORT = 1883;

const char* TOPIC_NOTE =
  "/user/YOUR_MQTTGO_ACCOUNT/bamboo/led/note";

const char* TOPIC_SPECTRUM =
  "/user/YOUR_MQTTGO_ACCOUNT/bamboo/led/spectrum";

const char* TOPIC_CONTROL =
  "/user/YOUR_MQTTGO_ACCOUNT/bamboo/led/control";

const char* TOPIC_STATUS =
  "/user/YOUR_MQTTGO_ACCOUNT/bamboo/led/status";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ============================================================
// 憿舐內???// ============================================================
constexpr uint8_t AUDIO_BANDS = 16;
uint8_t targetBands[AUDIO_BANDS] = {0};
float smoothBands[AUDIO_BANDS] = {0};

uint8_t currentNote = 0;
uint8_t currentEnergy = 0;
String currentGesture = "none";
CRGB currentModeColor = CRGB(80, 170, 255);

uint32_t lastSpectrumPacket = 0;
uint32_t lastNotePacket = 0;
uint32_t lastLedFrame = 0;
uint32_t lastStatusPublish = 0;

constexpr uint32_t SPECTRUM_TIMEOUT_MS = 1800;
constexpr uint32_t LED_FRAME_INTERVAL_MS = 50;
constexpr uint32_t STATUS_INTERVAL_MS = 8000;
constexpr uint32_t SPECTRUM_PACKET_MIN_INTERVAL_MS = 100;
constexpr uint32_t NOTE_EVENT_MIN_INTERVAL_MS = 140;
constexpr uint8_t NOTE_ANIMATION_MAX_PHASE = 22;

bool noteBurstActive = false;
uint8_t noteBurstRadius = 0;
uint8_t noteBurstBrightness = 0;
uint16_t skippedNoteEvents = 0;

// ============================================================
// 16?16 ?耦?拚摨扳?
// ============================================================
uint16_t xy(uint8_t x, uint8_t y) {
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) {
    return 0;
  }

  if (!ORIGIN_TOP_LEFT) {
    y = MATRIX_HEIGHT - 1 - y;
  }

  if (SERPENTINE && (y % 2 == 1)) {
    x = MATRIX_WIDTH - 1 - x;
  }

  return static_cast<uint16_t>(y) * MATRIX_WIDTH + x;
}

void drawPixelXY(uint8_t x, uint8_t y, const CRGB& color) {
  if (x < MATRIX_WIDTH && y < MATRIX_HEIGHT) {
    leds[xy(x, y)] = color;
  }
}

void addPixelXY(uint8_t x, uint8_t y, const CRGB& color) {
  if (x < MATRIX_WIDTH && y < MATRIX_HEIGHT) {
    leds[xy(x, y)] += color;
  }
}

void clearMatrix() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
}

// ============================================================
// 銋蝚阡???// ============================================================
CRGB noteColor(uint8_t note) {
  switch (note) {
    case 1: return CRGB(255, 45, 45);    // Do嚗?
    case 2: return CRGB(255, 120, 20);   // Re嚗?
    case 3: return CRGB(255, 220, 30);   // Mi嚗?
    case 4: return CRGB(40, 220, 70);    // Fa嚗?
    case 5: return CRGB(20, 210, 230);   // Sol嚗?
    case 6: return CRGB(45, 90, 255);    // La嚗?
    case 7: return CRGB(150, 55, 255);   // Si嚗換
    case 8: return CRGB(255, 65, 190);   // 擃 Do嚗?蝝?    case 9: return CRGB(245, 245, 255);  // 擃 Re嚗
    default: return CRGB(80, 170, 255);
  }
}

CRGB noteColorFixed(uint8_t note) {
  switch (note) {
    case 1: return CRGB(255, 45, 45);    // Do
    case 2: return CRGB(255, 120, 20);   // Re
    case 3: return CRGB(255, 220, 30);   // Mi
    case 4: return CRGB(40, 220, 70);    // Fa
    case 5: return CRGB(20, 210, 230);   // Sol
    case 6: return CRGB(45, 90, 255);    // La
    case 7: return CRGB(150, 55, 255);   // Si
    case 8: return CRGB(255, 65, 190);   // High Do
    case 9: return CRGB(245, 245, 255);  // High Re
    default: return CRGB(80, 170, 255);
  }
}

CRGB randomModeColor() {
  uint8_t hue = random8();
  uint8_t saturation = random8(180, 255);
  uint8_t value = random8(190, 255);
  return CHSV(hue, saturation, value);
}

CRGB noteEventColor() {
  CRGB color = currentModeColor;
  color.nscale8_video(noteBurstBrightness);
  return color;
}

// ============================================================
// Wi-Fi
// ============================================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Connecting Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t startedAt = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");

    if (millis() - startedAt > 20000) {
      Serial.println("\nWi-Fi timeout, retrying...");
      WiFi.disconnect(true);
      delay(800);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      startedAt = millis();
    }
  }

  Serial.println();
  Serial.println("Wi-Fi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
}

// ============================================================
// MQTT ?????// ============================================================
void publishStatus(bool online) {
  if (!mqttClient.connected()) {
    return;
  }

  StaticJsonDocument<192> doc;
  doc["online"] = online;
  doc["device"] = "esp32-32e-led";
  doc["rssi"] = WiFi.RSSI();
  doc["note"] = currentNote;
  doc["gesture"] = currentGesture;

  char payload[192];
  size_t length = serializeJson(doc, payload, sizeof(payload));

  mqttClient.publish(
    TOPIC_STATUS,
    reinterpret_cast<const uint8_t*>(payload),
    length,
    false
  );
}

// ============================================================
// MQTT JSON ??
// ============================================================
void handleSpectrumJson(const JsonDocument& doc) {
  JsonArrayConst bands = doc["bands"].as<JsonArrayConst>();

  if (bands.size() < AUDIO_BANDS) {
    Serial.println("Spectrum packet rejected: bands < 16");
    return;
  }

  for (uint8_t i = 0; i < AUDIO_BANDS; i++) {
    targetBands[i] = constrain(
      bands[i].as<int>(),
      0,
      MATRIX_HEIGHT
    );
  }

  currentNote = constrain(
    doc["note"] | currentNote,
    0,
    9
  );

  currentEnergy = constrain(
    doc["energy"] | currentEnergy,
    0,
    100
  );

  const char* gesture = doc["gesture"] | "none";
  currentGesture = gesture;

  lastSpectrumPacket = millis();
}

void handleNoteJson(const JsonDocument& doc) {
  uint8_t incomingNote = constrain(
    doc["note"] | 0,
    0,
    9
  );

  currentEnergy = constrain(
    doc["energy"] | 70,
    0,
    100
  );

  const char* gesture = doc["gesture"] | "none";
  currentGesture = gesture;

  uint32_t now = millis();

  if (now - lastNotePacket < NOTE_EVENT_MIN_INTERVAL_MS) {
    skippedNoteEvents++;
    return;
  }

  currentNote = incomingNote;
  currentModeColor = randomModeColor();
  noteBurstActive = true;
  noteBurstRadius = 0;
  noteBurstBrightness = map(currentEnergy, 0, 100, 140, 255);
  lastNotePacket = now;

  Serial.print("Note event: ");
  Serial.print(currentNote);
  Serial.print(" gesture=");
  Serial.print(currentGesture);
  Serial.print(" energy=");
  Serial.print(currentEnergy);
  Serial.print(" color=#");
  char colorText[8];
  snprintf(
    colorText,
    sizeof(colorText),
    "%02X%02X%02X",
    currentModeColor.r,
    currentModeColor.g,
    currentModeColor.b
  );
  Serial.print(colorText);
  Serial.print(" skipped=");
  Serial.println(skippedNoteEvents);
}

void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length
) {
  if (strcmp(topic, TOPIC_SPECTRUM) == 0) {
    uint32_t now = millis();

    if (now - lastSpectrumPacket < SPECTRUM_PACKET_MIN_INTERVAL_MS) {
      return;
    }
  }

  StaticJsonDocument<768> doc;

  DeserializationError error =
    deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  if (strcmp(topic, TOPIC_SPECTRUM) == 0) {
    handleSpectrumJson(doc);
    return;
  }

  if (strcmp(topic, TOPIC_NOTE) == 0) {
    handleNoteJson(doc);
    return;
  }

  if (strcmp(topic, TOPIC_CONTROL) == 0) {
    const char* event = doc["event"] | "";

    if (strcmp(event, "clear") == 0) {
      memset(targetBands, 0, sizeof(targetBands));
      clearMatrix();
      FastLED.show();
    }
  }
}

// ============================================================
// MQTT ??// ============================================================
String buildClientId() {
  uint64_t chipId = ESP.getEfuseMac();

  char id[40];
  snprintf(
    id,
    sizeof(id),
    "esp32-led-%04X%08X",
    static_cast<uint16_t>(chipId >> 32),
    static_cast<uint32_t>(chipId)
  );

  return String(id);
}

void connectMqtt() {
  if (mqttClient.connected()) {
    return;
  }

  String clientId = buildClientId();

  Serial.print("Connecting MQTT as ");
  Serial.println(clientId);

  bool connected = mqttClient.connect(
    clientId.c_str(),
    TOPIC_STATUS,
    0,
    false,
    "{\"online\":false}"
  );

  if (!connected) {
    Serial.print("MQTT failed, state=");
    Serial.println(mqttClient.state());
    return;
  }

  Serial.println("MQTT connected");

  mqttClient.subscribe(TOPIC_NOTE, 0);
  mqttClient.subscribe(TOPIC_SPECTRUM, 0);
  mqttClient.subscribe(TOPIC_CONTROL, 0);

  publishStatus(true);
}

// ============================================================
// ????皜祈岫
// ============================================================
void showStartupCorners() {
  clearMatrix();

  drawPixelXY(0, 0, CRGB::Red);
  drawPixelXY(15, 0, CRGB::Green);
  drawPixelXY(0, 15, CRGB::Blue);
  drawPixelXY(15, 15, CRGB::White);

  FastLED.show();
  delay(1200);

  clearMatrix();
  FastLED.show();
}

// ============================================================
// ?餉?憿舐內
// ============================================================
void drawSpectrum() {
  fadeToBlackBy(leds, NUM_LEDS, 90);

  bool dataFresh =
    millis() - lastSpectrumPacket < SPECTRUM_TIMEOUT_MS;

  CRGB baseColor = currentModeColor;

  for (uint8_t x = 0; x < AUDIO_BANDS; x++) {
    float desired = dataFresh ? targetBands[x] : 0.0f;

    if (smoothBands[x] < desired) {
      smoothBands[x] += (desired - smoothBands[x]) * 0.62f;
    } else {
      smoothBands[x] += (desired - smoothBands[x]) * 0.22f;
    }

    uint8_t height = constrain(
      static_cast<int>(roundf(smoothBands[x])),
      0,
      MATRIX_HEIGHT
    );

    for (uint8_t level = 0; level < height; level++) {
      uint8_t y = MATRIX_HEIGHT - 1 - level;

      uint8_t brightness = map(
        level,
        0,
        MATRIX_HEIGHT - 1,
        115,
        255
      );

      CRGB color = baseColor;
      color.nscale8_video(brightness);

      if (level == height - 1) {
        color += CRGB(80, 80, 80);
      }

      drawPixelXY(x, y, color);
    }
  }
}

// ============================================================
// ?喟泵閫貊?郭
// ============================================================
void drawRippleAt(int8_t centerX, int8_t centerY, uint8_t phase, const CRGB& color) {
  uint8_t radius = phase / 2;
  uint16_t inner = radius > 0 ? (radius - 1) * (radius - 1) : 0;
  uint16_t outer = (radius + 1) * (radius + 1);

  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      int8_t dx = static_cast<int8_t>(x) - centerX;
      int8_t dy = static_cast<int8_t>(y) - centerY;
      uint16_t distanceSquared = dx * dx + dy * dy;

      if (distanceSquared >= inner && distanceSquared <= outer) {
        addPixelXY(x, y, color);
      }
    }
  }
}

void drawRipple(uint8_t phase, const CRGB& color) {
  drawRippleAt(7, 7, phase, color);
}

void drawDoubleWave(uint8_t phase, const CRGB& color) {
  uint8_t radius = phase / 2;

  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      uint8_t leftDistance = abs(static_cast<int>(x) - 3);
      uint8_t rightDistance = abs(static_cast<int>(x) - 12);

      if (leftDistance == radius || rightDistance == radius) {
        CRGB c = color;
        uint8_t yFade = 255 - min<uint8_t>(180, abs(static_cast<int>(y) - 7) * 22);
        c.nscale8_video(yFade);
        addPixelXY(x, y, c);
      }
    }
  }
}

void drawTripleWave(uint8_t phase, const CRGB& color) {
  drawRippleAt(3, 8, phase, color);
  drawRippleAt(7, 7, phase + 2, color);
  drawRippleAt(12, 8, phase + 4, color);
}

void drawLightning(uint8_t phase, const CRGB& color) {
  uint8_t offset = (phase / 2) % 4;

  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    uint8_t y = 2 + ((x * 3 + offset) % 12);
    addPixelXY(x, y, color);

    if (y > 0) {
      CRGB dim = color;
      dim.nscale8_video(120);
      addPixelXY(x, y - 1, dim);
    }

    if (y + 1 < MATRIX_HEIGHT) {
      CRGB dim = color;
      dim.nscale8_video(120);
      addPixelXY(x, y + 1, dim);
    }
  }
}

void drawOceanWave(uint8_t phase, const CRGB& color) {
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    uint8_t wave = sin8(x * 18 + phase * 12);
    uint8_t y = map(wave, 0, 255, 3, 12);

    addPixelXY(x, y, color);

    CRGB foam = color;
    foam.nscale8_video(120);
    if (y > 0) {
      addPixelXY(x, y - 1, foam);
    }
    if (y + 1 < MATRIX_HEIGHT) {
      addPixelXY(x, y + 1, foam);
    }
  }
}

void drawHeartbeat(uint8_t phase, const CRGB& color) {
  uint8_t beat = phase % 16;
  uint8_t halfSize =
    (beat < 3 || (beat >= 7 && beat < 10)) ? 5 :
    (beat < 5 || (beat >= 10 && beat < 12)) ? 3 :
    1;

  for (uint8_t y = 7 - halfSize; y <= 8 + halfSize && y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 7 - halfSize; x <= 8 + halfSize && x < MATRIX_WIDTH; x++) {
      CRGB c = color;
      uint8_t edge = max<uint8_t>(abs(static_cast<int>(x) - 7), abs(static_cast<int>(y) - 7));
      c.nscale8_video(255 - edge * 26);
      addPixelXY(x, y, c);
    }
  }
}

void drawParticleGather(uint8_t phase, const CRGB& color) {
  uint8_t progress = min<uint16_t>(255, phase * 10);

  for (uint8_t i = 0; i < 28; i++) {
    uint8_t sx = (i * 5 + 2) % MATRIX_WIDTH;
    uint8_t sy = (i % 4 == 0) ? 0 : ((i % 4 == 1) ? 15 : ((i * 7) % MATRIX_HEIGHT));
    uint8_t tx = 6 + (i % 4);
    uint8_t ty = 6 + ((i / 4) % 4);

    uint8_t x = (sx * (255 - progress) + tx * progress) / 255;
    uint8_t y = (sy * (255 - progress) + ty * progress) / 255;

    addPixelXY(x, y, color);
  }
}

void drawFountain(uint8_t phase, const CRGB& color) {
  for (uint8_t i = 0; i < 24; i++) {
    uint8_t age = (phase * 2 + i * 5) % 32;
    int8_t x = 7 + ((i % 7) - 3) * age / 10;
    int8_t y = 15 - age / 2;

    if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT) {
      CRGB c = color;
      c.nscale8_video(255 - min<uint8_t>(180, age * 6));
      addPixelXY(x, y, c);
    }
  }
}

void drawFirework(uint8_t phase, const CRGB& color) {
  if (phase < 10) {
    uint8_t y = 15 - phase;
    addPixelXY(7, y, color);
    addPixelXY(8, y, color);
    return;
  }

  uint8_t burstPhase = phase - 10;
  drawRippleAt(7, 6, burstPhase, color);

  for (uint8_t i = 0; i < 20; i++) {
    uint8_t sparkleX = (i * 7 + burstPhase * 3) % MATRIX_WIDTH;
    uint8_t sparkleY = (i * 11 + burstPhase * 5) % MATRIX_HEIGHT;

    if (((sparkleX + sparkleY + burstPhase) % 5) == 0) {
      addPixelXY(sparkleX, sparkleY, color);
    }
  }
}

void drawNoteBurst() {
  if (!noteBurstActive) {
    return;
  }

  CRGB color = noteEventColor();

  switch (currentNote) {
    case 1:
      drawRipple(noteBurstRadius, color);
      break;
    case 2:
      drawDoubleWave(noteBurstRadius, color);
      break;
    case 3:
      drawTripleWave(noteBurstRadius, color);
      break;
    case 4:
      drawLightning(noteBurstRadius, color);
      break;
    case 5:
      drawOceanWave(noteBurstRadius, color);
      break;
    case 6:
      drawHeartbeat(noteBurstRadius, color);
      break;
    case 7:
      drawParticleGather(noteBurstRadius, color);
      break;
    case 8:
      drawFountain(noteBurstRadius, color);
      break;
    case 9:
      drawFirework(noteBurstRadius, color);
      break;
    default:
      drawRipple(noteBurstRadius, color);
      break;
  }

  noteBurstRadius++;

  if (noteBurstRadius > NOTE_ANIMATION_MAX_PHASE) {
    noteBurstActive = false;
  }
}

// ============================================================
// ?∟???璈???// ============================================================
void drawIdleIndicator() {
  if (millis() - lastSpectrumPacket < SPECTRUM_TIMEOUT_MS) {
    return;
  }

  uint8_t value = beatsin8(12, 8, 45);

  drawPixelXY(0, 15, CRGB(0, value, value));
  drawPixelXY(15, 15, CRGB(0, value, value));
}

// ============================================================
// Arduino setup / loop
// ============================================================
void setup() {
  delay(800);
  Serial.begin(115200);
  random16_add_entropy(static_cast<uint16_t>(esp_random()));
  random16_set_seed(static_cast<uint16_t>(esp_random()));

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(
    leds,
    NUM_LEDS
  );

  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(
    5,
    MAX_MILLIAMPS
  );

  clearMatrix();
  FastLED.show();
  showStartupCorners();

  connectWiFi();

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);
  mqttClient.setKeepAlive(30);

  connectMqtt();

  Serial.println("ESP32 MQTT LED receiver ready");
  Serial.println(TOPIC_NOTE);
  Serial.println(TOPIC_SPECTRUM);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    static uint32_t lastReconnectAttempt = 0;

    if (millis() - lastReconnectAttempt > 2000) {
      lastReconnectAttempt = millis();
      connectMqtt();
    }
  }

  mqttClient.loop();

  if (millis() - lastStatusPublish >= STATUS_INTERVAL_MS) {
    lastStatusPublish = millis();
    publishStatus(true);
  }

  if (millis() - lastLedFrame >= LED_FRAME_INTERVAL_MS) {
    lastLedFrame = millis();

    drawSpectrum();
    drawNoteBurst();
    drawIdleIndicator();

    FastLED.show();
  }
}

