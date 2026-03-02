"""Binary sensors for Bob integration connectivity state."""

from __future__ import annotations

import json
from collections.abc import Callable

from homeassistant.components import mqtt
from homeassistant.components.binary_sensor import BinarySensorDeviceClass, BinarySensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import CONF_COMMAND_PREFIX, CONF_NAME, DOMAIN


def _status_topic_from_prefix(prefix: str) -> str:
    """Convert command prefix to Bob status topic."""
    clean = (prefix or "").strip().rstrip("/")
    if clean.endswith("/cmd"):
        base = clean[:-4]
    else:
        base = clean
    if not base:
        base = "bob"
    return f"{base}/status"


def _parse_bool(value: object) -> bool | None:
    """Parse bool-like values from Bob JSON status payload."""
    if isinstance(value, bool):
        return value
    if value is None:
        return None
    text = str(value).strip().lower()
    if text in ("1", "true", "on", "yes", "connected"):
        return True
    if text in ("0", "false", "off", "no", "disconnected"):
        return False
    return None


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    """Set up Bob binary sensors from a config entry."""
    topic = _status_topic_from_prefix(entry.data.get(CONF_COMMAND_PREFIX, "bob/cmd"))
    async_add_entities(
        [
            BobConnectivityBinarySensor(entry, topic, "WiFi Connected", "wifi", "wifi"),
            BobConnectivityBinarySensor(entry, topic, "MQTT Connected", "mqtt", "access-point-network"),
        ],
        True,
    )


class BobConnectivityBinarySensor(BinarySensorEntity):
    """Base connectivity sensor driven by Bob status MQTT payload."""

    _attr_device_class = BinarySensorDeviceClass.CONNECTIVITY

    def __init__(
        self, entry: ConfigEntry, topic: str, name: str, json_key: str, icon_suffix: str
    ) -> None:
        self._entry = entry
        self._topic = topic
        self._json_key = json_key
        self._attr_has_entity_name = True
        self._attr_name = name
        self._attr_unique_id = f"{entry.entry_id}_{json_key}_connected"
        self._attr_icon = f"mdi:{icon_suffix}"
        self._attr_is_on = False
        self._unsub: Callable[[], None] | None = None

    @property
    def device_info(self) -> dict:
        """Return Bob device metadata."""
        return {
            "identifiers": {(DOMAIN, self._entry.entry_id)},
            "name": self._entry.data.get(CONF_NAME, "Bob"),
            "manufacturer": "HomeVox",
            "model": "Bob",
        }

    async def async_added_to_hass(self) -> None:
        """Subscribe to Bob status topic."""

        @callback
        def _message_received(msg: mqtt.ReceiveMessage) -> None:
            try:
                payload = json.loads(msg.payload)
            except Exception:
                return

            parsed = _parse_bool(payload.get(self._json_key))
            if parsed is None:
                return

            self._attr_is_on = parsed
            self.async_write_ha_state()

        self._unsub = await mqtt.async_subscribe(self.hass, self._topic, _message_received, 0)

    async def async_will_remove_from_hass(self) -> None:
        """Unsubscribe on entity removal."""
        if self._unsub is not None:
            self._unsub()
            self._unsub = None

