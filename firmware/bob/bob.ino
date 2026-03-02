// Bob - M5Stack CoreS3 Lite Animated Eyes
#include <M5Unified.h>
#include <M5CoreS3.h>
#include <M5GFX.h>
#include <math.h>
#include "esp_sleep.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#if __has_include(<qrcode.h>)
#include <qrcode.h>
#define BOB_HAS_QRCODE 1
#else
#define BOB_HAS_QRCODE 0
#endif
#include "esp_camera.h"
#include "img_converters.h"
#include "config.h"
#include "bob_expressions.h"
#include "bob_reactions.h"
#include "bob_personality.h"
#include "bob_image_b64.h"

// Essential type declarations
struct EyeDims {
  float w, h, r;
};

// Release hardening: default to production-safe logging unless overridden in config.
#ifndef BOB_PRODUCTION_BUILD
#define BOB_PRODUCTION_BUILD 1
#endif

#ifndef BOB_DEBUG_LEVEL
#if BOB_PRODUCTION_BUILD
#define BOB_DEBUG_LEVEL 0  // production default
#else
#define BOB_DEBUG_LEVEL 1
#endif
#endif

#define DEBUG_LEVEL BOB_DEBUG_LEVEL  // 0=minimal, 1=basic, 2=verbose

#if DEBUG_LEVEL >= 1
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(fmt, ...)
#endif

#if DEBUG_LEVEL >= 2
#define DEBUG_VERBOSE_PRINT(x) Serial.print(x)
#define DEBUG_VERBOSE_PRINTLN(x) Serial.println(x)
#define DEBUG_VERBOSE_PRINTF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#define DEBUG_VERBOSE_PRINT(x)
#define DEBUG_VERBOSE_PRINTLN(x)
#define DEBUG_VERBOSE_PRINTF(fmt, ...)
#endif

// Forward declarations (defined later in bob.ino)
void enterDeepSleep(uint32_t timeoutMinutes = 0);
void publishStatus(const char* status);
void rotateScreen();
void publishAutoDiscovery();
void setOllamaState(const char* newState);
const char* personalityName(PersonalityExtended p);
PersonalityExtended parsePersonality(const String& name, bool& ok);
void updateProximityEyeTracking(uint32_t now);
EyeDims computeEyeDims(const EyeShape& shape, float baseScale);
void initMatrix();
void initConfetti();
void spawnConfettiSnow(int index);
void spawnConfettiCannon(int index);
void updateConfetti();
void drawConfetti();
void drawEyes(float lookX, float lookY);
void startBlink();
void tickEmotionTween();
void behaviorsTick(bool anyTouch);
void handleWifiSetupPage();
void handleWifiSetupSave();
void handleDashboard();
void handleControlAction();
void handleApiStatus();
void handleHaOnboarding();
void startWifiSetupPortal();
void drawSetupPortalScreen();
void drawHaOnboardingScreen(const String& localHaUrl);
void loadRuntimeConnectivityConfig();
void startMdnsService();
void triggerWebNotificationPreview(const String& rawType, const String& customText);
void startBleProvisioning();
void stopBleProvisioning();
void startCameraStreaming();
void stopCameraStreaming();
bool initializeMQTT();
void setupHomeAssistantDiscovery();
void reconnectAfterDeepSleep();
void imuTriggers();
void processProximityDetection();
void processMicrophoneDetection();
void cleanupMicrophone();
void maintainConnections();
void publishWakeReason(const char* reason);
void wakeBobFromSleep(const char* reason, bool triggerWakeSequence = true);
extern String currentBehaviorName;
extern uint32_t behaviorStartTime;

#if BOB_HAS_QRCODE && defined(ESP_QRCODE_CONFIG_DEFAULT)
static int g_haQrX0 = 10;
static int g_haQrY0 = 52;
static int g_haQrMaxSize = 148;

static void drawHaOnboardingEspQr(esp_qrcode_handle_t qrcode) {
  const int qrSize = esp_qrcode_get_size(qrcode);
  int scale = 4;
  while (scale > 1 && (qrSize * scale) > g_haQrMaxSize) scale--;

  const int drawSize = qrSize * scale;
  M5.Display.fillRect(g_haQrX0 - 2, g_haQrY0 - 2, drawSize + 4, drawSize + 4, TFT_WHITE);
  for (int y = 0; y < qrSize; y++) {
    for (int x = 0; x < qrSize; x++) {
      if (esp_qrcode_get_module(qrcode, x, y)) {
        M5.Display.fillRect(g_haQrX0 + x * scale, g_haQrY0 + y * scale, scale, scale, TFT_BLACK);
      }
    }
  }
}
#endif

// ---------------- Timings & constanten ----------------
static const uint32_t FPS_DELAY_MS      = 16;   // ~60 FPS
static const uint32_t BLINK_MIN_MS      = 2600;
static const uint32_t BLINK_MAX_MS      = 5200;
static const uint32_t BLINK_DUR_MS      = 160;

static const float    PARALLAX_RATIO   = 0.12f;  // Was 0.06f, nu 0.12f voor meer 3D effect
static const float    FAR_MIN          = 0.70f;
static const float    NEAR_MAX         = 1.85f;
static const float    PEEK_ASYM_MAX    = 0.25f;  // Was 0.12f, nu 0.25f voor meer asymmetrie
static const float    LOOK_TAU_TOUCH   = 150.0f;
static const float    LOOK_TAU_IDLE    = 250.0f;
static const float    GAP_MARGIN_PX    = 6.0f;
static const float    TILT_PX          = 10.0f;
static const int      SKEW_MAX_PX      = 14;
static const uint32_t BOB_DUR_MS       = 2600;
static const uint32_t ZBF_PHASE_MS     = 700;

// Scherm/ogen basis (wordt geschaald in setup)
static const int   BASE_EYE_SPACING = EYE_SPACING_BASE_PX;
static const int   BASE_GLOW_OFFSET = EYE_GLOW_PX;
static const float BASE_MAX_LOOK_X  = MAX_LOOK_X_BASE;  // Subtielere beweging met sterk 3D effect
static const float BASE_MAX_LOOK_Y  = MAX_LOOK_Y_BASE;

// --- Sleep Timer ---
static uint32_t INACTIVITY_TIMEOUT_MS = DEFAULT_INACTIVITY_TIMEOUT; // Standaard inactiviteit voor slaap
static const uint32_t SLEEP_SCREEN_ON_MS = 3000; // Eyes-closed animation before screen off (3s)
static const uint32_t SLEEP_SCREEN_OFF_TO_DEEP_SLEEP_MS = 5000;

// Eye movement tracking constanten
const float EYE_TRACK_SMALL_MOVE_MIN = -0.40f;
const float EYE_TRACK_SMALL_MOVE_MAX = 0.40f;
const float EYE_TRACK_LARGE_MOVE_MIN = -0.80f;
const float EYE_TRACK_LARGE_MOVE_MAX = 0.80f;
const float EYE_TRACK_MAX_RANGE = 0.75f;
const int EYE_TRACK_SMALL_MOVE_CHANCE = 50; // Percentage
const unsigned long EYE_TRACK_INTERVAL_MIN_MS = 600;
const unsigned long EYE_TRACK_INTERVAL_MAX_MS = 2000;
uint32_t lastInteractionTime = 0;
bool isGoingToSleep = false;
bool isAsleep = false;
bool sleepPhase2Printed = false;

// Touch debounce voor runnerNext - verhoogd voor minder behavior spam
uint32_t lastTouchTime = 0;
const uint32_t TOUCH_DEBOUNCE_MS = 2000; // 2 seconden tussen behavior changes
bool touchStatePublished = false; // Track if touch state is published to MQTT

// Movement detection (touch, shake, proximity)
bool movementDetected = false;
bool lastMovementState = false;

// Shake tune rotation system
uint8_t shakeCounter = 0; // Teller voor verschillende deuntjes bij schudden

// Screen control
bool screenOn = true;
int screenBrightness = DEFAULT_SCREEN_BRIGHTNESS; // 0-255
bool autoBrightnessEnabled = true; // Auto brightness aan/uit
uint16_t ambientLightLevel = 0; // Ambient light sensor waarde
int screenRotation = 0; // 0=0Â°, 1=90Â°, 2=180Â°, 3=270Â°

bool proximityEnabled = false;  // DISABLED - causing false triggers every 2s
uint16_t proximityThreshold = DEFAULT_PROXIMITY_THRESHOLD;
uint32_t proximityDetectionTime = 0;
const uint32_t PROXIMITY_DEBOUNCE_MS = 500;  // 500ms debounce to avoid false triggers
bool proximityRawDetected = false;
uint32_t lastProximityTriggerTime = 0;  // Track last proximity event

// --- Camera for snapshots and streaming ---
bool cameraInitialized = false;
bool cameraStreaming = false;
bool cameraWebStreaming = false;  // Web preview stream only; keeps eyes rendering on display
bool clockMode = false;           // Digital clock mode in Bob eye style
uint32_t lastStreamFrame = 0;
const uint32_t STREAM_INTERVAL_MS = 140; // ~7 fps streaming (more stable, fewer VSYNC overflows)
uint16_t cameraGetFailCount = 0;
uint32_t lastCameraRecoverAt = 0;

bool lastPresenceState = false;
uint32_t presenceChangeTime = 0;
const uint32_t PRESENCE_NOTIFICATION_DELAY = 2000;

bool microphoneEnabled = false;  // DISABLED - causing constant sound triggers
float lastSoundLevel = 0.0f;
uint32_t lastMicrophoneCheck = 0;
bool soundDetected = false;
float soundThreshold = 200.0f;  // RMS threshold for microphone sound detection
const uint32_t MIC_UPDATE_INTERVAL = MIC_UPDATE_INTERVAL_MS;
int16_t* micBuffer = nullptr;

bool soundReactionActive = false;
uint32_t soundReactionStartTime = 0;
const uint32_t SOUND_REACTION_DURATION = 5000;  // 5 seconds - eyes stay big longer
const float SOUND_EYE_SCALE = 1.4f;  // Verhoogd van 1.2f naar 1.4f = 40% groter!

bool manualOverride = false;
uint32_t manualOverrideTimeout = 0;
const uint32_t MANUAL_OVERRIDE_DURATION = 10000;

bool alwaysAwake = false;
bool autoAwakeOnPower = false; // Automatisch wakker blijven als aan stroom
bool autoEmotionEnabled = true;

static constexpr PersonalityExtended AUTO_EMOTIONS[] = {
  PersonalityExtended::Happy,
  PersonalityExtended::Sad,
  PersonalityExtended::Thinking,
  PersonalityExtended::Excited,
  PersonalityExtended::Confused,
  PersonalityExtended::Angry,
  PersonalityExtended::Scared,
  PersonalityExtended::Sleepy,
  PersonalityExtended::Love,
  PersonalityExtended::Surprised,
  PersonalityExtended::Dizzy,
  PersonalityExtended::Bored
};
static constexpr size_t AUTO_EMOTION_COUNT = sizeof(AUTO_EMOTIONS) / sizeof(AUTO_EMOTIONS[0]);
uint32_t nextAutoEmotionAt = 0;
size_t autoEmotionIndex = 0;

bool objectTrackingEnabled = true;
float currentLookTargetX = 0.0f;
float currentLookTargetY = 0.0f;
unsigned long lastTrackingUpdate = 0;
const unsigned long TRACKING_INTERVAL_MIN = 800;  // Min tijd op Ã©Ã©n punt
const unsigned long TRACKING_INTERVAL_MAX = 2500; // Max tijd op Ã©Ã©n punt
unsigned long nextTrackingChange = 0;

bool shakeDetected = false;
uint32_t lastShakeTime = 0;
uint32_t shakeStoppedTime = 0;
bool showingIrritation = false;
SmartReactions smartReactions;
PersonalityExtended currentPersonality = PersonalityExtended::Neutral;
uint32_t personalityUntil = 0;
uint32_t lastPersonalityCheck = 0;
uint32_t emotionCloseUntil = 0; // Tijdelijk "dichterbij scherm" effect voor happy/excited
uint32_t sleepyMicroNapAt = 0;
uint32_t sleepyStartleFrom = 0;
uint32_t sleepyStartleUntil = 0;
uint32_t surprisedPulseStart = 0;
uint32_t surprisedPulseEnd = 0;
uint32_t surprisedBlinkAt1 = 0;
uint32_t surprisedBlinkAt2 = 0;
uint8_t surprisedBlinkStep = 0;



// Matrix mode - VERTICALE KOLOMMEN met continu vallende cijfers
bool matrixMode = false;
static constexpr int MATRIX_COLS = 40;           // Aantal verticale kolommen (320px / 8px = 40 kolommen)
static constexpr int MATRIX_DIGITS_PER_COL = 20; // Meer cijfers voor continu effect (enkele buiten scherm)
struct MatrixDigit {
  char digit;       // Het cijfer (0-9)
  float y;          // Y positie van dit specifieke cijfer
  uint8_t brightness; // Helderheid van dit cijfer
};
struct MatrixColumn {
  float speed;      // Val snelheid voor deze kolom (alle cijfers in kolom vallen met zelfde snelheid)
  MatrixDigit digits[MATRIX_DIGITS_PER_COL]; // Alle cijfers in deze kolom
};
MatrixColumn matrixCols[MATRIX_COLS];
uint32_t lastMatrixUpdate = 0;

// Confetti mode - Vallende gekleurde vlokken die willekeurig ophopen
enum ConfettiType {
  CONFETTI_NONE = 0,
  CONFETTI_SNOW = 1,      // Van boven vallen (sneeuw)
  CONFETTI_CANNONS = 2    // Kanonnen linksonder + rechtsonder
};
ConfettiType confettiMode = CONFETTI_NONE;
bool confettiSnowEnabled = false;      // State for confetti snow switch
bool confettiCannonsEnabled = false;   // State for confetti cannons switch
static constexpr int MAX_CONFETTI = 600;  // Maximum aantal confetti deeltjes (MEER voor vol scherm!)
struct ConfettiParticle {
  float x, y;           // Positie
  float vx, vy;         // Snelheid (velocity)
  float rotation;       // Rotatie hoek
  float rotationSpeed;  // Rotatie snelheid
  uint16_t color;       // Kleur
  uint8_t size;         // Grootte (4-12 pixels)
  uint8_t shape;        // 0=vierkant, 1=cirkel, 2=driehoek, 3=ster
  bool active;          // Is deze particle actief?
  bool settled;         // Is het gestopt met bewegen?
  float landHeight;     // Op welke hoogte moet deze landen?
};
ConfettiParticle confetti[MAX_CONFETTI];
uint32_t lastConfettiUpdate = 0;
uint32_t confettiStartTime = 0;
uint32_t lastConfettiSpawn = 0;
int activeConfettiCount = 0;
const uint32_t CONFETTI_DURATION = 120000;  // 120 seconden - blijft spawnen tot je stopt
float avgFillHeight = 240.0f;  // Gemiddelde hoogte waar confetti zich ophoopt

bool proximityDetected = false;
uint32_t lastProximityTime = 0;
uint32_t lastProximityTrigger = 0;
const uint32_t PROXIMITY_TIMEOUT = DEFAULT_PROXIMITY_TIMEOUT;
const uint32_t PROXIMITY_TRIGGER_COOLDOWN = 100;
uint16_t read_ps_value = 0;

String ollamaState = "idle";
uint32_t stateChangeTime = 0;
String lastWakeReason = "boot";
uint32_t lastWakeReasonAt = 0;
bool startupEventPending = false;
uint32_t startupEventAt = 0;

