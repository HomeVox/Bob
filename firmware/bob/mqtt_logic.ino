// Extracted from bob.ino for modular structure

// MQTT callback, connection lifecycle, discovery and status publishing
extern String runtimeMqttHost;
extern uint16_t runtimeMqttPort;
extern String runtimeMqttUser;
extern String runtimeMqttPass;
extern String runtimeMqttClientId;
extern bool runtimeMqttEnabled;

static void idleDelayWithMqtt(uint32_t ms) {
  uint32_t start = millis();
  while ((uint32_t)(millis() - start) < ms) {
    if (mqttEnabled && mqttClient.connected()) {
      mqttClient.loop();
    }
    delay(5);
  }
}

static String normalizeNotifyType(String t) {
  t.trim();
  t.toLowerCase();
  t.replace(" ", "_");
  t.replace("-", "_");
  return t;
}

static bool isAllowedSoundKey(const String& soundKey) {
  static const char* kAllowed[] = {
    "doorbell",
    "mail",
    "warning",
    "alarm",
    "success",
    "sleep",
    "chime",
    "matrix_start",
    "confetti_snow",
    "confetti_cannons"
  };
  for (size_t i = 0; i < sizeof(kAllowed) / sizeof(kAllowed[0]); i++) {
    if (soundKey.equalsIgnoreCase(kAllowed[i])) {
      return true;
    }
  }
  return false;
}

static void publishNotifySoundKey(const String& soundKey) {
  static uint32_t lastSoundAt = 0;
  static String lastSoundKey = "";
  uint32_t now = millis();
  if (soundKey == lastSoundKey && (now - lastSoundAt) < 250) {
    DEBUG_VERBOSE_PRINTF("Skipping duplicate notify_sound '%s'\n", soundKey.c_str());
    return;
  }
  lastSoundAt = now;
  lastSoundKey = soundKey;
  mqttClient.publish("bob/notify_sound", soundKey.c_str(), false);
}

