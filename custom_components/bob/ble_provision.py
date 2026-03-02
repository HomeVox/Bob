"""BLE provisioning helpers for Bob."""

from __future__ import annotations

import asyncio
from typing import Any

from homeassistant.exceptions import HomeAssistantError

BLE_PROV_SSID_UUID = "7a6b0002-10f6-4a6a-b4a0-3f1c2d9e1000"
BLE_PROV_PASS_UUID = "7a6b0003-10f6-4a6a-b4a0-3f1c2d9e1000"
BLE_PROV_APPLY_UUID = "7a6b0004-10f6-4a6a-b4a0-3f1c2d9e1000"
BLE_PROV_STATUS_UUID = "7a6b0005-10f6-4a6a-b4a0-3f1c2d9e1000"
BLE_PROV_MQTT_HOST_UUID = "7a6b0006-10f6-4a6a-b4a0-3f1c2d9e1000"
BLE_PROV_MQTT_PORT_UUID = "7a6b0007-10f6-4a6a-b4a0-3f1c2d9e1000"
BLE_PROV_MQTT_USER_UUID = "7a6b0008-10f6-4a6a-b4a0-3f1c2d9e1000"
BLE_PROV_MQTT_PASS_UUID = "7a6b0009-10f6-4a6a-b4a0-3f1c2d9e1000"
BLE_PROV_MQTT_CID_UUID = "7a6b000a-10f6-4a6a-b4a0-3f1c2d9e1000"
BLE_PROV_MQTT_EN_UUID = "7a6b000b-10f6-4a6a-b4a0-3f1c2d9e1000"


async def async_provision_over_ble(data: dict[str, Any]) -> None:
    """Provision Bob WiFi/MQTT credentials over BLE."""
    try:
        from bleak import BleakClient, BleakScanner
    except Exception as err:
        raise HomeAssistantError(
            "BLE provisioning requires bleak; ensure Bluetooth support is available in Home Assistant."
        ) from err

    ssid = data["ssid"].strip()
    password = data["password"]
    mqtt_host = data.get("mqtt_host", "").strip()
    mqtt_port = int(data.get("mqtt_port", 1883))
    mqtt_user = data.get("mqtt_user", "").strip()
    mqtt_password = data.get("mqtt_password", "")
    mqtt_client_id = data.get("mqtt_client_id", "bob").strip() or "bob"
    mqtt_enabled = bool(data.get("mqtt_enabled", True))
    ble_name = data.get("ble_name", "Bob-Setup-BLE").strip() or "Bob-Setup-BLE"
    ble_address = data.get("ble_address")
    timeout = int(data.get("timeout", 20))

    if len(ssid) == 0 or len(ssid) > 63 or len(password) > 63:
        raise HomeAssistantError("Invalid WiFi credentials length")
    if len(mqtt_host) > 80 or len(mqtt_user) > 63 or len(mqtt_password) > 63 or len(mqtt_client_id) > 40:
        raise HomeAssistantError("Invalid MQTT values length")

    device = None
    if ble_address:
        device = await BleakScanner.find_device_by_address(ble_address, timeout=timeout)
    if device is None:
        device = await BleakScanner.find_device_by_filter(
            lambda d, _ad: d.name == ble_name,
            timeout=timeout,
        )
    if device is None:
        raise HomeAssistantError(f"Could not find Bob over BLE (name '{ble_name}')")

    async with BleakClient(device, timeout=timeout) as client:
        await client.write_gatt_char(BLE_PROV_SSID_UUID, ssid.encode("utf-8"), response=True)
        await client.write_gatt_char(BLE_PROV_PASS_UUID, password.encode("utf-8"), response=True)
        await client.write_gatt_char(BLE_PROV_MQTT_HOST_UUID, mqtt_host.encode("utf-8"), response=True)
        await client.write_gatt_char(BLE_PROV_MQTT_PORT_UUID, str(mqtt_port).encode("ascii"), response=True)
        await client.write_gatt_char(BLE_PROV_MQTT_USER_UUID, mqtt_user.encode("utf-8"), response=True)
        await client.write_gatt_char(BLE_PROV_MQTT_PASS_UUID, mqtt_password.encode("utf-8"), response=True)
        await client.write_gatt_char(BLE_PROV_MQTT_CID_UUID, mqtt_client_id.encode("utf-8"), response=True)
        await client.write_gatt_char(BLE_PROV_MQTT_EN_UUID, (b"1" if mqtt_enabled else b"0"), response=True)
        await client.write_gatt_char(BLE_PROV_APPLY_UUID, b"APPLY", response=True)

        try:
            await asyncio.sleep(0.35)
            status_raw = await client.read_gatt_char(BLE_PROV_STATUS_UUID)
            status = status_raw.decode("utf-8", errors="ignore").strip().upper()
            if status == "ERROR":
                raise HomeAssistantError("Bob rejected provisioning values")
        except HomeAssistantError:
            raise
        except Exception:
            pass

