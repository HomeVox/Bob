"""Config flow for Bob integration."""

from __future__ import annotations

from typing import Any

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.data_entry_flow import FlowResult

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

            return self.async_create_entry(
                title=name,
                data={
                    CONF_NAME: name,
                    CONF_COMMAND_PREFIX: prefix,
                },
            )

        schema = vol.Schema(
            {
                vol.Required(CONF_NAME, default=DEFAULT_NAME): str,
                vol.Required(CONF_COMMAND_PREFIX, default=DEFAULT_COMMAND_PREFIX): str,
            }
        )

        return self.async_show_form(step_id="user", data_schema=schema, errors=errors)

