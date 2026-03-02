// Extracted from bob.ino for modular structure

// Behavior engine: emotion tweening, state transitions and behavior routing

// ---------------- Emotion tween ----------------
struct EmotionTween { bool active=false; EyeShape from, to; uint32_t startMs=0, durMs=220; } emo;

// Behavior status tracking
String currentBehaviorName = "Wake";
uint32_t behaviorStartTime = 0;
Behavior currentManualBehavior = Behavior::Wake;

void startEmotionTween(const EyeShape& to, uint32_t dur){
  emo.active=true; emo.from=currentShape; emo.to=to; emo.startMs=millis(); emo.durMs=dur;
}
void setEmotionInstant(const EyeShape& s){ emo.active=false; currentShape=s; }
void tickEmotionTween(){
  if (!emo.active) return;
  uint32_t now = millis();
  float t = (emo.durMs==0) ? 1.0f : clampf((now-emo.startMs)/(float)emo.durMs, 0.0f, 1.0f);
  float u = t*t*(3 - 2*t);
  EyeShape s {
    (int)(emo.from.width  + (emo.to.width  - emo.from.width ) * u),
    (int)(emo.from.height + (emo.to.height - emo.from.height) * u),
    (int)(emo.from.radius + (emo.to.radius - emo.from.radius) * u),
  };
  currentShape = s;
  if (t >= 1.0f) { emo.active=false; currentShape=emo.to; }
}

// ---------------- Essential Behaviors (BASIC MODE - simplified) ----------------
const Behavior program[] = {
  Behavior::Wake,
  Behavior::Sleep
};
const int PROGRAM_LEN = sizeof(program)/sizeof(program[0]);

Runner runner;

void runnerNext(){
  if (isGoingToSleep) return; // Niet wisselen als we willen slapen
  if (isAsleep) return; // NIET wisselen als we slapen - batterij besparing!

  // Don't cycle if manual override is still active (check is now centralized in loop())
  if (manualOverride) {
    return;
  }

  // Only cycle if we're in the program array (not manual mode)
  if (runner.idx >= 0) {
    runner.idx = (runner.idx + 1) % PROGRAM_LEN;
    runner.startAt = millis();
    runner.lastStep = runner.startAt;
    runner.sub = 0; runner.a=0; runner.b=0;
    renderMode = RenderMode::Normal;
    skewXL = skewXR = 0.0f;
    
    // Update behavior tracking
    currentBehaviorName = behaviorName(program[runner.idx]);
    behaviorStartTime = runner.startAt;
  } else {
    // In manual mode - only return to program if manual override has expired
    if (!manualOverride || millis() > manualOverrideTimeout) {
      runner.idx = 0;
      runner.startAt = millis();
      runner.lastStep = runner.startAt;
      runner.sub = 0; runner.a=0; runner.b=0;
      renderMode = RenderMode::Normal;
      skewXL = skewXR = 0.0f;
      
      currentBehaviorName = behaviorName(program[runner.idx]);
      behaviorStartTime = runner.startAt;
      
      DEBUG_PRINTLN("Manual override expired - returning to program cycle");
    } else {
      DEBUG_VERBOSE_PRINTLN("runnerNext ignored in manual mode");
    }
  }
}

void startBlink(){
  // Don't blink during proximity detection - keep eyes open and alert
  if (proximityDetected) {
    DEBUG_VERBOSE_PRINTLN("Blink suppressed due to proximity");
    return;
  }
  blinking=true;
  blinkStart=millis();
}


