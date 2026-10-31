"""Bob number platform."""

from __future__ import annotations

from dataclasses import dataclass

from homeassistant.components import mqtt
from homeassistant.components.number import NumberEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DATA_RUNTIME, DOMAIN
from .entity import BobEntity


@dataclass(frozen=True)
class BobNumberDescription:
    key: str
    name: str
    status_key: str
    command_suffix: str
    min_value: float
    max_value: float
    step: float = 1.0


NUMBERS: tuple[BobNumberDescription, ...] = (
    BobNumberDescription("brightness", "Brightness", "brightness", "brightness", 0, 255, 1),
    BobNumberDescription("sleep_timer", "Sleep Timer", "sleep_timeout", "sleep_timeout", 10, 3600, 1),
    BobNumberDescription(
        "proximity_threshold",
        "Proximity Gevoeligheid",
        "proximity_threshold",
        "proximity_threshold",
        0,
        2047,
        1,
    ),
)


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    runtime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]
    async_add_entities(BobNumberEntity(entry, runtime, desc) for desc in NUMBERS)


class BobNumberEntity(BobEntity, NumberEntity):
    def __init__(self, entry: ConfigEntry, runtime, desc: BobNumberDescription) -> None:
        super().__init__(entry, runtime, f"number_{desc.key}", desc.name)
        self.entity_description = desc
        self._attr_native_min_value = desc.min_value
        self._attr_native_max_value = desc.max_value
        self._attr_native_step = desc.step

    @property
    def native_value(self):
        value = self._runtime.get(self.entity_description.status_key)
        if value is None:
            return None
        try:
            return float(value)
        except (TypeError, ValueError):
            return None

    async def async_set_native_value(self, value: float) -> None:
        await mqtt.async_publish(
            self.hass,
            self._runtime.command_topic(self.entity_description.command_suffix),
            str(int(value)),
            qos=0,
            retain=False,
        )
