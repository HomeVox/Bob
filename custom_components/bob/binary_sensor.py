"""Bob binary sensor platform."""

from __future__ import annotations

from dataclasses import dataclass

from homeassistant.components.binary_sensor import BinarySensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DATA_RUNTIME, DOMAIN
from .entity import BobEntity


@dataclass(frozen=True)
class BobBinarySensorDescription:
    key: str
    name: str
    status_key: str


BINARY_SENSORS: tuple[BobBinarySensorDescription, ...] = (
    BobBinarySensorDescription("touch", "Touch", "touch_detected"),
    BobBinarySensorDescription("shake", "Shake", "shake_detected"),
    BobBinarySensorDescription("movement", "Movement", "movement"),
    BobBinarySensorDescription("proximity_detected", "Proximity Detected", "proximity_detected"),
    BobBinarySensorDescription("camera_streaming", "Camera Streaming", "camera_streaming"),
)


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    runtime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]
    async_add_entities(BobBinarySensorEntity(entry, runtime, desc) for desc in BINARY_SENSORS)


class BobBinarySensorEntity(BobEntity, BinarySensorEntity):
    def __init__(
        self, entry: ConfigEntry, runtime, desc: BobBinarySensorDescription
    ) -> None:
        super().__init__(entry, runtime, f"binary_sensor_{desc.key}", desc.name)
        self.entity_description = desc

    @property
    def is_on(self) -> bool | None:
        if self.entity_description.status_key not in self._runtime.status:
            return None
        return self._runtime.get_bool(self.entity_description.status_key)