static void handleNotifyCommand(const String& payload) {
  String text = payload;
  uint32_t duration = 4000;
  bool wake = true;
  String emotion = "";
  String notifyType = "generic";
  String sound = "";

  if (payload.startsWith("{")) {
    StaticJsonDocument<512> notifyDoc;
    DeserializationError err = deserializeJson(notifyDoc, payload);
    if (!err) {
      text = notifyDoc["text"] | notifyDoc["message"] | "";
      duration = notifyDoc["duration"] | 4000;
      wake = notifyDoc["wake"] | true;
      emotion = String((const char*)(notifyDoc["emotion"] | ""));
      notifyType = String((const char*)(notifyDoc["type"] | "generic"));
      // "icon" as alias for "type" for easier HA usage
      String iconField = normalizeNotifyType(String((const char*)(notifyDoc["icon"] | "")));
      if (iconField.length() > 0 && notifyType == "generic") {
        notifyType = iconField;
      }
      sound = String((const char*)(notifyDoc["sound"] | ""));
    }
  }

  text.trim();
  if (text.length() == 0) return;
  if (duration < 1000) duration = 1000;
  if (duration > 20000) duration = 20000;

  if (wake && (isAsleep || isGoingToSleep)) {
    wakeBobFromSleep("notify", true);
  }

  notifyType = normalizeNotifyType(notifyType);

  if (emotion.length() == 0) {
    if (notifyType == "doorbell" || notifyType == "bel" || notifyType == "deurbel") {
      emotion = "Surprised";
    } else if (notifyType == "mail" || notifyType == "brief" || notifyType == "post") {
      emotion = "Thinking";
    } else if (notifyType == "welcome") {
      emotion = "Happy";
    } else if (notifyType == "focus") {
      emotion = "Thinking";
    } else if (notifyType == "warning") {
      emotion = "Angry";
    } else if (notifyType == "error") {
      emotion = "Angry";
    } else if (notifyType == "alarm") {
      emotion = "Scared";
    } else if (notifyType == "sleep") {
      emotion = "Sleepy";
    } else if (notifyType == "success") {
      emotion = "Happy";
    }
  }

  if (emotion.length() > 0) {
    bool ok = false;
    PersonalityExtended p = parsePersonality(emotion, ok);
    if (ok) {
      applyPersonality(p, min<uint32_t>(duration, 6000));
    }
  }

  // Stel visueel XBM icoon in op basis van notifyType
  notifyType = normalizeNotifyType(notifyType); // al genormaliseerd, maar voor zekerheid
  if (notifyType == "mail" || notifyType == "brief" || notifyType == "post") {
    eyeNotifyVisual = EyeNotifyVisual::Mail;
    eyeNotifyUntil = millis() + min(duration, (uint32_t)6000);
  } else if (notifyType == "alarm" || notifyType == "warning" || notifyType == "alert") {
    eyeNotifyVisual = EyeNotifyVisual::Alarm;
    eyeNotifyUntil = millis() + min(duration, (uint32_t)5000);
  } else if (notifyType == "alarm_clock" || notifyType == "wekker" || notifyType == "timer") {
    eyeNotifyVisual = EyeNotifyVisual::AlarmClock;
    eyeNotifyUntil = millis() + min(duration, (uint32_t)6000);
  }

  if (sound.length() == 0) {
    if (notifyType == "doorbell" || notifyType == "bel" || notifyType == "deurbel") sound = "doorbell";
    else if (notifyType == "mail" || notifyType == "brief" || notifyType == "post") sound = "mail";
    else if (notifyType == "alarm") sound = "alarm";
    else if (notifyType == "warning") sound = "warning";
    else if (notifyType == "success") sound = "success";
    else if (notifyType == "sleep") sound = "sleep";
    else sound = "chime";
  }

  if (!isAllowedSoundKey(sound)) {
    DEBUG_PRINTF("Unknown sound key '%s', falling back to chime\n", sound.c_str());
    sound = "chime";
  }
  publishNotifySoundKey(sound);

  mqttClient.publish("bob/notify_state", "shown", true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  message.reserve(length); // Pre-allocate memory
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  DEBUG_VERBOSE_PRINTF("MQTT received [%s]: %s\n", topic, message.c_str());
  
  if (strcmp(topic, TOPIC_CMD_BEHAV) == 0) {
    // Handle special commands first
    if (message.equalsIgnoreCase("Auto")) {
      manualOverride = false;
      Serial.println("Manual override disabled - resuming automatic behaviors");
      return;
    }
    
    // Enable manual override to prevent automatic cycling
    manualOverride = true;
    manualOverrideTimeout = millis() + MANUAL_OVERRIDE_DURATION;
    
    // Find and trigger the requested behavior
    Behavior requestedBehavior = findBehaviorByName(message.c_str());

    // Add debug logging
    Serial.printf("MQTT Behavior request: '%s'\n", message.c_str());
    Serial.printf("Mapped to behavior enum: %s\n", behaviorName(requestedBehavior));

    triggerBehavior(requestedBehavior);

    if (message.equalsIgnoreCase("Follow")) {
    }

    Serial.printf("Behavior triggered: %s (manual override active for %d ms)\n", behaviorName(requestedBehavior), MANUAL_OVERRIDE_DURATION);
  } else if (strcmp(topic, TOPIC_CMD_SCREEN) == 0) {
    // Screen on/off control with automatic animation triggers
    if (message.equalsIgnoreCase("on")) {
      wakeBobFromSleep("mqtt_screen_on", true);
      
      Serial.println("Screen turned ON - WakeUpSequence triggered");
    } else if (message.equalsIgnoreCase("off")) {
      screenOn = false;
      manualSleepTime = millis(); // Start cooldown timer
      M5.Display.sleep(); // Fysiek scherm uitzetten
      M5.Display.setBrightness(0); // Brightness op 0

      // Trigger Sleep behavior if not in always awake mode (check charging too)
      bool shouldStayAwake = alwaysAwake || (autoAwakeOnPower && M5.Power.isCharging());
      if (!shouldStayAwake) {
        triggerBehavior(Behavior::Sleep);
        isGoingToSleep = true;
        Serial.println("Screen turned OFF - Sleep triggered (5s cooldown active)");
      } else {
        Serial.println("Screen turned OFF - Sleep skipped (Always Awake mode, 5s cooldown active)");
      }
    }
  } else if (strcmp(topic, TOPIC_CMD_WAKE) == 0) {
    // Remote wake command - wake Bob from deep sleep or normal sleep
    if (message.equalsIgnoreCase("ON") || message.equalsIgnoreCase("WAKE")) {
      Serial.println(" Wake command received from Home Assistant!");

      // If in deep sleep, this won't help (device is sleeping)
      // But if in normal sleep mode, we can wake it up
      if (isAsleep || isGoingToSleep) {
        Serial.println("  Waking Bob from sleep via MQTT command");
        wakeBobFromSleep("mqtt_wake", true);

        Serial.println(" Bob is now awake!");
      } else {
        Serial.println("Bob is already awake!");
      }
    } else {
      Serial.println(" Wake command OFF - Bob can sleep normally");
    }
  } else if (strcmp(topic, TOPIC_CMD_BRIGHTNESS) == 0) {
    // Brightness control (0-255) - disables auto brightness when manually set
    int brightness = message.toInt();
    Serial.printf("BRIGHTNESS COMMAND received: '%s' -> parsed: %d\n", message.c_str(), brightness);
    if (brightness >= 0 && brightness <= 255) {
      autoBrightnessEnabled = false; // Disable auto when manually set
      screenBrightness = brightness;
      if (screenOn) {
        M5.Display.setBrightness(screenBrightness);
        Serial.printf("Display brightness applied: %d (auto brightness OFF)\n", screenBrightness);
      } else {
        Serial.printf("Brightness stored (screen off): %d\n", screenBrightness);
      }
      // Publish status update immediately
      publishStatus("online");
    } else {
      Serial.printf("Invalid brightness value: %d (must be 0-255)\n", brightness);
    }
  } else if (strcmp(topic, "bob/cmd/auto_brightness") == 0) {
    // Auto brightness control
    if (message.equalsIgnoreCase("ON")) {
      autoBrightnessEnabled = true;
      Serial.println(" Auto brightness ENABLED");
    } else if (message.equalsIgnoreCase("OFF")) {
      autoBrightnessEnabled = false;
      Serial.println(" Auto brightness DISABLED");
    }
    publishStatus("online");
  } else if (strcmp(topic, TOPIC_CMD_SLEEP_TIMEOUT) == 0) {
    // Sleep timeout control (in seconds, convert to milliseconds)
    int timeoutSeconds = message.toInt();
    Serial.printf("WAKE TIME COMMAND received: '%s' -> parsed: %d seconds\n", message.c_str(), timeoutSeconds);
    if (timeoutSeconds >= 10 && timeoutSeconds <= 3600) { // 10 seconds to 1 hour
      INACTIVITY_TIMEOUT_MS = timeoutSeconds * 1000;
      Serial.printf("Wake tijd ingesteld op: %d seconden (%d ms)\n", timeoutSeconds, INACTIVITY_TIMEOUT_MS);
      // Publish status update immediately
      publishStatus("online");
    } else {
      Serial.printf("Ongeldige wake tijd: %d (moet 10-3600 seconden zijn)\n", timeoutSeconds);
    }

  } else if (strcmp(topic, TOPIC_CMD_ALWAYS_AWAKE) == 0) {
    // Always awake toggle
    if (message.equalsIgnoreCase("ON")) {
      alwaysAwake = true;
      // Always Awake should also wake up the screen immediately
      wakeBobFromSleep("always_awake", true);
      Serial.println("Always Awake ENABLED - Bob will never sleep automatically and screen is ON");
    } else if (message.equalsIgnoreCase("OFF")) {
      alwaysAwake = false;
      Serial.println("Always Awake DISABLED - Bob will sleep normally");
    }
    publishStatus("online"); // Update status
  } else if (strcmp(topic, "bob/cmd/auto_awake_on_power") == 0) {
    // Auto awake on power toggle
    if (message.equalsIgnoreCase("ON")) {
      autoAwakeOnPower = true;
      Serial.println("Auto Awake On Power ENABLED - Bob stays awake when charging");
    } else if (message.equalsIgnoreCase("OFF")) {
      autoAwakeOnPower = false;
      Serial.println("Auto Awake On Power DISABLED");
    }
    publishStatus("online"); // Confirm and update status

  } else if (strcmp(topic, "bob/cmd/test") == 0) {
    // Test commando's voor het testen van effecten
    if (message.equalsIgnoreCase("shake")) {
      Serial.println(" TEST: Simuleer shake effect");
      shakeDetected = true;
      lastShakeTime = millis();
      showingIrritation = false;
      matrixMode = false;
      M5.Display.setBrightness(255);
    } else if (message.equalsIgnoreCase("stop_shake")) {
      Serial.println(" TEST: Stop shake, start irritatie");
      shakeDetected = false;
      showingIrritation = true;
      shakeStoppedTime = millis();
    } else if (message.equalsIgnoreCase("matrix")) {
      Serial.println(" TEST: Matrix mode AAN");
      matrixMode = true;
      shakeDetected = false;
      showingIrritation = false;
      initMatrix(); // Initialiseer matrix kolommen
    } else if (message.equalsIgnoreCase("matrix_off")) {
      Serial.println(" TEST: Matrix mode UIT");
      matrixMode = false;
    } else if (message.equalsIgnoreCase("center") || message.equalsIgnoreCase("reset")) {
      Serial.println(" TEST: Reset naar centrum");
      eyeLookX = 0;
      eyeLookY = 0;
      shakeDetected = false;
      showingIrritation = false;
      matrixMode = false;
      M5.Display.setBrightness(screenBrightness);
    }

  } else if (strcmp(topic, TOPIC_CMD_MICROPHONE) == 0) {
    // Microphone control
    if (message.equalsIgnoreCase("OFF")) {
      microphoneEnabled = false;
      cleanupMicrophone();
      Serial.println(" Microphone disabled and buffer cleaned up");
    } else if (message.equalsIgnoreCase("ON")) {
      Serial.println(" Microphone enabling not supported (audio-free version)");
    }
  } else if (strcmp(topic, TOPIC_CMD_PROXIMITY) == 0) {
    // Proximity sensor on/off control
    if (message.equalsIgnoreCase("on")) {
      proximityEnabled = true;
      Serial.println("Proximity sensor enabled");
    } else if (message.equalsIgnoreCase("off")) {
      proximityEnabled = false;
      Serial.println("Proximity sensor disabled");
    }
  } else if (strcmp(topic, TOPIC_CMD_PROXIMITY_THRESHOLD) == 0) {
    // Proximity sensitivity control (0-255, higher = more sensitive)
    int sensitivity = message.toInt();
    if (sensitivity >= 0 && sensitivity <= 255) {
      proximityThreshold = sensitivity;
      Serial.printf("Proximity sensitivity set to: %d (higher = more sensitive, effective threshold: %d)\n",
                    proximityThreshold, (255 - proximityThreshold));
    } else {
      Serial.printf("Invalid proximity sensitivity: %d (must be 0-255)\n", sensitivity);
    }
  } else if (strcmp(topic, TOPIC_CMD_EYE_X) == 0) {
    // Manual eye X control
    float eyeX = message.toFloat();
    eyeX = clampf(eyeX, -MAX_LOOK_X, MAX_LOOK_X);
    targetLookX = eyeX;
    Serial.printf("Manual eye X set to: %.2f\n", eyeX);
  } else if (strcmp(topic, TOPIC_CMD_EYE_Y) == 0) {
    // Manual eye Y control
    float eyeY = message.toFloat();
    eyeY = clampf(eyeY, -MAX_LOOK_Y, MAX_LOOK_Y);
    targetLookY = eyeY;
    Serial.printf("Manual eye Y set to: %.2f\n", eyeY);
  } else if (strcmp(topic, "bob/cmd/snapshot") == 0) {
    // Camera snapshot command (MQTT)
    Serial.println(" Snapshot command received via MQTT");
    takeCameraSnapshot();
  } else if (strcmp(topic, "bob/cmd/snapshot_ha") == 0) {
    // Upload snapshot to Home Assistant via REST API
    Serial.println(" HA Snapshot command received via MQTT");
    uploadSnapshotToHA();
  } else if (strcmp(topic, "bob/cmd/camera_stream") == 0) {
    // Toggle camera streaming
    if (message == "ON") {
      Serial.println(" Camera streaming ON");
      startCameraStreaming();
    } else if (message == "OFF") {
      Serial.println(" Camera streaming OFF");
      stopCameraStreaming();
    }
  } else if (strcmp(topic, "bob/cmd/matrix") == 0) {
    // Matrix mode toggle
    if (message == "ON") {
      matrixMode = true;
      initMatrix();
      Serial.println(" Matrix mode ENABLED");
      publishNotifySoundKey("matrix_start");
    } else {
      matrixMode = false;
      Serial.println(" Matrix mode DISABLED");
    }
  } else if (strcmp(topic, "bob/cmd/clock") == 0 || strcmp(topic, "bob/cmd/screensaver") == 0) {
    // Clock/screensaver mode toggle
    if (message == "ON") {
      clockMode = true;
      matrixMode = false;
      confettiMode = CONFETTI_NONE;
      confettiSnowEnabled = false;
      confettiCannonsEnabled = false;
      Serial.println(" Clock/Screensaver mode ENABLED");
    } else {
      clockMode = false;
      Serial.println(" Clock/Screensaver mode DISABLED");
    }
  } else if (strcmp(topic, "bob/cmd/tracking") == 0) {
    // Natural eye movement toggle
    if (message == "ON") {
      objectTrackingEnabled = true;
      Serial.println(" Natural eye movement ENABLED");
    } else {
      objectTrackingEnabled = false;
      Serial.println(" Natural eye movement DISABLED");
    }
  } else if (strcmp(topic, TOPIC_CMD_NOTIFY) == 0) {
    handleNotifyCommand(message);
  } else if (strcmp(topic, TOPIC_CMD_PERSONALITY) == 0) {
    if (message.equalsIgnoreCase("Random")) {
      if (!isAsleep && !isGoingToSleep) {
        size_t pick = random(AUTO_EMOTION_COUNT);
        applyPersonality(AUTO_EMOTIONS[pick], 4000);
      }
    } else {
      bool ok = false;
      PersonalityExtended p = parsePersonality(message, ok);
      if (ok) {
        applyPersonality(p, 4000);
      } else {
        Serial.printf("Unknown personality: %s\n", message.c_str());
      }
    }
  } else if (strcmp(topic, TOPIC_CMD_PERSONALITY_AUTO) == 0) {
    if (message.equalsIgnoreCase("ON")) {
      autoEmotionEnabled = true;
      Serial.println("Auto emotion enabled");
    } else if (message.equalsIgnoreCase("OFF")) {
      autoEmotionEnabled = false;
      Serial.println("Auto emotion disabled");
    }
  } else if (strcmp(topic, "bob/cmd/confetti") == 0) {
    // Confetti SNOW mode (van boven vallend) - SWITCH CONTROL
    if (message == "ON" || message == "FIRE") {
      confettiSnowEnabled = true;
      confettiMode = CONFETTI_SNOW;
      initConfetti();
      Serial.println(" CONFETTI SNOW MODE - Falling from top!");
      Serial.println(" CONFETTI SNOW MODE ENABLED - PARTY TIME! ");
      publishNotifySoundKey("confetti_snow");
    } else if (message == "OFF") {
      confettiSnowEnabled = false;
      if (confettiMode == CONFETTI_SNOW) {
        confettiMode = CONFETTI_NONE;
      }
      Serial.println(" Confetti snow DISABLED");
    }
  } else if (strcmp(topic, "bob/cmd/confetti_cannons") == 0) {
    // Confetti CANNONS mode (linksonder + rechtsonder kanonnen) - SWITCH CONTROL
    if (message == "ON" || message == "FIRE") {
      confettiCannonsEnabled = true;
      confettiMode = CONFETTI_CANNONS;
      initConfetti();
      Serial.println(" CONFETTI CANNONS MODE - BOOM BOOM! ");
      Serial.println(" CONFETTI CANNONS ENABLED - FIRE! ");
      publishNotifySoundKey("confetti_cannons");
    } else if (message == "OFF") {
      confettiCannonsEnabled = false;
      if (confettiMode == CONFETTI_CANNONS) {
        confettiMode = CONFETTI_NONE;
      }
      Serial.println(" Confetti cannons DISABLED");
    }
  } else if (strcmp(topic, TOPIC_CMD_ROTATION) == 0) {
    // Screen rotation control (0=0deg, 1=90deg, 2=180deg, 3=270deg)
    int rotation = message.toInt();
    if (rotation >= 0 && rotation <= 3) {
      screenRotation = rotation;
      rotateScreen();
      Serial.printf("Scherm rotatie ingesteld op: %d (%d)\n", screenRotation * 90, screenRotation);
    } else {
      Serial.printf("Ongeldige rotatie waarde: %d (moet 0-3 zijn)\n", rotation);
    }
  } else if (strcmp(topic, "bob/cmd/answer") == 0) {
    // Bob says YES or NO by moving eyes
    if (message == "yes") {
      // NOD animation - nod head (eyes up/down) - yes = nodding
      DEBUG_PRINTLN(" Yes - nodding head");
      currentAnimation = ANIM_NO;
      isAnimatingEyes = true;
      animationStartTime = millis();
      animationEndTime = millis() + 1800;  // 300ms per position * 6 positions

      // Publish state
      if (mqttClient.connected()) {
        mqttClient.publish("bob/animation_state", "yes", true);
      }

    } else if (message == "no") {
      // SHAKE animation - shake head (eyes left/right) - no = shaking
      DEBUG_PRINTLN(" No - shaking head");
      currentAnimation = ANIM_YES;
      isAnimatingEyes = true;
      animationStartTime = millis();
      animationEndTime = millis() + 1800;  // 300ms per position * 6 positions

      // Publish state
      if (mqttClient.connected()) {
        mqttClient.publish("bob/animation_state", "no", true);
      }
    }

  } else if (strcmp(topic, "bob/cmd/animate") == 0) {
    // Animation menu - trigger various animations
    DEBUG_PRINTF(" Animation command: %s\n", message.c_str());

    if (message == "confetti_snow") {
      confettiMode = CONFETTI_SNOW;
      confettiSnowEnabled = true;
      DEBUG_PRINTLN(" Confetti Snow started");

    } else if (message == "confetti_cannons") {
      confettiMode = CONFETTI_CANNONS;
      confettiCannonsEnabled = true;
      DEBUG_PRINTLN(" Confetti Cannons started");

    } else if (message == "stop_confetti") {
      confettiMode = CONFETTI_NONE;
      confettiSnowEnabled = false;
      confettiCannonsEnabled = false;
      DEBUG_PRINTLN(" Confetti stopped");

    } else if (message == "wake") {
      triggerBehavior(Behavior::WakeUpSequence);
      DEBUG_PRINTLN(" Wake animation");

    } else if (message == "sleep") {
      triggerBehavior(Behavior::Sleep);
      DEBUG_PRINTLN(" Sleep animation");

    } else if (message == "wake_up_sequence") {
      currentManualBehavior = Behavior::WakeUpSequence;
      runner.idx = -1;
      currentBehaviorName = "Waking Up";
      behaviorStartTime = millis();
      DEBUG_PRINTLN(" Wake up sequence started");

    } else if (message == "blink") {
      // Trigger a blink
      blinking = true;
      blinkStart = millis();
      currentShape = blinkShape;
      DEBUG_PRINTLN(" Blink");

    } else {
      DEBUG_PRINTF(" Unknown animation: %s\n", message.c_str());
      Serial.println("Available: confetti_snow, confetti_cannons, stop_confetti, wake, sleep, wake_up_sequence, blink");
      return;
    }

    // Publish animation state back to HA
    if (mqttClient.connected()) {
      mqttClient.publish("bob/animation_state", message.c_str(), true);
      DEBUG_PRINTF(" Animation state published: %s\n", message.c_str());
    }
  }
}

bool initializeMQTT() {
  Serial.printf("Connecting to MQTT broker: %s:%d\n", runtimeMqttHost.c_str(), runtimeMqttPort);
  Serial.printf("Client ID: %s, Username: %s\n", runtimeMqttClientId.c_str(), runtimeMqttUser.c_str());
  
  setupMQTTBuffer(); // Increase buffer size for auto-discovery
  mqttClient.setServer(runtimeMqttHost.c_str(), runtimeMqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60); // Keep alive 60 seconds
  mqttClient.setSocketTimeout(10); // Socket timeout 10 seconds

  if (mqttClient.connect(runtimeMqttClientId.c_str(), runtimeMqttUser.c_str(), runtimeMqttPass.c_str(),
                         "bob/online", 0, true, "offline")) {
    Serial.println("MQTT connected successfully");
    mqttClient.publish("bob/online", "online", true); // retained: device is online

    // Subscribe to command topics
    mqttClient.subscribe(TOPIC_CMD_BEHAV);
    mqttClient.subscribe(TOPIC_CMD_SCREEN);
    mqttClient.subscribe(TOPIC_CMD_WAKE);  // Remote wake command for deep sleep
    mqttClient.subscribe(TOPIC_CMD_BRIGHTNESS);
    mqttClient.subscribe(TOPIC_CMD_SLEEP_TIMEOUT);
    mqttClient.subscribe(TOPIC_CMD_ALWAYS_AWAKE);
    mqttClient.subscribe("bob/cmd/auto_awake_on_power");
    mqttClient.subscribe("bob/cmd/test"); // Test commando's
    mqttClient.subscribe(TOPIC_CMD_MICROPHONE);
    mqttClient.subscribe(TOPIC_CMD_PROXIMITY);
    mqttClient.subscribe(TOPIC_CMD_PROXIMITY_THRESHOLD);
    mqttClient.subscribe(TOPIC_CMD_PERSONALITY);
    mqttClient.subscribe(TOPIC_CMD_PERSONALITY_AUTO);
    mqttClient.subscribe(TOPIC_CMD_NOTIFY);
    mqttClient.subscribe(TOPIC_CMD_EYE_X);
    mqttClient.subscribe(TOPIC_CMD_EYE_Y);
    mqttClient.subscribe(TOPIC_CMD_ROTATION);
    mqttClient.subscribe("bob/cmd/snapshot");
    mqttClient.subscribe("bob/cmd/snapshot_ha");
    mqttClient.subscribe("bob/cmd/camera_stream");
    mqttClient.subscribe("bob/cmd/matrix");
    mqttClient.subscribe("bob/cmd/clock");
    mqttClient.subscribe("bob/cmd/screensaver");
    mqttClient.subscribe("bob/cmd/tracking");
    mqttClient.subscribe("bob/cmd/confetti");
    mqttClient.subscribe("bob/cmd/confetti_cannons");
    mqttClient.subscribe("bob/cmd/auto_brightness");

    // Bob answer (yes/no) with head shake/nod
    mqttClient.subscribe("bob/cmd/answer");

    // Bob animation menu
    mqttClient.subscribe("bob/cmd/animate");

    Serial.println(" MQTT subscriptions configured");

    // Publish auto discovery first
    Serial.println("Publishing MQTT Auto Discovery...");
    publishAutoDiscovery();

    // Publish MP3 mediaplayer discovery for Home Assistant (disabled)
    /*
    idleDelayWithMqtt(100); // Wait for auto-discovery to fully complete
    if (mqttClient.connected()) {  // Re-check MQTT connection
      publishMP3MediaplayerDiscovery();
      idleDelayWithMqtt(50);
    } else {
      Serial.println("  MQTT disconnected after auto-discovery, skipping mediaplayer discovery");
    }
    */

    // Then publish status
    Serial.println("Publishing initial status...");
    if (mqttClient.connected()) {
      publishStatus("online");
      idleDelayWithMqtt(200); // Wait for status to publish
    } else {
      Serial.println("  MQTT disconnected, skipping status publish");
    }

    // Publish initial sensor states
    mqttClient.publish("bob/touch", "OFF", true);
    mqttClient.publish("bob/shake", "OFF", true);
    mqttClient.publish("bob/proximity", "OFF", true);
    mqttClient.publish("bob/proximity_detected", "OFF", true);
    mqttClient.publish("bob/tracking_status", "OFF", true);
    mqttClient.publish("bob/ollama_state", "idle", true); // Initialize Ollama state
    mqttClient.publish("bob/notify_state", "idle", true);
    Serial.println("Initial sensor states published: Proximity=OFF");

    return true;
  } else {
    Serial.printf("MQTT connection failed, rc=%d\n", mqttClient.state());
    return false;
  }
}


void publishAutoDiscovery() {
  if (!mqttClient.connected()) return;

  DEBUG_PRINTLN("Publishing auto-discovery configuration...");

  const uint16_t DISCOVERY_DELAY = 100;  // Shorter delay between publishes

  // Test met alleen brightness control
  String brightnessConfig = "{\"name\":\"Bob Brightness\",\"uniq_id\":\"bob_brightness\",\"cmd_t\":\"bob/cmd/brightness\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.brightness}}\",\"min\":0,\"max\":255,\"icon\":\"mdi:brightness-6\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result1 = mqttClient.publish("homeassistant/number/bob/brightness/config", brightnessConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Brightness: %s\n", result1 ? "SUCCESS" : "FAILED");
  yield();  // Give CPU time to WiFi stack
  idleDelayWithMqtt(DISCOVERY_DELAY);

  // Touch sensor
  String touchConfig = "{\"name\":\"Bob Touch\",\"uniq_id\":\"bob_touch\",\"stat_t\":\"bob/touch\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"motion\",\"icon\":\"mdi:hand-front-right\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result2 = mqttClient.publish("homeassistant/binary_sensor/bob/touch/config", touchConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Touch: %s\n", result2 ? "SUCCESS" : "FAILED");
  yield();
  idleDelayWithMqtt(DISCOVERY_DELAY);

  // Proximity sensor (waarde)
  String proximityConfig = "{\"name\":\"Bob Proximity Value\",\"uniq_id\":\"bob_proximity_value\",\"stat_t\":\"bob/proximity_value\",\"unit_of_measurement\":\"\",\"icon\":\"mdi:hand-wave\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result3 = mqttClient.publish("homeassistant/sensor/bob/proximity_value/config", proximityConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Proximity Value: %s\n", result3 ? "SUCCESS" : "FAILED");
  yield();
  idleDelayWithMqtt(DISCOVERY_DELAY);

  // Proximity detected (binary)
  String proximityBinaryConfig = "{\"name\":\"Bob Proximity Detected\",\"uniq_id\":\"bob_proximity_detected\",\"stat_t\":\"bob/proximity_detected\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"motion\",\"icon\":\"mdi:motion-sensor\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result4 = mqttClient.publish("homeassistant/binary_sensor/bob/proximity_detected/config", proximityBinaryConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Proximity Detected: %s\n", result4 ? "SUCCESS" : "FAILED");
  yield();
  idleDelayWithMqtt(DISCOVERY_DELAY);

  // Shake sensor
  String shakeConfig = "{\"name\":\"Bob Shake\",\"uniq_id\":\"bob_shake\",\"stat_t\":\"bob/shake\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"motion\",\"icon\":\"mdi:vibrate\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result5 = mqttClient.publish("homeassistant/binary_sensor/bob/shake/config", shakeConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Shake: %s\n", result5 ? "SUCCESS" : "FAILED");
  yield();
  idleDelayWithMqtt(DISCOVERY_DELAY);

  // Movement sensor (touch, shake, or proximity detected)
  String movementConfig = "{\"name\":\"Bob Movement\",\"uniq_id\":\"bob_movement\",\"stat_t\":\"bob/movement\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"motion\",\"icon\":\"mdi:motion-sensor\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result_movement = mqttClient.publish("homeassistant/binary_sensor/bob/movement/config", movementConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Movement: %s\n", result_movement ? "SUCCESS" : "FAILED");
  yield();
  idleDelayWithMqtt(DISCOVERY_DELAY);

  // Screen control
  String screenConfig = "{\"name\":\"Bob Screen\",\"uniq_id\":\"bob_screen\",\"cmd_t\":\"bob/cmd/screen\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.screen_on}}\",\"payload_on\":\"on\",\"payload_off\":\"off\",\"icon\":\"mdi:monitor\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result6 = mqttClient.publish("homeassistant/switch/bob/screen/config", screenConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Screen: %s\n", result6 ? "SUCCESS" : "FAILED");
  yield();
  idleDelayWithMqtt(DISCOVERY_DELAY);

  // Yes button
  String yesButtonConfig = "{\"name\":\"Bob Yes\",\"uniq_id\":\"bob_yes\",\"cmd_t\":\"bob/cmd/answer\",\"payload_press\":\"yes\",\"icon\":\"mdi:check-circle\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool resultYes = mqttClient.publish("homeassistant/button/bob/yes/config", yesButtonConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Yes Button: %s\n", resultYes ? "SUCCESS" : "FAILED");
  yield();
  idleDelayWithMqtt(DISCOVERY_DELAY);

  // No button
  String noButtonConfig = "{\"name\":\"Bob No\",\"uniq_id\":\"bob_no\",\"cmd_t\":\"bob/cmd/answer\",\"payload_press\":\"no\",\"icon\":\"mdi:close-circle\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool resultNo = mqttClient.publish("homeassistant/button/bob/no/config", noButtonConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - No Button: %s\n", resultNo ? "SUCCESS" : "FAILED");
  yield();
  idleDelayWithMqtt(DISCOVERY_DELAY);









  // Behavior selector - VERWIJDERD (niet meer nodig)

  // Always awake switch
  String alwaysAwakeConfig = "{\"name\":\"Bob Always Awake\",\"uniq_id\":\"bob_always_awake\",\"cmd_t\":\"bob/cmd/always_awake\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.always_awake}}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:sleep-off\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result10 = mqttClient.publish("homeassistant/switch/bob/always_awake/config", alwaysAwakeConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Always Awake: %s\n", result10 ? "SUCCESS" : "FAILED");
  idleDelayWithMqtt(1000);

  // Volume control (0-10) auto discovery removed

  // Eye tracking X position sensor
  String eyeXConfig = "{\"name\":\"Bob Eye X Position\",\"uniq_id\":\"bob_eye_x\",\"stat_t\":\"bob/eye_position\",\"val_tpl\":\"{{value_json.eye_x}}\",\"unit_of_measurement\":\"deg\",\"icon\":\"mdi:eye\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result14 = mqttClient.publish("homeassistant/sensor/bob/eye_x/config", eyeXConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Eye X: %s\n", result14 ? "SUCCESS" : "FAILED");
  idleDelayWithMqtt(1000);

  // Eye tracking Y position sensor
  String eyeYConfig = "{\"name\":\"Bob Eye Y Position\",\"uniq_id\":\"bob_eye_y\",\"stat_t\":\"bob/eye_position\",\"val_tpl\":\"{{value_json.eye_y}}\",\"unit_of_measurement\":\"deg\",\"icon\":\"mdi:eye\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result15 = mqttClient.publish("homeassistant/sensor/bob/eye_y/config", eyeYConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Eye Y: %s\n", result15 ? "SUCCESS" : "FAILED");
  idleDelayWithMqtt(1000);

  // Microphone sound detection binary sensor
  // Extra check: ensure MQTT is still connected before publishing
  if (!mqttClient.connected()) {
    Serial.println(" MQTT disconnected before microphone config - reconnecting...");
    mqttClient.connect(runtimeMqttClientId.c_str(), runtimeMqttUser.c_str(), runtimeMqttPass.c_str());
    idleDelayWithMqtt(500);
  }

  String micConfig = "{\"name\":\"Bob Microphone\",\"uniq_id\":\"bob_microphone\",\"stat_t\":\"bob/microphone\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"sound\",\"icon\":\"mdi:microphone\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result17 = mqttClient.publish("homeassistant/binary_sensor/bob/microphone/config", micConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Microphone: %s\n", result17 ? "SUCCESS" : "FAILED");

  if (!result17) {
    Serial.printf(" Microphone discovery failed! MQTT state: %d, buffer size: %d\n",
                  mqttClient.state(), micConfig.length());
  }

  yield();
  idleDelayWithMqtt(500);  // Longer delay for microphone (was DISCOVERY_DELAY=100ms)

  // Sound level sensor (numeric value)
  String soundLevelConfig = "{\"name\":\"Bob Sound Level\",\"uniq_id\":\"bob_sound_level\",\"stat_t\":\"bob/sensors\",\"val_tpl\":\"{{value_json.sound_level}}\",\"unit_of_measurement\":\"\",\"icon\":\"mdi:waveform\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result18 = mqttClient.publish("homeassistant/sensor/bob/sound_level/config", soundLevelConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Sound Level: %s\n", result18 ? "SUCCESS" : "FAILED");
  yield();
  idleDelayWithMqtt(DISCOVERY_DELAY);

  // Wake tijd slider
  String wakeTimeConfig = "{\"name\":\"Bob Sleep Timer\",\"uniq_id\":\"bob_sleep_timer\",\"cmd_t\":\"bob/cmd/sleep_timeout\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.sleep_timeout}}\",\"min\":10,\"max\":600,\"step\":5,\"icon\":\"mdi:timer\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result11 = mqttClient.publish("homeassistant/number/bob/sleep_timer/config", wakeTimeConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Sleep Timer: %s\n", result11 ? "SUCCESS" : "FAILED");
  idleDelayWithMqtt(1000);

  // Auto awake on power switch
  String autoAwakeConfig = "{\"name\":\"Bob Auto Awake On Power\",\"uniq_id\":\"bob_auto_awake_power\",\"cmd_t\":\"bob/cmd/auto_awake_on_power\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.auto_awake_on_power}}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:power-plug\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  bool result20 = mqttClient.publish("homeassistant/switch/bob/auto_awake_power/config", autoAwakeConfig.c_str(), true);
  DEBUG_PRINTF("Auto Discovery - Auto Awake On Power: %s\n", result20 ? "SUCCESS" : "FAILED");
  yield();
  idleDelayWithMqtt(DISCOVERY_DELAY);



  // Bob State Sensor (awake/sleeping/going_to_sleep)
  String stateConfig = "{\"name\":\"Bob State\",\"uniq_id\":\"bob_state\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.state}}\",\"icon\":\"mdi:power\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  DEBUG_PRINTF("Publishing state config (len=%d)\n", stateConfig.length());
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected before state config");
  } else {
    bool result_state = mqttClient.publish("homeassistant/sensor/bob/state/config", stateConfig.c_str(), true);
    DEBUG_PRINTF("Auto Discovery - State: %s\n", result_state ? "SUCCESS" : "FAILED");
  }
  idleDelayWithMqtt(1000);

  String wakeReasonConfig = "{\"name\":\"Bob Wake Reason\",\"uniq_id\":\"bob_wake_reason\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.wake_reason}}\",\"icon\":\"mdi:history\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    bool result_wake_reason = mqttClient.publish("homeassistant/sensor/bob/wake_reason/config", wakeReasonConfig.c_str(), true);
    DEBUG_PRINTF("Auto Discovery - Wake Reason: %s\n", result_wake_reason ? "SUCCESS" : "FAILED");
  }
  idleDelayWithMqtt(DISCOVERY_DELAY);
  // Proximity Control Switch
  String proximityControlConfig = "{\"name\":\"Bob Proximity\",\"uniq_id\":\"bob_proximity_control\",\"cmd_t\":\"bob/cmd/proximity\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.proximity_enabled}}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:motion-sensor\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  DEBUG_PRINTF("Publishing proximity control config (len=%d)\n", proximityControlConfig.length());
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected before proximity control config");
  } else {
    bool result_prox_ctrl = mqttClient.publish("homeassistant/switch/bob/proximity_control/config", proximityControlConfig.c_str(), true);
    DEBUG_PRINTF("Auto Discovery - Proximity Control: %s\n", result_prox_ctrl ? "SUCCESS" : "FAILED");
  }
  idleDelayWithMqtt(1000);
  // Ollama State Sensor (simplified)
  String ollamaStateConfig = "{\"name\":\"Bob Ollama\",\"uniq_id\":\"bob_ollama\",\"stat_t\":\"bob/ollama_state\",\"icon\":\"mdi:robot\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  DEBUG_PRINTF("Publishing ollama state config (len=%d)\n", ollamaStateConfig.length());
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected before ollama config");
  } else {
    bool result_ollama = mqttClient.publish("homeassistant/sensor/bob/ollama_state/config", ollamaStateConfig.c_str(), true);
    DEBUG_PRINTF("Auto Discovery - Ollama State: %s\n", result_ollama ? "SUCCESS" : "FAILED");
  }
  idleDelayWithMqtt(2000); // Longer delay




  // Proximity threshold control
  String proximityThresholdConfig = "{\"name\":\"Bob Proximity Gevoeligheid\",\"uniq_id\":\"bob_proximity_threshold\",\"cmd_t\":\"bob/cmd/proximity_threshold\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.proximity_threshold}}\",\"min\":0,\"max\":255,\"icon\":\"mdi:tune\",\"unit_of_measurement\":\"\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  DEBUG_PRINTF("Publishing proximity threshold config (len=%d)\n", proximityThresholdConfig.length());
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected before proximity threshold config");
  } else {
    bool result_prox_threshold = mqttClient.publish("homeassistant/number/bob/proximity_threshold/config", proximityThresholdConfig.c_str(), true);
    DEBUG_PRINTF("Auto Discovery - Proximity Threshold: %s\n", result_prox_threshold ? "SUCCESS" : "FAILED");
  }

  idleDelayWithMqtt(1000);

  // Auto brightness switch
  String autoBrightnessConfig = "{\"name\":\"Bob Auto Brightness\",\"uniq_id\":\"bob_auto_brightness\",\"cmd_t\":\"bob/cmd/auto_brightness\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.auto_brightness}}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:brightness-auto\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  DEBUG_PRINTF("Publishing auto brightness config (len=%d)\n", autoBrightnessConfig.length());
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected before auto brightness config");
  } else {
    bool result_auto_brightness = mqttClient.publish("homeassistant/switch/bob/auto_brightness/config", autoBrightnessConfig.c_str(), true);
    DEBUG_PRINTF("Auto Discovery - Auto Brightness: %s\n", result_auto_brightness ? "SUCCESS" : "FAILED");
  }

  idleDelayWithMqtt(1000);

  // Camera via MQTT discovery
  // Uses image_topic for automatic camera detection in HA
  String cameraConfig = "{\"name\":\"Bob Camera\",\"uniq_id\":\"bob_camera\",\"image_topic\":\"bob/camera/image\",\"icon\":\"mdi:camera\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  DEBUG_PRINTF("Publishing camera config (len=%d)\n", cameraConfig.length());
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected before camera config");
  } else {
    bool result_camera = mqttClient.publish("homeassistant/camera/bob/camera/config", cameraConfig.c_str(), true);
    DEBUG_PRINTF("Auto Discovery - Camera: %s\n", result_camera ? "SUCCESS" : "FAILED");

    // Publish initial blank image to create the entity
    mqttClient.publish("bob/camera/image", "", true);
  }

  idleDelayWithMqtt(1000);

  // Camera Snapshot Button
  String snapshotBtnConfig = "{\"name\":\"Bob Camera Snapshot\",\"uniq_id\":\"bob_snapshot\",\"cmd_t\":\"bob/cmd/snapshot_ha\",\"icon\":\"mdi:camera\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  DEBUG_PRINTF("Publishing snapshot button config (len=%d)\n", snapshotBtnConfig.length());
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected before snapshot button");
  } else {
    bool result_snapshot = mqttClient.publish("homeassistant/button/bob/snapshot/config", snapshotBtnConfig.c_str(), true);
    DEBUG_PRINTF("Auto Discovery - Snapshot Button: %s\n", result_snapshot ? "SUCCESS" : "FAILED");
  }

  idleDelayWithMqtt(1000);

  // Camera Streaming Toggle Switch
  String streamSwitchConfig = "{\"name\":\"Bob Camera Streaming\",\"uniq_id\":\"bob_camera_streaming\",\"cmd_t\":\"bob/cmd/camera_stream\",\"stat_t\":\"bob/camera/streaming\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"state_on\":\"active\",\"state_off\":\"inactive\",\"icon\":\"mdi:video-wireless\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  DEBUG_PRINTF("Publishing camera stream switch config (len=%d)\n", streamSwitchConfig.length());
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected before stream switch");
  } else {
    bool result_stream = mqttClient.publish("homeassistant/switch/bob/camera_streaming/config", streamSwitchConfig.c_str(), true);
    DEBUG_PRINTF("Auto Discovery - Camera Stream Switch: %s\n", result_stream ? "SUCCESS" : "FAILED");

    // Publish initial state
    mqttClient.publish("bob/camera/streaming", "inactive", true);
  }

  idleDelayWithMqtt(1000);

  // ========== BEHAVIORS ==========

  // Wake Behavior Button
  String wakeBtnConfig = "{\"name\":\"Bob Wake\",\"uniq_id\":\"bob_wake\",\"cmd_t\":\"bob/cmd/behavior\",\"payload_press\":\"Wake\",\"icon\":\"mdi:eye\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/button/bob/wake/config", wakeBtnConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Wake Button: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // Sleep Behavior Button
  String sleepBtnConfig = "{\"name\":\"Bob Sleep\",\"uniq_id\":\"bob_sleep\",\"cmd_t\":\"bob/cmd/behavior\",\"payload_press\":\"Sleep\",\"icon\":\"mdi:sleep\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/button/bob/sleep/config", sleepBtnConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Sleep Button: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // Follow Behavior Button
  String followBtnConfig = "{\"name\":\"Bob Follow Mode\",\"uniq_id\":\"bob_follow\",\"cmd_t\":\"bob/cmd/behavior\",\"payload_press\":\"Follow\",\"icon\":\"mdi:eye-arrow-right\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/button/bob/follow/config", followBtnConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Follow Button: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // WakeUpSequence Button
  String wakeupBtnConfig = "{\"name\":\"Bob WakeUp Sequence\",\"uniq_id\":\"bob_wakeup_seq\",\"cmd_t\":\"bob/cmd/behavior\",\"payload_press\":\"WakeUpSequence\",\"icon\":\"mdi:alarm\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/button/bob/wakeup_seq/config", wakeupBtnConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - WakeUp Sequence Button: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // ========== TEST BUTTON & TEXT INPUT (REMOVED - Use MQTT direct) ==========

  // ========== VISUAL MODES/OVERLAYS ==========

  // Matrix Mode Switch
  String matrixConfig = "{\"name\":\"Bob Matrix Mode\",\"uniq_id\":\"bob_matrix\",\"cmd_t\":\"bob/cmd/matrix\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.matrix_mode}}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:matrix\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/switch/bob/matrix/config", matrixConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Matrix Mode: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // Clock/Screensaver Mode Switch
  String clockConfig = "{\"name\":\"Bob Clock Screensaver\",\"uniq_id\":\"bob_clock_screensaver\",\"cmd_t\":\"bob/cmd/screensaver\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.clock_mode}}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:clock-outline\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/switch/bob/clock_screensaver/config", clockConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Clock Screensaver: SUCCESS");
  }
  idleDelayWithMqtt(500);



  // ========== TRACKING & SENSORS ==========

  // Object Tracking (natural eye movement)
  String eyeMovementConfig = "{\"name\":\"Bob Eye Movement\",\"uniq_id\":\"bob_eye_movement\",\"cmd_t\":\"bob/cmd/tracking\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.tracking}}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:eye-circle\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/switch/bob/eye_movement/config", eyeMovementConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Eye Movement: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // Confetti Snow Switch (falling from top) - ON/OFF TOGGLE
  String confettiSnowSwitchConfig = "{\"name\":\"Bob Confetti Snow\",\"uniq_id\":\"bob_confetti_snow\",\"cmd_t\":\"bob/cmd/confetti\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.confetti_snow_enabled}}\",\"pl_on\":\"ON\",\"pl_off\":\"OFF\",\"icon\":\"mdi:weather-snowy\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/switch/bob/confetti_snow/config", confettiSnowSwitchConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Confetti Snow Switch: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // Confetti Cannons Switch (left-bottom + right-bottom) - ON/OFF TOGGLE
  String confettiCannonsSwitchConfig = "{\"name\":\"Bob Confetti Cannons\",\"uniq_id\":\"bob_confetti_cannons\",\"cmd_t\":\"bob/cmd/confetti_cannons\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.confetti_cannons_enabled}}\",\"pl_on\":\"ON\",\"pl_off\":\"OFF\",\"icon\":\"mdi:firework\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/switch/bob/confetti_cannons/config", confettiCannonsSwitchConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Confetti Cannons Switch: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // ========== EMOTION SHAPES (READ-ONLY DISPLAY) ==========

  // Current Behavior Sensor
  String behaviorSensorConfig = "{\"name\":\"Bob Current Behavior\",\"uniq_id\":\"bob_current_behavior\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.behavior}}\",\"icon\":\"mdi:emoticon\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/sensor/bob/current_behavior/config", behaviorSensorConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Current Behavior Sensor: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // Emotion select
  String emotionSelectConfig = "{\"name\":\"Bob Emotion\",\"uniq_id\":\"bob_emotion\",\"cmd_t\":\"bob/cmd/personality\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.personality}}\",\"options\":[\"Neutral\",\"Happy\",\"Sad\",\"Thinking\",\"Excited\",\"Confused\",\"Angry\",\"Scared\",\"Sleepy\",\"Love\",\"Surprised\",\"Dizzy\",\"Bored\",\"Random\"],\"icon\":\"mdi:emoticon-outline\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/select/bob/emotion/config", emotionSelectConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Emotion Select: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // Notification text command for HA automations
  String notifyTextConfig = "{\"name\":\"Bob Notification\",\"uniq_id\":\"bob_notification_text\",\"cmd_t\":\"bob/cmd/notify\",\"mode\":\"text\",\"icon\":\"mdi:message-text\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/text/bob/notification/config", notifyTextConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Notification Text: SUCCESS");
  }
  idleDelayWithMqtt(500);

  // Auto emotion switch
  String autoEmotionConfig = "{\"name\":\"Bob Auto Emotion\",\"uniq_id\":\"bob_auto_emotion\",\"cmd_t\":\"bob/cmd/personality_auto\",\"stat_t\":\"bob/status\",\"val_tpl\":\"{{value_json.auto_emotion}}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:auto-fix\",\"dev\":{\"name\":\"Bob\",\"ids\":[\"bob\"]}}";
  if (mqttClient.connected()) {
    mqttClient.publish("homeassistant/switch/bob/auto_emotion/config", autoEmotionConfig.c_str(), true);
    DEBUG_PRINTLN("Auto Discovery - Auto Emotion Switch: SUCCESS");
  }
  idleDelayWithMqtt(500);

  DEBUG_PRINTLN("=== Bob Auto Discovery Compleet ===");
  DEBUG_PRINTLN(" Behaviors: Wake, Sleep, Follow, WakeUpSequence");
  DEBUG_PRINTLN(" Visual Modes: Matrix");
  DEBUG_PRINTLN(" Controls: Eye Movement, Camera");
}

