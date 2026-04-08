# Custom wake word: “pixel”

The hub loads **`pixel.onnx`** from this folder automatically (see [`wake_listen.py`](../../wake_listen.py)). If the file is missing, it falls back to the pretrained **hey_jarvis** model.

## Quick setup

1. Train or export an **openWakeWord-compatible** `.onnx` model for the phrase **“pixel”** (see links below).
2. Save it **exactly** as:
   - **`pixel.onnx`** in **this directory**  
   **or**
   - Any path, and set env var **`OMNIBOT_WAKE_WORD_MODEL`** to that full path (Windows example: `C:\Users\you\models\pixel.onnx`).
3. Restart the OmniBot hub so it reloads the model.
4. If detection is flaky, tune (env vars, optional):
   - **`OMNIBOT_WAKE_THRESHOLD`** — default `0.55`; try **`0.35`–`0.5`** for a new model.
   - **`OMNIBOT_WAKE_SILENCE_MS`** — how much silence ends your command (default `550`).

## How to get `pixel.onnx`

Training is **not** part of this repo; you generate the file using the **openWakeWord** ecosystem:

| Resource | Notes |
|----------|--------|
| [dscripka/openWakeWord](https://github.com/dscripka/openWakeWord) | Upstream project; see **`notebooks/training_models.ipynb`** for the official training flow (synthetic data, negatives, export). |
| [openwakeword-trainer](https://github.com/lgpearson1771/openwakeword-trainer) | Community pipeline (Linux/WSL + GPU typical); produces small ONNX models. |
| [easy-oww](https://github.com/pjdoland/easy-oww) | CLI-oriented workflow to go from recordings/TTS to ONNX. |

Requirements are usually: **16 kHz mono PCM** training data, phrase **“pixel”**, and an export step that produces an ONNX compatible with openWakeWord’s **embedding + classifier** layout (follow the notebook or tool docs).

## Verify

When the hub first creates the wake listener, the log line includes the model label (e.g. `pixel` vs `hey_jarvis`). Say **“pixel”**, pause, then your question; the hub uses **WebRTC VAD** to detect end of speech after the wake.

## Git

`pixel.onnx` is usually **not** committed (binary, user-specific). Add an exception in `.gitignore` if you keep trained files only locally.
