# Cardputer ADV Ops Recipes

Use these recipes for repeatable Cardputer firmware/bridge work. They are intentionally explicit because most regressions came from mixing build trees, flashing without provisioning env, or applying OTA from the full runtime.

## Ground Rules

- Work from firmware branch `codex/cardputer-ota-baseline-0.2.53-clean` unless a newer clean branch replaces it.
- Use `/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh`; do not use bare `pio` unless provisioning env is already exported.
- Keep secrets out of commits, logs, and handoff docs.
- Do not commit `hosts/...` runtime snapshots unless the task is explicitly host-config work.
- Do not change OTA architecture while debugging voice, UI, or bridge logic.
- Keep docs minimal: `fresh-start`, `ota-clean-boot-baseline`, `server-routing-architecture`, and this runbook are the canonical docs.

## Repo Health Check

```bash
git -C /Users/s1z0v/kd-projects/Openclaw/cardputer-firmware status --short --branch
git -C /Users/s1z0v/kd-projects/Openclaw status --short --branch
git -C /Users/s1z0v/kd-projects/Homio/openclaw-config status --short --branch
```

Expected clean state:

- firmware branch tracks `fork/codex/cardputer-ota-baseline-0.2.53-clean`;
- OpenClaw bridge branch tracks `origin/codex/cardputer-bridge-deploy`;
- Homio/openclaw-config has no dirty host snapshots.

If host snapshots appear, stash them instead of committing:

```bash
git -C /Users/s1z0v/kd-projects/Openclaw stash push -m "local host snapshots before Cardputer work" -- hosts
git -C /Users/s1z0v/kd-projects/Homio/openclaw-config stash push -m "local host snapshots before Cardputer work" -- hosts
```

## Bump Version

Replace `0.2.53-dev` with the next dev version in both firmware version locations.

```bash
cd /Users/s1z0v/kd-projects/Openclaw/cardputer-firmware
OLD=0.2.53-dev
NEW=0.2.54-dev
python3 - <<'PY'
from pathlib import Path
old = "0.2.53-dev"
new = "0.2.54-dev"
paths = [
    Path("bots/CardputerADV/platformio.ini"),
    Path("bots/CardputerADV/src/main_parts/001_main.cpp.inc"),
]
for path in paths:
    text = path.read_text()
    if old not in text:
        raise SystemExit(f"{path}: {old} not found")
    path.write_text(text.replace(old, new, 1))
PY
git diff -- bots/CardputerADV/platformio.ini bots/CardputerADV/src/main_parts/001_main.cpp.inc
```

Do not bump version for doc-only changes unless publishing a new OTA candidate.

## Build Firmware

```bash
/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh build
```

Expected result:

- binary: `/Users/s1z0v/kd-projects/Openclaw/cardputer-firmware/bots/CardputerADV/.pio/build/cardputer_adv/firmware.bin`;
- flash usage must stay below the OTA slot limit;
- build must use real provisioning env through the wrapper.

## Cable Flash

Use cable flashing when OTA is broken or a partition/boot-path change needs a hard reset.

```bash
/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh upload
```

If upload cannot connect:

1. Hold `G0`.
2. Press `Reset`.
3. Release `G0`.
4. Re-run the upload command.

Do not flash from a bare PlatformIO command with empty Wi-Fi/bridge env. That is how `BT setup` regressions happen.

## Publish OTA Candidate

Build first, then publish the exact built binary:

```bash
cd /Users/s1z0v/kd-projects/Openclaw/cardputer-bridge
npm run publish:firmware -- \
  --config config.s1z0v.local.json \
  --bin /Users/s1z0v/kd-projects/Openclaw/cardputer-firmware/bots/CardputerADV/.pio/build/cardputer_adv/firmware.bin \
  --version 0.2.54-dev \
  --git "$(git -C /Users/s1z0v/kd-projects/Openclaw/cardputer-firmware rev-parse --short HEAD)"
```

If the binary was built from uncommitted code, either commit first or intentionally pass `<sha>-dirty` and say that explicitly in release notes. Prefer commit-first.

## OTA Smoke

Check manifest and binary hash without printing tokens:

```bash
TOKEN="$(python3 - <<'PY'
import json
with open('/Users/s1z0v/kd-projects/Openclaw/cardputer-bridge/config.s1z0v.local.json') as f:
    print(json.load(f)['devices'][0]['device_token'])
PY
)"
BRIDGE=https://bridge.ai.k-digital.pro
FW="$(curl -fsS -H "Authorization: Bearer $TOKEN" "$BRIDGE/api/cardputer/firmware/manifest")"
echo "$FW" | jq '{version, size, sha256, firmware_url, build_git_sha}'
FW_URL="$(echo "$FW" | jq -r '.firmware_url')"
FW_SHA="$(echo "$FW" | jq -r '.sha256')"
curl -fL -H "Authorization: Bearer $TOKEN" "$FW_URL" -o /tmp/cardputer-fw-smoke.bin
test "$(shasum -a 256 /tmp/cardputer-fw-smoke.bin | awk '{print $1}')" = "$FW_SHA"
```

On device:

- open OTA screen;
- apply candidate;
- expect download to complete, reboot to clean boot updater, staged flash, second reboot, and new version on boot.

The clean boot updater is mandatory. If the firmware tries to flash directly from full runtime, stop and fix that regression first.

## Bridge Code Deploy

For code changes in `/Users/s1z0v/kd-projects/Openclaw/cardputer-bridge`:

```bash
cd /Users/s1z0v/kd-projects/Openclaw
git status --short --branch
git add cardputer-bridge/src/server.mjs cardputer-bridge/scripts cardputer-bridge/package.json cardputer-bridge/README.md
git commit -m "Describe bridge change"
git push origin codex/cardputer-bridge-deploy
```