void publishStatus(const char* status) {
  if (!mqttClient.connected()) {
    DEBUG_PRINTLN("MQTT not connected - cannot publish status");
    return;
  }

  // Complete status for Home Assistant entities
  StaticJsonDocument<512> doc;
  doc["status"] = status;
  doc["behavior"] = currentBehaviorName;
  doc["brightness"] = screenBrightness;
  doc["auto_brightness"] = autoBrightnessEnabled ? "ON" : "OFF";
  doc["ambient_light"] = ambientLightLevel;
  doc["always_awake"] = alwaysAwake ? "ON" : "OFF";
  doc["auto_awake_on_power"] = autoAwakeOnPower ? "ON" : "OFF";
  doc["is_charging"] = M5.Power.isCharging() ? "ON" : "OFF";
  doc["sleep_timeout"] = INACTIVITY_TIMEOUT_MS / 1000; // Convert to seconds for HA
  doc["screen_on"] = screenOn ? "on" : "off";
  doc["rotation"] = screenRotation;
  doc["personality"] = getExtendedPersonalityName(currentPersonality);
  doc["auto_emotion"] = autoEmotionEnabled ? "ON" : "OFF";
  doc["proximity_enabled"] = proximityEnabled ? "ON" : "OFF";
  doc["proximity_detected"] = proximityDetected ? "ON" : "OFF";
  doc["proximity_value"] = read_ps_value;
  doc["proximity_threshold"] = proximityThreshold;
  doc["shake_detected"] = shakeDetected ? "ON" : "OFF";
  doc["touch_detected"] = (M5.Touch.getCount() > 0) ? "ON" : "OFF";
  doc["state"] = isAsleep ? "sleeping" : (isGoingToSleep ? "going_to_sleep" : "awake");
  doc["wake_reason"] = lastWakeReason;
  doc["wake_reason_ms"] = lastWakeReasonAt;
  doc["idle_movement"] = objectTrackingEnabled ? "ON" : "OFF";

  // Visual modes/overlays
  doc["matrix_mode"] = matrixMode ? "ON" : "OFF";
  doc["clock_mode"] = clockMode ? "ON" : "OFF";
  doc["tracking"] = objectTrackingEnabled ? "ON" : "OFF";

  // Confetti modes state
  doc["confetti_snow_enabled"] = confettiSnowEnabled ? "ON" : "OFF";
  doc["confetti_cannons_enabled"] = confettiCannonsEnabled ? "ON" : "OFF";

  String jsonString;
  size_t jsonSize = serializeJson(doc, jsonString);

  // Check for buffer overflow
  if (doc.overflowed()) {
    DEBUG_PRINTLN("WARNING: JSON buffer overflow! Increase StaticJsonDocument size.");
  }
  bool result = mqttClient.publish(TOPIC_STATUS, jsonString.c_str());
  if (!result) {
    DEBUG_PRINTF("Status publish failed (topic: %s)\n", TOPIC_STATUS);
  } else {
    DEBUG_VERBOSE_PRINTF("Status published (%d bytes)\n", (int)jsonSize);
  }
}