inline bool timeElapsed(uint32_t previousTime, uint32_t interval) {
  return (uint32_t)(millis() - previousTime) >= interval;
}

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
WebServer webServer(80);  // HTTP server op poort 80
Preferences wifiPrefs;
bool wifiSetupPortalActive = false;
uint32_t lastSetupScreenDraw = 0;
uint32_t lastSetupPortalRetry = 0;

// Runtime connectivity settings (editable via setup page, with config.h defaults as fallback)
String runtimeMqttHost = MQTT_HOST;
uint16_t runtimeMqttPort = MQTT_PORT;
String runtimeMqttUser = MQTT_USERNAME;
String runtimeMqttPass = MQTT_PASSWORD;
String runtimeMqttClientId = MQTT_CLIENT_ID;
bool runtimeMqttEnabled = true;
String runtimeHaBaseUrl = HA_BASE_URL;
String runtimeHaToken = HA_LONG_TOKEN;
bool runtimeHaEnabled = true;

// BLE provisioning (fallback setup path)
static const char* BLE_PROV_SERVICE_UUID = "7a6b0001-10f6-4a6a-b4a0-3f1c2d9e1000";
static const char* BLE_PROV_SSID_UUID    = "7a6b0002-10f6-4a6a-b4a0-3f1c2d9e1000";
static const char* BLE_PROV_PASS_UUID    = "7a6b0003-10f6-4a6a-b4a0-3f1c2d9e1000";
static const char* BLE_PROV_APPLY_UUID   = "7a6b0004-10f6-4a6a-b4a0-3f1c2d9e1000";
static const char* BLE_PROV_STATUS_UUID  = "7a6b0005-10f6-4a6a-b4a0-3f1c2d9e1000";

BLEServer* bleProvServer = nullptr;
BLEService* bleProvService = nullptr;
BLECharacteristic* bleProvStatusChar = nullptr;
bool bleProvisioningActive = false;
String blePendingSsid = "";
String blePendingPassword = "";
bool bleApplyRequested = false;

class BleSsidCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    blePendingSsid = value;
    blePendingSsid.trim();
    DEBUG_PRINTF("BLE SSID received (%d chars)\n", blePendingSsid.length());
  }
};

class BlePasswordCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    blePendingPassword = value;
    DEBUG_PRINTF("BLE password received (%d chars)\n", blePendingPassword.length());
  }
};

class BleApplyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String cmd = pCharacteristic->getValue();
    cmd.trim();
    cmd.toUpperCase();
    if (cmd == "APPLY" || cmd == "SAVE") {
      bleApplyRequested = true;
    }
  }
};

void setupMQTTBuffer() {
  mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
}

bool wifiEnabled = false;
bool mqttEnabled = false;
uint32_t lastMqttReconnect = 0;
uint32_t lastSensorPublish = 0;
uint32_t lastStatusPublish = 0;
uint32_t manualSleepTime = 0;
const uint32_t MANUAL_SLEEP_COOLDOWN = 5000;
const uint32_t MQTT_RECONNECT_INTERVAL = MQTT_RECONNECT_INTERVAL_MS;
const uint32_t SENSOR_PUBLISH_INTERVAL = SENSOR_PUBLISH_INTERVAL_MS; // Snellere updates
const uint32_t STATUS_PUBLISH_INTERVAL = STATUS_PUBLISH_INTERVAL_MS;

// CRITICAL FIX: Exponential backoff for MQTT reconnection
static uint32_t mqttFailureCount = 0;
static const uint32_t MAX_MQTT_FAILURES = 12;  // Max retries before giving up (60 sec at 5s intervals)
static const uint32_t MAX_MQTT_BACKOFF_MS = 300000;  // Max 5 minutes between attempts

struct Runner {
  int      idx = 0;
  uint32_t startAt = 0;
  uint32_t lastStep = 0;
  int      sub = 0;
  float    a = 0, b = 0;
};

enum class Behavior : uint8_t {
  Wake,
  Sleep,
  WakeUpSequence,
  Follow,
  Curious,
  NodYes,
  ShakeNo,
  StartupCelebration
};

const char* behaviorName(Behavior b);
Behavior findBehaviorByName(const char* name);
void triggerBehavior(Behavior behavior);
void showNotification(String message, uint16_t color, uint32_t duration);
void drawNotificationBar();
void processPresenceNotifications();


// ---------------- Oogvormen (EMOTIE-SPECIFIEK) ----------------
extern const EyeShape neutralShape    = { 75, 110, 35 };  // Normaal/neutraal - rond en open
extern const EyeShape baseShape       = { 75, 110, 35 };  // Alias voor neutral
extern const EyeShape happyShape      = { 95, 125, 45 };  // Blij - groter en ronder, expressief
extern const EyeShape blinkShape      = { 115, 10, 5  };  // Knipperen - breed en dun
extern const EyeShape wakeShape       = { 85, 120, 40 };  // Wakker - iets groter dan normaal
extern const EyeShape angryShape      = { 64, 74, 14 };   // Boos - net iets smaller
extern const EyeShape annoyedShape    = { 65, 95, 28 };   // GeÃ¯rriteerd - iets smaller dan normaal
extern const EyeShape irritatedShape  = { 85, 85, 3 };    // GeÃ¯rriteerd na shake - VIERKANT (lage radius)
extern const EyeShape boredShape      = { 120, 55,  22 };  // Verveeld - breed + half dicht
extern const EyeShape proudShape      = { 65,  125, 32 };  // Trots - smal + hoog
extern const EyeShape shyShape        = { 60,  80,  25 };  // Verlegen - klein + teruggetrokken
extern const EyeShape flirtyShape     = { 95,  120, 42 };  // Flirterig
extern const EyeShape sleepShape      = { 110, 18, 8  };  // Slapen - breed en dun
extern const EyeShape sadShape        = { 70, 120, 35 };  // Verdrietig - smaller, lang
extern const EyeShape surprisedShape  = { 90, 130, 50 };  // Verrast - groot en rond
extern const EyeShape thinkingShape   = { 75, 95, 35 };   // Denkend - iets smaller
extern const EyeShape excitedShape    = { 100, 130, 50 }; // Opgewonden - groot
extern const EyeShape confusedShape   = { 70, 105, 30 };  // Verward - smaller
extern const EyeShape scaredShape     = { 95, 135, 50 };  // Bang - groot en rond
extern const EyeShape sleepyShape     = { 88, 78, 26 };   // Slaperig - open ogen, wat zwaarder
extern const EyeShape winkShape       = { 110, 12, 6  };  // Knipoog
extern const EyeShape loveShape       = { 90, 125, 45 };  // Liefdevol - zacht en rond
extern const EyeShape suspiciousShape = { 65, 100, 20 };  // Wantrouwig - strak
extern const EyeShape dizzyShape      = { 80, 110, 8  };  // Duizelig - vierkanter

// ---------------- Globals (render) ----------------
M5Canvas canvas(&M5.Display);
int    centerX, centerY;
int    eyeSpacing, glowOffset;
float screenScale = 1.0f;
float MAX_LOOK_X, MAX_LOOK_Y;

EyeShape currentShape = baseShape;

float eyeLookX = 0, eyeLookY = 0;
float targetLookX = 0, targetLookY = 0;
float eyeOpenness = 1.0f; // 1.0 = fully open, 0.0 = closed

// Animation override - disables proximity tracking temporarily
uint32_t animationStartTime = 0;
uint32_t animationEndTime = 0;
bool isAnimatingEyes = false;
enum AnimationType { ANIM_NONE, ANIM_YES, ANIM_NO };
AnimationType currentAnimation = ANIM_NONE;

// Proximity-based eye tracking
float proximityEyeX = 0, proximityEyeY = 0;
uint32_t lastProximityEyeUpdate = 0;
uint32_t proximityEyeChangeTime = 0;
bool proximityEyeTracking = false;

float depthScale = 1.0f;
float asym3D     = 0.0f;

uint32_t nextBlinkAt = 0;
bool     blinking    = false;
uint32_t blinkStart  = 0;

uint32_t lastFrameMs = 0;

enum class RenderMode : uint8_t { Normal };
RenderMode renderMode = RenderMode::Normal;

float tiltOffsetL = 0.0f;
float tiltOffsetR = 0.0f;
float skewXL = 0.0f;
float skewXR = 0.0f;

enum class EyeNotifyVisual : uint8_t { None, Mail, Alarm, AlarmClock };
EyeNotifyVisual eyeNotifyVisual = EyeNotifyVisual::None;
uint32_t eyeNotifyUntil = 0;
float lidDropProgress = 0.0f; // 0.0 = open, 1.0 = gesloten (Sleepy emotie)

// Notification text overlay state (drawn on top of eyes).
String notificationText = "";
uint16_t notificationColor = TFT_WHITE;
uint32_t notificationUntil = 0;
bool notificationActive = false;

// ---------------- Helpers ----------------
static inline float clampf(float v, float a, float b){ return v < a ? a : (v > b ? b : v); }
static inline float mylerp(float a, float b, float t){ return a + (b - a) * t; }
int iround(float v){ return (int)lroundf(v); }
uint32_t rnd(uint32_t a, uint32_t b){
  if (b < a) return a; // Guard tegen verkeerde invoer
  return a + (esp_random() % (b - a + 1));
}

static inline bool isPersonalityActive() {
  return (currentPersonality != PersonalityExtended::Neutral) &&
         (personalityUntil > 0) && (millis() < personalityUntil);
}

static inline uint32_t randomBlinkDelayForCurrentPersonality() {
  uint32_t minMs = BLINK_MIN_MS;
  uint32_t maxMs = BLINK_MAX_MS;

  if (isPersonalityActive()) {
    switch (currentPersonality) {
      case PersonalityExtended::Thinking:
        minMs = 5200; maxMs = 9000;  // Langzamer knipperen
        break;
      case PersonalityExtended::Confused:
        minMs = 1200; maxMs = 2300;  // Meer knipperen
        break;
      case PersonalityExtended::Angry:
        minMs = 1600; maxMs = 2800;
        break;
      case PersonalityExtended::Sad:
        minMs = 3600; maxMs = 6200;
        break;
      case PersonalityExtended::Excited:
        minMs = 1700; maxMs = 2900;
        break;
      case PersonalityExtended::Sleepy:
        minMs = 3800; maxMs = 7600;
        break;
      default:
        break;
    }
  }
  return rnd(minMs, maxMs);
}

float smoothTo(float cur, float target, float dt_ms, float tau_ms){
  float alpha = (tau_ms <= 0.0f) ? 1.0f : (1.0f - expf(-dt_ms / tau_ms));
  alpha = clampf(alpha, 0.0f, 1.0f);
  return cur + (target - cur) * alpha;
}

inline void setTarget(float x, float y){
  targetLookX = clampf(x, -MAX_LOOK_X, MAX_LOOK_X);
  targetLookY = clampf(y, -MAX_LOOK_Y, MAX_LOOK_Y);
}

void publishWakeReason(const char* reason) {
  lastWakeReason = (reason && reason[0] != '\0') ? String(reason) : String("unknown");
  lastWakeReasonAt = millis();
  if (mqttEnabled && mqttClient.connected()) {
    mqttClient.publish("bob/wake_reason", lastWakeReason.c_str(), true);
  }
}

void wakeBobFromSleep(const char* reason, bool triggerWakeSequence) {
  bool wasSleeping = isAsleep || isGoingToSleep || !screenOn;
  isAsleep = false;
  isGoingToSleep = false;
  sleepPhase2Printed = false;
  screenOn = true;
  M5.Display.wakeup();
  M5.Display.setBrightness(screenBrightness);
  lastInteractionTime = millis();
  lastProximityTime = lastInteractionTime;
  publishWakeReason(reason);

  if (triggerWakeSequence && wasSleeping) {
    triggerBehavior(Behavior::WakeUpSequence);
  }
}

// Improved proximity-based eye tracking - Direct and responsive finger tracking

void applyPersonality(PersonalityExtended p, uint32_t durationMs = 4000) {
  if (p == PersonalityExtended::Neutral) {
    return;
  }
  currentPersonality = p;
  personalityUntil = millis() + durationMs;
  if (p == PersonalityExtended::Happy || p == PersonalityExtended::Excited) {
    uint32_t closeMs = (durationMs < 2600) ? durationMs : 2600;
    emotionCloseUntil = millis() + closeMs;
  }
  if (p == PersonalityExtended::Sleepy) {
    sleepyMicroNapAt = millis() + random(1800, 5200);
  } else {
    sleepyMicroNapAt = 0;
    sleepyStartleFrom = 0;
    sleepyStartleUntil = 0;
  }
  if (p == PersonalityExtended::Surprised) {
    // Verrast: neutrale oogvorm, maar groter via renderer + korte dubbel-reactie.
    startEmotionTween(neutralShape, 170);
    surprisedPulseStart = millis();
    surprisedPulseEnd = surprisedPulseStart + 980;
    surprisedBlinkAt1 = surprisedPulseStart + 120;
    surprisedBlinkAt2 = surprisedPulseStart + 420;
    surprisedBlinkStep = 0;
  } else if (p == PersonalityExtended::Dizzy) {
    // Dizzy: neutrale oogvorm, alleen beweging maakt de emotie.
    startEmotionTween(neutralShape, 170);
    surprisedPulseStart = 0;
    surprisedPulseEnd = 0;
    surprisedBlinkAt1 = 0;
    surprisedBlinkAt2 = 0;
    surprisedBlinkStep = 0;
  } else {
    startEmotionTween(getExtendedPersonalityShape(p), 220);
    surprisedPulseStart = 0;
    surprisedPulseEnd = 0;
    surprisedBlinkAt1 = 0;
    surprisedBlinkAt2 = 0;
    surprisedBlinkStep = 0;
  }
  nextBlinkAt = millis() + randomBlinkDelayForCurrentPersonality();
}


// ---------------- Blink ----------------
// ---------------- IMU triggers â†’ gedrag ----------------
bool initializeCamera() {
  Serial.println("Initializing camera for snapshots...");

  if (!CoreS3.Camera.begin()) {
    Serial.println(" Camera initialization failed");
    return false;
  }

  // Set camera settings for better visibility
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    // Use CoreS3/GC0308 library defaults (no manual image tuning).
    Serial.println(" Camera defaults active (CoreS3 GC0308)");
  }

  Serial.println(" Camera initialized successfully");
  cameraInitialized = true;
  return true;
}

bool ensureCameraInitialized() {
  if (cameraInitialized) return true;

  // Retry once: first init can occasionally fail on cold start.
  if (initializeCamera()) {
    // Warmup frames: first frame(s) after init can be invalid/empty.
    for (int i = 0; i < 2; ++i) {
      if (CoreS3.Camera.get()) {
        CoreS3.Camera.free();
      }
      delay(30);
    }
    return true;
  }
  delay(120);
  if (initializeCamera()) {
    for (int i = 0; i < 2; ++i) {
      if (CoreS3.Camera.get()) {
        CoreS3.Camera.free();
      }
      delay(30);
    }
    return true;
  }
  return false;
}

static bool isFrameFlatRGB565(const camera_fb_t* fb) {
  (void)fb;
  return false;
}

static bool captureUsableFrame(uint8_t maxAttempts = 4) {
  for (uint8_t attempt = 1; attempt <= maxAttempts; ++attempt) {
    if (!CoreS3.Camera.get()) {
      delay(25);
      continue;
    }
    camera_fb_t* fb = CoreS3.Camera.fb;
    if (!fb || !fb->buf || fb->len == 0) {
      CoreS3.Camera.free();
      delay(25);
      continue;
    }

    return true; // Keep this frame; caller must free().
  }
  return false;
}

