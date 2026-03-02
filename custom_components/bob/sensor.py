"""Sensor platform for Bob integration."""

from __future__ import annotations

from homeassistant.components.sensor import SensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity import EntityCategory
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import CONF_COMMAND_PREFIX, CONF_NAME, DOMAIN


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    """Set up Bob sensor entities from a config entry."""
    async_add_entities([BobCommandPrefixSensor(entry)], True)


class BobCommandPrefixSensor(SensorEntity):
    """Expose Bob MQTT command prefix as a simple diagnostic entity."""

    _attr_icon = "mdi:format-list-bulleted"
    _attr_entity_category = EntityCategory.DIAGNOSTIC

    def __init__(self, entry: ConfigEntry) -> None:
        self._entry = entry
        self._attr_has_entity_name = True
        self._attr_name = "Command Prefix"
        self._attr_unique_id = f"{entry.entry_id}_command_prefix"

    @property
    def native_value(self) -> str:
        """Return current configured command prefix."""
        return self._entry.data.get(CONF_COMMAND_PREFIX, "")

    @property
    def device_info(self) -> dict:
        """Return Bob device metadata."""
        return {
            "identifiers": {(DOMAIN, self._entry.entry_id)},
            "name": self._entry.data.get(CONF_NAME, "Bob"),
            "manufacturer": "HomeVox",
            "model": "Bob",
        }
