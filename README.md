# Bob - GitHub Publish Bundle

This folder only contains what is needed to publish Bob on GitHub:

- `firmware/bob` (firmware)
- `custom_components/bob` (HACS custom integration)
- `hacs.json` (HACS metadata)
- `LICENSE`

## Quick Publish

1. Create a new empty GitHub repository.
2. Copy the contents of this `github/` folder into that repository.
3. Commit and push.
4. In HACS, add the repo as a Custom Repository (type: `Integration`).

## Important

- Set your real `BOB_HA_GITHUB_URL` in `firmware/bob/config.h`.
- Check WiFi/MQTT defaults in `config.h` for your environment.