static bool frameToJpeg(camera_fb_t* fb, uint8_t** outBuf, size_t* outLen, bool* outMustFree, uint8_t quality = 80) {
  if (!fb || !fb->buf || fb->len == 0 || !outBuf || !outLen || !outMustFree) return false;
  *outBuf = nullptr;
  *outLen = 0;
  *outMustFree = false;

  if (fb->format == PIXFORMAT_JPEG) {
    *outBuf = fb->buf;
    *outLen = fb->len;
    return true;
  }

  uint8_t* jpg = nullptr;
  size_t jpgLen = 0;
  if (!frame2jpg(fb, quality, &jpg, &jpgLen) || !jpg || jpgLen == 0) {
    // Fallback converter path for boards that provide RGB565 buffers where frame2jpg fails.
    jpg = nullptr;
    jpgLen = 0;
    if (!fmt2jpg(fb->buf, fb->len, fb->width, fb->height, fb->format, quality, &jpg, &jpgLen) || !jpg || jpgLen == 0) {
      return false;
    }
  }
  *outBuf = jpg;
  *outLen = jpgLen;
  *outMustFree = true;
  return true;
}

// Camera snapshot function - takes photo and publishes to MQTT
void takeCameraSnapshot() {
  Serial.println(" Taking camera snapshot...");

  if (!ensureCameraInitialized()) {
    Serial.println("Camera not initialized");
    if (mqttEnabled && mqttClient.connected()) {
      mqttClient.publish("bob/camera/status", "error: camera not initialized");
    }
    return;
  }

  // Get camera frame (with retries against flat/invalid frames).
  if (!captureUsableFrame()) {
    Serial.println(" Camera.get() failed or unusable frame");
    if (mqttEnabled && mqttClient.connected()) {
      mqttClient.publish("bob/camera/status", "error: failed to capture");
    }
    return;
  }

  camera_fb_t* fb = CoreS3.Camera.fb;
  if (!fb || !fb->buf) {
    CoreS3.Camera.free();
    Serial.println(" No frame buffer!");
    if (mqttEnabled && mqttClient.connected()) {
      mqttClient.publish("bob/camera/status", "error: no frame buffer");
    }
    return;
  }

  Serial.printf(" Snapshot captured: %dx%d, %d bytes, format: %d\n",
                fb->width, fb->height, fb->len, fb->format);

  if (!mqttEnabled || !mqttClient.connected()) {
    Serial.println("MQTT not connected - skipping snapshot publish");
    CoreS3.Camera.free();
    return;
  }

  // Publish complete JPEG image to MQTT for Home Assistant camera entity.
  uint8_t* jpgBuf = nullptr;
  size_t jpgLen = 0;
  bool freeJpgBuf = false;
  bool converted = frameToJpeg(fb, &jpgBuf, &jpgLen, &freeJpgBuf, 80);
  bool success = false;
  if (converted) {
    success = mqttClient.publish("bob/camera/image", jpgBuf, jpgLen, false);
  }

  if (freeJpgBuf) {
    free(jpgBuf);
  }
  CoreS3.Camera.free();
  mqttClient.loop();

  if (success && converted) {
    Serial.printf(" Image published to MQTT (%d bytes)\n", (int)jpgLen);
    mqttClient.publish("bob/camera/status", "snapshot complete");
  } else if (!converted) {
    Serial.println(" Failed to convert snapshot to JPEG");
    mqttClient.publish("bob/camera/status", "error: jpeg conversion failed");
  } else {
    Serial.println(" Failed to publish image to MQTT");
    mqttClient.publish("bob/camera/status", "error: failed to publish");
  }
}

// HTTP handler for camera snapshot (REST API)
void handleCameraSnapshot() {
  Serial.println(" HTTP snapshot request received");

  if (wifiSetupPortalActive) {
    webServer.send(503, "text/plain", "Camera disabled during setup mode");
    return;
  }

  // Snapshot and live stream can conflict on some camera drivers.
  // Pause stream briefly to get a stable still frame.
  bool resumeStream = cameraStreaming;
  if (cameraStreaming) {
    stopCameraStreaming();
    delay(40);
  }
  bool sent = false;
  const char* errorMessage = "Snapshot failed";

  for (int cycle = 0; cycle < 2 && !sent; ++cycle) {
    if (cycle > 0) {
      // One recovery cycle for first-request failures after idle/cold camera state.
      cameraInitialized = false;
      delay(120);
    }

    if (!ensureCameraInitialized()) {
      errorMessage = "Camera not initialized";
      continue;
    }

    if (!captureUsableFrame(8)) {
      errorMessage = "Failed to capture image";
      continue;
    }

    camera_fb_t* fb = CoreS3.Camera.fb;
    if (!fb || !fb->buf || fb->len == 0) {
      if (fb) CoreS3.Camera.free();
      errorMessage = "No frame buffer";
      continue;
    }

    Serial.printf(" Sending image source: %dx%d, %d bytes, fmt=%d\n", fb->width, fb->height, fb->len, fb->format);

    // Ensure we always return JPEG for browser compatibility.
    uint8_t* jpgBuf = nullptr;
    size_t jpgLen = 0;
    bool freeJpgBuf = false;
    if (!frameToJpeg(fb, &jpgBuf, &jpgLen, &freeJpgBuf, 80)) {
      CoreS3.Camera.free();
      errorMessage = "Snapshot JPEG conversion failed";
      continue;
    }

    webServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    webServer.sendHeader("Pragma", "no-cache");
    webServer.sendHeader("Content-Disposition", "inline; filename=snapshot.jpg");
    webServer.send_P(200, "image/jpeg", (const char*)jpgBuf, jpgLen);
    Serial.printf(" Snapshot sent as JPEG: %d bytes\n", (int)jpgLen);

    if (freeJpgBuf) {
      free(jpgBuf);
    }
    CoreS3.Camera.free();
    sent = true;
  }

  if (!sent) {
    webServer.send(500, "text/plain", errorMessage);
  } else {
    Serial.println(" Image sent via HTTP");
  }

  if (resumeStream) {
    startCameraStreaming();
  }
}


