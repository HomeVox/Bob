"""Bob button platform."""

from __future__ import annotations

from dataclasses import dataclass

from homeassistant.components import mqtt
from homeassistant.components.button import ButtonEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DATA_RUNTIME, DOMAIN
from .entity import BobEntity


@dataclass(frozen=True)
class BobButtonDescription:
    key: str
    name: str
    command_suffix: str
    payload: str


BUTTONS: tuple[BobButtonDescription, ...] = (
    BobButtonDescription("wake", "Wake", "behavior", "Wake"),
    BobButtonDescription("sleep", "Sleep", "behavior", "Sleep"),
    BobButtonDescription("follow", "Follow", "behavior", "Follow"),
    BobButtonDescription("wakeup_sequence", "WakeUp Sequence", "behavior", "WakeUpSequence"),
    BobButtonDescription("snapshot", "Camera Snapshot", "snapshot_ha", "TAKE"),
    BobButtonDescription("yes", "Yes", "answer", "yes"),
    BobButtonDescription("no", "No", "answer", "no"),
)


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    runtime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]
    async_add_entities(BobButtonEntity(entry, runtime, desc) for desc in BUTTONS)


class BobButtonEntity(BobEntity, ButtonEntity):
    def __init__(self, entry: ConfigEntry, runtime, desc: BobButtonDescription) -> None:
        super().__init__(entry, runtime, f"button_{desc.key}", desc.name)
        self.entity_description = desc

    async def async_press(self) -> None:
        await mqtt.async_publish(
            self.hass,
            self._runtime.command_topic(self.entity_description.command_suffix),
            self.entity_description.payload,
            qos=0,
            retain=False,
        )