// ---------------- Gedrag tick ----------------
void behaviorsTick(bool anyTouch){
  // Tijdens touch alleen eye tracking, geen nieuwe behaviors starten
  if (anyTouch && !isGoingToSleep) return;

  const uint32_t now = millis();

  // Check for manual WakeUpSequence first (runner.idx = -1 and currentBehaviorName = "Waking Up")
  if (runner.idx == -1 && currentBehaviorName == "Waking Up") {
    // Simple WakeUpSequence: slow eye open + 2 blinks
    uint32_t elapsed = now - runner.startAt;

    if (runner.sub == 0 && elapsed > 200) {
      // Phase 1: Slowly open eyes (1.5 seconds)
      startEmotionTween(baseShape, 1500);
      setTarget(0, 0);
      depthScale = 1.0f;
      runner.sub = 1;
      runner.lastStep = now;
      Serial.println("WakeUp: Eyes slowly opening...");
    } else if (runner.sub == 1 && elapsed > 1700) {
      // Phase 2: First blink
      startBlink();
      runner.sub = 2;
      runner.lastStep = now;
      Serial.println("WakeUp: First blink...");
    } else if (runner.sub == 2 && now - runner.lastStep > 400) {
      // Phase 3: Second blink
      startBlink();
      runner.sub = 3;
      runner.lastStep = now;
      Serial.println("WakeUp: Second blink...");
    } else if (runner.sub == 3 && now - runner.lastStep > 400) {
      // Wake up complete - go to Wake behavior to respect wake time
      for (int i = 0; i < PROGRAM_LEN; i++) {
        if (program[i] == Behavior::Wake) {
          runner.idx = i;
          runner.startAt = millis();
          runner.sub = 3; // Skip to awake phase
          runner.lastStep = millis();
          currentBehaviorName = behaviorName(program[i]);
          behaviorStartTime = runner.startAt;
          Serial.printf("WakeUp complete - staying awake for %d seconds...\n", INACTIVITY_TIMEOUT_MS/1000);
          break;
        }
      }
    }
    return;
  }

  // Check for manual Sleep animation (runner.idx = -1 and currentBehaviorName = "Going to Sleep")
  if (runner.idx == -1 && currentBehaviorName == "Going to Sleep") {
    // Simple Sleep animation: 2 blinks + slow eye close
    uint32_t elapsed = now - runner.startAt;

    if (runner.sub == 0 && elapsed > 200) {
      // Phase 1: First blink
      startBlink();
      runner.sub = 1;
      runner.lastStep = now;
      Serial.println("Sleep: First blink...");
    } else if (runner.sub == 1 && now - runner.lastStep > 400) {
      // Phase 2: Second blink
      startBlink();
      runner.sub = 2;
      runner.lastStep = now;
      Serial.println("Sleep: Second blink...");
    } else if (runner.sub == 2 && now - runner.lastStep > 400) {
      // Phase 3: Slowly close eyes (1.5 seconds)
      startEmotionTween(sleepShape, 1500);
      setTarget(0, 2);
      depthScale = 0.9f;
      runner.sub = 3;
      runner.lastStep = now;
      Serial.println("Sleep: Eyes slowly closing...");
    } else if (runner.sub == 3 && now - runner.lastStep > 1600) {
      // Sleep complete - enter display sleep
      Serial.println("Sleep complete - entering display sleep");

      currentBehaviorName = "DisplaySleep";
      if (mqttEnabled) {
        publishStatus("sleeping");
      }

      // Enter display sleep
      M5.Display.sleep();
      isAsleep = true;
      currentBehaviorName = "DisplaySleep";
    }
    return;
  }

  // Determine current behavior - either from program array or manual trigger
  Behavior beh;
  static Behavior lastBehavior = Behavior::Wake;

  if (runner.idx >= 0 && runner.idx < PROGRAM_LEN) {
    beh = program[runner.idx];
  } else {
    // Manual behavior (runner.idx = -1)
    beh = currentManualBehavior;

    // Debug log for manual behaviors
    if (beh != lastBehavior) {
      DEBUG_VERBOSE_PRINTF("behaviorsTick manual behavior: %s\n", behaviorName(beh));
      lastBehavior = beh;
    }
  }

  switch (beh){
    case Behavior::Wake:{
      if (runner.sub==0){
        setEmotionInstant(sleepShape);
        depthScale=0.95f;
        asym3D=0;
        runner.sub=1;
        runner.lastStep=now;
        Serial.println("Bob is WAKING UP...");
        currentBehaviorName = "Wake";
        if (mqttEnabled) {
          publishStatus("online");
        }
      }
      else if (runner.sub==1 && now - runner.lastStep > 300){
        startEmotionTween(wakeShape);
        setTarget(0, -MAX_LOOK_Y*0.25f);
        runner.sub=2;
        runner.lastStep=now;
        Serial.println("Bob is opening eyes...");
      } else if (runner.sub==2 && now - runner.lastStep > 600){
        startEmotionTween(baseShape);
        setTarget(0,0); // Static center look
        depthScale=1.0f; // Static scale
        runner.sub=3;
        runner.lastStep=now;
        Serial.printf("Bob is awake! Staying awake for %d seconds...\n", INACTIVITY_TIMEOUT_MS/1000);
      } else if (runner.sub==3 && now - lastInteractionTime > INACTIVITY_TIMEOUT_MS){
        // Check if we should stay awake (manual setting or charging)
        bool shouldStayAwake = alwaysAwake || (autoAwakeOnPower && M5.Power.isCharging());
        if (!shouldStayAwake) {
          // Show wake state for configured wake time before going to Sleep (only if not always awake)
          Serial.printf("Wake time finished (%d sec), going to sleep...\n", INACTIVITY_TIMEOUT_MS/1000);
          triggerBehavior(Behavior::Sleep);
        } else if (autoAwakeOnPower && M5.Power.isCharging()) {
          Serial.println("Staying awake - connected to power");
        }
      }
    } break;

    case Behavior::Sleep:{
      if (runner.sub==0){
        // PHASE 1: Show sleep animation (eyes closing) - SCREEN STILL ON
        startEmotionTween(sleepShape);  // Tween naar slaap emoji
        runner.sub=1;
        runner.lastStep=now;
        Serial.println("Bob is CLOSING EYES... (going to sleep)");
        currentBehaviorName = "Sleep";
        if (mqttEnabled) {
          publishStatus("online");
        }
      }
      // PHASE 2: Keep sleeping with screen on for a minute, then snore
      else if (runner.sub==1 && now - runner.lastStep > SLEEP_SCREEN_ON_MS){
        // After a minute of sleep, turn off screen
        Serial.println("Bob sleeping... turning screen OFF");
        M5.Display.sleep();
        M5.Display.setBrightness(0);
        screenOn = false;
        isGoingToSleep = true;
        runner.sub=2;
        runner.lastStep=now;

        currentBehaviorName = "SleepDimmed";
        if (mqttEnabled) {
          publishStatus("dimmed");
        }
      }

      // PHASE 3: Breathing effect during sleep (always active)
      float t = (now - runner.startAt) / 1000.0f;
      depthScale = 0.96f + 0.02f * sinf(t * 0.8f); // Slow breathing rhythm
      asym3D = mylerp(asym3D, 0.0f, 0.08f);

      // Gentle breathing eye movement
      float breathingX = 2.0f * sinf(t * 0.6f);
      float breathingY = 1.0f * sinf(t * 0.5f + 0.5f);
      setTarget(breathingX, breathingY);
      if (runner.sub==2 && now - runner.lastStep > SLEEP_SCREEN_OFF_TO_DEEP_SLEEP_MS){
        // Keep Bob in display-sleep (no deep sleep) so touch wake stays responsive.
        if (!sleepPhase2Printed) {
          Serial.println("Sleep phase 2: Display off sleep active (deep sleep disabled for touch wake)");
          sleepPhase2Printed = true;
        }

        isAsleep = true;
        isGoingToSleep = false;
        runner.sub = 3;  // Latch this state; do not re-run this block every loop.

        currentBehaviorName = "SleepDisplayOff";
        if (mqttEnabled) {
          publishStatus("sleeping");
        }
      }
    } break;

    case Behavior::WakeUpSequence:{
      const uint32_t TOTAL_DURATION = 2000; // 2 seconds total (0.5 sec screen + 1.5 sec eyes) - SNELLER!
      uint32_t elapsed = now - runner.startAt;
      float progress = clampf(elapsed / (float)TOTAL_DURATION, 0.0f, 1.0f);

      if (runner.sub == 0) {
        // Phase 1: Screen on, eyes closed (0-0.5 sec) - VEEL KORTER
        setEmotionInstant(sleepShape); // Ogen dicht
        depthScale = 0.85f;
        setTarget(0, 0);

        // Ensure screen is awake and brightness is applied
        if (!screenOn) {
          screenOn = true;
          M5.Display.wakeup();
          Serial.println("WakeUpSequence: Screen awakened");
        }
        M5.Display.setBrightness(screenBrightness);
        Serial.printf("WakeUpSequence: Brightness restored to %d\n", screenBrightness);

        runner.sub = 1;
        runner.lastStep = now;
      } else if (runner.sub == 1) {
        // Phase 1: Kort wachten (0-0.5 sec)
        if (elapsed >= 500) {
          runner.sub = 2;
          runner.lastStep = now;
        }
      } else if (runner.sub == 2) {
        // Phase 2: Snel ogen openen (0.5-2 sec)
        float phase2Progress = clampf((elapsed - 500) / 1500.0f, 0.0f, 1.0f);
        float smoothProgress = phase2Progress * phase2Progress * (3.0f - 2.0f * phase2Progress); // smooth curve

        EyeShape currentShape = {
          (int)(sleepShape.width + (baseShape.width - sleepShape.width) * smoothProgress),
          (int)(sleepShape.height + (baseShape.height - sleepShape.height) * smoothProgress),
          (int)(sleepShape.radius + (baseShape.radius - sleepShape.radius) * smoothProgress)
        };
        setEmotionInstant(currentShape);

        depthScale = mylerp(depthScale, 1.0f, 0.02f);
        setTarget(0, 0); // Recht vooruit kijken

        if (elapsed >= 2000) {
          runner.sub = 3;
          runner.lastStep = now;
        }
      } else if (runner.sub == 3) {
        // Phase 3: Ogen volledig open, ga naar neutraal (Wake behavior)
        setEmotionInstant(baseShape); // Volledig wakker
        depthScale = 1.0f;
        setTarget(0, 0); // Neutraal kijken

        // Sequence complete - reset manual sleep cooldown and go to Wake behavior
        manualSleepTime = 0; // Reset manual sleep cooldown after normal wake-up
        Serial.println("WakeUpSequence complete - manual sleep cooldown reset");

        for (int i = 0; i < PROGRAM_LEN; i++) {
          if (program[i] == Behavior::Wake) {
            runner.idx = i;
            runner.startAt = millis();
            runner.sub = 3; // Skip initial wake-up phase in Wake behavior
            currentBehaviorName = behaviorName(program[i]);
            behaviorStartTime = runner.startAt;
            break;
          }
        }
      }
    } break;

    case Behavior::Follow:{
      // Follow behavior for proximity tracking - no blinking, eyes stay open
      if (runner.sub == 0) {
        Serial.println("Follow: Started proximity tracking behavior");
        setEmotionInstant(baseShape); // Keep eyes wide open
        runner.sub = 1;
        runner.lastStep = now;
      }

      // Keep eyes open and alert during tracking
      // Eye movement is handled directly in main loop when proximityDetected = true

      // Stay in Follow behavior longer - only exit after 3 seconds of no proximity
      static uint32_t lastProximityInFollow = 0;
      if (proximityDetected) {
        lastProximityInFollow = now;
      } else if (now - lastProximityInFollow > 3000) { // 3 seconds delay
        Serial.println("Follow: No proximity for 3s - ending Follow behavior");
        runnerNext(); // Return to normal behavior sequence
      }
    } break;

    case Behavior::Curious: {
      static const float lookPattern[][2] = {{-1,0},{0,-0.6f},{1,0},{0.5f,0.5f},{0,0}};
      static const int PATTERN_LEN = 5;
      if (runner.sub == 0) {
        applyPersonality(PersonalityExtended::Thinking, 99999);
        runner.sub = 1;
        runner.lastStep = now;
      }
      uint32_t stepMs = 1200;
      int step = ((now - runner.startAt) / stepMs) % PATTERN_LEN;
      setTarget(lookPattern[step][0] * MAX_LOOK_X, lookPattern[step][1] * MAX_LOOK_Y);
      skewXL = mylerp(skewXL, lookPattern[step][0] * 6.0f, 0.05f);
      skewXR = mylerp(skewXR, lookPattern[step][0] * 6.0f, 0.05f);
      if (now - runner.startAt > 6000 && !manualOverride) {
        skewXL = skewXR = 0;
        runnerNext();
      }
    } break;

    case Behavior::NodYes: {
      float t = (now - runner.startAt) / 1000.0f;
      setTarget(0, sinf(t * M_PI * 3.0f) * MAX_LOOK_Y * 0.7f);
      if (now - runner.startAt > 2000 && !manualOverride) {
        setTarget(0, 0);
        runnerNext();
      }
    } break;

    case Behavior::ShakeNo: {
      float t = (now - runner.startAt) / 1000.0f;
      setTarget(sinf(t * M_PI * 3.0f) * MAX_LOOK_X * 0.7f, 0);
      if (now - runner.startAt > 2000 && !manualOverride) {
        setTarget(0, 0);
        runnerNext();
      }
    } break;

    case Behavior::StartupCelebration: {
      if (runner.sub == 0) {
        applyPersonality(PersonalityExtended::Excited, 5000);
        runner.sub = 1;
        runner.lastStep = now;
      }
      float t = (now - runner.startAt) / 1000.0f;
      setTarget(sinf(t * 4.0f) * MAX_LOOK_X * 0.7f, sinf(t * 2.5f) * MAX_LOOK_Y * 0.4f);
      depthScale = 1.0f + 0.1f * sinf(t * 8.0f);
      if (now - runner.startAt > 5000 && !manualOverride) {
        depthScale = 1.0f;
        runnerNext();
      }
    } break;

    default: {
      // Fallback for unknown behaviors - just show base shape
      if (runner.sub == 0) {
        Serial.printf("Unknown/unsupported behavior: %s - using base shape\n", behaviorName(beh));
        startEmotionTween(baseShape);
        setTarget(0, 0);
        runner.sub = 1;
      }
      // Auto-return after 2 seconds
      if (now - runner.startAt > 2000 && (!manualOverride || millis() > manualOverrideTimeout)) {
        runnerNext();
      }
    } break;
  }
}

