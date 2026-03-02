<p align="center">
  <img src="https://github.com/HomeVox/housekeeping-addon/blob/main/housekeeping/logo.png?raw=true" alt="HomeVox" width="220" />
  &nbsp;&nbsp;&nbsp;
  <img src="https://github.com/HomeVox/Bob/blob/main/docs/images/bob-screen-2.png?raw=true" alt="Bob Screen" width="140" />
</p>

<h1 align="center">BOB by HomeVox</h1>
<p align="center"><strong>Made by: <a href="https://homevox.nl" target="_blank" rel="noopener">HomeVox.nl</a></strong></p>

BOB (Bad Or Brilliant) transforms your M5Stack CoreS3 into a premium Home Assistant companion with personality.  
Instead of silent automations hidden in dashboards, BOB gives your smart home a face, a mood, and instant visual feedback.

From alerts to routines, BOB reacts in real time with expressive eyes, emotion-driven behavior, and on-screen messages.

It’s either bad or brilliant.

## What BOB Brings to Your Home

BOB combines style and function in one device:

- Cinematic eye animations with smooth emotional transitions
- On-screen text messages for instant notifications
- Signature modes: screensaver/clock, matrix, snow, celebrate
- Home Assistant control via HACS + MQTT services
- Practical sleep/wake behavior tuned for daily use

## Why You Should Choose BOB

Most smart-home setups are powerful, but they still feel impersonal.  
BOB changes that by adding presence, emotion, and instant visual context to your automations.

- Instant feedback you can see across the room
- More engaging automations that feel alive
- Better household clarity for alerts and routines
- A standout centerpiece for any Home Assistant setup

## Gallery

![Bob Screen 5](docs/images/bob-screen-5.png)

## Requirements

- M5Stack CoreS3
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

Build and flash with your preferred Arduino/PlatformIO workflow.

### 3. Configure Firmware

Use:

- `firmware/bob/config.h`
- `firmware/bob/config.example.h` as template

Set at least:

- WiFi credentials
- MQTT host/user/password
- `BOB_HA_GITHUB_URL` (already points to this repository)

## First Run Experience

On first boot, BOB can show a QR onboarding screen to accelerate setup:

- QR opens BOB’s local `/ha` onboarding page
- That page links users directly to this GitHub + HACS flow

Result: less friction, faster installation, better user experience.

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