void handleDashboard() {
  String html = F(R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bob</title>
<style>
:root{--bg:#0a1520;--panel:rgba(255,255,255,.07);--line:rgba(255,255,255,.13);--text:#f0f6ff;--muted:#8aaccc;--accent:#34d6a8;--accent2:#4aa8ff;--danger:#ff5566;--warn:#ffcc44}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:var(--bg);color:var(--text);min-height:100vh;display:flex;flex-direction:column}
.topbar{display:flex;align-items:center;gap:8px;padding:10px 16px;background:rgba(0,0,0,.3);border-bottom:1px solid var(--line);flex-wrap:wrap}
.topbar h1{font-size:1.2rem;margin-right:8px}
.pill{padding:4px 10px;border-radius:999px;font-size:.8rem;border:1px solid var(--line);background:rgba(255,255,255,.05)}
.pill.ok{border-color:rgba(52,214,168,.5);color:#7fefd4}
.pill.off{border-color:rgba(255,85,102,.4);color:#ff8899}
.pill.sleep{border-color:rgba(100,130,200,.4);color:#aac4ff}
.main{display:flex;flex:1;gap:0;align-items:stretch}
.hero{width:260px;min-width:220px;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:20px;border-right:1px solid var(--line)}
.hero img{width:auto;max-width:220px;height:auto;border-radius:16px}
.sidebar{flex:1;padding:16px;overflow-y:auto;display:flex;flex-direction:column;gap:14px}
.section-title{font-size:.75rem;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin-bottom:6px}
.emotion-grid{display:grid;grid-template-columns:repeat(6,1fr);gap:4px}
.ebtn{border:1px solid var(--line);background:var(--panel);border-radius:8px;padding:6px 2px;cursor:pointer;font-size:1.1rem;text-align:center;transition:background 120ms}
.ebtn:hover,.ebtn.active{background:rgba(52,214,168,.18);border-color:var(--accent)}
.ebtn span{display:block;font-size:.55rem;color:var(--muted);margin-top:2px}
.mini-eyes{display:flex;justify-content:center;align-items:center;gap:4px;height:16px;margin-bottom:2px}
.mini-eyes i{display:block;width:9px;height:12px;border-radius:5px;background:#fff;box-shadow:0 0 0 1px rgba(255,255,255,.12),0 0 6px rgba(255,255,255,.2)}
.mini-eyes.e-happy i{height:8px;margin-top:3px;border-radius:8px}
.mini-eyes.e-sad i{height:10px;margin-top:0;border-radius:5px 5px 8px 8px}
.mini-eyes.e-thinking i{width:8px;height:11px}
.mini-eyes.e-excited i{width:10px;height:13px}
.mini-eyes.e-confused i:first-child{height:9px}.mini-eyes.e-confused i:last-child{height:12px}
.mini-eyes.e-angry i:first-child{transform:skewX(10deg)}.mini-eyes.e-angry i:last-child{transform:skewX(-10deg)}
.mini-eyes.e-scared i{width:10px;height:14px;border-radius:6px}
.mini-eyes.e-sleepy i{width:12px;height:3px;border-radius:3px}
.mini-eyes.e-wink i:first-child{width:12px;height:3px;border-radius:3px}.mini-eyes.e-wink i:last-child{height:12px}
.mini-eyes.e-love i{width:10px;height:13px;border-radius:6px}
.mini-eyes.e-surprised i{width:10px;height:14px;border-radius:7px}
.mini-eyes.e-suspicious i:first-child{width:12px;height:4px;border-radius:3px}.mini-eyes.e-suspicious i:last-child{height:10px}
.mini-eyes.e-dizzy i{width:9px;height:9px;border-radius:2px}
.mini-eyes.e-bored i{width:12px;height:5px;border-radius:4px}
.mini-eyes.e-proud i{width:8px;height:13px}
.mini-eyes.e-shy i{width:8px;height:9px}
.mini-eyes.e-flirty i:first-child{width:12px;height:4px;border-radius:3px}.mini-eyes.e-flirty i:last-child{height:11px}
.action-row{display:flex;flex-wrap:wrap;gap:6px}
.btn{padding:8px 12px;border-radius:9px;border:1px solid var(--line);background:var(--panel);color:var(--text);cursor:pointer;font-size:.85rem;transition:background 120ms}
.btn:hover{background:rgba(255,255,255,.12)}
.btn.primary{background:linear-gradient(135deg,var(--accent),#1ea87a);border:none;color:#041a10;font-weight:700}
.btn.danger{border-color:rgba(255,85,102,.4);color:#ff8899}
.btn.ico{display:inline-flex;align-items:center;gap:7px}
.btn.ico svg{width:24px;height:24px;display:block}
.toggle{padding:8px 12px;border-radius:9px;border:1px solid var(--line);background:var(--panel);color:var(--muted);cursor:pointer;font-size:.85rem}
.toggle.on{background:rgba(52,214,168,.15);border-color:var(--accent);color:var(--accent)}
.slider-row{display:flex;align-items:center;gap:10px}
.slider-row input[type=range]{flex:1;accent-color:var(--accent)}
.slider-row.compact input[type=range]{flex:0 0 190px;max-width:190px}
.slider-val{min-width:32px;text-align:right;font-size:.85rem;color:var(--muted)}
.text-input{width:100%;max-width:360px;padding:9px 10px;border-radius:9px;border:1px solid var(--line);background:var(--panel);color:var(--text);font-size:.9rem}
.link{text-decoration:none;color:var(--accent2);font-size:.85rem}
@media(max-width:1200px){.main{flex-direction:column}.hero{width:100%;min-width:0;border-right:none;border-bottom:1px solid var(--line);padding:12px}.hero img{max-width:360px}.emotion-grid{grid-template-columns:repeat(6,1fr)}.slider-row.compact input[type=range]{flex:1;max-width:none}}
</style>
</head>
<body>
<div class="topbar">
  <h1>Bob</h1>
  <span class="pill" id="p-behavior">Wake</span>
  <span class="pill" id="p-mqtt">MQTT</span>
  <span class="pill" id="p-wifi">WiFi</span>
  <a class="link" href="/setup" style="margin-left:auto">&#9881; Instellingen</a>
</div>
<div class="main">
  <div class="hero">
    <img src="data:image/jpeg;base64,)HTML");

  html += String(BOB_IMAGE_B64);

  html += F(R"HTML(" alt="Bob">
  </div>
  <div class="sidebar">

    <div>
      <div class="section-title">Emoties</div>
      <div class="emotion-grid" id="egrid">
        <button class="ebtn" data-emotion="neutral" onclick="setE('neutral',this)"><div class="mini-eyes"><i></i><i></i></div><span>Neutral</span></button>
        <button class="ebtn" data-emotion="happy" onclick="setE('happy',this)"><div class="mini-eyes e-happy"><i></i><i></i></div><span>Happy</span></button>
        <button class="ebtn" data-emotion="sad" onclick="setE('sad',this)"><div class="mini-eyes e-sad"><i></i><i></i></div><span>Sad</span></button>
        <button class="ebtn" data-emotion="thinking" onclick="setE('thinking',this)"><div class="mini-eyes e-thinking"><i></i><i></i></div><span>Thinking</span></button>
        <button class="ebtn" data-emotion="excited" onclick="setE('excited',this)"><div class="mini-eyes e-excited"><i></i><i></i></div><span>Excited</span></button>
        <button class="ebtn" data-emotion="confused" onclick="setE('confused',this)"><div class="mini-eyes e-confused"><i></i><i></i></div><span>Confused</span></button>
        <button class="ebtn" data-emotion="angry" onclick="setE('angry',this)"><div class="mini-eyes e-angry"><i></i><i></i></div><span>Angry</span></button>
        <button class="ebtn" data-emotion="scared" onclick="setE('scared',this)"><div class="mini-eyes e-scared"><i></i><i></i></div><span>Scared</span></button>
        <button class="ebtn" data-emotion="sleepy" onclick="setE('sleepy',this)"><div class="mini-eyes e-sleepy"><i></i><i></i></div><span>Sleepy</span></button>
        <button class="ebtn" data-emotion="love" onclick="setE('love',this)"><div class="mini-eyes e-love"><i></i><i></i></div><span>Love</span></button>
        <button class="ebtn" data-emotion="surprised" onclick="setE('surprised',this)"><div class="mini-eyes e-surprised"><i></i><i></i></div><span>Surprised</span></button>
        <button class="ebtn" data-emotion="dizzy" onclick="setE('dizzy',this)"><div class="mini-eyes e-dizzy"><i></i><i></i></div><span>Dizzy</span></button>
        <button class="ebtn" data-emotion="bored" onclick="setE('bored',this)"><div class="mini-eyes e-bored"><i></i><i></i></div><span>Bored</span></button>
      </div>
    </div>

    <div>
      <div class="section-title">Acties</div>
      <div class="action-row">
        <button class="btn primary" onclick="act('wake')">&#9200; Wake</button>
        <button class="btn" onclick="act('sleep')">&#128164; Sleep</button>
        <button class="btn" onclick="act('blink')">&#128065; Blink</button>
      </div>
      <div class="action-row" style="margin-top:6px">
        <button class="btn" onclick="act('curious')">&#128269; Curious</button>
        <button class="btn" onclick="act('nod_yes')">&#9989; Yes</button>
        <button class="btn" onclick="act('shake_no')">&#10060; No</button>
      </div>
    </div>

    <div>
      <div class="section-title">Tekst</div>
      <div class="action-row">
        <input id="notify-text" class="text-input" type="text" maxlength="120" placeholder="Typ een bericht..." onkeydown="if(event.key==='Enter'){event.preventDefault();sendNotifyText();}">
        <button class="btn" onclick="sendNotifyText()">Verstuur</button>
      </div>
    </div>

    <div>
      <div class="section-title">Camera</div>
      <div class="action-row">
        <button class="btn" onclick="cameraSnapshot()">&#128248; Snapshot</button>
        <button class="toggle" id="t-cam" onclick="toggleCameraStream()">&#127909; Stream</button>
      </div>
      <div style="margin-top:8px;border:1px solid var(--line);border-radius:12px;overflow:hidden;background:#000;max-width:360px">
        <img id="cam-preview" alt="Camera preview" src="/snapshot?ts=0" style="display:block;width:100%;height:auto">
      </div>
    </div>

    <div>
      <div class="section-title">Live Modes</div>
      <div class="action-row">
        <button class="toggle" id="t-matrix" onclick="toggleMode('matrix')">&#129001; Matrix</button>
        <button class="toggle" id="t-snow" onclick="toggleMode('snow')">&#10052; Snow</button>
        <button class="btn" onclick="act('celebrate')">&#127881; Celebrate</button>
        <button class="toggle" id="t-saver" onclick="toggleMode('screensaver')">&#128336; Screensaver</button>
        <button class="toggle" id="t-clock" onclick="toggleMode('clock')">&#128336; Clock</button>
      </div>
    </div>

    <div>
      <div class="section-title">Helderheid</div>
      <div class="slider-row compact">
        <input type="range" min="1" max="100" id="brightness-slider" oninput="setBrightness(this.value)">
        <span class="slider-val" id="brightness-val">50</span>
      </div>
    </div>

    <div>
      <div class="section-title">Wakker Tijd (sec)</div>
      <div class="slider-row compact">
        <input type="range" min="10" max="3600" step="10" id="wake-slider" oninput="setWakeTime(this.value)">
        <span class="slider-val" id="wake-val">60</span>
      </div>
    </div>

  </div>
</div>
<script>
let matrixOn=false,snowOn=false,camOn=false,webStreamOn=false,webStreamTimer=null;
function post(data){fetch('/control/run',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(data)}).catch(()=>{});}
function act(a){post({action:a});}
function setE(v,btn){
  post({action:'personality',value:v});
  document.querySelectorAll('.ebtn').forEach(b=>b.classList.remove('active'));
  if(btn) btn.classList.add('active');
}
function sendNotifyText(){
  const el=document.getElementById('notify-text');
  const txt=(el&&el.value?el.value.trim():'');
  if(!txt)return;
  post({action:'notify',value:'generic',text:txt});
  el.value='';
}
function loadSnapshot(){const img=document.getElementById('cam-preview');img.src='/snapshot?ts='+Date.now();}
function startWebStream(){if(webStreamOn)return;webStreamOn=true;loadSnapshot();webStreamTimer=setInterval(loadSnapshot,350);}
function stopWebStream(){webStreamOn=false;if(webStreamTimer){clearInterval(webStreamTimer);webStreamTimer=null;}}
function syncWebStream(){if(camOn)startWebStream();else stopWebStream();}
function cameraSnapshot(){loadSnapshot();post({action:'camera_snapshot'});}
function toggleCameraStream(){
  camOn=!camOn;
  post({action:'camera_stream',value:camOn?'on':'off'});
  document.getElementById('t-cam').classList.toggle('on',camOn);
  syncWebStream();
}
function toggleMode(m){
  if(m==='matrix'){matrixOn=!matrixOn;post({action:'matrix',value:matrixOn?'on':'off'});document.getElementById('t-matrix').classList.toggle('on',matrixOn);}
  else if(m==='snow'){snowOn=!snowOn;post({action:'confetti',value:snowOn?'snow':'off'});document.getElementById('t-snow').classList.toggle('on',snowOn);}
  else if(m==='clock'){const el=document.getElementById('t-clock');const on=!el.classList.contains('on');post({action:'clock',value:on?'on':'off'});el.classList.toggle('on',on);}
  else if(m==='screensaver'){const el=document.getElementById('t-saver');const on=!el.classList.contains('on');post({action:'screensaver',value:on?'on':'off'});el.classList.toggle('on',on);document.getElementById('t-clock').classList.toggle('on',on);}
}
let bTimer;
function setBrightness(v){
  const pct=Math.max(1,Math.min(100,parseInt(v||'50',10)));
  document.getElementById('brightness-val').textContent=pct;
  const raw=Math.round((pct*255)/100);
  clearTimeout(bTimer);
  bTimer=setTimeout(()=>post({action:'brightness',value:String(raw)}),300);
}
let wTimer;
function setWakeTime(v){document.getElementById('wake-val').textContent=v;clearTimeout(wTimer);wTimer=setTimeout(()=>post({action:'wake_time',value:v}),300);}
function pollStatus(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    const em=(d.personality||'neutral').toLowerCase();
    document.querySelectorAll('.ebtn').forEach(b=>b.classList.toggle('active', b.dataset.emotion===em));
    document.getElementById('p-behavior').textContent=d.behavior||'';
    const mq=document.getElementById('p-mqtt');
    mq.textContent=d.mqtt?'MQTT \u2713':'MQTT \u2717';
    mq.className='pill '+(d.mqtt?'ok':'off');
    const wf=document.getElementById('p-wifi');
    wf.textContent=d.wifi?'WiFi \u2713':'WiFi \u2717';
    wf.className='pill '+(d.wifi?'ok':'off');
    if(d.sleeping)document.getElementById('p-behavior').className='pill sleep';
    else document.getElementById('p-behavior').className='pill';
    camOn=!!d.camera_streaming;
    document.getElementById('t-cam').classList.toggle('on',camOn);
    document.getElementById('t-clock').classList.toggle('on',!!d.clock_mode);
    document.getElementById('t-saver').classList.toggle('on',!!d.clock_mode);
    syncWebStream();
    const sl=document.getElementById('brightness-slider');
    if(document.activeElement!==sl){
      const pct=Math.max(1,Math.min(100,Math.round(((d.brightness||0)*100)/255)));
      sl.value=pct;
      document.getElementById('brightness-val').textContent=pct;
    }
    const wl=document.getElementById('wake-slider');
    if(document.activeElement!==wl && typeof d.wake_time_sec!=='undefined'){
      wl.value=d.wake_time_sec;
      document.getElementById('wake-val').textContent=d.wake_time_sec;
    }
  }).catch(()=>{});
}
setInterval(pollStatus,3000);
pollStatus();
loadSnapshot();
</script>
</body>
</html>)HTML");

  webServer.send(200, "text/html", html);
}

void triggerWebNotificationPreview(const String& rawType, const String& customText) {
  String t = rawType;
  String msg = customText;
  t.trim(); t.toLowerCase(); t.replace(" ", "_"); t.replace("-", "_");
  msg.trim();
  if (t.length() == 0) t = "generic";

  PersonalityExtended p = PersonalityExtended::Neutral;
  const char* soundKey = "chime";
  eyeNotifyVisual = EyeNotifyVisual::None;
  uint32_t visualMs = 2600;

  // Als gebruiker tekst invult: altijd zichtbare emotionele reactie geven.
  if (msg.length() > 0 && t == "generic") {
    p = PersonalityExtended::Thinking;
    visualMs = 3200;
  }

  if (t == "mail" || t == "brief" || t == "post") {
    p = PersonalityExtended::Thinking; soundKey = "mail";
    eyeNotifyVisual = EyeNotifyVisual::Mail; visualMs = 3200;
  } else if (t == "warning" || t == "alarm" || t == "alert") {
    p = PersonalityExtended::Scared; soundKey = (t == "warning") ? "warning" : "alarm";
    eyeNotifyVisual = EyeNotifyVisual::Alarm; visualMs = 3400;
  } else if (t == "alarm_clock" || t == "wekker" || t == "timer") {
    p = PersonalityExtended::Excited; soundKey = "alarm";
    eyeNotifyVisual = EyeNotifyVisual::AlarmClock; visualMs = 4000;
  }

  if (eyeNotifyVisual != EyeNotifyVisual::None) {
    eyeNotifyUntil = millis() + visualMs;
  }

  if (msg.length() > 0) {
    showNotification(msg, TFT_WHITE, visualMs);
  }

  if (p != PersonalityExtended::Neutral) applyPersonality(p, 4500);

  if (mqttEnabled && mqttClient.connected()) {
    mqttClient.publish("bob/notify_sound", soundKey, false);
    mqttClient.publish("bob/notify_state", "shown", true);
  }
}

void handleControlAction() {
  String action = webServer.arg("action");
  String value = webServer.arg("value");
  String text = webServer.arg("text");
  action.trim();
  value.trim();
  text.trim();
  action.toLowerCase();
  value.toLowerCase();

  if (action.length() == 0) {
    webServer.send(400, "text/plain", "Missing action");
    return;
  }

  bool touchedState = false;
  bool actionHandled = false;
  bool resetSleepTimer = true;

  // Alle acties (behalve expliciet slapen/wake) moeten Bob eerst wakker maken.
  if (action != "sleep" && action != "wake") {
    wakeBobFromSleep("web_action", false);
  }

  if (action == "wake") {
    actionHandled = true;
    wakeBobFromSleep("web", true);
  } else if (action == "sleep") {
    actionHandled = true;
    resetSleepTimer = false; // Expliciete slaapactie moet direct blijven gelden
    triggerBehavior(Behavior::Sleep);
  } else if (action == "wakeup_sequence") {
    actionHandled = true;
    triggerBehavior(Behavior::WakeUpSequence);
  } else if (action == "blink") {
    actionHandled = true;
    startBlink();
  } else if (action == "matrix") {
    actionHandled = true;
    matrixMode = (value == "on");
    if (matrixMode) initMatrix();
  } else if (action == "confetti") {
    actionHandled = true;
    if (value == "snow") {
      confettiSnowEnabled = true;
      confettiCannonsEnabled = false;
      confettiMode = CONFETTI_SNOW;
      initConfetti();
    } else if (value == "cannons") {
      confettiSnowEnabled = false;
      confettiCannonsEnabled = true;
      confettiMode = CONFETTI_CANNONS;
      initConfetti();
    } else {
      confettiSnowEnabled = false;
      confettiCannonsEnabled = false;
      confettiMode = CONFETTI_NONE;
    }
  } else if (action == "tracking") {
    actionHandled = true;
    objectTrackingEnabled = (value == "on");
    touchedState = true;
  } else if (action == "auto_emotion") {
    actionHandled = true;
    autoEmotionEnabled = (value == "on");
    touchedState = true;
  } else if (action == "proximity") {
    actionHandled = true;
    proximityEnabled = (value == "on");
    touchedState = true;
  } else if (action == "auto_brightness") {
    actionHandled = true;
    autoBrightnessEnabled = (value == "on");
    touchedState = true;
  } else if (action == "clock") {
    actionHandled = true;
    clockMode = (value == "on");
    if (clockMode) {
      matrixMode = false;
      confettiMode = CONFETTI_NONE;
    }
  } else if (action == "screensaver") {
    actionHandled = true;
    clockMode = (value == "on");
    if (clockMode) {
      matrixMode = false;
      confettiMode = CONFETTI_NONE;
    }
  } else if (action == "brightness") {
    actionHandled = true;
    int b = value.toInt();
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    screenBrightness = b;
    if (screenOn) {
      M5.Display.setBrightness(screenBrightness);
    }
    touchedState = true;
  } else if (action == "wake_time") {
    actionHandled = true;
    int sec = value.toInt();
    if (sec < 10) sec = 10;
    if (sec > 3600) sec = 3600;
    INACTIVITY_TIMEOUT_MS = (uint32_t)sec * 1000;
    touchedState = true;
  } else if (action == "personality") {
    actionHandled = true;
    bool ok = false;
    PersonalityExtended p = parsePersonality(value, ok);
    if (ok) {
      applyPersonality(p, 4500);
    }
  } else if (action == "notify") {
    actionHandled = true;
    triggerWebNotificationPreview(value, text);
  } else if (action == "camera_snapshot") {
    actionHandled = true;
    takeCameraSnapshot();
  } else if (action == "camera_stream") {
    actionHandled = true;
    // Web-only streaming mode: dashboard refreshes /snapshot, eyes keep rendering.
    cameraWebStreaming = (value == "on");
  } else if (action == "curious") {
    actionHandled = true;
    triggerBehavior(Behavior::Curious);
  } else if (action == "nod_yes") {
    actionHandled = true;
    triggerBehavior(Behavior::NodYes);
  } else if (action == "shake_no") {
    actionHandled = true;
    triggerBehavior(Behavior::ShakeNo);
  } else if (action == "celebrate" || action == "startup_celebration") {
    actionHandled = true;
    // Gecombineerde actie: celebrate + confetti cannons
    confettiSnowEnabled = false;
    confettiCannonsEnabled = true;
    confettiMode = CONFETTI_CANNONS;
    matrixMode = false;
    clockMode = false;
    initConfetti();
    triggerBehavior(Behavior::StartupCelebration);
  }

  if (!actionHandled) {
    webServer.send(400, "text/plain", "Unknown action");
    return;
  }

  if (resetSleepTimer) {
    lastInteractionTime = millis();
    lastProximityTime = lastInteractionTime;
  }

  if (touchedState && mqttEnabled) {
    publishStatus(isAsleep ? "offline" : "online");
  }

  webServer.sendHeader("Location", "/control");
  webServer.send(303, "text/plain", "OK");
}

void handleWifiSetupPage() {
  String fSsid = "";
  String fPassword = "";
  wifiPrefs.begin("wifi", true);
  fSsid = wifiPrefs.getString("ssid", "");
  fPassword = wifiPrefs.getString("password", "");
  wifiPrefs.end();

  String html = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Bob WiFi Setup</title>
  <style>
    :root {
      --bg: #0e1d2d;
      --panel: #162a3e;
      --line: #274562;
      --text: #eef6ff;
      --muted: #adc3d9;
      --accent: #37d3a7;
      --focus: #38a8ff;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: 'Segoe UI', Tahoma, sans-serif;
      background: linear-gradient(160deg, #09131f, var(--bg));
      color: var(--text);
      min-height: 100vh;
      display: grid;
      place-items: center;
      padding: 16px;
    }
    .card {
      width: 100%;
      max-width: 560px;
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 18px;
      padding: 22px;
      box-shadow: 0 18px 45px rgba(0,0,0,0.35);
    }
    h1 { margin: 0 0 8px; }
    p { color: var(--muted); margin-top: 0; }
    label {
      display: block;
      margin: 14px 0 6px;
      font-weight: 600;
    }
    input {
      width: 100%;
      border-radius: 12px;
      border: 1px solid var(--line);
      background: #0d1c2c;
      color: var(--text);
      padding: 12px 14px;
      outline: none;
    }
    input:focus {
      border-color: var(--focus);
      box-shadow: 0 0 0 3px rgba(56,168,255,0.25);
    }
    .row {
      display: flex;
      gap: 10px;
      margin-top: 18px;
      flex-wrap: wrap;
    }
    .btn {
      display: inline-block;
      border: none;
      border-radius: 12px;
      padding: 12px 16px;
      font-weight: 700;
      text-decoration: none;
      cursor: pointer;
    }
    .btn-primary {
      background: linear-gradient(135deg, var(--accent), #2bb388);
      color: #042419;
    }
    .btn-ghost {
      background: transparent;
      border: 1px solid var(--line);
      color: var(--text);
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>WiFi Setup</h1>
    <p>Enter your home network details. Bob saves them locally and restarts automatically.</p>
    <form method="POST" action="/setup/save">
      <label for="ssid">SSID</label>
      <input id="ssid" name="ssid" maxlength="63" required placeholder="e.g. MyWifi" value=")HTML";
  html += fSsid;
  html += R"HTML(">

      <label for="password">Password</label>
      <input id="password" type="password" name="password" maxlength="63" placeholder="********" value=")HTML";
  html += fPassword;
  html += R"HTML(">

      <label for="sleep_timeout">Wake Time (sec)</label>
      <input id="sleep_timeout" name="sleep_timeout" type="number" min="10" max="3600" value=")HTML";
  html += String(INACTIVITY_TIMEOUT_MS / 1000);
  html += R"HTML(">

      <label for="brightness">Brightness (0-255)</label>
      <input id="brightness" name="brightness" type="number" min="0" max="255" value=")HTML";
  html += String(screenBrightness);
  html += R"HTML(">

      <label for="auto_brightness">Auto Brightness</label>
      <input id="auto_brightness" name="auto_brightness" type="checkbox" )HTML";
  if (autoBrightnessEnabled) html += "checked";
  html += R"HTML( style="width:auto; transform:scale(1.25); margin-right:8px;"> Enabled

      <label for="mqtt_enabled">MQTT Integration</label>
      <input id="mqtt_enabled" name="mqtt_enabled" type="checkbox" )HTML";
  if (runtimeMqttEnabled) html += "checked";
  html += R"HTML( style="width:auto; transform:scale(1.25); margin-right:8px;"> Enabled

      <label for="mqtt_host">MQTT Host</label>
      <input id="mqtt_host" name="mqtt_host" maxlength="80" placeholder="192.168.1.10" value=")HTML";
  html += runtimeMqttHost;
  html += R"HTML(">

      <label for="mqtt_port">MQTT Port</label>
      <input id="mqtt_port" name="mqtt_port" type="number" min="1" max="65535" value=")HTML";
  html += String(runtimeMqttPort);
  html += R"HTML(">

      <label for="mqtt_user">MQTT Username</label>
      <input id="mqtt_user" name="mqtt_user" maxlength="63" placeholder="mqtt_user" value=")HTML";
  html += runtimeMqttUser;
  html += R"HTML(">

      <label for="mqtt_pass">MQTT Password</label>
      <input id="mqtt_pass" name="mqtt_pass" type="password" maxlength="63" placeholder="mqtt_password" value=")HTML";
  html += runtimeMqttPass;
  html += R"HTML(">

      <label for="mqtt_cid">MQTT Client ID</label>
      <input id="mqtt_cid" name="mqtt_cid" maxlength="40" placeholder="bob" value=")HTML";
  html += runtimeMqttClientId;
  html += R"HTML(">

      <label for="ha_enabled">Home Assistant Integration</label>
      <input id="ha_enabled" name="ha_enabled" type="checkbox" )HTML";
  if (runtimeHaEnabled) html += "checked";
  html += R"HTML( style="width:auto; transform:scale(1.25); margin-right:8px;"> Enabled

      <label for="ha_url">Home Assistant URL</label>
      <input id="ha_url" name="ha_url" maxlength="120" placeholder="http://homeassistant.local:8123" value=")HTML";
  html += runtimeHaBaseUrl;
  html += R"HTML(">

      <label for="ha_token">Home Assistant Long-Lived Token</label>
      <input id="ha_token" name="ha_token" type="password" maxlength="220" placeholder="Token" value=")HTML";
  html += runtimeHaToken;
  html += R"HTML(">

      <div class="row">
        <button class="btn btn-primary" type="submit">Save and Connect</button>
        <a class="btn btn-ghost" href="/">Back</a>
      </div>
    </form>
  </div>
</body>
</html>)HTML";
  webServer.send(200, "text/html", html);
}

void handleWifiSetupSave() {
  if (!webServer.hasArg("ssid")) {
    webServer.send(400, "text/plain", "Missing SSID");
    return;
  }
  String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");
  String sleepTimeoutStr = webServer.arg("sleep_timeout");
  String brightnessStr = webServer.arg("brightness");
  bool autoBrightness = webServer.hasArg("auto_brightness");
  bool mqttEnabledOpt = webServer.hasArg("mqtt_enabled");
  String mqttHost = webServer.arg("mqtt_host");
  String mqttPortStr = webServer.arg("mqtt_port");
  String mqttUser = webServer.arg("mqtt_user");
  String mqttPass = webServer.arg("mqtt_pass");
  String mqttCid = webServer.arg("mqtt_cid");
  bool haEnabled = webServer.hasArg("ha_enabled");
  String haUrl = webServer.arg("ha_url");
  String haToken = webServer.arg("ha_token");
  ssid.trim();
  mqttHost.trim();
  mqttPortStr.trim();
  mqttUser.trim();
  mqttPass.trim();
  mqttCid.trim();
  haUrl.trim();
  haToken.trim();
  sleepTimeoutStr.trim();
  brightnessStr.trim();

  uint32_t mqttPort = (uint32_t)mqttPortStr.toInt();
  if (mqttPort == 0) mqttPort = MQTT_PORT;
  uint32_t sleepTimeoutS = (uint32_t)sleepTimeoutStr.toInt();
  uint32_t brightnessVal = (uint32_t)brightnessStr.toInt();
  if (sleepTimeoutS == 0) sleepTimeoutS = INACTIVITY_TIMEOUT_MS / 1000;
  if (brightnessStr.length() == 0) brightnessVal = (uint32_t)screenBrightness;

  if (ssid.length() == 0 || ssid.length() > 63 || password.length() > 63 ||
      mqttHost.length() > 80 || mqttUser.length() > 63 || mqttPass.length() > 63 ||
      mqttCid.length() > 40 || haUrl.length() > 120 || haToken.length() > 220 ||
      mqttPort > 65535 || sleepTimeoutS < 10 || sleepTimeoutS > 3600 || brightnessVal > 255) {
    webServer.send(400, "text/plain", "Invalid setup values");
    return;
  }

  wifiPrefs.begin("wifi", false);
  wifiPrefs.putString("ssid", ssid);
  wifiPrefs.putString("password", password);
  if (mqttHost.length() > 0) wifiPrefs.putString("mqtt_host", mqttHost);
  wifiPrefs.putBool("mqtt_enabled", mqttEnabledOpt);
  wifiPrefs.putUInt("mqtt_port", mqttPort);
  wifiPrefs.putString("mqtt_user", mqttUser);
  wifiPrefs.putString("mqtt_pass", mqttPass);
  if (mqttCid.length() > 0) wifiPrefs.putString("mqtt_cid", mqttCid);
  wifiPrefs.putUInt("sleep_timeout_s", sleepTimeoutS);
  wifiPrefs.putUInt("brightness", brightnessVal);
  wifiPrefs.putBool("brightness_user_set", true);
  wifiPrefs.putBool("auto_brightness", autoBrightness);
  wifiPrefs.putBool("ha_enabled", haEnabled);
  if (haUrl.length() > 0) wifiPrefs.putString("ha_url", haUrl);
  if (haToken.length() > 0) wifiPrefs.putString("ha_token", haToken);
  wifiPrefs.end();

  String html = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Saved</title>
  <style>
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      font-family: 'Segoe UI', Tahoma, sans-serif;
      background: linear-gradient(145deg, #0a1724, #12324a);
      color: #eaf5ff;
      padding: 16px;
    }
    .box {
      max-width: 520px;
      width: 100%;
      background: rgba(255,255,255,0.08);
      border: 1px solid rgba(255,255,255,0.2);
      border-radius: 16px;
      padding: 20px;
      text-align: center;
    }
    h1 { margin-top: 0; }
    p { color: #b9cee2; }
  </style>
</head>
<body>
  <div class="box">
    <h1>Saved</h1>
    <p>WiFi settings saved.</p>
    <p>Bob is now restarting and will try to connect immediately.</p>
  </div>
</body>
</html>)HTML";
  webServer.send(200, "text/html", html);
  delay(300);

  WiFi.disconnect(true, true);
  wifiSetupPortalActive = false;
  delay(200);
  ESP.restart();
}

void startWifiSetupPortal() {
  // Reset WiFi stack before enabling AP to avoid silent AP-start failures.
  WiFi.disconnect(true, true);
  delay(150);
  WiFi.mode(WIFI_AP_STA);
  delay(100);
  String apSsid = "Bob-Setup";
  bool apStarted = WiFi.softAP(apSsid.c_str());
  if (!apStarted) {
    // Fallback: pure AP mode retry
    WiFi.mode(WIFI_AP);
    delay(100);
    apStarted = WiFi.softAP(apSsid.c_str());
  }

  wifiSetupPortalActive = apStarted;
  lastSetupScreenDraw = 0;
  if (apStarted) {
    Serial.printf("WiFi setup portal active: SSID=%s, IP=%s\n",
                  apSsid.c_str(), WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("ERROR: Failed to start WiFi setup portal AP");
  }
}

void drawSetupPortalScreen() {
  uint32_t now = millis();
  if (now - lastSetupScreenDraw < 1000) return;
  lastSetupScreenDraw = now;

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(12, 20);
  M5.Display.println("Bob Setup Mode");

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(12, 68);
  M5.Display.println("1) Connect WiFi: Bob-Setup");
  M5.Display.setCursor(12, 92);
  M5.Display.println("2) Open: http://192.168.4.1/setup");
  M5.Display.setCursor(12, 116);
  M5.Display.println("3) Save your home WiFi");
  M5.Display.setCursor(12, 140);
  M5.Display.println("BLE: Bob-Setup-BLE");
}

void startBleProvisioning() {
  if (bleProvisioningActive) return;

  BLEDevice::init("Bob-Setup-BLE");
  bleProvServer = BLEDevice::createServer();
  bleProvService = bleProvServer->createService(BLE_PROV_SERVICE_UUID);

  BLECharacteristic* ssidChar = bleProvService->createCharacteristic(
    BLE_PROV_SSID_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  BLECharacteristic* passChar = bleProvService->createCharacteristic(
    BLE_PROV_PASS_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  BLECharacteristic* applyChar = bleProvService->createCharacteristic(
    BLE_PROV_APPLY_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  bleProvStatusChar = bleProvService->createCharacteristic(
    BLE_PROV_STATUS_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );

  ssidChar->setCallbacks(new BleSsidCallbacks());
  passChar->setCallbacks(new BlePasswordCallbacks());
  applyChar->setCallbacks(new BleApplyCallbacks());

  ssidChar->setValue("");
  bleProvStatusChar->setValue("READY");

  bleProvService->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLE_PROV_SERVICE_UUID);
  advertising->start();

  bleProvisioningActive = true;
  Serial.println("BLE provisioning active: Bob-Setup-BLE");
}

void stopBleProvisioning() {
  if (!bleProvisioningActive) return;
  BLEDevice::getAdvertising()->stop();
  BLEDevice::deinit(true);
  bleProvisioningActive = false;
}

// Upload snapshot to Home Assistant via REST API
void uploadSnapshotToHA() {
  Serial.println(" Uploading snapshot to Home Assistant...");

  if (!runtimeHaEnabled) {
    Serial.println("Home Assistant integration disabled - skipping upload");
    return;
  }

  if (!ensureCameraInitialized()) {
    Serial.println("Camera not initialized");
    return;
  }

  // Get camera frame (with retries against flat/invalid frames).
  if (!captureUsableFrame()) {
    Serial.println(" Camera.get() failed!");
    return;
  }

  camera_fb_t* fb = CoreS3.Camera.fb;
  if (!fb || !fb->buf || fb->len == 0) {
    if (fb) CoreS3.Camera.free();
    Serial.println(" No frame buffer or empty buffer!");
    return;
  }

  Serial.printf(" Snapshot captured: %dx%d, %d bytes\n", fb->width, fb->height, fb->len);

  uint8_t* jpgBuf = nullptr;
  size_t jpgLen = 0;
  bool freeJpgBuf = false;
  if (!frameToJpeg(fb, &jpgBuf, &jpgLen, &freeJpgBuf, 80)) {
    CoreS3.Camera.free();
    Serial.println(" Failed to convert snapshot for HA upload");
    return;
  }

  // Send to Home Assistant via REST API
  HTTPClient http;
  String url = runtimeHaBaseUrl + "/api/camera_proxy_stream/camera.bob_camera";

  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + runtimeHaToken);
  http.addHeader("Content-Type", "image/jpeg");

  int httpResponseCode = http.POST(jpgBuf, jpgLen);

  if (freeJpgBuf) {
    free(jpgBuf);
  }
  CoreS3.Camera.free();

  if (httpResponseCode > 0) {
    Serial.printf(" Snapshot uploaded to HA (HTTP %d)\n", httpResponseCode);
  } else {
    Serial.printf(" Failed to upload to HA (HTTP %d)\n", httpResponseCode);
  }

  http.end();
}

// Start camera streaming (auto-confirm, no button needed)
void startCameraStreaming() {
  if (!ensureCameraInitialized()) {
    Serial.println("Camera not initialized");
    return;
  }

  // Direct start without confirmation (touch conflicts with camera)
  cameraStreaming = true;

  // Disable matrix mode during camera streaming to save CPU
  if (matrixMode) {
    matrixMode = false;
    Serial.println(" Matrix mode disabled during camera streaming");
  }

  // Keep touch active so Bob can still wake/respond on touch during streaming.
  Serial.println(" Camera streaming started - Touch remains enabled");
  cameraGetFailCount = 0;
  // Warmup frames reduce first-frame failures on some boards.
  for (int i = 0; i < 2; i++) {
    if (CoreS3.Camera.get()) {
      CoreS3.Camera.free();
    }
    delay(20);
  }
  if (mqttEnabled && mqttClient.connected()) {
    mqttClient.publish("bob/camera/streaming", "active");
  }
}

// Stop camera streaming
void stopCameraStreaming() {
  cameraStreaming = false;

  Serial.println(" Camera streaming stopped");
  if (mqttEnabled && mqttClient.connected()) {
    mqttClient.publish("bob/camera/streaming", "inactive");
  }
}

// Stream camera frame to MQTT AND display on M5Stack
void streamCameraFrame() {
  if (!cameraStreaming || !cameraInitialized) return;

  uint32_t now = millis();
  if (now - lastStreamFrame < STREAM_INTERVAL_MS) return;

  lastStreamFrame = now;

  // Get camera frame (with retries against flat/invalid frames).
  if (!captureUsableFrame()) {
    cameraGetFailCount++;
    Serial.printf(" Camera.get() failed during streaming (%u)\n", cameraGetFailCount);
    uint32_t nowMs = millis();
    if (cameraGetFailCount >= 8 && (nowMs - lastCameraRecoverAt) > 2000) {
      lastCameraRecoverAt = nowMs;
      Serial.println(" Camera recovery: reinitializing camera...");
      cameraInitialized = false;
      if (initializeCamera()) {
        cameraGetFailCount = 0;
        // Warmup after reinit.
        for (int i = 0; i < 2; i++) {
          if (CoreS3.Camera.get()) {
            CoreS3.Camera.free();
          }
          delay(20);
        }
        Serial.println(" Camera recovery successful");
      } else {
        Serial.println(" Camera recovery failed");
      }
    }
    return;
  }
  cameraGetFailCount = 0;

  camera_fb_t* fb = CoreS3.Camera.fb;
  if (!fb || !fb->buf || fb->len == 0) {
    if (fb) CoreS3.Camera.free();
    Serial.println(" No frame buffer or empty buffer during streaming");
    return;
  }

  // Debug output
  static uint32_t frameCount = 0;
  if (frameCount % 30 == 0) { // Every 30 frames (~3 seconds)
    Serial.printf(" Streaming frame #%d: %dx%d, format=%d, size=%d\n",
                  frameCount, fb->width, fb->height, fb->format, fb->len);
  }
  frameCount++;

  // Display camera feed on M5Stack screen only (no MQTT streaming)
  M5.Display.startWrite();

  if (fb->format == PIXFORMAT_JPEG) {
    M5.Display.drawJpg(fb->buf, fb->len, 0, 0, M5.Display.width(), M5.Display.height());
  } else if (fb->format == PIXFORMAT_RGB565) {
    // Fast-path for live stream rendering.
    M5.Display.pushImage(0, 0, fb->width, fb->height, (uint16_t*)fb->buf);
  } else {
    // Convert non-JPEG to JPEG first; avoids RGB565 byte-order/format quirks on some boards.
    uint8_t* jpgBuf = nullptr;
    size_t jpgLen = 0;
    bool freeJpgBuf = false;
    if (frameToJpeg(fb, &jpgBuf, &jpgLen, &freeJpgBuf, 70)) {
      M5.Display.drawJpg(jpgBuf, jpgLen, 0, 0, M5.Display.width(), M5.Display.height());
    } else {
      // Visible fallback marker instead of blank/grey block.
      M5.Display.fillScreen(TFT_BLACK);
      M5.Display.setTextColor(TFT_RED, TFT_BLACK);
      M5.Display.setTextSize(2);
      M5.Display.setCursor(16, 16);
      M5.Display.println("Camera decode error");
    }
    if (freeJpgBuf) {
      free(jpgBuf);
    }
  }

  M5.Display.endWrite();

  CoreS3.Camera.free();
}

// ---------------- WiFi & MQTT Functions ----------------
void loadRuntimeConnectivityConfig() {
  wifiPrefs.begin("wifi", true);

  runtimeMqttHost = wifiPrefs.getString("mqtt_host", String(MQTT_HOST));
  runtimeMqttUser = wifiPrefs.getString("mqtt_user", String(MQTT_USERNAME));
  runtimeMqttPass = wifiPrefs.getString("mqtt_pass", String(MQTT_PASSWORD));
  runtimeMqttClientId = wifiPrefs.getString("mqtt_cid", String(MQTT_CLIENT_ID));
  bool hasSavedMqttConfig = wifiPrefs.isKey("mqtt_host");
  runtimeMqttEnabled = wifiPrefs.getBool("mqtt_enabled", hasSavedMqttConfig);
  runtimeHaBaseUrl = wifiPrefs.getString("ha_url", String(HA_BASE_URL));
  runtimeHaToken = wifiPrefs.getString("ha_token", String(HA_LONG_TOKEN));
  runtimeHaEnabled = wifiPrefs.getBool("ha_enabled", true);
  uint32_t port = wifiPrefs.getUInt("mqtt_port", MQTT_PORT);
  uint32_t savedSleepTimeout = wifiPrefs.getUInt("sleep_timeout_s", DEFAULT_INACTIVITY_TIMEOUT / 1000);
  bool brightnessUserSet = wifiPrefs.getBool("brightness_user_set", false);
  uint32_t savedBrightness = brightnessUserSet
    ? wifiPrefs.getUInt("brightness", DEFAULT_SCREEN_BRIGHTNESS)
    : (uint32_t)DEFAULT_SCREEN_BRIGHTNESS;
  bool savedAutoBrightness = wifiPrefs.getBool("auto_brightness", true);

  wifiPrefs.end();

  runtimeMqttHost.trim();
  runtimeMqttUser.trim();
  runtimeMqttPass.trim();
  runtimeMqttClientId.trim();
  runtimeHaBaseUrl.trim();
  runtimeHaToken.trim();

  if (port == 0 || port > 65535) port = MQTT_PORT;
  runtimeMqttPort = (uint16_t)port;

  if (savedSleepTimeout < 10 || savedSleepTimeout > 3600) savedSleepTimeout = DEFAULT_INACTIVITY_TIMEOUT / 1000;
  INACTIVITY_TIMEOUT_MS = savedSleepTimeout * 1000;

  if (savedBrightness > 255) savedBrightness = DEFAULT_SCREEN_BRIGHTNESS;
  screenBrightness = (int)savedBrightness;
  autoBrightnessEnabled = savedAutoBrightness;
}

bool initializeWiFi() {
  String targetSsid = WIFI_SSID;
  String targetPassword = WIFI_PASSWORD;

  wifiPrefs.begin("wifi", true);
  String savedSsid = wifiPrefs.getString("ssid", "");
  String savedPassword = wifiPrefs.getString("password", "");
  wifiPrefs.end();

  if (savedSsid.length() > 0) {
    targetSsid = savedSsid;
    targetPassword = savedPassword;
    Serial.printf("Connecting to saved WiFi SSID: %s\n", targetSsid.c_str());
  } else {
    Serial.printf("Connecting to default WiFi SSID: %s\n", targetSsid.c_str());

    // If compile-time credentials are placeholders, skip retries and go straight to setup mode.
    String ssidUpper = targetSsid;
    String passUpper = targetPassword;
    ssidUpper.toUpperCase();
    passUpper.toUpperCase();
    if (ssidUpper == "YOUR_WIFI_SSID" || passUpper == "YOUR_WIFI_PASSWORD" || targetSsid.length() == 0) {
      Serial.println("Default WiFi credentials are placeholders/empty - starting setup mode");
      return false;
    }
  }
  
  // Disconnect if already connected
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    delay(25);
  }
  
  // Configure WiFi mode and settings
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE); // Clear static config
  WiFi.setHostname("Bob-CoreS3-Eyes");
  
  // Start connection with reduced timeout for faster startup
  for (int attempt = 1; attempt <= 2; attempt++) {
    Serial.printf("WiFi attempt %d/2...\n", attempt);
    WiFi.begin(targetSsid.c_str(), targetPassword.c_str());
    
    Serial.print("Connecting");
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 8000) {
      delay(250);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());
      return true;
    } else {
      String statusText = "";
      switch(WiFi.status()) {
        case WL_NO_SSID_AVAIL: statusText = "SSID not found"; break;
        case WL_CONNECT_FAILED: statusText = "Connection failed (wrong password?)"; break;
        case WL_CONNECTION_LOST: statusText = "Connection lost"; break;
        case WL_DISCONNECTED: statusText = "Disconnected"; break;
        default: statusText = "Unknown error"; break;
      }
      Serial.printf("\nAttempt %d failed: %s (code: %d)\n", attempt, statusText.c_str(), WiFi.status());
      if (attempt < 2) {
        WiFi.disconnect();
        delay(1000);
      }
    }
  }

  // Fallback to compile-time credentials if saved credentials failed.
  if (savedSsid.length() > 0 && (targetSsid != String(WIFI_SSID))) {
    Serial.println("Saved WiFi failed, trying default credentials...");
    targetSsid = WIFI_SSID;
    targetPassword = WIFI_PASSWORD;
    for (int attempt = 1; attempt <= 2; attempt++) {
      Serial.printf("Fallback WiFi attempt %d/2...\n", attempt);
      WiFi.begin(targetSsid.c_str(), targetPassword.c_str());
      uint32_t startTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startTime < 8000) {
        delay(250);
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Fallback WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
      }
      WiFi.disconnect();
      delay(500);
    }
  }

  Serial.println("All WiFi connection attempts failed");
  return false;
}

bool hasProvisionedWiFiCredentials() {
  wifiPrefs.begin("wifi", true);
  String savedSsid = wifiPrefs.getString("ssid", "");
  wifiPrefs.end();
  savedSsid.trim();
  return savedSsid.length() > 0;
}

void startMdnsService() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (MDNS.begin("bob")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS started: http://bob.local");
  } else {
    Serial.println("mDNS start failed");
  }
}

