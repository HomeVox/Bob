<p align="center">
  <img src="https://github.com/HomeVox/housekeeping-addon/blob/main/housekeeping/logo.png?raw=true" alt="HomeVox" width="220" />
  &nbsp;&nbsp;&nbsp;
  <a href="https://github.com/HomeVox/Bob/blob/main/docs/images/bob-screen-2.png?raw=true" target="_blank" rel="noopener"><img src="https://github.com/HomeVox/Bob/blob/main/docs/images/bob-screen-2.png?raw=true" alt="Bob Screen" width="140" /></a>
</p>

<h1 align="center">BOB by HomeVox</h1>

BOB (Bad Or Brilliant) transforms your M5Stack CoreS3 Lite into a premium Home Assistant companion with personality.  
Instead of silent automations hidden in dashboards, BOB gives your smart home a face, a mood, and instant visual feedback.

BOB is fully open source end to end: from 3D-printing your own shell to firmware code and Home Assistant integration.

It’s either bad or brilliant.

## What BOB Brings to Your Home

BOB combines style and function in one device:

- Cinematic eye animations with smooth emotional transitions
- On-screen text messages for instant notifications
- Signature modes: screensaver/clock, matrix, snow, celebrate
- Home Assistant control via HACS + MQTT services
- Practical sleep/wake behavior tuned for daily use

## Built for Home Assistant

BOB is designed with Home Assistant as the core experience.  
You can drive notifications, emotions, and behavior directly from automations and scripts, so events are not only logged, but felt in your home.

Use BOB for:

- Visual notifications and message feedback
- Automation reactions with emotion and movement
- Daily routines that feel alive and personal
- Richer interaction than static dashboard cards

## Why You Should Choose BOB

Most smart-home setups are powerful, but they still feel impersonal.  
BOB changes that by adding presence, emotion, and instant visual context to your automations.

- Instant feedback you can see across the room
- More engaging automations that feel alive
- Better household clarity for alerts and routines
- A standout centerpiece for any Home Assistant setup

## Gallery

![Bob Screen 2](docs/images/bob-screen-2.png)
![Bob Screen 5](docs/images/bob-screen-5.png)

## Bambu Lab / MakerWorld

Want to publish or use BOB as a printable model on Bambu Lab?  
Use:

- [Bambu Upload Kit](docs/BAMBU-LAB.md)
- [Assembly Guide](docs/ASSEMBLY.md)
- [Assembly PDF](docs/BAMBU-GUIDE.pdf)

## Requirements

- M5Stack CoreS3 Lite
- 3D-printed housing/enclosure for Bob
- Home Assistant
- MQTT broker configured in Home Assistant
- HACS installed

## Installation

### 1. Install Integration via HACS

1. Open HACS in Home Assistant.
2. Add this repository as a Custom Repository:
   - URL: `https://github.com/HomeVox/Bob`
   - Category: `Integration`
3. Install **Bob**.
4. Restart Home Assistant.
5. Add integration: `Settings -> Devices & Services -> Add Integration -> Bob`.

### 2. Flash Firmware

Firmware source:

- `firmware/bob`

Arduino IDE quick setup:

1. Install Arduino IDE 2.x.
2. Add board manager URL:
   - `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
3. Install board package:
   - `M5Stack` by M5Stack
4. Select board:
   - `M5Stack-CoreS3`
5. Install required libraries (Library Manager):
   - `M5Unified`
   - `M5GFX`
   - `PubSubClient`
   - `ArduinoJson`
6. Open `firmware/bob/bob.ino`, select the correct COM port, then Upload.

Important:
- This firmware targets **M5Stack CoreS3 Lite / CoreS3**.
- **Do not select M5AtomS3** for this project.

### 3. Configure via Home Assistant (BLE)

Firmware can boot with placeholder values in `config.h`.
Provision real credentials from Home Assistant with BLE service:

- `bob.provision_ble_wifi`
- default Bob BLE setup name: `Bob-Setup-BLE`

Set at least in that service call:

- WiFi SSID/password
- MQTT host/user/password (and optional port/client ID)

If Bob is in setup mode and not connected to your home WiFi yet:

1. Connect your phone/laptop to WiFi network `Bob-Setup`
2. Open `http://192.168.4.1/setup`
3. Save your WiFi + MQTT settings

After Bob connects to your home network, you can open setup at:

- `http://bob.local/setup`
- or `http://<bob-ip>/setup`

## Home Assistant Services

Domain: `bob`

- `bob.send_text`
- `bob.set_emotion`
- `bob.run_action`
- `bob.set_mode`
- `bob.provision_ble_wifi`

Service definitions:

- `custom_components/bob/services.yaml`

## Service Examples

### Send text

```yaml
service: bob.send_text
data:
  message: "Coffee is ready"
  type: generic
  duration: 4000
  wake: true
```

### Set emotion

```yaml
service: bob.set_emotion
data:
  emotion: Happy
```

### Trigger action

```yaml
service: bob.run_action
data:
  action: celebrate
```

### Enable screensaver

```yaml
service: bob.set_mode
data:
  mode: screensaver
  enabled: true
```

### Provision WiFi and MQTT over BLE

```yaml
service: bob.provision_ble_wifi
data:
  ssid: "MyHomeWiFi"
  password: "supersecret"
  mqtt_host: "192.168.1.10"
  mqtt_port: 1883
  mqtt_user: "mqtt_user"
  mqtt_password: "mqtt_pass"
  mqtt_client_id: "bob"
  mqtt_enabled: true
  ble_name: "Bob-Setup-BLE"
```

## Modes and Actions, as One Experience

BOB gives you a complete motion-and-mood toolkit out of the box.  
You can change atmosphere with `matrix`, `snow`, and `cannons`, keep things elegant with `screensaver` or `clock`, and make behavior feel alive using `tracking`, `camera_stream`, `auto_emotion`, `proximity`, and `auto_brightness`.

When it is time to interact, BOB responds instantly with `wake`, `sleep`, `blink`, `follow`, `curious`, `wakeup_sequence`, `celebrate`, `snapshot`, `yes`, and `no`.

Together, these modes and actions let you move from practical status feedback to pure personality in one seamless Home Assistant flow.

## What’s Next

The next major upgrades are:

- Native **TTS** for spoken responses
- Richer **Sound FX** for stronger emotional presence

BOB will not only show what your smart home is doing, but also sound alive.

www.homevox.nl

## Repository Structure

- `custom_components/bob` - Home Assistant HACS integration
- `firmware/bob` - M5Stack CoreS3 Lite firmware
- `docs/images` - README image assets
- `hacs.json` - HACS metadata

## License

MIT - see `LICENSE`.



