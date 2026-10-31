"""Bob sensor platform."""

from __future__ import annotations

from dataclasses import dataclass

from homeassistant.components.sensor import SensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DATA_RUNTIME, DOMAIN
from .entity import BobEntity


@dataclass(frozen=True)
class BobSensorDescription:
    key: str
    name: str
    status_key: str


SENSORS: tuple[BobSensorDescription, ...] = (
    BobSensorDescription("current_behavior", "Current Behavior", "behavior"),
    BobSensorDescription("state", "State", "state"),
    BobSensorDescription("wake_reason", "Wake Reason", "wake_reason"),
    BobSensorDescription("proximity_value", "Proximity Value", "proximity_value"),
    BobSensorDescription("ollama_state", "Ollama", "ollama_state"),
)


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    runtime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]
    async_add_entities(BobSensorEntity(entry, runtime, desc) for desc in SENSORS)


class BobSensorEntity(BobEntity, SensorEntity):
    def __init__(self, entry: ConfigEntry, runtime, desc: BobSensorDescription) -> None:
        super().__init__(entry, runtime, f"sensor_{desc.key}", desc.name)
        self.entity_description = desc

    @property
    def native_value(self):
        return self._runtime.get(self.entity_description.status_key)