void setupLayout(){
  int w = M5.Display.width();
  int h = M5.Display.height();
  centerX = w/2;
  centerY = h/2;

  float refH = 240.0f;
  screenScale = ((float)fminf((float)w,(float)h)/refH) * EYE_SCREEN_SCALE;
  eyeSpacing  = iround((BASE_EYE_SPACING / EYE_SCREEN_SCALE) * screenScale * 1.15f);
  glowOffset  = (int)lroundf(BASE_GLOW_OFFSET * screenScale * 1.0f);
  MAX_LOOK_X  = BASE_MAX_LOOK_X * screenScale * 1.05f;
  MAX_LOOK_Y  = BASE_MAX_LOOK_Y * screenScale * 1.05f;
}

void rotateScreen() {
  M5.Display.setRotation(screenRotation);
  DEBUG_PRINTF("Scherm geroteerd naar: %d (%d)\n", screenRotation * 90, screenRotation);

  // Herbereken layout na rotatie
  setupLayout();

  // Clear screen before canvas recreation to prevent artifacts
  M5.Display.fillScreen(TFT_BLACK);

  // Recreate canvas with new dimensions
  canvas.deleteSprite();
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.setColorDepth(16);

  // Fill with black to prevent garbage data
  canvas.fillScreen(TFT_BLACK);

  DEBUG_PRINTF("New display dimensions: %dx%d\n", M5.Display.width(), M5.Display.height());
}

