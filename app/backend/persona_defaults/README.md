# Persona templates (committed)

These `.md` files are the **canonical defaults** for each bot’s persona. The hub loads them from disk at startup (`persona._read_persona_default`).

- **In Git:** Edit these files when you want to change the default SOUL, USER, BOOTSTRAP ritual, etc., for new installs and for **Reset to defaults** in Bot Settings.
- **On disk at runtime:** The hub copies them into `DATA_DIR/persona/<device_id>/` (by default `app/backend/persona/<device_id>/` when `OMNIBOT_DATA_DIR` is unset). That folder is **gitignored** so local bootstrap runs, daily logs, and customized markdown are not pushed.

`BOOTSTRAP.md` here is the template used when you click **Give me a soul**; the hub writes it into the runtime persona folder and removes it after `bootstrap_complete`.

If you previously committed files under `app/backend/persona/`, stop tracking them once:

```bash
git rm -r --cached app/backend/persona
```

Then commit the change with `.gitignore` so only `persona_defaults/` stays in the repo.
