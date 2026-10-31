// Extracted from bob.ino for modular structure

// Sensor pipeline: proximity, microphone, IMU and presence notifications

void processProximityDetection() {
  // Skip if proximity sensor is disabled
  if (!proximityEnabled) {
    proximityDetected = false;
    return;
  }

  uint32_t now = millis();
  bool hasProximity = false;

  // Use exact same configuration as working M5Stack example
  static bool ltrSensorChecked = false;
  static bool ltrSensorAvailable = false;

  if (!ltrSensorChecked) {
    Serial.println("Initializing LTR553 sensor...");

    // MAXIMUM SENSITIVITY configuration - fastest measurement rate!
    Ltr5xx_Init_Basic_Para device_init_base_para = LTR5XX_BASE_PARA_CONFIG_DEFAULT;
    device_init_base_para.ps_led_pulse_freq = LTR5XX_LED_PULSE_FREQ_40KHZ;
    device_init_base_para.ps_measurement_rate = LTR5XX_PS_MEASUREMENT_RATE_10MS;  // Fastest possible!
    device_init_base_para.als_gain = LTR5XX_ALS_GAIN_48X;

    // Use CoreS3 interface for LTR553
    if (CoreS3.Ltr553.begin(&device_init_base_para)) {
      ltrSensorAvailable = true;
      Serial.println("LTR553 Init Success");

      // Active ps using CoreS3 interface
      CoreS3.Ltr553.setPsMode(LTR5XX_PS_ACTIVE_MODE);
      CoreS3.Ltr553.setAlsMode(LTR5XX_ALS_ACTIVE_MODE);
    } else {
      ltrSensorAvailable = false;
      Serial.println("WARNING: LTR553 Init Failed - proximity detection disabled");
      Serial.println("Device will continue without proximity sensor functionality");
    }
    ltrSensorChecked = true;
  }

  if (ltrSensorAvailable) {
    // Read proximity value using CoreS3 interface
    read_ps_value = CoreS3.Ltr553.getPsValue();

    // Enhanced error handling for sensor failures
    static uint32_t consecutiveErrors = 0;
    static uint32_t lastErrorLog = 0;

    // Check for sensor read error (max value 0xFFFF indicates errors)
    if (read_ps_value == 0xFFFF) {
      consecutiveErrors++;

      // Log error every 5 seconds max
      if (millis() - lastErrorLog > 5000) {
        Serial.printf("WARNING: LTR553 sensor read error: %d (consecutive errors: %d)\n", read_ps_value, consecutiveErrors);
        lastErrorLog = millis();
      }

      // If too many consecutive errors, attempt sensor recovery
      if (consecutiveErrors >= 10) {
        Serial.println("ERROR: Too many consecutive LTR553 sensor failures - attempting recovery");

        // Attempt to reinitialize the sensor with MAXIMUM sensitivity
        Ltr5xx_Init_Basic_Para device_init_base_para = LTR5XX_BASE_PARA_CONFIG_DEFAULT;
        device_init_base_para.ps_led_pulse_freq = LTR5XX_LED_PULSE_FREQ_40KHZ;
        device_init_base_para.ps_measurement_rate = LTR5XX_PS_MEASUREMENT_RATE_10MS;  // Fastest possible!
        device_init_base_para.als_gain = LTR5XX_ALS_GAIN_48X;

        if (CoreS3.Ltr553.begin(&device_init_base_para)) {
          Serial.println(" LTR553 sensor recovery successful");
          CoreS3.Ltr553.setPsMode(LTR5XX_PS_ACTIVE_MODE);
          CoreS3.Ltr553.setAlsMode(LTR5XX_ALS_ACTIVE_MODE);
          consecutiveErrors = 0; // Reset error counter
        } else {
          Serial.println(" LTR553 sensor recovery failed - disabling proximity detection");
          ltrSensorAvailable = false;

          // Report critical sensor failure to Home Assistant
          if (mqttEnabled) {
            Serial.println("ERROR: LTR553 sensor recovery failed - proximity detection disabled permanently");
          }
        }
      }

      read_ps_value = 0; // Reset to safe value
    } else {
      consecutiveErrors = 0; // Reset error counter on successful read

      // Raw proximity detection with inverted threshold (higher threshold = more sensitive)
      proximityRawDetected = (read_ps_value > (255 - proximityThreshold));

      // DEBOUNCED proximity detection - avoid false triggers
      if (proximityRawDetected) {
        // Only trigger if enough time has passed since last trigger
        if (now - lastProximityTriggerTime > PROXIMITY_DEBOUNCE_MS) {
          if (proximityDetectionTime == 0) {
            proximityDetectionTime = now;
            lastProximityTriggerTime = now;  // Update debounce timer
            DEBUG_VERBOSE_PRINTF("Proximity detected: %d (threshold: %d)\n", read_ps_value, proximityThreshold);

            // DIRECT WAKE-UP bij eerste detectie - geen wachten!
            if (isAsleep || isGoingToSleep) {
              DEBUG_PRINTLN("Instant wake-up on first proximity detection");
              wakeBobFromSleep("proximity", true);
            }
            // NOTE: Do NOT reset lastInteractionTime if already awake - proximity shouldn't prevent sleep!
          }
        }

        // INSTANT tracking - no debounce delay!
        hasProximity = true;
      } else {
        // Reset detection timer if no raw detection
        if (proximityDetectionTime > 0) {
          DEBUG_VERBOSE_PRINTF("Proximity ended after %dms\n", now - proximityDetectionTime);
          proximityDetectionTime = 0;
        }
      }

      // Debug: show proximity value every 2 seconds (10s during sleep for battery)
      static uint32_t lastProximityDebug = 0;
      uint32_t debugInterval = isAsleep ? 10000 : 2000; // Less frequent during sleep
      if (millis() - lastProximityDebug > debugInterval) {
        if (!isAsleep || read_ps_value > (255 - proximityThreshold)) { // Only log during sleep if above threshold
          DEBUG_VERBOSE_PRINTF("LTR553 proximity: %d, sensitivity: %d, threshold: %d, detected: %s\n",
                               read_ps_value, proximityThreshold, (255 - proximityThreshold), proximityRawDetected ? "YES" : "NO");
        }
        lastProximityDebug = millis();
      }
    }
  }

  // Track proximity state changes properly
  bool previousProximityState = proximityDetected;

  if (hasProximity) {
    proximityDetected = true;
    lastProximityTime = now;
    // NOTE: Do NOT reset sleep timer on proximity - prevents Bob from sleeping!
    // Only wake Bob if he's asleep, but don't keep him awake
    DEBUG_VERBOSE_PRINTF("Proximity tracking active (%d)\n", read_ps_value);

    // Don't trigger wake-up during sleep sequences - let them complete
    if (isGoingToSleep) {
      DEBUG_VERBOSE_PRINTF("Proximity detected (%d) but ignoring during sleep sequence\n", read_ps_value);
      return; // Exit early to not trigger wake-up
    }

    // Check manual sleep cooldown
    bool inCooldown = (manualSleepTime > 0 && now - manualSleepTime < MANUAL_SLEEP_COOLDOWN);

    if (!inCooldown && (now - lastProximityTrigger > PROXIMITY_TRIGGER_COOLDOWN)) {
      // Proximity actions: Trigger Follow behavior for tracking
      DEBUG_PRINTLN("Proximity trigger -> Follow behavior");
      triggerBehavior(Behavior::Follow);
      lastProximityTrigger = now;

      // Publish direct naar MQTT voor snelle response
      if (mqttEnabled && mqttClient.connected()) {
        mqttClient.publish("bob/proximity_detected", "ON");
        mqttClient.publish("bob/proximity_value", String(read_ps_value).c_str());
      }

      // Als Bob slaapt, maak hem wakker
      if (isAsleep || isGoingToSleep) {
        DEBUG_PRINTLN("Bob waking up - proximity detected");
        wakeBobFromSleep("proximity", true);
      }
    } else {
      DEBUG_VERBOSE_PRINTF("Proximity in cooldown (%d ms remaining)\n",
                           MANUAL_SLEEP_COOLDOWN - (now - manualSleepTime));
    }
  } else {
    // No proximity detected
    proximityDetected = false;
  }

  // Publish state change only when it actually changed
  if (proximityDetected != previousProximityState) {
    if (mqttEnabled && mqttClient.connected()) {
      const char* state = proximityDetected ? "ON" : "OFF";
      mqttClient.publish("bob/proximity_detected", state);
      DEBUG_PRINTF("Proximity state changed: %s\n", state);
    }

    if (proximityDetected && !isAsleep && !isGoingToSleep) {
      int proximityPercent = map(read_ps_value, 0, 255, 0, 100);
      PersonalityExtended reaction = smartReactions.reactToProximity(proximityPercent, true);
      if (reaction != PersonalityExtended::Neutral) {
        applyPersonality(reaction, 3000);
      }
      BobExpress::onProximity();
    }
  }

  // Check if we should enter deep sleep mode due to no proximity for extended time
  bool shouldStayAwake = alwaysAwake || (autoAwakeOnPower && M5.Power.isCharging());
  if (!shouldStayAwake && now - lastProximityTime > PROXIMITY_TIMEOUT) {
    if (!isGoingToSleep && !isAsleep) {
      // Don't trigger sleep during WakeUpSequence
      bool isWakeUpActive = (runner.idx == -1 && currentBehaviorName == "Waking Up") ||
                           (currentBehaviorName == "Waking Up");

      if (!isWakeUpActive) {
        DEBUG_VERBOSE_PRINTLN("No proximity for 30s - staying awake (sleep by inactivity/manual only)");
        // Bob blijft gewoon wakker - geen automatische sleep bij proximity timeout
        // Reset proximity timer om deze check te voorkomen
        lastProximityTime = now;
      } else {
        DEBUG_VERBOSE_PRINTLN("WakeUpSequence active - delaying proximity check");
        lastProximityTime = now - (PROXIMITY_TIMEOUT - 5000); // Give 5 more seconds
      }
    }
  }

  // Auto brightness adjustment based on ambient light
  if (autoBrightnessEnabled && ltrSensorAvailable && screenOn) {
    static uint32_t lastBrightnessUpdate = 0;
    if (now - lastBrightnessUpdate > 2000) { // Update every 2 seconds
      // Read ambient light sensor (ALS)
      ambientLightLevel = CoreS3.Ltr553.getAlsValue();

      // Map ambient light (0-65535) to brightness (50-255) - ZACHTER!
      // Meer licht = feller scherm, minder licht = dimmer scherm
      // Dark: 0-50 â†’ 65 brightness (zacht minimum)
      // Low: 50-200 â†’ 85-130 brightness
      // Medium: 200-1000 â†’ 130-170 brightness
      // Bright: 1000-5000 â†’ 170-210 brightness
      // Very bright: >5000 â†’ 210-255 brightness
      int targetBrightness;
      if (ambientLightLevel < 50) {
        targetBrightness = 65;  // Heel donker - zacht minimum
      } else if (ambientLightLevel < 200) {
        targetBrightness = map(ambientLightLevel, 50, 200, 85, 130);  // Donker - matig
      } else if (ambientLightLevel < 1000) {
        targetBrightness = map(ambientLightLevel, 200, 1000, 130, 170);  // Medium - normaal
      } else if (ambientLightLevel < 5000) {
        targetBrightness = map(ambientLightLevel, 1000, 5000, 170, 210);  // Licht - feller
      } else {
        targetBrightness = 255;  // Heel licht - maximaal
      }

      // Smooth brightness changes (avoid sudden jumps)
      if (abs(targetBrightness - screenBrightness) > 5) {
        screenBrightness = targetBrightness;
        M5.Display.setBrightness(screenBrightness);

        static uint32_t lastBrightnessLog = 0;
        if (now - lastBrightnessLog > 10000) {
          DEBUG_VERBOSE_PRINTF("Auto brightness: light=%d, brightness=%d\n", ambientLightLevel, screenBrightness);
          lastBrightnessLog = now;
        }
      }

      lastBrightnessUpdate = now;
    }
  }
}

