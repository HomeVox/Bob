"""Bob integration."""

from __future__ import annotations

import json
from typing import Any

import voluptuous as vol

from homeassistant.components import mqtt
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.exceptions import HomeAssistantError
import homeassistant.helpers.config_validation as cv

from .const import (
    ACTIONS,
    CONF_COMMAND_PREFIX,
    CONF_NAME,
    DOMAIN,
    EMOTIONS,
    MODES,
    SERVICE_RUN_ACTION,
    SERVICE_SEND_TEXT,
    SERVICE_SET_EMOTION,
    SERVICE_SET_MODE,
)


SEND_TEXT_SCHEMA = vol.Schema(
    {
        vol.Required("message"): cv.string,
        vol.Optional("type", default="generic"): cv.string,
        vol.Optional("duration", default=4000): vol.All(vol.Coerce(int), vol.Range(min=1000, max=20000)),
        vol.Optional("wake", default=True): cv.boolean,
        vol.Optional("emotion"): cv.string,
        vol.Optional("sound"): cv.string,
        vol.Optional("entry_id"): cv.string,
    }
)

SET_EMOTION_SCHEMA = vol.Schema(
    {
        vol.Required("emotion"): vol.In(EMOTIONS),
        vol.Optional("entry_id"): cv.string,
    }
)

RUN_ACTION_SCHEMA = vol.Schema(
    {
        vol.Required("action"): vol.In(ACTIONS),
        vol.Optional("entry_id"): cv.string,
    }
)

SET_MODE_SCHEMA = vol.Schema(
    {
        vol.Required("mode"): vol.In(MODES),
        vol.Required("enabled"): cv.boolean,
        vol.Optional("entry_id"): cv.string,
    }
)


def _get_entry(hass: HomeAssistant, call: ServiceCall) -> ConfigEntry:
    """Resolve target Bob config entry from service call."""
    entries = hass.config_entries.async_entries(DOMAIN)
    if not entries:
        raise HomeAssistantError("No Bob config entry found")

    target_entry_id = call.data.get("entry_id")
    if target_entry_id:
        for entry in entries:
            if entry.entry_id == target_entry_id:
                return entry
        raise HomeAssistantError(f"Bob entry_id '{target_entry_id}' not found")

    return entries[0]


async def _publish(hass: HomeAssistant, topic: str, payload: str) -> None:
    """Publish MQTT message."""
    try:
        await mqtt.async_publish(hass, topic, payload, qos=0, retain=False)
    except Exception as err:
        raise HomeAssistantError(f"Failed to publish MQTT message to {topic}: {err}") from err


def _mode_to_topic_and_payload(prefix: str, mode: str, enabled: bool) -> tuple[str, str]:
    """Map mode command to MQTT topic/payload."""
    on = "ON" if enabled else "OFF"

    if mode == "matrix":
        return f"{prefix}/matrix", on
    if mode == "snow":
        return f"{prefix}/confetti", on
    if mode == "cannons":
        return f"{prefix}/confetti_cannons", on
    if mode in ("screensaver", "clock"):
        return f"{prefix}/screensaver", on
    if mode == "tracking":
        return f"{prefix}/tracking", on
    if mode == "camera_stream":
        return f"{prefix}/camera_stream", on
    if mode == "auto_emotion":
        return f"{prefix}/personality_auto", on
    if mode == "proximity":
        return f"{prefix}/proximity", on
    if mode == "auto_brightness":
        return f"{prefix}/auto_brightness", on

    raise HomeAssistantError(f"Unsupported mode '{mode}'")