// ==================== API HANDLERS ====================

void handleApiStatus() {
  String personality = getExtendedPersonalityName(currentPersonality);
  String json = "{";
  json += "\"personality\":\"" + personality + "\",";
  json += "\"behavior\":\"" + currentBehaviorName + "\",";
  json += "\"mqtt\":" + String(mqttEnabled && mqttClient.connected() ? "true" : "false") + ",";
  json += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"camera_streaming\":" + String(cameraWebStreaming ? "true" : "false") + ",";
  json += "\"clock_mode\":" + String(clockMode ? "true" : "false") + ",";
  json += "\"brightness\":" + String(screenBrightness) + ",";
  json += "\"wake_time_sec\":" + String((int)(INACTIVITY_TIMEOUT_MS / 1000)) + ",";
  json += "\"sleeping\":" + String(isAsleep ? "true" : "false") + ",";
  json += "\"always_awake\":" + String(alwaysAwake ? "true" : "false");
  json += "}";
  webServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Access-Control-Allow-Origin", "*");
  webServer.send(200, "application/json", json);
}

void handleHaOnboarding() {
  String html = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Bob naar Home Assistant</title>
  <style>
    :root { color-scheme: dark; }
    body {
      margin: 0;
      font-family: "Segoe UI", Tahoma, sans-serif;
      background: #07131f;
      color: #eaf5ff;
      padding: 20px;
    }
    .wrap {
      max-width: 760px;
      margin: 0 auto;
      background: #102235;
      border: 1px solid #28425f;
      border-radius: 14px;
      padding: 20px;
    }
    h1 { margin-top: 0; font-size: 1.4rem; }
    p, li { color: #c8d8e8; }
    a.btn {
      display: inline-block;
      margin-top: 12px;
      padding: 12px 16px;
      border-radius: 10px;
      text-decoration: none;
      color: #07131f;
      background: #7cd9ff;
      font-weight: 600;
    }
    code { color: #9fe1ff; }
  </style>
</head>
<body>
  <div class="wrap">
    <h1>Bob Installation in Home Assistant</h1>
    <p>Follow these 3 steps:</p>
    <ol>
      <li>Open Bob's GitHub page and install the integration via HACS.</li>
      <li>Restart Home Assistant.</li>
      <li>Add the <strong>Bob</strong> integration and keep MQTT on prefix <code>bob/cmd</code>.</li>
    </ol>
    <a class="btn" href=")HTML";
  html += BOB_HA_GITHUB_URL;
  html += R"HTML(" target="_blank" rel="noopener">Open Bob on GitHub</a>
  </div>
</body>
</html>)HTML";
  webServer.send(200, "text/html; charset=utf-8", html);
}

void drawHaOnboardingScreen(const String& localHaUrl) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 8);
  M5.Display.println("Connect Bob to HA");

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(10, 34);
  M5.Display.println("Scan QR -> HACS install -> Add Integration");

#if BOB_HAS_QRCODE
  String qrPayload = localHaUrl;
  if (qrPayload.length() == 0) {
    qrPayload = String(BOB_HA_GITHUB_URL);
  }

  const int x0 = 10;
  const int y0 = 52;
#if defined(ESP_QRCODE_CONFIG_DEFAULT)
  g_haQrX0 = x0;
  g_haQrY0 = y0;
  g_haQrMaxSize = 148;

  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.display_func = drawHaOnboardingEspQr;
  cfg.max_qrcode_version = 5;
  cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  if (esp_qrcode_generate(&cfg, qrPayload.c_str()) != ESP_OK) {
    M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
    M5.Display.setCursor(10, 68);
    M5.Display.println("Failed to generate QR code.");
  }
#else
  const uint8_t qrVersion = 5;
  uint8_t qrcodeData[qrcode_getBufferSize(qrVersion)];
  QRCode qrcode;
  qrcode_initText(&qrcode, qrcodeData, qrVersion, ECC_LOW, qrPayload.c_str());

  const int qrSize = qrcode.size;
  int scale = 4;
  while (scale > 1 && (qrSize * scale) > 148) scale--;
  const int drawSize = qrSize * scale;

  M5.Display.fillRect(x0 - 2, y0 - 2, drawSize + 4, drawSize + 4, TFT_WHITE);
  for (int y = 0; y < qrSize; y++) {
    for (int x = 0; x < qrSize; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        M5.Display.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, TFT_BLACK);
      }
    }
  }
#endif
#else
  M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5.Display.setCursor(10, 68);
  M5.Display.println("QR library is not available in this build.");
#endif

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(176, 62);
  M5.Display.println("1) Scan the QR");
  M5.Display.setCursor(176, 82);
  M5.Display.println("2) Open GitHub");
  M5.Display.setCursor(176, 102);
  M5.Display.println("3) Install via HACS");
  M5.Display.setCursor(176, 122);
  M5.Display.println("4) Add 'Bob' in HA");

  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.setCursor(10, 226);
  M5.Display.println("Tap screen to skip");
}

// ==================== DEEP SLEEP FUNCTIONS ====================