const char* behaviorName(Behavior b){
  switch (b){
    case Behavior::Wake: return "Wake";
    case Behavior::Sleep: return "Sleep";
    case Behavior::WakeUpSequence: return "WakeUpSequence";
    case Behavior::Follow: return "Follow";
    case Behavior::Curious:             return "Curious";
    case Behavior::NodYes:              return "NodYes";
    case Behavior::ShakeNo:             return "ShakeNo";
    case Behavior::StartupCelebration:  return "StartupCelebration";
    default: return "";
  }
}

Behavior findBehaviorByName(const char* name) {
  if (strcmp(name, "Wake") == 0) return Behavior::Wake;
  if (strcmp(name, "Sleep") == 0) return Behavior::Sleep;
  if (strcmp(name, "WakeUpSequence") == 0) return Behavior::WakeUpSequence;
  if (strcmp(name, "Follow") == 0) return Behavior::Follow;
  if (strcmp(name, "Curious") == 0)            return Behavior::Curious;
  if (strcmp(name, "NodYes") == 0)             return Behavior::NodYes;
  if (strcmp(name, "ShakeNo") == 0)            return Behavior::ShakeNo;
  if (strcmp(name, "StartupCelebration") == 0) return Behavior::StartupCelebration;
  return Behavior::Wake; // fallback
}