PersonalityExtended parsePersonality(const String& name, bool& ok) {
  String lower = name;
  lower.toLowerCase();
  lower.trim();
  PersonalityExtended p = findExtendedPersonalityByName(lower.c_str());
  ok = (p != PersonalityExtended::Neutral || lower == "neutral");
  return p;
}


void publishSensorData() {
  if (!mqttEnabled) return;

  StaticJsonDocument<400> doc;
  doc["timestamp"] = millis();

  // Add touch detection (always available)
  bool touchDetected = M5.Touch.getCount() > 0;
  doc["touch_detected"] = touchDetected ? "ON" : "OFF";

  // Publish touch data to dedicated topic (always publish for reliable updates)
  String touchState = touchDetected ? "ON" : "OFF";
  bool touchResult = mqttClient.publish("bob/touch", touchState.c_str(), true); // retained message

  // Publish shake data to dedicated topic
  String shakeState = "OFF";
  if (M5.Imu.isEnabled()) {
    shakeState = shakeDetected ? "ON" : "OFF";
  }
  bool shakeResult = mqttClient.publish("bob/shake", shakeState.c_str(), true); // retained message

  // Publish movement detection (touch or shake - proximity disabled)
  movementDetected = touchDetected || shakeDetected;  // proximityDetected disabled
  String movementState = movementDetected ? "ON" : "OFF";
  bool movementResult = mqttClient.publish("bob/movement", movementState.c_str(), true);

  if (movementDetected != lastMovementState) {
    DEBUG_PRINTF("Movement state changed: %s -> bob/movement publish: %s\n",
                 movementDetected ? "DETECTED" : "STOPPED",
                 movementResult ? "SUCCESS" : "FAILED");
    lastMovementState = movementDetected;
  }

  // Proximity value publishing disabled

  // Debug touch, shake and proximity detection
  static bool lastTouchState = false;
  static bool lastShakeState = false;
  static bool lastProximityState = false;
  static uint32_t lastTouchDebug = 0;
  uint32_t now = millis();

  if (touchDetected != lastTouchState) {
    DEBUG_PRINTF("Touch state changed: %s -> bob/touch publish: %s\n",
                 touchDetected ? "PRESSED" : "RELEASED",
                 touchResult ? "SUCCESS" : "FAILED");
    lastTouchState = touchDetected;
    lastTouchDebug = now;
  }

  bool currentShakeState = (shakeState == "ON");
  if (currentShakeState != lastShakeState) {
    DEBUG_PRINTF("Shake state changed: %s -> bob/shake publish: %s\n",
                 currentShakeState ? "DETECTED" : "STOPPED",
                 shakeResult ? "SUCCESS" : "FAILED");
    lastShakeState = currentShakeState;
  }

  if (proximityDetected != lastProximityState) {
    DEBUG_PRINTF("Proximity state changed: %s\n",
                 proximityDetected ? "DETECTED" : "CLEAR");
    lastProximityState = proximityDetected;
  }

  // Add essential data only
  doc["behavior"] = currentBehaviorName;
  doc["shake_detected"] = shakeDetected ? "ON" : "OFF";

  // Add proximity detection
  doc["proximity_detected"] = proximityDetected ? "ON" : "OFF";
  doc["proximity_value"] = read_ps_value;  // 0-255 proximity sensor value

  // Add microphone detection
  doc["sound_detected"] = soundDetected ? "ON" : "OFF";
  doc["sound_level"] = lastSoundLevel;

  String jsonString;
  serializeJson(doc, jsonString);
  bool result = mqttClient.publish(TOPIC_SENSORS, jsonString.c_str());

  // Publish eye position data separately for tracking
  StaticJsonDocument<200> eyeDoc;
  eyeDoc["eye_x"] = eyeLookX;
  eyeDoc["eye_y"] = eyeLookY;
  eyeDoc["tracking_active"] = proximityDetected ? "ON" : "OFF";  // String waarde voor HA
  eyeDoc["target_x"] = targetLookX;
  eyeDoc["target_y"] = targetLookY;

  String eyeJsonString;
  serializeJson(eyeDoc, eyeJsonString);
  mqttClient.publish("bob/eye_position", eyeJsonString.c_str());

  // Publish separate proximity binary sensor data
  mqttClient.publish("bob/proximity_detected", proximityDetected ? "ON" : "OFF");
  mqttClient.publish("bob/proximity_value", String(read_ps_value).c_str());

  // DEBUG: Always publish proximity status for testing
  static uint32_t lastProximityDebugPublish = 0;
  if (millis() - lastProximityDebugPublish > 3000) { // Every 3 seconds
    DEBUG_VERBOSE_PRINTF("MQTT proximity: detected=%s, value=%d, threshold=%d\n",
                         proximityDetected ? "ON" : "OFF", read_ps_value, proximityThreshold);
    lastProximityDebugPublish = millis();
  }

  // Publish separate tracking status for binary sensor (proximity only)
  mqttClient.publish("bob/tracking_status", proximityDetected ? "ON" : "OFF");

  // Debug sensor publishing
  static uint32_t lastDebugTime = 0;
  if (now - lastDebugTime > 15000) { // Debug every 15 seconds
    DEBUG_PRINTF("Sensor publish to %s: %s\n", TOPIC_SENSORS, result ? "SUCCESS" : "FAILED");
    DEBUG_VERBOSE_PRINTF("Touch status: %s\n", touchDetected ? "ON" : "OFF");
    DEBUG_VERBOSE_PRINTF("Sensor JSON: %s\n", jsonString.c_str());
    if (!result) {
      Serial.printf("MQTT publish failed - JSON size: %d\n", jsonString.length());
    }
    lastDebugTime = now;
  }
}


