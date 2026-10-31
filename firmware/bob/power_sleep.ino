// Extracted from bob.ino for modular structure

// Sleep/deep-sleep power lifecycle management
extern String runtimeMqttUser;
extern String runtimeMqttPass;
extern String runtimeMqttClientId;

void shutdownWiFiAndMQTT() {
  Serial.println(" Shutting down WiFi and MQTT...");

  // Disconnect MQTT gracefully
  if (mqttEnabled && mqttClient.connected()) {
    currentBehaviorName = "DeepSleep";
    publishStatus("deep_sleep");
    delay(100); // Give MQTT time to send

    mqttClient.disconnect();
    Serial.println(" MQTT disconnected");
  }

  // Disconnect WiFi
  WiFi.disconnect(true); // true = turn off WiFi radio to save power
  Serial.println(" WiFi disconnected and powered off");

  delay(200); // Give WiFi time to fully power down
}

// Enter deep sleep mode
// timeoutMinutes: how long to sleep before auto-waking (0 = sleep indefinitely until external trigger)
void enterDeepSleep(uint32_t timeoutMinutes) {
  Serial.println("\n");
  Serial.println("  BOB ENTERING DEEP SLEEP MODE    ");
  Serial.println("");

  // Turn off display completely
  M5.Display.sleep();
  M5.Display.setBrightness(0);

  // Shutdown WiFi and MQTT
  shutdownWiFiAndMQTT();

  // Configure wake-up triggers
  // Option 1: Timer-based wakeup (configurable timeout)
  if (timeoutMinutes > 0) {
    uint64_t sleepTimeUs = timeoutMinutes * 60ULL * 1000000ULL; // Convert to microseconds
    esp_sleep_enable_timer_wakeup(sleepTimeUs);
    Serial.printf("  Timer wakeup enabled: %d minutes\n", timeoutMinutes);
  } else {
    Serial.println("  Timer wakeup DISABLED - Bob will sleep until touched or remote wake");
  }

  // Enable external wakeup sources if available
  // Note: M5CoreS3 doesn't have native INT pin for proximity sensor,
  // so we rely on timer or RTC wakeup from MQTT command
  Serial.println(" All systems prepared for deep sleep");
  Serial.println(" To wake Bob remotely: Publish to bob/cmd/wake (Home Assistant)");
  Serial.println(" To wake Bob physically: Press screen or proximity sensor");

  // Go to deep sleep
  Serial.flush(); // Ensure all serial data is sent before sleep
  delay(100);

  esp_deep_sleep_start(); // This never returns - device wakes up and reboots
}

// Reconnect WiFi and MQTT after deep sleep wakeup
void reconnectAfterDeepSleep() {
  Serial.println("\n");
  Serial.println("  BOB WAKING UP FROM DEEP SLEEP    ");
  Serial.println("");

  // Turn on display
  M5.Display.wakeup();
  M5.Display.setBrightness(screenBrightness);
  screenOn = true;

  // Reconnect to WiFi
  Serial.println(" Reconnecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) { // 20 seconds timeout
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n WiFi reconnection failed - will retry in loop");
  }

  // Reconnect to MQTT
  if (mqttEnabled) {
    Serial.println(" Connecting to MQTT...");
    if (mqttClient.connect(runtimeMqttClientId.c_str(), runtimeMqttUser.c_str(), runtimeMqttPass.c_str())) {
      Serial.println(" MQTT connected!");

      // Resubscribe to command topics
      mqttClient.subscribe(TOPIC_CMD_BEHAV);
      mqttClient.subscribe(TOPIC_CMD_SCREEN);
      mqttClient.subscribe(TOPIC_CMD_WAKE);  // Remote wake command
      mqttClient.subscribe(TOPIC_CMD_PERSONALITY);
      mqttClient.subscribe(TOPIC_CMD_PERSONALITY_AUTO);
      mqttClient.subscribe("bob/cmd/confetti");
      mqttClient.subscribe("bob/cmd/confetti_cannons");
      Serial.println(" Resubscribed to MQTT topics");
    } else {
      Serial.println(" MQTT reconnection failed - will retry in loop");
    }
  }

  // Trigger wake behavior
  Serial.println("  Starting wake behavior...");
  wakeBobFromSleep("power", true);

}