def _action_to_topic_and_payload(prefix: str, action: str) -> tuple[str, str]:
    """Map high-level actions to MQTT topic/payload."""
    if action == "wake":
        return f"{prefix}/behavior", "Wake"
    if action == "sleep":
        return f"{prefix}/behavior", "Sleep"
    if action == "blink":
        return f"{prefix}/animate", "blink"
    if action == "follow":
        return f"{prefix}/behavior", "Follow"
    if action == "curious":
        return f"{prefix}/behavior", "Curious"
    if action == "wakeup_sequence":
        return f"{prefix}/behavior", "WakeUpSequence"
    if action == "celebrate":
        return f"{prefix}/behavior", "StartupCelebration"
    if action == "snapshot":
        return f"{prefix}/snapshot_ha", "TAKE"
    if action == "yes":
        return f"{prefix}/answer", "yes"
    if action == "no":
        return f"{prefix}/answer", "no"

    raise HomeAssistantError(f"Unsupported action '{action}'")


async def async_setup(hass: HomeAssistant, config: dict[str, Any]) -> bool:
    """Set up Bob integration from YAML (unused)."""
    return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up Bob from a config entry."""
    hass.data.setdefault(DOMAIN, {})
    hass.data[DOMAIN][entry.entry_id] = {
        CONF_NAME: entry.data[CONF_NAME],
        CONF_COMMAND_PREFIX: entry.data[CONF_COMMAND_PREFIX],
    }

    if not hass.services.has_service(DOMAIN, SERVICE_SEND_TEXT):

        async def handle_send_text(call: ServiceCall) -> None:
            target_entry = _get_entry(hass, call)
            prefix = target_entry.data[CONF_COMMAND_PREFIX]

            payload: dict[str, Any] = {
                "text": call.data["message"],
                "type": call.data["type"],
                "duration": call.data["duration"],
                "wake": call.data["wake"],
            }
            if "emotion" in call.data and call.data["emotion"]:
                payload["emotion"] = call.data["emotion"]
            if "sound" in call.data and call.data["sound"]:
                payload["sound"] = call.data["sound"]

            await _publish(hass, f"{prefix}/notify", json.dumps(payload, ensure_ascii=True))

        async def handle_set_emotion(call: ServiceCall) -> None:
            target_entry = _get_entry(hass, call)
            prefix = target_entry.data[CONF_COMMAND_PREFIX]
            await _publish(hass, f"{prefix}/personality", call.data["emotion"])

        async def handle_run_action(call: ServiceCall) -> None:
            target_entry = _get_entry(hass, call)
            prefix = target_entry.data[CONF_COMMAND_PREFIX]
            topic, payload = _action_to_topic_and_payload(prefix, call.data["action"])
            await _publish(hass, topic, payload)

        async def handle_set_mode(call: ServiceCall) -> None:
            target_entry = _get_entry(hass, call)
            prefix = target_entry.data[CONF_COMMAND_PREFIX]
            topic, payload = _mode_to_topic_and_payload(prefix, call.data["mode"], call.data["enabled"])
            await _publish(hass, topic, payload)

        hass.services.async_register(
            DOMAIN, SERVICE_SEND_TEXT, handle_send_text, schema=SEND_TEXT_SCHEMA
        )
        hass.services.async_register(
            DOMAIN, SERVICE_SET_EMOTION, handle_set_emotion, schema=SET_EMOTION_SCHEMA
        )
        hass.services.async_register(
            DOMAIN, SERVICE_RUN_ACTION, handle_run_action, schema=RUN_ACTION_SCHEMA
        )
        hass.services.async_register(
            DOMAIN, SERVICE_SET_MODE, handle_set_mode, schema=SET_MODE_SCHEMA
        )

    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload Bob config entry."""
    if DOMAIN in hass.data and entry.entry_id in hass.data[DOMAIN]:
        hass.data[DOMAIN].pop(entry.entry_id)

    if not hass.config_entries.async_entries(DOMAIN):
        for service_name in (
            SERVICE_SEND_TEXT,
            SERVICE_SET_EMOTION,
            SERVICE_RUN_ACTION,
            SERVICE_SET_MODE,
        ):
            if hass.services.has_service(DOMAIN, service_name):
                hass.services.async_remove(DOMAIN, service_name)

    return True

