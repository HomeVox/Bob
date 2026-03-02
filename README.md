<p align="center">
  <img src="https://github.com/HomeVox/housekeeping-addon/blob/main/housekeeping/logo.png?raw=true" alt="HomeVox" width="220" />
  &nbsp;&nbsp;&nbsp;
  <img src="https://github.com/HomeVox/Bob/blob/main/docs/images/bob-screen-2.png?raw=true" alt="Bob Screen" width="140" />
</p>

# BOB by HomeVox

**Made by: HomeVox.nl**

BOB (Bad Or Brilliant) turns your M5Stack CoreS3 into a character-driven Home Assistant companion.  
It reacts with expressive eyes, behavior-based emotions, and on-screen messages, so your automations feel alive instead of static.

BOB is designed for easy onboarding:
- Scan the QR shown on Bob's screen
- Install through HACS
- Add the integration in Home Assistant
- Start controlling Bob with simple services and MQTT actions

It’s either bad or brilliant.

## What BOB Does

BOB combines visual personality with practical smart-home feedback:

- Emotional eye engine with smooth transitions
- Text notifications displayed directly on Bob’s screen
- Screensaver/clock mode in Bob style
- Matrix mode, snow mode, and celebration effects
- Sleep/wake behavior tuned for real-world usage
- Fast Home Assistant control through HACS integration

## Why It’s Different

Most smart-home feedback is hidden in apps and dashboards.  
BOB gives your automations a physical face in your home:

- You can *see* states and events instantly
- You can *trigger* reactions from HA automations/scripts
- You can *personalize* behavior and mood for your setup

## Gallery

![Bob Screen 5](docs/images/bob-screen-5.png)

## Requirements

- M5Stack CoreS3
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

Build and flash with your preferred Arduino/PlatformIO flow.

### 3. Configure Firmware

Use:

- `firmware/bob/config.h`
- `firmware/bob/config.example.h` as template

Set at least:

- WiFi credentials
- MQTT host/user/password
- `BOB_HA_GITHUB_URL` (already points to this repository)

## First Run Experience

On boot, Bob can show a QR onboarding screen:

- QR opens Bob’s local `/ha` onboarding page
- That page links users straight to this GitHub repo/HACS flow

This keeps setup simple for end users before deeper customization.

## Home Assistant Services

Domain: `bob`

- `bob.send_text`
- `bob.set_emotion`
- `bob.run_action`
- `bob.set_mode`

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

## Supported Modes

- `matrix`
- `snow`
- `cannons`
- `screensaver`
- `clock`
- `tracking`
- `camera_stream`
- `auto_emotion`
- `proximity`
- `auto_brightness`

## Supported Actions

- `wake`
- `sleep`
- `blink`
- `follow`
- `curious`
- `wakeup_sequence`
- `celebrate`
- `snapshot`
- `yes`
- `no`

## Repository Structure

- `custom_components/bob` - Home Assistant HACS integration
- `firmware/bob` - M5Stack CoreS3 firmware
- `docs/images` - README image assets
- `hacs.json` - HACS metadata

## License

MIT - see `LICENSE`.