Deploy to live host only after review/build/smoke:

```bash
scp /Users/s1z0v/kd-projects/Openclaw/cardputer-bridge/src/server.mjs cnt-openclaw:/opt/openclaw-cardputer-bridge/src/server.mjs
scp -r /Users/s1z0v/kd-projects/Openclaw/cardputer-bridge/scripts cnt-openclaw:/opt/openclaw-cardputer-bridge/
scp /Users/s1z0v/kd-projects/Openclaw/cardputer-bridge/package.json cnt-openclaw:/opt/openclaw-cardputer-bridge/package.json
ssh cnt-openclaw 'systemctl restart openclaw-cardputer-bridge.service && systemctl status --no-pager openclaw-cardputer-bridge.service | sed -n "1,20p"'
```

Health:

```bash
curl -fsS https://bridge.ai.k-digital.pro/healthz | jq .
```

## Bridge Debug

For the current message/voice/realtime/OTA routing architecture, read:

- `bots/CardputerADV/docs/server-routing-architecture.md`

Use the bundled debug skill script:

```bash
python3 /Users/s1z0v/.codex/skills/openclaw-cardputer-bridge-debug/scripts/cardputer_bridge_tail.py --hours 2 --history 5 --journal
```

Read output in this order:

- manifest version and SHA;
- current/recent topics;
- recent histories;
- `voice_turns`;
- warnings.

For `voice transport failed` where no `voice_turns` appear, the failure is likely before the bridge accepts/logs the upload. Collect serial logs around the firmware HTTP/TLS connect and heap state.

## Standard Voice Path QA

Expected standard voice flow:

1. Device records audio.
2. Device sends raw PCM to `/api/device-audio-turn-raw`.
3. Bridge logs `[voice-raw] uploaded`.
4. Bridge transcribes.
5. Bridge routes text to OpenClaw/current topic.
6. Bridge returns text plus optional audio URL/body.
7. Device shows transcript/reply and plays audio.

Failure split:

- `connect failed`: check Wi-Fi state, bridge host/port, TLS heap, DNS, and bridge health.
- `body failed`: check upload chunking, content length, timeout, and Wi-Fi reconnect.
- `header failed`: check bridge response status, auth, and timeout.
- no bridge log: failure is on device/network before request reaches Node.
- bridge log exists but no reply: check OpenClaw upstream wait, STT, TTS, or topic route.

## Realtime Voice QA

Realtime exists but is not the stable main UX yet.

On device:

- press `Ctrl+R`;
- expected statuses: `Realtime connecting`, `Realtime ready`, `Realtime listening`, `Realtime thinking`, then audio/text response.

Bridge:

```bash
cd /Users/s1z0v/kd-projects/Openclaw/cardputer-bridge
CARDPUTER_DEVICE_TOKEN="$(python3 - <<'PY'
import json
with open('config.s1z0v.local.json') as f:
    print(json.load(f)['devices'][0]['device_token'])
PY
)" node scripts/smoke-realtime.mjs
```

If realtime works, decide whether to expose it as a separate “live voice” mode or merge it into pulse. Do not replace the standard voice path until realtime has repeatable hardware QA.

## Commit And Push Rules

Firmware:

```bash
cd /Users/s1z0v/kd-projects/Openclaw/cardputer-firmware
git status --short
git diff --check
/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh build
git add bots/CardputerADV
git commit -m "Short accurate firmware change"
git push fork HEAD
```

Bridge:

```bash
cd /Users/s1z0v/kd-projects/Openclaw
git status --short
git diff --check
git add cardputer-bridge
git commit -m "Short accurate bridge change"
git push origin codex/cardputer-bridge-deploy
```

Never use `git reset --hard` or `git checkout --` to clean unknown changes without first classifying them. For sensitive local snapshots, prefer stash with a descriptive message.

## Repo Cleanup Rules

Keep:

- `fresh-start-2026-05-02.md`;
- `ota-clean-boot-baseline.md`;
- `cardputer-ops-recipes.md`;
- source code and build config required to build the current firmware.

Remove or avoid reintroducing:

- stale handoff docs that encode obsolete tasks;
- 1x1 HTML UX review gallery;
- canceled M5PORKCHOP settings import docs;
- wake-word plans;
- host runtime snapshots in feature commits;
- generated `.pio`, temp binaries, macOS `._*` files, and `.DS_Store`.

## Closed Loop Release Recipe

For the future “dictate firmware request -> OTA candidate” loop:

1. User dictates request into the Cardputer development topic.
2. Bridge classifies it as firmware work and creates a Codex task.
3. Codex patches firmware/bridge in a clean worktree.
4. Codex builds with the wrapper.
5. Codex commits code and bumps version.
6. Codex publishes OTA candidate.
7. Bridge sends a compact “version ready” message to Cardputer.
8. User applies OTA.
9. Device uploads OTA result logs.
10. Codex marks the loop success/failure and either commits a stabilization patch or rolls forward.

Current MVP route, proven manually on 2026-05-02:

1. Cardputer voice/text request lands in the active OpenClaw bridge topic.
2. Codex reads the latest bridge context and firmware runbook.
3. Codex patches the firmware in `/Users/s1z0v/kd-projects/Openclaw/cardputer-firmware`.
4. Codex builds with `/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh build`.
5. Codex commits and pushes the firmware branch.
6. Codex publishes the built binary as the bridge OTA candidate.
7. Cardputer fetches the manifest from the Firmware OTA screen and applies the update.

Missing automation before this is a true OpenClaw skill:

- dedicated firmware-development topic or classifier route;
- bridge-side task envelope with repo, branch, runbook, requested change, and publish policy;
- Codex task launcher plus completion callback;
- device-visible “OTA candidate ready” notification and OTA result upload.
