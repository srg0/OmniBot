# Security

## Reporting issues

If you discover a security vulnerability, please open a **private** advisory or contact the maintainers with enough detail to reproduce the issue. Do not post exploit details in public issues before a fix is available.

## Hub exposure

The FastAPI hub is intended for **trusted local networks**. By default it listens on all interfaces (`0.0.0.0:8000`). Anyone who can reach the hub can:

- Use BLE provisioning and bot APIs.
- **Read or set API keys** via `GET`/`POST /api/hub/settings` unless you protect the deployment.

For production or shared networks:

- Bind to **localhost** only, or
- Put the hub behind a **reverse proxy** with authentication, or
- Inject secrets with **environment variables** (they override file-based secrets) and restrict network access with firewalls.

Secrets on disk live under the hub data directory as `hub_secrets.json` (see `OMNIBOT_DATA_DIR` in the README). Clock and Maps location live in `hub_app_settings.json` (not secret, but sensitive). Restrict file permissions on the host.

## Before making the repository public

- **Never commit** `.env`, `hub_secrets.json`, `hub_app_settings.json`, `bot_settings.json`, `known_bots.json`, or other files under `.gitignore` in `app/backend/`. They can contain API keys, addresses, or device history.
- **Rotate** any API key that has ever appeared in a commit, a screenshot, or a shared branch — Git history keeps old blobs until rewritten.
- **Pixel firmware:** replace `backend_ip` in `bots/Pixel/src/main.cpp` with a placeholder LAN address before publishing; use your real IP only locally.
- Prefer **`bot_settings.json.example`** (empty `{}`) as documentation; the hub creates `bot_settings.json` at runtime when needed.

## Maps and third-party keys

The Google Maps JavaScript API key is exposed to the browser by design for the Maps widget. **Gemini** and other server keys must never be shipped in frontend bundles; configure them via environment variables or Hub settings (stored server-side).