void cleanupMicrophone() {
  if (micBuffer != nullptr) {
    free(micBuffer);
    micBuffer = nullptr;
    Serial.println(" Microphone buffer freed");
  }
}

void processMicrophoneDetection() {
  if (!microphoneEnabled) {
    soundDetected = false;
    return;
  }

  uint32_t now = millis();
  if (now - lastMicrophoneCheck < MIC_UPDATE_INTERVAL) {
    return; // Check microphone less frequently
  }
  lastMicrophoneCheck = now;

  // Read microphone data
  if (M5.Mic.isEnabled()) {
    if (!micBuffer) {
      static uint32_t lastMicBufferWarn = 0;
      if (now - lastMicBufferWarn > 5000) {
        DEBUG_PRINTLN("WARNING: Microphone buffer not allocated");
        lastMicBufferWarn = now;
      }
      return;
    }

    bool success = M5.Mic.record(micBuffer, MIC_BUFFER_SIZE);

    if (success) {
      // Calculate RMS (Root Mean Square) for volume level
      float sum = 0.0f;
      size_t samples = MIC_BUFFER_SIZE;

      for (size_t i = 0; i < samples; i++) {
        float sample = (float)micBuffer[i];
        sum += sample * sample;
      }

      lastSoundLevel = sqrtf(sum / samples);

      // Apply simple smoothing
      static float smoothedLevel = 0.0f;
      smoothedLevel = smoothedLevel * 0.8f + lastSoundLevel * 0.2f;

      // Detect sound above threshold
      bool currentSoundState = smoothedLevel > soundThreshold;

      // Publish to MQTT if sound state changed
      if (currentSoundState != soundDetected) {
        soundDetected = currentSoundState;

        if (soundDetected) {
          // Trigger visual reaction: blink + VEEL grotere ogen + kijk naar geluid
          soundReactionActive = true;
          soundReactionStartTime = millis();
          startBlink();  // Trigger blink immediately
          if (!manualOverride && !isAsleep && !isGoingToSleep) {
            applyPersonality(PersonalityExtended::Surprised, 2500);
            BobExpress::onSoundDetected();
          }

          // Ogen kijken in random richting (alsof ze naar geluidsbron zoeken)
          if (!manualOverride) {
            targetLookX = random(-MAX_LOOK_X * 70, MAX_LOOK_X * 70) / 100.0f;
            targetLookY = random(-MAX_LOOK_Y * 70, MAX_LOOK_Y * 70) / 100.0f;
          }

          DEBUG_PRINTF("Sound detected (level: %.1f, threshold: %.1f)\n",
                       smoothedLevel, soundThreshold);
        } else {
          DEBUG_VERBOSE_PRINTF("Sound cleared (level: %.1f, threshold: %.1f)\n",
                               smoothedLevel, soundThreshold);
        }

        if (mqttEnabled) {
          mqttClient.publish("bob/microphone", soundDetected ? "ON" : "OFF", true);
        }
      }

      // Debug output every 10 seconds
      static uint32_t lastMicDebug = 0;
      if (now - lastMicDebug > 10000) {
        DEBUG_VERBOSE_PRINTF("Microphone: level=%.1f, smoothed=%.1f, detected=%s\n",
                             lastSoundLevel, smoothedLevel, soundDetected ? "YES" : "NO");
        lastMicDebug = now;
      }
    }
  } else {
    static uint32_t lastMicDisabledWarn = 0;
    if (now - lastMicDisabledWarn > 10000) {
      DEBUG_VERBOSE_PRINTLN("Microphone not enabled");
      lastMicDisabledWarn = now;
    }
    soundDetected = false;
  }
}

