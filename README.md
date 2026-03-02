<h1 align="center">BOB by HomeVox</h1>

BOB is an animated Home Assistant companion for M5Stack CoreS3 / CoreS3 Lite.

## What Works Right Now

- Firmware for `M5Stack CoreS3 / CoreS3 Lite`
- Web onboarding via Bob setup portal
- MQTT control + status updates
- Home Assistant HACS integration with native Bob entities (v0.2.9)
- Optional BLE provisioning service from Home Assistant

## Requirements

- M5Stack CoreS3 or CoreS3 Lite
- Home Assistant + HACS
- MQTT broker (Mosquitto)

## Installation

### 1. Install Bob Integration (HACS)

1. Open HACS in Home Assistant.
2. Add custom repository: `https://github.com/HomeVox/Bob` (category `Integration`).
3. Install **Bob**.
4. Restart Home Assistant.
5. Add integration via `Settings -> Devices & Services -> Add Integration -> Bob`.

### 2. Flash Firmware

Firmware path: `firmware/bob`

Arduino IDE setup:

1. Add board manager URL:
   - `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
2. Install board package:
   - `M5Stack`
3. Select board:
   - `M5Stack-CoreS3`
4. Install libraries:
   - `M5Unified`
   - `M5GFX`
   - `PubSubClient`
   - `ArduinoJson`
5. Open and upload:
   - `firmware/bob/bob.ino`

Important:

- This project targets CoreS3/CoreS3 Lite.
- Do not use M5AtomS3 board profile.

### 3. Configure Bob (Recommended: Web Setup)

If Bob has no valid WiFi credentials, it starts AP setup mode:

1. Connect phone/laptop to WiFi: `Bob-Setup`
2. Open: `http://192.168.4.1/setup`
3. Fill in WiFi + MQTT credentials
4. Click **Save and Connect**

After Bob joins your home network:

- `http://bob.local/setup`
- or `http://<bob-ip>/setup`

MQTT must be correct:

- Host (IP recommended, e.g. `192.168.x.x`)
- Port `1883` (plain MQTT)
- Username/password accepted by your broker

### 4. Optional: Provision via BLE from Home Assistant

Service: `bob.provision_ble_wifi`

Default BLE name: `Bob-Setup-BLE`

## Home Assistant Integration (v0.2.9)

The Bob integration now exposes native entities under the Bob device, including:

- Switches (Always Awake, Auto Brightness, Matrix, Clock, etc.)
- Numbers (Brightness, Sleep Timer, Proximity Threshold)
- Buttons (Wake, Sleep, Follow, Snapshot, Yes/No)
- Select (Emotion)
- Text (Notification)
- Sensors/Binary sensors (State, Behavior, Touch, Movement, Proximity, etc.)

Services are also available:

- `bob.send_text`
- `bob.set_emotion`
- `bob.run_action`
- `bob.set_mode`
- `bob.provision_ble_wifi`

Service definitions:

- `custom_components/bob/services.yaml`

## Troubleshooting

- MQTT `rc=5` means your broker rejected credentials.
- Setup page not opening:
  - ensure you are connected to `Bob-Setup`
  - open `http://192.168.4.1/setup` (not HTTPS)
- If entities do not appear immediately:
  - restart Mosquitto
  - restart Home Assistant
  - reload Bob integration

## Repository Structure

- `custom_components/bob` - Home Assistant HACS integration
- `firmware/bob` - M5Stack firmware
- `docs/images` - README image assets
- `hacs.json` - HACS metadata

## License

MIT - see `LICENSE`.

