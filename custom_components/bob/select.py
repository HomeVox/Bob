"""Bob select platform."""

from __future__ import annotations

from homeassistant.components import mqtt
from homeassistant.components.select import SelectEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DATA_RUNTIME, DOMAIN, EMOTIONS
from .entity import BobEntity


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    runtime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]
    async_add_entities([BobEmotionSelect(entry, runtime)])


class BobEmotionSelect(BobEntity, SelectEntity):
    def __init__(self, entry: ConfigEntry, runtime) -> None:
        super().__init__(entry, runtime, "select_emotion", "Emotion")
        self._attr_options = EMOTIONS

    @property
    def current_option(self) -> str | None:
        value = self._runtime.get("personality")
        if value is None:
            return None
        return str(value)

    async def async_select_option(self, option: str) -> None:
        await mqtt.async_publish(
            self.hass,
            self._runtime.command_topic("personality"),
            option,
            qos=0,
            retain=False,
        )
