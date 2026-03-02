"""Config flow for Bob integration."""

from __future__ import annotations

from typing import Any

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.data_entry_flow import FlowResult
from homeassistant.exceptions import HomeAssistantError

from .ble_provision import async_provision_over_ble
from .const import CONF_COMMAND_PREFIX, CONF_NAME, DEFAULT_COMMAND_PREFIX, DEFAULT_NAME, DOMAIN


class BobConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for Bob."""

    VERSION = 1

    def __init__(self) -> None:
        """Initialize flow state."""
        self._pending: dict[str, Any] = {}

    async def async_step_user(self, user_input: dict[str, Any] | None = None) -> FlowResult:
        """Step 1: basic integration settings."""
        errors: dict[str, str] = {}

        if user_input is not None:
            prefix = user_input[CONF_COMMAND_PREFIX].strip().rstrip("/")
            name = user_input[CONF_NAME].strip() or DEFAULT_NAME

            for entry in self._async_current_entries():
                if entry.data.get(CONF_COMMAND_PREFIX) == prefix:
                    return self.async_abort(reason="already_configured")

            self._pending = {
                CONF_NAME: name,
                CONF_COMMAND_PREFIX: prefix,
            }

            if user_input["provision_via_ble"]:
                return await self.async_step_ble_wifi()

            return self.async_create_entry(title=name, data=self._pending)

        schema = vol.Schema(
            {
                vol.Required(CONF_NAME, default=DEFAULT_NAME): str,
                vol.Required(CONF_COMMAND_PREFIX, default=DEFAULT_COMMAND_PREFIX): str,
                vol.Optional("provision_via_ble", default=True): bool,
            }
        )
        return self.async_show_form(step_id="user", data_schema=schema, errors=errors)

    async def async_step_ble_wifi(self, user_input: dict[str, Any] | None = None) -> FlowResult:
        """Step 2: WiFi + BLE transport settings."""
        errors: dict[str, str] = {}

        if user_input is not None:
            ssid = user_input["ssid"].strip()
            if not ssid:
                errors["base"] = "ssid_required"
            else:
                self._pending.update(
                    {
                        "ssid": ssid,
                        "password": user_input["password"],
                        "ble_name": user_input["ble_name"].strip() or "Bob-Setup-BLE",
                        "ble_address": user_input["ble_address"].strip(),
                        "timeout": user_input["timeout"],
                    }
                )
                return await self.async_step_ble_mqtt()

        schema = vol.Schema(
            {
                vol.Required("ssid", default=self._pending.get("ssid", "")): str,
                vol.Required("password", default=self._pending.get("password", "")): str,
                vol.Optional("ble_name", default=self._pending.get("ble_name", "Bob-Setup-BLE")): str,
                vol.Optional("ble_address", default=self._pending.get("ble_address", "")): str,
                vol.Optional("timeout", default=self._pending.get("timeout", 20)): vol.All(
                    vol.Coerce(int), vol.Range(min=5, max=90)
                ),
            }
        )
        return self.async_show_form(step_id="ble_wifi", data_schema=schema, errors=errors)

    async def async_step_ble_mqtt(self, user_input: dict[str, Any] | None = None) -> FlowResult:
        """Step 3: MQTT values and provisioning apply."""
        errors: dict[str, str] = {}

        if user_input is not None:
            payload = {
                "ssid": self._pending["ssid"],
                "password": self._pending["password"],
                "ble_name": self._pending["ble_name"],
                "ble_address": self._pending["ble_address"] or None,
                "timeout": self._pending["timeout"],
                "mqtt_host": user_input["mqtt_host"],
                "mqtt_port": user_input["mqtt_port"],
                "mqtt_user": user_input["mqtt_user"],
                "mqtt_password": user_input["mqtt_password"],
                "mqtt_client_id": user_input["mqtt_client_id"],
                "mqtt_enabled": user_input["mqtt_enabled"],
            }
            try:
                await async_provision_over_ble(payload)
            except HomeAssistantError:
                errors["base"] = "ble_provision_failed"
            except Exception:
                errors["base"] = "unknown"

            if not errors:
                return self.async_create_entry(
                    title=self._pending[CONF_NAME],
                    data={
                        CONF_NAME: self._pending[CONF_NAME],
                        CONF_COMMAND_PREFIX: self._pending[CONF_COMMAND_PREFIX],
                    },
                )

        schema = vol.Schema(
            {
                vol.Optional("mqtt_enabled", default=True): bool,
                vol.Optional("mqtt_host", default=""): str,
                vol.Optional("mqtt_port", default=1883): vol.All(vol.Coerce(int), vol.Range(min=1, max=65535)),
                vol.Optional("mqtt_user", default=""): str,
                vol.Optional("mqtt_password", default=""): str,
                vol.Optional("mqtt_client_id", default="bob"): str,
            }
        )
        return self.async_show_form(step_id="ble_mqtt", data_schema=schema, errors=errors)
