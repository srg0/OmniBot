# Persona templates (committed)

These `.md` files are the **canonical defaults** for each bot’s persona. The hub loads them from disk at startup (`persona._read_persona_default` in [`persona.py`](../persona.py)).

- **In Git:** Edit these files when you want to change the default SOUL, USER, BOOTSTRAP ritual, etc., for new installs and for **Reset to defaults** in **Pixel bot** settings (and as the baseline before **Give me a soul** copies **BOOTSTRAP** into the runtime folder).
- **On disk at runtime:** The hub ensures `DATA_DIR/persona/<device_id>/` exists and **seeds any missing** markdown from here (by default `app/backend/persona/<device_id>/` when `OMNIBOT_DATA_DIR` is unset). That runtime tree is **gitignored** so local bootstrap runs, daily logs, and customized markdown are not pushed.

## Files in this folder

| File | Purpose |
|------|---------|
| **AGENTS.md** | Default behavior guide injected for the model (humans often edit per bot in the dashboard). |
| **SOUL.md** | Default personality and boundaries (text behavior). |
| **IDENTITY.md** | Default bot identity scaffold. |
| **USER.md** | Default human profile scaffold. |
| **TOOLS.md** | Documents tools for model context. |
| **MEMORY.md** | Starting long-term memory (usually short). |
| **HEARTBEAT.md** | Default instructions for periodic heartbeat maintenance (`memory_replace` only in that pass). |
| **BOOTSTRAP.md** | Template for the **Give me a soul** ritual; copied to the bot folder, then deleted when the model calls **`bootstrap_complete`**. |

The hub also creates **`logs/daily/`** under each bot’s persona directory for **`daily_log_append`** (not templated as a single file; logs are created per UTC day).

End-user overview: [root README](../../../README.md) (section **Persona framework**).

`BOOTSTRAP.md` here is the template used when you click **Give me a soul** in **Hub settings**; the hub writes it into the runtime persona folder and removes it after **`bootstrap_complete`**.

If you previously committed files under `app/backend/persona/`, stop tracking them once:

```bash
git rm -r --cached app/backend/persona
```

Then commit the change with `.gitignore` so only `persona_defaults/` stays in the repo.
