"""Bob switch platform."""

from __future__ import annotations

from dataclasses import dataclass

from homeassistant.components import mqtt
from homeassistant.components.switch import SwitchEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DATA_RUNTIME, DOMAIN
from .entity import BobEntity


@dataclass(frozen=True)
class BobSwitchDescription:
    key: str
    name: str
    status_key: str
    command_suffix: str
    payload_on: str = "ON"
    payload_off: str = "OFF"


SWITCHES: tuple[BobSwitchDescription, ...] = (
    BobSwitchDescription("always_awake", "Always Awake", "always_awake", "always_awake"),
    BobSwitchDescription(
        "auto_awake_on_power",
        "Auto Awake On Power",
        "auto_awake_on_power",
        "auto_awake_on_power",
    ),
    BobSwitchDescription("auto_brightness", "Auto Brightness", "auto_brightness", "auto_brightness"),
    BobSwitchDescription("auto_emotion", "Auto Emotion", "auto_emotion", "personality_auto"),
    BobSwitchDescription("proximity", "Proximity", "proximity_enabled", "proximity"),
    BobSwitchDescription("tracking", "Follow Mode", "tracking", "tracking"),
    BobSwitchDescription("screen", "Screen", "screen_on", "screen", payload_on="on", payload_off="off"),
    BobSwitchDescription("matrix", "Matrix Mode", "matrix_mode", "matrix"),
    BobSwitchDescription("clock", "Clock Screensaver", "clock_mode", "clock"),
    BobSwitchDescription(
        "confetti_snow",
        "Confetti Snow",
        "confetti_snow_enabled",
        "confetti",
    ),
    BobSwitchDescription(
        "confetti_cannons",
        "Confetti Cannons",
        "confetti_cannons_enabled",
        "confetti_cannons",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    runtime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]
    async_add_entities(BobSwitchEntity(entry, runtime, desc) for desc in SWITCHES)


class BobSwitchEntity(BobEntity, SwitchEntity):
    def __init__(self, entry: ConfigEntry, runtime, desc: BobSwitchDescription) -> None:
        super().__init__(entry, runtime, f"switch_{desc.key}", desc.name)
        self.entity_description = desc

    @property
    def is_on(self) -> bool:
        return self._runtime.get_bool(self.entity_description.status_key, False)

    async def async_turn_on(self, **kwargs) -> None:
        await mqtt.async_publish(
            self.hass,
            self._runtime.command_topic(self.entity_description.command_suffix),
            self.entity_description.payload_on,
            qos=0,
            retain=False,
        )

    async def async_turn_off(self, **kwargs) -> None:
        await mqtt.async_publish(
            self.hass,
            self._runtime.command_topic(self.entity_description.command_suffix),
            self.entity_description.payload_off,
            qos=0,
            retain=False,
        )
