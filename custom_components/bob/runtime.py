"""Shared MQTT runtime state for Bob entities."""

from __future__ import annotations

import json
from collections.abc import Callable
from typing import Any

from homeassistant.components import mqtt
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import CALLBACK_TYPE, HomeAssistant, callback

from .const import CONF_COMMAND_PREFIX


def _to_bool(value: Any) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    text = str(value).strip().lower()
    return text in {"on", "true", "1", "yes", "active", "awake"}


class BobRuntime:
    """Holds latest MQTT state and notifies listeners."""

    def __init__(self, hass: HomeAssistant, entry: ConfigEntry) -> None:
        self.hass = hass
        self.entry = entry
        self.command_prefix: str = entry.data[CONF_COMMAND_PREFIX]
        self.base_topic = (
            self.command_prefix[:-4]
            if self.command_prefix.endswith("/cmd")
            else self.command_prefix
        )
        self._status: dict[str, Any] = {}
        self._listeners: set[Callable[[], None]] = set()
        self._unsubs: list[CALLBACK_TYPE] = []

    @property
    def status(self) -> dict[str, Any]:
        return self._status

    async def async_start(self) -> None:
        """Subscribe to Bob MQTT state topics."""
        self._unsubs.append(
            await mqtt.async_subscribe(
                self.hass, f"{self.base_topic}/status", self._on_status, qos=0
            )
        )
        topic_map = {
            f"{self.base_topic}/touch": "touch_detected",
            f"{self.base_topic}/shake": "shake_detected",
            f"{self.base_topic}/movement": "movement",
            f"{self.base_topic}/proximity_detected": "proximity_detected",
            f"{self.base_topic}/proximity_value": "proximity_value",
            f"{self.base_topic}/camera/streaming": "camera_streaming",
            f"{self.base_topic}/ollama_state": "ollama_state",
            f"{self.base_topic}/notify_state": "notify_state",
        }
        for topic, key in topic_map.items():
            self._unsubs.append(
                await mqtt.async_subscribe(
                    self.hass,
                    topic,
                    lambda msg, k=key: self._on_simple(k, msg.payload),
                    qos=0,
                )
            )

    async def async_stop(self) -> None:
        """Unsubscribe MQTT listeners."""
        while self._unsubs:
            unsub = self._unsubs.pop()
            unsub()
        self._listeners.clear()

    @callback
    def add_listener(self, listener: Callable[[], None]) -> CALLBACK_TYPE:
        """Register callback and return remover."""
        self._listeners.add(listener)

        @callback
        def _remove() -> None:
            self._listeners.discard(listener)

        return _remove

    @callback
    def _on_status(self, msg: Any) -> None:
        try:
            parsed = json.loads(msg.payload)
        except (json.JSONDecodeError, TypeError):
            return
        if not isinstance(parsed, dict):
            return
        self._status.update(parsed)
        self._dispatch()

    @callback
    def _on_simple(self, key: str, payload: str) -> None:
        self._status[key] = payload
        self._dispatch()

    @callback
    def _dispatch(self) -> None:
        for listener in list(self._listeners):
            listener()

    def get(self, key: str, default: Any = None) -> Any:
        return self._status.get(key, default)

    def get_bool(self, key: str, default: bool = False) -> bool:
        if key not in self._status:
            return default
        return _to_bool(self._status[key])

    def command_topic(self, suffix: str) -> str:
        return f"{self.command_prefix}/{suffix}"