void maintainConnections() {
  // Check WiFi first with rate limiting
  static uint32_t lastWifiReconnect = 0;
  static bool wifiReconnecting = false;

  if (wifiEnabled && WiFi.status() != WL_CONNECTED && !wifiReconnecting) {
    if (timeElapsed(lastWifiReconnect, 10000)) { // Wait 10 seconds between WiFi reconnection attempts
      Serial.println("WiFi connection lost, attempting reconnection...");
      wifiReconnecting = true;
      wifiEnabled = initializeWiFi();
      lastWifiReconnect = millis();
      wifiReconnecting = false;

      if (!wifiEnabled) {
        mqttEnabled = false;
        Serial.println("WiFi reconnection failed - disabling MQTT");
        return;
      } else {
        Serial.println("WiFi reconnection successful");
      }
    } else {
      // WiFi reconnect is rate-limited, skip MQTT attempt
      return;
    }
  }

  // Then check MQTT (only if WiFi is stable and connected)
  if (runtimeMqttEnabled && wifiEnabled && WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    // CRITICAL FIX: Exponential backoff instead of fixed 5 second retry
    // Calculate backoff delay: 5s, 10s, 20s, 40s... up to 5 minutes
    uint32_t backoffDelay = MQTT_RECONNECT_INTERVAL;
    if (mqttFailureCount > 0) {
      backoffDelay = min(MQTT_RECONNECT_INTERVAL * (1U << mqttFailureCount), MAX_MQTT_BACKOFF_MS);
    }

    if (timeElapsed(lastMqttReconnect, backoffDelay)) {
      // Check if we've exceeded max retries
      if (mqttFailureCount >= MAX_MQTT_FAILURES) {
        Serial.printf("MQTT disabled after %u consecutive failures\n", mqttFailureCount);
        mqttEnabled = false;
        runtimeMqttEnabled = false;
        return;
      }

      Serial.printf("Reconnecting to MQTT... (attempt %u, reason: %d)\n", mqttFailureCount + 1, mqttClient.state());
      mqttEnabled = initializeMQTT();
      lastMqttReconnect = millis();

      if (mqttEnabled) {
        Serial.println("MQTT reconnected successfully");
        mqttFailureCount = 0;  // Reset counter on success
        // Removed idleDelayWithMqtt(25) - don't block main loop
      } else {
        mqttFailureCount++;
        Serial.printf("MQTT reconnection failed (failure count: %u/%u)\n", mqttFailureCount, MAX_MQTT_FAILURES);

        // Report MQTT connection failures (when WiFi is available)
        static uint32_t lastMqttErrorReport = 0;
        if (millis() - lastMqttErrorReport > 60000) { // Report max once per minute
          // Try a direct publish without going through the publishError function
          // since MQTT might be temporarily down
          Serial.printf("MQTT connection failed (state: %d)\n", mqttClient.state());
          lastMqttErrorReport = millis();
        }
      }
    }
  }
  
  if (mqttEnabled) {
    mqttClient.loop();

    // Publish sensor data periodically (less frequent during sleep)
    uint32_t now = millis();
    uint32_t sensorInterval = isAsleep ? SENSOR_PUBLISH_INTERVAL * 5 : SENSOR_PUBLISH_INTERVAL; // 15s during sleep vs 3s awake
    if (now - lastSensorPublish > sensorInterval) {
      publishSensorData();
      lastSensorPublish = now;
    }

    // Publish status data periodically (less frequent during sleep)
    uint32_t statusInterval = isAsleep ? STATUS_PUBLISH_INTERVAL * 3 : STATUS_PUBLISH_INTERVAL; // 30s during sleep vs 10s awake
    if (now - lastStatusPublish > statusInterval) {
      publishStatus(isAsleep ? "offline" : "online");
      lastStatusPublish = now;
    }
  }
}