// Gracefully disconnect WiFi and MQTT before deep sleep
void setup(){
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== Bob CoreS3 Eyes (AUDIO-FREE VERSION) ===");

  auto cfg = M5.config();
  cfg.external_rtc = false;

  M5.begin(cfg);
  CoreS3.begin();

  Serial.println("M5Stack CoreS3 Lite initialized");
  Serial.printf("Display: %dx%d\n", M5.Display.width(), M5.Display.height());
  Serial.printf("Touch: %s\n", M5.Touch.isEnabled() ? "Available" : "Not Available");
  Serial.printf("IMU: %s\n", M5.Imu.isEnabled() ? "Available" : "Not Available");

  // ==================== MP3 HANDLER DISABLED ====================
  // MP3 handler not used; SD audio and Speaker removed
  // setupMP3Handler();
  // DEBUG_PRINTLN(" MP3 handler disabled");

  // Initialize M5.Mic for sound detection
  if (microphoneEnabled) {
    // Allocate microphone buffer
    micBuffer = (int16_t*)malloc(MIC_BUFFER_SIZE * sizeof(int16_t));
    if (!micBuffer) {
      Serial.println(" Failed to allocate microphone buffer - disabling mic");
      microphoneEnabled = false;
    } else {
      auto micConfig = M5.Mic.config();
      micConfig.noise_filter_level = 0;
      micConfig.over_sampling = 0;
      M5.Mic.config(micConfig);
      M5.Mic.begin();
      Serial.printf(" Microphone initialized with %d sample buffer\n", MIC_BUFFER_SIZE);
    }
  }

  // Audio streaming removed for stability

  // Check wake-up reason after deep sleep
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool wokingFromDeepSleep = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 ||
                               wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Bob woken by touch/proximity");
      lastWakeReason = "touch_or_proximity";
      lastWakeReasonAt = millis();
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Bob woken by timer");
      lastWakeReason = "timer";
      lastWakeReasonAt = millis();
      break;
    default:
      Serial.println("Bob - M5CoreS3 Lite Eyes + SD + MQTT starting...");
      lastWakeReason = "boot";
      lastWakeReasonAt = millis();
      wokingFromDeepSleep = false;
      break;
  }

  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.setColorDepth(16);

  setupLayout();
  setEmotionInstant(baseShape);
  nextBlinkAt = millis() + randomBlinkDelayForCurrentPersonality();
  loadRuntimeConnectivityConfig();
  
  // Fast provisioning check:
  // if no stored WiFi config exists, skip connect attempts and go straight to setup mode.
  if (!hasProvisionedWiFiCredentials()) {
    Serial.println("No stored WiFi config found - starting setup mode");
    wifiEnabled = false;
    startWifiSetupPortal();
    startBleProvisioning();
  } else {
    // Initialiseer WiFi voor MQTT logging
    wifiEnabled = initializeWiFi();
    if (!wifiEnabled) {
      startWifiSetupPortal();
      startBleProvisioning();
    } else {
      stopBleProvisioning();
      // NTP tijdsynchronisatie (Nederland: CET/CEST)
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
      tzset();
      Serial.println("NTP time sync gestart (CET/CEST)");
    }
  }

  // Initialiseer MQTT als WiFi werkt
  if (wifiEnabled) {
    startMdnsService();
    if (runtimeMqttEnabled) {
      mqttEnabled = initializeMQTT();
    } else {
      mqttEnabled = false;
      Serial.println("MQTT disabled by setup settings");
    }

    // Callback already set in initializeMQTT()
    if (mqttEnabled) {
      Serial.println("MQTT callback configured for all commands");
    }

    // Gebruik alleen de nieuwe auto-discovery (helderheid, touch, rotatie)
    if (mqttEnabled) {
      Serial.println("Starting Bob auto-discovery...");
    }
  }

  // Initialize camera server if WiFi is available

  const char* mqttStateText = mqttEnabled ? "OK" : (runtimeMqttEnabled ? "FAILED" : "DISABLED");
  Serial.printf("BASIC MODE Setup complete - WiFi: %s, MQTT: %s\n",
    wifiEnabled ? "OK" : "FAILED",
    mqttStateText);

  Serial.printf("Proximity sensor: %s\n", proximityEnabled ? "ENABLED" : "DISABLED");

  // Camera is initialized lazily on first use for startup stability.
  cameraInitialized = false;
  if (wifiSetupPortalActive) {
    Serial.println("Setup mode active - camera init skipped");
  } else {
    Serial.println("Camera init deferred until first use");
  }

  Serial.println("Bob ready - all systems initialized");

  // Minimal HTTP endpoints (no control webapp).
  webServer.on("/", HTTP_GET, []() {
    webServer.sendHeader("Location", "/ha");
    webServer.send(302, "text/plain", "Redirecting to /ha");
  });
  webServer.on("/snapshot", handleCameraSnapshot);
  webServer.on("/setup", HTTP_GET, handleWifiSetupPage);
  webServer.on("/setup/save", HTTP_POST, handleWifiSetupSave);
  webServer.on("/ha", HTTP_GET, handleHaOnboarding);
  webServer.begin();
  if (wifiSetupPortalActive) {
    Serial.printf("HTTP setup portal started on http://%s/\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.printf("HTTP server started on http://%s/\n", WiFi.localIP().toString().c_str());
  }

#if BOB_SHOW_HA_QR_ON_BOOT
  if (!wifiSetupPortalActive && wifiEnabled) {
    String localHaUrl = String("http://") + WiFi.localIP().toString() + "/ha";
    drawHaOnboardingScreen(localHaUrl);
    uint32_t onboardingStart = millis();
    while ((uint32_t)(millis() - onboardingStart) < BOB_HA_ONBOARDING_MS) {
      M5.update();
      webServer.handleClient();
      if (mqttEnabled && mqttClient.connected()) {
        mqttClient.loop();
      }
      if (M5.Touch.isEnabled() && M5.Touch.getCount() > 0) {
        break;
      }
      delay(20);
    }
    M5.Display.fillScreen(TFT_BLACK);
  }
#endif

  // Setup Home Assistant MQTT Discovery (optional)
  if (mqttEnabled && runtimeHaEnabled) {
    setupHomeAssistantDiscovery();
    publishWakeReason(lastWakeReason.c_str());
  } else if (mqttEnabled) {
    publishWakeReason(lastWakeReason.c_str());
  }

  // ==================== STARTUP ANNOUNCEMENT ====================
  // Publish startup event without blocking setup.
  if (mqttEnabled) {
    startupEventPending = true;
    startupEventAt = millis() + 2000;
  }



  // No extra startup melody

  // Handle wake-up from deep sleep
  if (wokingFromDeepSleep) {
    Serial.println("\nHandling wake-up from deep sleep...");
    reconnectAfterDeepSleep();
  } else {
    // Normal startup - start met WakeUpSequence
    triggerBehavior(Behavior::WakeUpSequence);
  }
  
  lastFrameMs = millis();
  lastInteractionTime = millis();

  // TTS via Home Assistant (Ollama or configured TTS engine)
  Serial.println("Bob startup sequence complete");
}