void imuTriggers(){
  if (isGoingToSleep) return; // Geen triggers tijdens slaapsequentie
  if (!M5.Imu.isEnabled()) return;
  if (!M5.Imu.update()) return;

  // IMU data is valid since update() succeeded above
  auto d = M5.Imu.getImuData();

  // Calculate total acceleration magnitude
  float amag = sqrtf(d.accel.x*d.accel.x + d.accel.y*d.accel.y + d.accel.z*d.accel.z);

  // Calculate total gyroscope magnitude
  float gmag = sqrtf(d.gyro.x*d.gyro.x + d.gyro.y*d.gyro.y + d.gyro.z*d.gyro.z);

  // Shake detection - high gyroscope activity indicates shaking
  uint32_t now = millis();
  static bool wasShaking = false;
  const float SHAKE_THRESHOLD = 320.0f; // Less sensitive: ignore normal lifting/handling
  const uint32_t SHAKE_CONFIRM_MS = 140; // Must remain above threshold briefly
  static uint32_t shakeAboveThresholdSince = 0;

  if (gmag > SHAKE_THRESHOLD) {
    if (shakeAboveThresholdSince == 0) {
      shakeAboveThresholdSince = now;
    }
    bool shakeConfirmed = (now - shakeAboveThresholdSince) >= SHAKE_CONFIRM_MS;
    if (!shakeDetected && shakeConfirmed) {
      shakeDetected = true;
      DEBUG_PRINTLN("Shake detected");

      // SHAKE EFFECT: Alleen oog-animatie effect, confetti ONLY via Home Assistant
      DEBUG_VERBOSE_PRINTLN("Shake effect active");
      // Confetti is nu ALLEEN via MQTT commands: bob/cmd/confetti_snow en bob/cmd/confetti_cannons

      // Publish direct naar MQTT voor snelle response
      if (mqttEnabled && mqttClient.connected()) {
        mqttClient.publish("bob/shake", "ON");
      }

      if (isAsleep || isGoingToSleep) {
        wakeBobFromSleep("shake", true);
        DEBUG_PRINTLN("Shake detected - waking up Bob");
      } else {
        // Intentionally keep eye shape unchanged on shake.
        // Shake only affects gaze/skew movement in this branch.
      }
    }
    lastShakeTime = now; // Keep updating while shaking
    wasShaking = true;
  } else if (wasShaking && (now - lastShakeTime > 500)) {
    shakeAboveThresholdSince = 0;
    // Shake stopped - still showing shake effects for a bit longer
    shakeDetected = false;
    wasShaking = false;
    shakeStoppedTime = now;
    showingIrritation = true;
    DEBUG_VERBOSE_PRINTLN("Shake stopped");

    // Publish direct naar MQTT
    if (mqttEnabled && mqttClient.connected()) {
      mqttClient.publish("bob/shake", "OFF");
    }
  } else if (!wasShaking) {
    shakeAboveThresholdSince = 0;
  }

  // CHAOTIC STUTTER shake effect - ogen stotteren wild
  static uint32_t lastShakeEyeUpdate = 0;
  static float shakeTargetX = 0, shakeTargetY = 0;
  static uint8_t shakeStutterCount = 0; // Teller voor stutter effect

  // VEEL SNELLERE updates voor stutter effect
  uint32_t shakeUpdateInterval = (shakeDetected) ? 15 : 40; // Was 30ms, nu 15ms tijdens schudden!

  if ((shakeDetected || showingIrritation) && (now - lastShakeEyeUpdate > shakeUpdateInterval)) {
    // Heftige intensity - volledige kracht!
    float shakeIntensity = (shakeDetected) ? 1.5f : 0.8f;

    // STUTTER EFFECT: soms "hangen" op 1 positie, dan SPRINGEN naar nieuwe
    shakeStutterCount++;
    if (shakeStutterCount > random(1, 4)) { // Elke 1-4 frames: nieuwe positie (stutter!)
      shakeStutterCount = 0;

      // Nieuwe random targets - EXTREME bereik voor chaotisch effect
      shakeTargetX = random(-MAX_LOOK_X * 140, MAX_LOOK_X * 140) / 100.0f * shakeIntensity; // Was 120, nu 140!
      shakeTargetY = random(-MAX_LOOK_Y * 140, MAX_LOOK_Y * 140) / 100.0f * shakeIntensity;

      // Clamp targets to screen bounds
      shakeTargetX = clampf(shakeTargetX, -MAX_LOOK_X * 1.2f, MAX_LOOK_X * 1.2f); // Was 1.1, nu 1.2
      shakeTargetY = clampf(shakeTargetY, -MAX_LOOK_Y * 1.2f, MAX_LOOK_Y * 1.2f);
    }
    // Anders: blijf op zelfde target (= stutter/hang effect)

    lastShakeEyeUpdate = now;
  }

  if (shakeDetected || showingIrritation) {
    // ZEER SNELLE, STOTTERENDE bewegingen met directe sprongen
    float lerpSpeed = (shakeDetected) ? 0.65f : 0.3f; // Was 0.4f, nu 0.65f = bijna direct springen!
    eyeLookX = mylerp(eyeLookX, shakeTargetX, lerpSpeed);
    eyeLookY = mylerp(eyeLookY, shakeTargetY, lerpSpeed);

    // EXTREME visuele effecten met stutter
    float skewSpeed = 0.6f; // Was 0.35f, nu 0.6f voor snappier effect
    skewXL = mylerp(skewXL, random(-20, 20), skewSpeed); // Was -15/15, nu -20/20
    skewXR = mylerp(skewXR, random(-20, 20), skewSpeed);
    tiltOffsetL = mylerp(tiltOffsetL, random(-15, 15), skewSpeed); // Was -10/10, nu -15/15
    tiltOffsetR = mylerp(tiltOffsetR, random(-15, 15), skewSpeed);
  } else if (!shakeDetected && !showingIrritation) {
    // Reset skew en tilt als shake stopt
    skewXL = mylerp(skewXL, 0, 0.2f);
    skewXR = mylerp(skewXR, 0, 0.2f);
    tiltOffsetL = mylerp(tiltOffsetL, 0, 0.2f);
    tiltOffsetR = mylerp(tiltOffsetR, 0, 0.2f);
  }

  // End shake irritation window after 5 seconds.
  if (showingIrritation && (now - shakeStoppedTime > 5000)) {
    showingIrritation = false;
    DEBUG_VERBOSE_PRINTLN("Shake irritation ended");
  }

  // Debug output every 5 seconds to monitor sensor values
  static uint32_t lastDebugTime = 0;
  if (now - lastDebugTime > 5000) {
    DEBUG_VERBOSE_PRINTF("IMU: amag=%.2f, gmag=%.2f, shake=%s, accel.x=%.2f\n",
                         amag, gmag, shakeDetected ? "YES" : "NO", d.accel.x);
    lastDebugTime = now;
  }

}

// ---------------- Camera Functions ----------------

void processPresenceNotifications() {
  uint32_t now = millis();
  bool currentPresence = proximityDetected;

  // Check for presence state change
  if (currentPresence != lastPresenceState) {
    presenceChangeTime = now;
    lastPresenceState = currentPresence;
  }

  // Show notification after delay to avoid spam
  if (now - presenceChangeTime > PRESENCE_NOTIFICATION_DELAY) {
    if (currentPresence && !lastPresenceState) {
      // Someone detected
      showNotification("PERSOON", TFT_GREEN, 3000);
      lastPresenceState = true;
    } else if (!currentPresence && lastPresenceState) {
      // No one detected
      showNotification("NIEMAND", TFT_ORANGE, 2000);
      lastPresenceState = false;
    }
  }
}

