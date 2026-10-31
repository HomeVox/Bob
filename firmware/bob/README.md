# Bob Firmware (M5CoreS3)

Main firmware for Bob.

## Arduino Flash
- Board package URL: `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
- Board package: `M5Stack`
- Board selection: `M5Stack-CoreS3`
- Required libraries:
  - `M5Unified`
  - `M5GFX`
  - `PubSubClient`
  - `ArduinoJson`

Open `bob.ino`, select the correct COM port, and upload.

## Hardware Note
- Supported: `M5Stack CoreS3 Lite / CoreS3`
- Not supported for this firmware: `M5AtomS3`

## Important
- Use `config.example.h` as a template.
- Keep personal credentials only in your local `config.h`.

## Sleep Mode Policy
- Default: `display-sleep`.
  - Bob keeps running with the display off, so touch wake stays responsive.
- Deep sleep: opt-in only.
  - Use only if you want maximum battery savings and accept slower wake/reboot.
  - Deep sleep is not used automatically in the normal sleep flow.

## Status Publishing
- Wake/sleep state is published through a single path: `publishStatus()`.
- Avoid direct `TOPIC_STATUS` publishes outside this function.

## Wake Telemetry
- Wake transitions go through shared helper `wakeBobFromSleep(...)`.
- Wake reason is published on `bob/wake_reason` and included in `bob/status` as `wake_reason`.