void loop(){
  M5.update();

  // Handle HTTP requests
  webServer.handleClient();

  // Handle BLE WiFi provisioning requests.
  if (bleApplyRequested) {
    bleApplyRequested = false;
    blePendingSsid.trim();
    if (blePendingSsid.length() > 0 && blePendingSsid.length() <= 63 && blePendingPassword.length() <= 63) {
      wifiPrefs.begin("wifi", false);
      wifiPrefs.putString("ssid", blePendingSsid);
      wifiPrefs.putString("password", blePendingPassword);
      wifiPrefs.end();
      if (bleProvStatusChar) {
        bleProvStatusChar->setValue("SAVED");
        bleProvStatusChar->notify();
      }
      Serial.println("BLE credentials saved, restarting...");
      delay(300);
      ESP.restart();
    } else {
      if (bleProvStatusChar) {
        bleProvStatusChar->setValue("ERROR");
        bleProvStatusChar->notify();
      }
      Serial.println("BLE credentials invalid, ignoring apply");
    }
  }

  // Keep setup mode lightweight and stable.
  if (wifiSetupPortalActive && WiFi.status() != WL_CONNECTED) {
    // Self-heal AP if it dropped.
    if (WiFi.softAPIP().toString() == "0.0.0.0" && millis() - lastSetupPortalRetry > 5000) {
      lastSetupPortalRetry = millis();
      Serial.println("Setup AP inactive, retrying start...");
      startWifiSetupPortal();
    }
    drawSetupPortalScreen();
    delay(30);
    return;
  }

  // ==================== VOICE STREAMING LOOP ====================
  // Voice streaming disabled
  // loopVoiceStreaming();

  // ==================== MP3 HANDLER LOOP DISABLED ====================
  // MP3 handler loop not used
  // loopMP3Handler();

  // Handle audio processing
  // audio.loop();  // Niet nodig met M5.Speaker

  // Handle automatic natural eye movement (idle behavior)
  bool personalityActive = isPersonalityActive();
  bool suppressIdleTrackingByPersonality =
    personalityActive &&
    (currentPersonality == PersonalityExtended::Sad ||
     currentPersonality == PersonalityExtended::Thinking ||
     currentPersonality == PersonalityExtended::Confused ||
     currentPersonality == PersonalityExtended::Angry ||
     currentPersonality == PersonalityExtended::Dizzy);

  // Sad/Thinking/Confused/Angry/Dizzy: geen idle rondkijken
  if (objectTrackingEnabled && !manualOverride && !proximityDetected && !suppressIdleTrackingByPersonality) {
    uint32_t now = millis();
    float smallMin = EYE_TRACK_SMALL_MOVE_MIN;
    float smallMax = EYE_TRACK_SMALL_MOVE_MAX;
    float largeMin = EYE_TRACK_LARGE_MOVE_MIN;
    float largeMax = EYE_TRACK_LARGE_MOVE_MAX;
    float maxRange = EYE_TRACK_MAX_RANGE;
    int smallMoveChance = EYE_TRACK_SMALL_MOVE_CHANCE;
    unsigned long intervalMin = EYE_TRACK_INTERVAL_MIN_MS;
    unsigned long intervalMax = EYE_TRACK_INTERVAL_MAX_MS;

    // Emotiegedrag laag: niet alleen vorm, maar ook bewegingsprofiel.
    if (personalityActive) {
      switch (currentPersonality) {
        case PersonalityExtended::Excited:
          smallMin = -0.62f; smallMax = 0.62f;
          largeMin = -1.08f; largeMax = 1.08f;
          maxRange = 0.98f;
          smallMoveChance = 26;
          intervalMin = 250; intervalMax = 780;
          break;
        case PersonalityExtended::Happy:
          smallMin = -0.45f; smallMax = 0.45f;
          largeMin = -0.90f; largeMax = 0.90f;
          maxRange = 0.82f;
          smallMoveChance = 42;
          intervalMin = 450; intervalMax = 1200;
          break;
        case PersonalityExtended::Sad:
          smallMin = -0.18f; smallMax = 0.18f;
          largeMin = -0.35f; largeMax = 0.35f;
          maxRange = 0.35f;
          smallMoveChance = 82;
          intervalMin = 1100; intervalMax = 2600;
          break;
        default:
          break;
      }
    }

    // Initialize next change time on first run
    if (nextTrackingChange == 0) {
      nextTrackingChange = now + random(intervalMin, intervalMax);
    }

    // Time to pick a new look target?
    if (now >= nextTrackingChange) {
      // Generate natural, smooth random movements
      // Bias towards center and smaller movements for more realistic idle behavior

      // Small move chance vs larger move
      if (random(100) < smallMoveChance) {
        // Small adjustment (natural drift)
        currentLookTargetX += random((int)(smallMin * 100), (int)(smallMax * 100) + 1) / 100.0f;
        currentLookTargetY += random((int)(smallMin * 100), (int)(smallMax * 100) + 1) / 100.0f;
      } else {
        // Larger look somewhere else - kijk rond
        currentLookTargetX = random((int)(largeMin * 100), (int)(largeMax * 100) + 1) / 100.0f;
        currentLookTargetY = random((int)(largeMin * 100), (int)(largeMax * 100) + 1) / 100.0f;
      }

      // Clamp to reasonable range
      currentLookTargetX = constrain(currentLookTargetX, -maxRange, maxRange);
      currentLookTargetY = constrain(currentLookTargetY, -maxRange, maxRange);

      // Apply to target
      targetLookX = currentLookTargetX * MAX_LOOK_X;
      targetLookY = currentLookTargetY * MAX_LOOK_Y;

      // Random interval for more natural movement
      nextTrackingChange = now + random(intervalMin, intervalMax);

      // Occasional debug
      static uint32_t lastDebug = 0;
      if (now - lastDebug > 5000) {
        DEBUG_VERBOSE_PRINTF("Natural look: (%.2f, %.2f)\n", currentLookTargetX, currentLookTargetY);
        lastDebug = now;
      }
    }
  }

  // Camera streaming (if active)
  streamCameraFrame();

  
  // Touch detection - single pass
  bool touchDetected = M5.Touch.isEnabled() && (M5.Touch.getCount() > 0);
  uint32_t touchNow = millis();

  if (touchDetected && !touchStatePublished) { // Edge detection - alleen bij nieuwe touch
    // Check manual sleep cooldown
    bool inCooldown = (manualSleepTime > 0 && touchNow - manualSleepTime < MANUAL_SLEEP_COOLDOWN);
    bool shouldWakeFromTouch = (isAsleep || isGoingToSleep);

    // Touch should always wake Bob immediately, regardless of cooldown.
    if (shouldWakeFromTouch) {
      wakeBobFromSleep("touch", true);
      DEBUG_PRINTLN("Touch detected - wake helper invoked");
    } else if (!inCooldown) {
      lastInteractionTime = touchNow;
      lastProximityTime = touchNow; // Reset proximity timeout - touch counts as interaction
        BobMove::alertUp();
        if (!manualOverride && !isAsleep && !isGoingToSleep) {
          applyPersonality(PersonalityExtended::Happy, 2500);
        }
        DEBUG_PRINTLN("Touch detected - personality reaction");
    } else {
      DEBUG_VERBOSE_PRINTF("Touch detected but in manual sleep cooldown (%d ms remaining)\n",
                           MANUAL_SLEEP_COOLDOWN - (touchNow - manualSleepTime));
    }

    // Publish touch state to MQTT
    if (mqttEnabled) {
      mqttClient.publish("bob/touch", "ON", true);
      DEBUG_VERBOSE_PRINTLN("TOUCH PRESSED -> Published to bob/touch: ON");
    }
    touchStatePublished = true;
  } else if (!touchDetected && touchStatePublished) {
    // Touch released - publish OFF state
    if (mqttEnabled) {
      mqttClient.publish("bob/touch", "OFF", true);
      DEBUG_VERBOSE_PRINTLN("TOUCH RELEASED -> Published to bob/touch: OFF");
    }
    touchStatePublished = false;
  }

  bool anyTouch = touchDetected;
  if (touchDetected) {
    lastInteractionTime = touchNow; // Reset timer op touch

    // Eye tracking from touch - with safety checks
    auto t = M5.Touch.getDetail(0);

    // Validate touch data is within screen bounds
    uint16_t displayWidth = M5.Display.width();
    uint16_t displayHeight = M5.Display.height();

    if (t.x >= 0 && t.x < displayWidth && t.y >= 0 && t.y < displayHeight) {
      float dx = (float)t.x - centerX;
      float dy = (float)t.y - centerY;
      const float dead = 6.0f * screenScale;
      if (fabsf(dx) < dead) dx = 0;
      if (fabsf(dy) < dead) dy = 0;
      targetLookX = clampf(dx * 0.25f, -MAX_LOOK_X, MAX_LOOK_X);
      targetLookY = clampf(dy * 0.25f, -MAX_LOOK_Y, MAX_LOOK_Y);
    }

    // Touch behavior switching
    // Debounce runnerNext om constant switching te voorkomen
    if (touchNow - lastTouchTime > TOUCH_DEBOUNCE_MS) {
      // Touch no longer changes behavior; keep only for debouncing/logs.
      if (manualOverride) {
        DEBUG_VERBOSE_PRINTLN("Touch ignored - manual override active");
      }
      lastTouchTime = touchNow;
    }
  }

  // Inactivity timer check moved to bottom of loop() for consistency

  // BASIC MODE - IMU triggers enabled for shake detection
  imuTriggers();

  // Check confetti modes - disable if both switches are OFF
  if (!confettiSnowEnabled && !confettiCannonsEnabled) {
    if (confettiMode != CONFETTI_NONE) {
      confettiMode = CONFETTI_NONE;
      DEBUG_PRINTLN("Confetti disabled (both switches OFF)");
    }
  }

  // Process proximity detection for LTR-553ALS-WA sensor
  processProximityDetection();

  // Process microphone detection
  processMicrophoneDetection();

  // Process presence notifications
  processPresenceNotifications();

  // Sensor publishing is handled in maintainConnections() - removed duplicate here

  // Check manual override timeout
  if (manualOverride && millis() > manualOverrideTimeout) {
    manualOverride = false;
    if (!isAsleep) {
      DEBUG_PRINTLN("Manual override expired - resuming automatic behaviors");
    } else {
      DEBUG_PRINTLN("Manual override expired during sleep - staying asleep for battery");
    }
  }

  // Auto-return to idle state after speaking (10 seconds timeout)
  if (ollamaState == "speaking" && (millis() - stateChangeTime > 10000)) {
    setOllamaState("idle");
  }

  // Maintain WiFi and MQTT connections with rate limiting
  static uint32_t lastConnectionCheck = 0;
  if (timeElapsed(lastConnectionCheck, 5000)) { // Check every 5 seconds
    maintainConnections();
    lastConnectionCheck = millis();
  }

  // Publish startup event once after MQTT settles, without blocking loop.
  if (startupEventPending && mqttEnabled && mqttClient.connected() &&
      (int32_t)(millis() - startupEventAt) >= 0) {
    mqttClient.publish("bob/event", "startup", true);
    DEBUG_PRINTLN("Startup event published to Home Assistant");
    startupEventPending = false;
  }

  // BASIC MODE - Maintain MQTT connection for logging
  if (mqttEnabled) {
    mqttClient.loop();
  }

  // Check for inactivity timeout
  // Controleer of we wakker moeten blijven door stroom
  bool shouldStayAwake = alwaysAwake;
  if (autoAwakeOnPower && M5.Power.isCharging()) {
    shouldStayAwake = true; // Blijf wakker als aan stroom
  }

  if (!shouldStayAwake && !isAsleep && !isGoingToSleep) {
    if (timeElapsed(lastInteractionTime, INACTIVITY_TIMEOUT_MS)) {
      DEBUG_PRINTLN("Inactivity timeout reached - triggering sleep");
      isGoingToSleep = true;
      triggerBehavior(Behavior::Sleep);
    }
  }

  // Simple behavior system for basic mode
  behaviorsTick(anyTouch);

  uint32_t personalityNow = millis();
  if (!isAsleep && !isGoingToSleep && currentBehaviorName == "Wake" && !manualOverride && autoEmotionEnabled) {
    if (nextAutoEmotionAt == 0) {
      nextAutoEmotionAt = personalityNow + random(45000, 120000);
    }
    if (personalityUntil == 0 && personalityNow >= nextAutoEmotionAt) {
      PersonalityExtended p = AUTO_EMOTIONS[autoEmotionIndex % AUTO_EMOTION_COUNT];
      autoEmotionIndex = (autoEmotionIndex + 1) % AUTO_EMOTION_COUNT;
      applyPersonality(p, 4000);
      nextAutoEmotionAt = personalityNow + random(45000, 120000);
    }
  }
  lastPersonalityCheck = personalityNow;

  if (personalityUntil > 0 && personalityNow > personalityUntil &&
      !isAsleep && !isGoingToSleep && currentBehaviorName == "Wake") {
    currentPersonality = PersonalityExtended::Neutral;
    personalityUntil = 0;
    startEmotionTween(baseShape, 220);
  }

  tickEmotionTween();

  // Non-blocking frame limiter: keeps loop responsive for MQTT/sensors while capping render FPS.
  static uint32_t nextRenderAt = 0;
  uint32_t now = millis();
  if ((int32_t)(now - nextRenderAt) < 0) {
    return;
  }
  nextRenderAt = now + FPS_DELAY_MS;

  float dt = (float)(now - lastFrameMs);
  lastFrameMs = now;

  // Apply eye tracking with priority: Proximity > Normal
  float finalTargetX = targetLookX;
  float finalTargetY = targetLookY;

  // Update animation based on elapsed time
  if (isAnimatingEyes) {
    uint32_t elapsed = now - animationStartTime;
    float progress = (float)elapsed / 1800.0f;  // 0.0 to 1.0 (normalized 0-1800ms)

    if (currentAnimation == ANIM_YES) {
      // Shake: 30 -> -30 -> 30 -> -30 -> 30 -> -30 -> 0
      // Using sine wave for smooth animation
      float cycle = progress * 3.0f * M_PI;  // 3 full oscillations
      targetLookX = 30.0f * sinf(cycle);
      targetLookY = 0.0f;
    } else if (currentAnimation == ANIM_NO) {
      // Nod: 20 -> -20 -> 20 -> -20 -> 20 -> -20 -> 0
      float cycle = progress * 3.0f * M_PI;  // 3 full oscillations
      targetLookX = 0.0f;
      targetLookY = 20.0f * sinf(cycle);
    }
  }

  // Check if animation is still active
  if (isAnimatingEyes && now >= animationEndTime) {
    isAnimatingEyes = false;
    currentAnimation = ANIM_NONE;
    targetLookX = 0;
    targetLookY = 0;
  }

  // Use proximity-based eye tracking whenever proximity is detected (unless animating)
  if (proximityDetected && !isAnimatingEyes) {
    updateProximityEyeTracking(now);

    // Dead zone: ignore small movements to prevent twitching
    static float lastProxX = 0;
    static float lastProxY = 0;
    const float DEAD_ZONE = 2.0f;

    float newTargetX = -proximityEyeX;
    float newTargetY = proximityEyeY;

    if (fabsf(newTargetX - lastProxX) > DEAD_ZONE || fabsf(newTargetY - lastProxY) > DEAD_ZONE) {
      lastProxX = newTargetX;
      lastProxY = newTargetY;
    }

    finalTargetX += lastProxX;
    finalTargetY += lastProxY;
    finalTargetX = clampf(finalTargetX, -MAX_LOOK_X, MAX_LOOK_X);
    finalTargetY = clampf(finalTargetY, -MAX_LOOK_Y, MAX_LOOK_Y);

    // Debug output every 2 seconds
    static uint32_t lastEyeDebug = 0;
    if (now - lastEyeDebug > 2000) {
      DEBUG_VERBOSE_PRINTF("Eye tracking active - proximity: %d, final eye pos: (%.1f, %.1f)\n",
                           read_ps_value, finalTargetX, finalTargetY);
      lastEyeDebug = now;
    }
  }

  // Emotiegedrag bovenop look-targets (alleen in normale wake toestand)
  if (personalityActive && !proximityDetected && !isAnimatingEyes && currentBehaviorName == "Wake") {
    switch (currentPersonality) {
      case PersonalityExtended::Happy:
        // Groter/ronder + even dichterbij
        finalTargetY = mylerp(finalTargetY, -MAX_LOOK_Y * 0.06f, 0.20f);
        if (now < emotionCloseUntil) {
          depthScale = mylerp(depthScale, 1.20f, 0.06f);
        }
        break;
      case PersonalityExtended::Sad:
        // Stil en duidelijk lager in scherm
        finalTargetX = mylerp(finalTargetX, 0.0f, 0.55f);
        finalTargetY = mylerp(finalTargetY, MAX_LOOK_Y * 0.40f, 0.42f);
        depthScale = mylerp(depthScale, 0.95f, 0.03f);
        break;
      case PersonalityExtended::Thinking:
        // Echt omhoog + stil: geen rondkijken, alleen knipperen
        finalTargetX = mylerp(finalTargetX, 0.0f, 0.65f);
        finalTargetY = mylerp(finalTargetY, -MAX_LOOK_Y * 0.42f, 0.60f);
        break;
      case PersonalityExtended::Excited:
        // Happy + veel actiever rondkijken (profiel hierboven), plus dichterbij
        finalTargetY += sinf((float)now / 1000.0f * 6.5f) * (MAX_LOOK_Y * 0.08f); // subtiele verticale bounce
        if (now < emotionCloseUntil) {
          depthScale = mylerp(depthScale, 1.25f, 0.09f);
        }
        break;
      case PersonalityExtended::Confused:
        // Minder kijken, sneller knipperen
        finalTargetX = mylerp(finalTargetX, 0.0f, 0.40f);
        finalTargetY = mylerp(finalTargetY, 0.0f, 0.40f);
        depthScale = mylerp(depthScale, 0.94f, 0.03f);
        break;
      case PersonalityExtended::Sleepy:
        // Open ogen als basis, soms dichtvallen en dan "wakker schrikken"
        finalTargetX = mylerp(finalTargetX, 0.0f, 0.35f);
        finalTargetY = mylerp(finalTargetY, MAX_LOOK_Y * 0.10f, 0.22f);
        depthScale = mylerp(depthScale, 0.97f, 0.03f);

        if (sleepyMicroNapAt == 0) {
          sleepyMicroNapAt = now + random(2200, 5600);
        }
        if (!blinking && now >= sleepyMicroNapAt) {
          startBlink(); // "ogen dicht vallen"
          sleepyStartleFrom = now + BLINK_DUR_MS;
          sleepyStartleUntil = sleepyStartleFrom + 280;
          sleepyMicroNapAt = now + random(5200, 11000);
        }
        if (now >= sleepyStartleFrom && now < sleepyStartleUntil) {
          finalTargetY = mylerp(finalTargetY, -MAX_LOOK_Y * 0.28f, 0.55f); // "open schrikken"
          depthScale = mylerp(depthScale, 1.06f, 0.14f);
        }
        break;
      case PersonalityExtended::Surprised:
        // Twee keer "naar achter en naar voren" met dubbele knipper.
        if (surprisedPulseStart > 0 && now < surprisedPulseEnd) {
          float phase = (float)(now - surprisedPulseStart) / (float)(surprisedPulseEnd - surprisedPulseStart);
          float pulse = -sinf(phase * 4.0f * PI); // 2 cycli: achter -> voor
          depthScale = mylerp(depthScale, 1.0f + pulse * 0.16f, 0.26f);
          finalTargetX = mylerp(finalTargetX, 0.0f, 0.40f);
          finalTargetY = mylerp(finalTargetY, -MAX_LOOK_Y * 0.05f, 0.28f);
        }
        if (surprisedBlinkStep == 0 && now >= surprisedBlinkAt1 && !blinking) {
          startBlink();
          surprisedBlinkStep = 1;
        } else if (surprisedBlinkStep == 1 && now >= surprisedBlinkAt2 && !blinking) {
          startBlink();
          surprisedBlinkStep = 2;
        }
        break;
      case PersonalityExtended::Dizzy: {
        // Neutraal, maar shaken.
        float t = (float)now / 1000.0f;
        finalTargetX = sinf(t * 18.0f) * MAX_LOOK_X * 0.46f;
        finalTargetY = cosf(t * 11.0f) * MAX_LOOK_Y * 0.10f;
        depthScale = mylerp(depthScale, 0.98f, 0.05f);
      } break;
      case PersonalityExtended::Angry: {
        // Klein links-rechts patroon
        float t = (float)now / 1000.0f;
        finalTargetX = sinf(t * 6.0f) * MAX_LOOK_X * 0.30f;
        finalTargetY = MAX_LOOK_Y * 0.04f;
        depthScale = mylerp(depthScale, 0.95f, 0.04f);
      } break;
      default:
        break;
    }
    finalTargetX = clampf(finalTargetX, -MAX_LOOK_X, MAX_LOOK_X);
    finalTargetY = clampf(finalTargetY, -MAX_LOOK_Y, MAX_LOOK_Y);
  }

  float tau = proximityDetected ? 300.0f : (anyTouch ? LOOK_TAU_TOUCH : LOOK_TAU_IDLE);
  if (personalityActive && !proximityDetected && !anyTouch) {
    switch (currentPersonality) {
      case PersonalityExtended::Thinking: tau = 520.0f; break; // Rustiger/meer stilstand
      case PersonalityExtended::Sad:      tau = 420.0f; break;
      case PersonalityExtended::Excited:  tau = 145.0f; break; // Sneller reageren
      case PersonalityExtended::Confused: tau = 340.0f; break;
      case PersonalityExtended::Dizzy:    tau = 130.0f; break;
      case PersonalityExtended::Angry:    tau = 200.0f; break;
      default: break;
    }
  }
  eyeLookX = smoothTo(eyeLookX, finalTargetX, dt, tau);
  eyeLookY = smoothTo(eyeLookY, finalTargetY, dt, tau);
  asym3D     = clampf(asym3D, -PEEK_ASYM_MAX, PEEK_ASYM_MAX);
  depthScale = clampf(depthScale, FAR_MIN, NEAR_MAX);

  // Prevent blinking during proximity detection to keep eyes open and alert
  if (!blinking && now >= nextBlinkAt && !anyTouch && !proximityDetected) startBlink();

  // Stop blinking immediately if proximity is detected
  if (blinking && proximityDetected) {
    blinking = false;
    nextBlinkAt = millis() + randomBlinkDelayForCurrentPersonality();
    DEBUG_VERBOSE_PRINTLN("Stopping blink due to proximity detection");
  }

  // Draw eyes
  if (blinking){
    uint32_t et = now - blinkStart;
    if (et >= BLINK_DUR_MS){
      blinking=false; nextBlinkAt = millis() + randomBlinkDelayForCurrentPersonality();
      drawEyes(eyeLookX, eyeLookY);
    }else{
      EyeShape keep = currentShape;
      setEmotionInstant(blinkShape);
      drawEyes(eyeLookX, eyeLookY);
      setEmotionInstant(keep);
    }
  }else{
    drawEyes(eyeLookX, eyeLookY);
  }
}


// ================== OLLAMA STATE MANAGEMENT ==================

void setOllamaState(const char* newState) {
  if (ollamaState != newState) {
    ollamaState = newState;
    stateChangeTime = millis();
    DEBUG_PRINTF("Ollama state changed to: %s\n", newState);

    // Publish state to Home Assistant
    if (mqttEnabled && mqttClient.connected()) {
      mqttClient.publish("bob/ollama_state", newState, true);
    }
  }
}

// ===================== NOTIFICATION SYSTEM =====================

void showNotification(String message, uint16_t color, uint32_t duration) {
  message.trim();
  if (message.length() == 0) {
    notificationActive = false;
    notificationText = "";
    notificationUntil = 0;
    return;
  }

  if (duration < 800) duration = 800;
  if (duration > 12000) duration = 12000;

  notificationText = message;
  notificationColor = color;
  notificationUntil = millis() + duration;
  notificationActive = true;
  DEBUG_PRINTF("Notification: %s\n", notificationText.c_str());
}

void drawNotificationBar() {
  if (!notificationActive) return;

  if (millis() >= notificationUntil) {
    notificationActive = false;
    notificationText = "";
    return;
  }

  String text = notificationText;
  if (text.length() == 0) return;

  const int pad = 8;
  const int barH = 58;
  const int radius = 12;
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  const int x = 10;
  const int y = h - barH - 10;
  const int maxW = w - (x * 2) - (pad * 2);

  canvas.setTextFont(2);
  canvas.setTextSize(2);

  while (text.length() > 0 && canvas.textWidth(text) > maxW) {
    text.remove(text.length() - 1);
  }
  if (text != notificationText && text.length() > 3) {
    text.remove(text.length() - 3);
    text += "...";
  }

  canvas.fillRoundRect(x, y, w - (x * 2), barH, radius, TFT_BLACK);
  canvas.drawRoundRect(x, y, w - (x * 2), barH, radius, TFT_DARKGREY);
  canvas.setTextColor(notificationColor, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString(text, w / 2, y + (barH / 2));
}