void triggerBehavior(Behavior behavior) {
  DEBUG_PRINTF("triggerBehavior: %s\n", behaviorName(behavior));

  // Protect WakeUpSequence from interruption
  bool isSequenceBehavior = (behavior == Behavior::WakeUpSequence);

  // Check if WakeUpSequence is currently running
  bool currentlyInSequence = (currentBehaviorName == "WakeUpSequence");

  if (currentlyInSequence && !isSequenceBehavior) {
    // Don't interrupt WakeUpSequence with regular behaviors
    uint32_t elapsed = millis() - behaviorStartTime;
    Serial.printf("Ignoring behavior '%s' - sequence '%s' in progress (%.1fs elapsed)\n",
                  behaviorName(behavior), currentBehaviorName.c_str(), elapsed / 1000.0f);
    return;
  }

  // If triggering WakeUpSequence, enable manual override protection
  if (isSequenceBehavior) {
    Serial.printf("WakeUpSequence detected - enabling protection for 6000 ms\n");
    manualOverride = true;
    manualOverrideTimeout = millis() + 6000;
  }

  // Log if we're interrupting another behavior
  if (currentBehaviorName != "Wake" && currentBehaviorName != behaviorName(behavior)) {
    Serial.printf("INTERRUPTION: Switching from '%s' to '%s'\n",
                  currentBehaviorName.c_str(), behaviorName(behavior));
  }

  // Find the behavior in program array first (for normal cycling)
  bool foundInProgram = false;
  for (int i = 0; i < PROGRAM_LEN; i++) {
    if (program[i] == behavior) {
      runner.idx = i;
      runner.startAt = millis();
      runner.sub = 0;
      currentBehaviorName = behaviorName(program[i]);
      behaviorStartTime = runner.startAt;
      foundInProgram = true;
      Serial.printf("Found in program array at index %d\n", i);
      break;
    }
  }
  
  // If not in program array, trigger directly (manual behavior)
  if (!foundInProgram) {
    Serial.printf("Not in program array, triggering as manual behavior\n");
    
    // Create a temporary "out of program" state
    runner.idx = -1; // Special indicator
    runner.startAt = millis();
    runner.sub = 0;
    runner.a = 0; 
    runner.b = 0;
    renderMode = RenderMode::Normal;
    skewXL = skewXR = 0.0f;
    
    currentBehaviorName = behaviorName(behavior);
    behaviorStartTime = runner.startAt;
    
    // Store the behavior for tick processing
    currentManualBehavior = behavior;
    
    Serial.printf("Manual behavior set: %s (runner.idx = -1)\n", currentBehaviorName.c_str());
  }
}

