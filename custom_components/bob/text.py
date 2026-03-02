"""Bob text platform."""

from __future__ import annotations

import json

from homeassistant.components import mqtt
from homeassistant.components.text import TextEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DATA_RUNTIME, DOMAIN
from .entity import BobEntity


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    runtime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]
    async_add_entities([BobNotificationText(entry, runtime)])


class BobNotificationText(BobEntity, TextEntity):
    def __init__(self, entry: ConfigEntry, runtime) -> None:
        super().__init__(entry, runtime, "text_notification", "Notification")
        self._value: str | None = None

    @property
    def native_value(self) -> str | None:
        return self._value

    async def async_set_value(self, value: str) -> None:
        self._value = value
        payload = json.dumps(
            {"text": value, "type": "generic", "duration": 4000, "wake": True},
            ensure_ascii=True,
        )
        await mqtt.async_publish(
            self.hass,
            self._runtime.command_topic("notify"),
            payload,
            qos=0,
            retain=False,
        )
        self.async_write_ha_state()