// mqttCallbackBasic function removed - using full mqttCallback instead

// ---------------- Home Assistant MQTT Discovery ----------------
void setupHomeAssistantDiscovery() {
  if (!mqttEnabled) return;

  Serial.println(" Setting up Home Assistant MQTT Discovery...");

  // Proximity Binary Sensor
  StaticJsonDocument<512> proximityConfig;
  proximityConfig["name"] = "Bob Proximity Detected";
  proximityConfig["unique_id"] = "bob_proximity_detected";
  proximityConfig["state_topic"] = "bob/proximity_detected";
  proximityConfig["device_class"] = "motion";
  proximityConfig["payload_on"] = "ON";
  proximityConfig["payload_off"] = "OFF";

  // Device info
  JsonObject device = proximityConfig.createNestedObject("device");
  device["name"] = "Bob";
  device["identifiers"][0] = "bob";
  device["manufacturer"] = "Custom";
  device["model"] = "M5CoreS3";

  String proximityConfigStr;
  serializeJson(proximityConfig, proximityConfigStr);
  mqttClient.publish("homeassistant/binary_sensor/bob/proximity_detected/config", proximityConfigStr.c_str(), true);

  // Proximity Value Sensor
  StaticJsonDocument<512> proximityValueConfig;
  proximityValueConfig["name"] = "Bob Proximity Value";
  proximityValueConfig["unique_id"] = "bob_proximity_value";
  proximityValueConfig["state_topic"] = "bob/sensors";
  proximityValueConfig["value_template"] = "{{ value_json.proximity_value }}";
  proximityValueConfig["unit_of_measurement"] = "";
  proximityValueConfig["device"] = device;

  String proximityValueConfigStr;
  serializeJson(proximityValueConfig, proximityValueConfigStr);
  mqttClient.publish("homeassistant/sensor/bob/proximity_value/config", proximityValueConfigStr.c_str(), true);

  // Microphone Binary Sensor
  // Microphone config already published in main auto-discovery
  // (See line ~1903-1906 for microphone discovery)

  Serial.println(" HA Discovery setup completed");
}

// ---------------- Setup/Loop ----------------




