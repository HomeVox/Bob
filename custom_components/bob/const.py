"""Constants for the Bob integration."""

from __future__ import annotations

DOMAIN = "bob"

CONF_COMMAND_PREFIX = "command_prefix"
CONF_NAME = "name"

DEFAULT_NAME = "Bob"
DEFAULT_COMMAND_PREFIX = "bob/cmd"

SERVICE_SEND_TEXT = "send_text"
SERVICE_SET_EMOTION = "set_emotion"
SERVICE_RUN_ACTION = "run_action"
SERVICE_SET_MODE = "set_mode"
SERVICE_PROVISION_BLE = "provision_ble_wifi"

EMOTIONS = [
    "Neutral",
    "Happy",
    "Sad",
    "Thinking",
    "Excited",
    "Confused",
    "Angry",
    "Scared",
    "Sleepy",
    "Love",
    "Surprised",
    "Dizzy",
    "Bored",
    "Random",
]

ACTIONS = [
    "wake",
    "sleep",
    "blink",
    "follow",
    "curious",
    "wakeup_sequence",
    "celebrate",
    "snapshot",
    "yes",
    "no",
]

MODES = [
    "matrix",
    "snow",
    "cannons",
    "screensaver",
    "clock",
    "tracking",
    "camera_stream",
    "auto_emotion",
    "proximity",
    "auto_brightness",
]
