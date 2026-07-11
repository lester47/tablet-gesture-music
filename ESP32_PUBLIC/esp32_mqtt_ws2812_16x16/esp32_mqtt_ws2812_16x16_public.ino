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

uint32_t lastSpectrumPacket = 0;
uint32_t lastNotePacket = 0;
uint32_t lastLedFrame = 0;
uint32_t lastStatusPublish = 0;

constexpr uint32_t SPECTRUM_TIMEOUT_MS = 1800;
constexpr uint32_t LED_FRAME_INTERVAL_MS = 33;
constexpr uint32_t STATUS_INTERVAL_MS = 5000;

bool noteBurstActive = false;
uint8_t noteBurstRadius = 0;
uint8_t noteBurstBrightness = 0;

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
  currentNote = constrain(
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

  noteBurstActive = true;
  noteBurstRadius = 0;
  noteBurstBrightness = map(currentEnergy, 0, 100, 140, 255);
  lastNotePacket = millis();

  Serial.print("Note event: ");
  Serial.print(currentNote);
  Serial.print(" gesture=");
  Serial.print(currentGesture);
  Serial.print(" energy=");
  Serial.println(currentEnergy);
}

void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length
) {
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

  CRGB baseColor = noteColor(currentNote);

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
void drawNoteBurst() {
  if (!noteBurstActive) {
    return;
  }

  const int8_t centerX = 7;
  const int8_t centerY = 7;
  CRGB color = noteColor(currentNote);
  color.nscale8_video(noteBurstBrightness);

  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      int8_t dx = static_cast<int8_t>(x) - centerX;
      int8_t dy = static_cast<int8_t>(y) - centerY;
      uint8_t distance =
        static_cast<uint8_t>(sqrtf(dx * dx + dy * dy));

      if (
        distance == noteBurstRadius ||
        distance + 1 == noteBurstRadius
      ) {
        leds[xy(x, y)] += color;
      }
    }
  }

  noteBurstRadius++;

  if (noteBurstRadius > 12) {
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

