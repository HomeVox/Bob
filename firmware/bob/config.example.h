// config.example.h
#ifndef CONFIG_H
#define CONFIG_H

/******************************
 * WiFi
 ******************************/
#define WIFI_SSID        ""
#define WIFI_PASSWORD    ""

/******************************
 * MQTT (Home Assistant broker)
 ******************************/
#define MQTT_HOST        ""
#define MQTT_PORT        1883
#define MQTT_USERNAME    ""
#define MQTT_PASSWORD    ""
#define MQTT_CLIENT_ID   "bob"

#define TOPIC_STATUS     "bob/status"
#define TOPIC_SENSORS    "bob/sensors"
#define TOPIC_EVENT      "bob/event"

#define TOPIC_CMD_SAY    "bob/cmd/say"
#define TOPIC_CMD_BEHAV  "bob/cmd/behavior"
#define TOPIC_CMD_SCREEN "bob/cmd/screen"
#define TOPIC_CMD_BRIGHTNESS "bob/cmd/brightness"
#define TOPIC_CMD_ROTATION "bob/cmd/rotation"
#define TOPIC_CMD_WAKE "bob/cmd/wake"
#define TOPIC_CMD_SLEEP_TIMEOUT "bob/cmd/sleep_timeout"
#define TOPIC_CMD_ALWAYS_AWAKE "bob/cmd/always_awake"
#define TOPIC_CMD_MICROPHONE "bob/cmd/microphone"
#define TOPIC_CMD_PROXIMITY "bob/cmd/proximity"
#define TOPIC_CMD_EYE_X "bob/cmd/eye_x"
#define TOPIC_CMD_EYE_Y "bob/cmd/eye_y"
#define TOPIC_CMD_PROXIMITY_THRESHOLD "bob/cmd/proximity_threshold"
#define TOPIC_CMD_PERSONALITY "bob/cmd/personality"
#define TOPIC_CMD_PERSONALITY_AUTO "bob/cmd/personality_auto"
#define TOPIC_CMD_SCENE "bob/cmd/scene"
#define TOPIC_CMD_NOTIFY "bob/cmd/notify"

/******************************
 * System Configuration Constants
 ******************************/
// Build profile:
// 1 = production (minimal logs), 0 = development
#define BOB_PRODUCTION_BUILD 1
// Optional explicit override (0..2). Uncomment to force:
// #define BOB_DEBUG_LEVEL 1

#define DEFAULT_PROXIMITY_TIMEOUT 30000
#define DEFAULT_INACTIVITY_TIMEOUT 60000
#define DEFAULT_SCREEN_BRIGHTNESS 60
#define DEFAULT_PROXIMITY_THRESHOLD 255

#define SENSOR_PUBLISH_INTERVAL_MS 500
#define STATUS_PUBLISH_INTERVAL_MS 10000
#define MQTT_RECONNECT_INTERVAL_MS 5000
#define MIC_UPDATE_INTERVAL_MS 500
#define PROXIMITY_CHECK_INTERVAL_MS 10

#define MIC_BUFFER_SIZE 512
#define MQTT_BUFFER_SIZE 2048

#define HA_BASE_URL      "http://homeassistant.local:8123"
#define HA_LONG_TOKEN    "YOUR_LONG_LIVED_ACCESS_TOKEN"

#define EYE_SCREEN_SCALE     1.10f
#define EYE_SPACING_BASE_PX  100
#define EYE_GLOW_PX          10
#define MAX_LOOK_X_BASE      38.0f
#define MAX_LOOK_Y_BASE      28.0f

#endif // CONFIG_H
