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

    async def async_step_user(self, user_input: dict[str, Any] | None = None) -> FlowResult:
        """Handle the initial step."""
        errors: dict[str, str] = {}

        if user_input is not None:
            prefix = user_input[CONF_COMMAND_PREFIX].strip().rstrip("/")
            name = user_input[CONF_NAME].strip() or DEFAULT_NAME

            for entry in self._async_current_entries():
                if entry.data.get(CONF_COMMAND_PREFIX) == prefix:
                    return self.async_abort(reason="already_configured")

            if user_input["provision_via_ble"]:
                if not user_input["ssid"].strip():
                    errors["base"] = "ssid_required"
                else:
                    try:
                        await async_provision_over_ble(
                            {
                                "ssid": user_input["ssid"],
                                "password": user_input["password"],
                                "mqtt_host": user_input["mqtt_host"],
                                "mqtt_port": user_input["mqtt_port"],
                                "mqtt_user": user_input["mqtt_user"],
                                "mqtt_password": user_input["mqtt_password"],
                                "mqtt_client_id": user_input["mqtt_client_id"],
                                "mqtt_enabled": user_input["mqtt_enabled"],
                                "ble_name": user_input["ble_name"],
                                "ble_address": user_input["ble_address"],
                                "timeout": user_input["timeout"],
                            }
                        )
                    except HomeAssistantError:
                        errors["base"] = "ble_provision_failed"
                    except Exception:
                        errors["base"] = "unknown"

            if errors:
                return self.async_show_form(step_id="user", data_schema=_schema(user_input), errors=errors)

            return self.async_create_entry(
                title=name,
                data={
                    CONF_NAME: name,
                    CONF_COMMAND_PREFIX: prefix,
                },
            )

        return self.async_show_form(step_id="user", data_schema=_schema(None), errors=errors)


def _schema(user_input: dict[str, Any] | None) -> vol.Schema:
    """Return form schema for config flow."""
    defaults = user_input or {}
    return vol.Schema(
        {
            vol.Required(CONF_NAME, default=defaults.get(CONF_NAME, DEFAULT_NAME)): str,
            vol.Required(
                CONF_COMMAND_PREFIX, default=defaults.get(CONF_COMMAND_PREFIX, DEFAULT_COMMAND_PREFIX)
            ): str,
            vol.Optional("provision_via_ble", default=defaults.get("provision_via_ble", False)): bool,
            vol.Optional("ssid", default=defaults.get("ssid", "")): str,
            vol.Optional("password", default=defaults.get("password", "")): str,
            vol.Optional("mqtt_host", default=defaults.get("mqtt_host", "")): str,
            vol.Optional("mqtt_port", default=defaults.get("mqtt_port", 1883)): vol.All(
                vol.Coerce(int), vol.Range(min=1, max=65535)
            ),
            vol.Optional("mqtt_user", default=defaults.get("mqtt_user", "")): str,
            vol.Optional("mqtt_password", default=defaults.get("mqtt_password", "")): str,
            vol.Optional("mqtt_client_id", default=defaults.get("mqtt_client_id", "bob")): str,
            vol.Optional("mqtt_enabled", default=defaults.get("mqtt_enabled", True)): bool,
            vol.Optional("ble_name", default=defaults.get("ble_name", "Bob-Setup-BLE")): str,
            vol.Optional("ble_address", default=defaults.get("ble_address", "")): str,
            vol.Optional("timeout", default=defaults.get("timeout", 20)): vol.All(
                vol.Coerce(int), vol.Range(min=5, max=90)
            ),
        }
    )
