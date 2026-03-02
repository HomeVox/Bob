"""Base entity helpers for Bob platforms."""

from __future__ import annotations

from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity import Entity

from .const import DOMAIN
from .runtime import BobRuntime


class BobEntity(Entity):
    """Base Bob entity bound to a config entry and runtime."""

    _attr_has_entity_name = True

    def __init__(
        self,
        entry: ConfigEntry,
        runtime: BobRuntime,
        unique_key: str,
        name: str,
    ) -> None:
        self._entry = entry
        self._runtime = runtime
        self._attr_unique_id = f"{entry.entry_id}_{unique_key}"
        self._attr_name = name
        self._remove_listener = None

    @property
    def device_info(self) -> DeviceInfo:
        return DeviceInfo(
            identifiers={(DOMAIN, self._entry.entry_id)},
            manufacturer="HomeVox",
            model="Bob",
            name=self._entry.title or self._entry.data.get("name", "Bob"),
        )

    async def async_added_to_hass(self) -> None:
        self._remove_listener = self._runtime.add_listener(self._handle_runtime_update)
        self._handle_runtime_update()

    async def async_will_remove_from_hass(self) -> None:
        if self._remove_listener:
            self._remove_listener()
            self._remove_listener = None

    def _handle_runtime_update(self) -> None:
        self.async_write_ha_state()
