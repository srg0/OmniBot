import io
import os
import uvicorn
import tempfile
import base64
import cv2
import numpy as np
import imageio
import asyncio # Added for non-blocking operations
from fastapi import FastAPI, UploadFile, File, Form, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from PIL import Image
from dotenv import load_dotenv
from google import genai
from google.genai import types
import json
from pydantic import BaseModel
from bleak import BleakScanner, BleakClient
import subprocess
import uuid

# ==========================================
#          LOAD SECRETS & SETUP
# ==========================================
load_dotenv()

API_KEY = os.getenv("GEMINI_API_KEY")

# Settings Configuration
SETTINGS_FILE = "bot_settings.json"
DEFAULT_MODEL = "gemini-3.1-flash-lite-preview"
DEFAULT_SYSTEM_INSTRUCTION = "You are Pixel, a friendly and intelligent small desktop robot.\n\nContext:\n- If I provide audio, I am speaking directly to you through your microphone.\n- If I provide text, I am typing to you from the OmniBot Hub dashboard.\n- If I provide a video, it is a real-time recording from your onboard camera.\n\nRules:\n- Keep your answers concise and conversational, as if we are speaking face-to-face.\n- Only discuss or analyze the video content if it is relevant to my current prompt or question.\n- Do not greet me in every message.\n- You are helpful, slightly curious, and highly efficient."

if not API_KEY:
    print("ERROR: API Key not found in .env file!")
    exit()

def get_all_settings():
    if not os.path.exists(SETTINGS_FILE):
        return {}
    try:
        with open(SETTINGS_FILE, 'r') as f:
            return json.load(f)
    except Exception as e:
        print(f"Error loading settings: {e}")
        return {}

def save_all_settings(settings):
    try:
        with open(SETTINGS_FILE, 'w') as f:
            json.dump(settings, f, indent=4)
    except Exception as e:
        print(f"Error saving settings: {e}")

def get_bot_settings(device_id: str):
    settings = get_all_settings()
    bot_conf = settings.get(device_id, {})
    return {
        "model": bot_conf.get("model", DEFAULT_MODEL),
        "system_instruction": bot_conf.get("system_instruction", DEFAULT_SYSTEM_INSTRUCTION)
    }

def assemble_video(jpeg_frames: list[bytes], fps: int = 10) -> bytes:
    """Takes a list of JPEG byte arrays and assembles them into an MP4 (H.264) video in memory."""
    if not jpeg_frames:
        return b""
        
    tmp_path = os.path.join(tempfile.gettempdir(), "pixel_clip.mp4")
    
    # Write frames using imageio's ffmpeg plugin with libx264 codec 
    # This guarantees cross-browser HTML5 <video> compatibility
    with imageio.get_writer(tmp_path, fps=fps, codec='libx264', format='FFMPEG') as writer:
        for jpg_bytes in jpeg_frames:
            frame = cv2.imdecode(np.frombuffer(jpg_bytes, np.uint8), cv2.IMREAD_COLOR)
            if frame is not None:
                # cv2 imdecode returns BGR, imageio expects RGB
                frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                writer.append_data(frame_rgb)
    
    with open(tmp_path, 'rb') as f:
        video_bytes = f.read()
    os.remove(tmp_path)
    
    return video_bytes

def create_wav_header(data_size: int, sample_rate: int = 16000, num_channels: int = 1, bits_per_sample: int = 16) -> bytes:
    """Creates a 44-byte standard RIFF WAV header for raw PCM audio."""
    byte_rate = sample_rate * num_channels * (bits_per_sample // 8)
    block_align = num_channels * (bits_per_sample // 8)
    chunk_size = data_size + 36

    header = bytearray()
    header.extend(b'RIFF')
    header.extend(chunk_size.to_bytes(4, byteorder='little'))
    header.extend(b'WAVE')
    header.extend(b'fmt ')
    header.extend((16).to_bytes(4, byteorder='little')) # Subchunk1Size (16 for PCM)
    header.extend((1).to_bytes(2, byteorder='little'))  # AudioFormat (1 for PCM)
    header.extend(num_channels.to_bytes(2, byteorder='little'))
    header.extend(sample_rate.to_bytes(4, byteorder='little'))
    header.extend(byte_rate.to_bytes(4, byteorder='little'))
    header.extend(block_align.to_bytes(2, byteorder='little'))
    header.extend(bits_per_sample.to_bytes(2, byteorder='little'))
    header.extend(b'data')
    header.extend(data_size.to_bytes(4, byteorder='little'))
    return bytes(header)

client = genai.Client(api_key=API_KEY)
app = FastAPI(title="ESP32 Gemini Brain Monitor")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ==========================================
#          WEBSOCKET MANAGER
# ==========================================
class ConnectionManager:
    def __init__(self):
        self.active_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)

    async def broadcast(self, message: dict):
        for connection in self.active_connections:
            try:
                await connection.send_text(json.dumps(message))
            except Exception as e:
                print(f"Failed to send to WS: {e}")

manager = ConnectionManager()

@app.websocket("/ws/monitor")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            # We don't expect the frontend to send us anything, but we keep the loop open
            await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)

class WifiCredentials(BaseModel):
    ssid: str
    password: str
    device_address: str

class BotSettingsSchema(BaseModel):
    model: str
    system_instruction: str

# Use a standard BLE UUID for our custom generic characteristic
WIFI_CREDS_CHAR_UUID = "12345678-1234-5678-1234-56789abcdef0"

# ==========================================
#          API ENDPOINTS (ESP32 SETUP)
# ==========================================
@app.get("/setup/scan")
async def setup_scan():
    """Scans for nearby BLE devices and returns those that might be the Desktop Bot."""
    print("BLE Scanning...")
    devices = await BleakScanner.discover(timeout=5.0)
    results = []
    
    for d in devices:
        # Look for a specific name prefix
        if d.name and "Pixel" in d.name:
            results.append({"name": d.name, "address": d.address})
            
    # For testing without the ESP32 code ready, return a fake device if none found
    if not results:
        results.append({"name": "DesktopBot_Setup (Simulated)", "address": "00:00:00:00:00:00"})
        
    return {"devices": results}

@app.post("/setup/provision")
async def setup_provision(creds: WifiCredentials):
    """Connects to the ESP32 via BLE and writes the WiFi SSID/Password."""
    payload = json.dumps({"ssid": creds.ssid, "password": creds.password}).encode('utf-8')
    print(f"Provisioning {creds.device_address} with SSID {creds.ssid}...")
    
    # Simulate success if using the fake device
    if creds.device_address == "00:00:00:00:00:00":
        return {"status": "success", "message": "Simulated provisioning complete."}
        
    try:
        async with BleakClient(creds.device_address) as client:
            await client.write_gatt_char(WIFI_CREDS_CHAR_UUID, payload)
        return {"status": "success", "message": "Credentials sent successfully."}
    except Exception as e:
        error_msg = str(e)
        print(f"BLE Provisioning Error: {error_msg}")
        raise HTTPException(status_code=500, detail=error_msg)

@app.get("/setup/wifi-networks")
async def setup_wifi_networks():
    """Scans for nearby Wi-Fi networks using the host OS (Windows)."""
    try:
        out = subprocess.run(["netsh", "wlan", "show", "networks"], capture_output=True, text=True).stdout
        networks = []
        for line in out.split('\n'):
            if 'SSID' in line and ':' in line:
                ssid = line.split(':', 1)[1].strip()
                if ssid and ssid not in networks:
                    networks.append(ssid)
        return {"networks": networks}
    except Exception as e:
        print(f"Wi-Fi Scan Error: {e}")
        return {"networks": []}

# ==========================================
#          API ENDPOINTS (SETTINGS)
# ==========================================
@app.get("/api/settings/{device_id}")
async def get_settings(device_id: str):
    """Gets the current settings for a specific bot."""
    return get_bot_settings(device_id)

@app.post("/api/settings/{device_id}")
async def update_settings(device_id: str, new_settings: BotSettingsSchema):
    """Updates the settings for a specific bot."""
    settings = get_all_settings()
    settings[device_id] = {
        "model": new_settings.model,
        "system_instruction": new_settings.system_instruction
    }
    save_all_settings(settings)
    return {"status": "success", "settings": settings[device_id]}

# ==========================================
#          ESP32 WEBSOCKET STREAMING
# ==========================================
# Store active streaming sessions by ESP32 WebSocket connection
active_streams = {}

@app.websocket("/ws/stream")
async def esp32_stream_endpoint(websocket: WebSocket):
    await websocket.accept()
    print("ESP32 connected to streaming endpoint!")
    
    # Initialize session buffers
    active_streams[websocket] = {
        "audio_buffer": bytearray(),
        "video_frames": [],
        "device_id": "default_bot" # We'll just use default for now
    }
    
    try:
        while True:
            # Receive binary packet from ESP32
            data = await websocket.receive_bytes()
            if not data:
                continue
                
            packet_type = data[0]
            payload = data[1:]
            
            session = active_streams[websocket]
            
            if packet_type == 0x01:
                # Audio chunk
                session["audio_buffer"].extend(payload)
                
            elif packet_type == 0x02:
                # Video frame (JPEG)
                session["video_frames"].append(payload)
                
            elif packet_type == 0x03:
                # Stop Recording & Process Command
                print(f"\n--- STOP RECORDING RECEIVED ---")
                print(f"Collected {len(session['audio_buffer'])} bytes of audio and {len(session['video_frames'])} video frames.")
                
                # Broadcast to UI that processing started
                await manager.broadcast({
                    "type": "processing_started",
                    "data": "Stream finished. Processing via Gemini..."
                })
                
                try:
                    # 1. Create Audio Bytes with WAV Header
                    raw_audio = bytes(session["audio_buffer"])
                    wav_header = create_wav_header(len(raw_audio), sample_rate=16000)
                    audio_bytes = wav_header + raw_audio
                    
                    # 2. Assemble Video Bytes (OFFLOADED TO BACKGROUND THREAD)
                    print("Assembling video in background...", end=" ", flush=True)
                    video_bytes = await asyncio.to_thread(assemble_video, session["video_frames"], 10)
                    print(f"Done! ({len(video_bytes)} bytes)")
                    
                    # 3. Broadcast Video to React UI
                    video_b64 = base64.b64encode(video_bytes).decode('utf-8')
                    video_data_url = f"data:video/mp4;base64,{video_b64}"
                    await manager.broadcast({
                        "type": "video_captured",
                        "data": video_data_url
                    })
                    
                    # Broadcast Audio to React UI
                    audio_b64 = base64.b64encode(audio_bytes).decode('utf-8')
                    audio_data_url = f"data:audio/wav;base64,{audio_b64}"
                    await manager.broadcast({
                        "type": "audio_captured",
                        "data": audio_data_url
                    })
                    
                    # 4. Call Gemini (USING THE ASYNC CLIENT)
                    print("Sending to Gemini...", end=" ", flush=True)
                    bot_settings = get_bot_settings(session["device_id"])

                    # Stream response to UI incrementally
                    stream_id = str(uuid.uuid4())
                    full_text = ""
                    await manager.broadcast({
                        "type": "ai_response_stream_start",
                        "stream_id": stream_id
                    })

                    stream = await client.aio.models.generate_content_stream(
                        model=bot_settings["model"],
                        contents=[
                            "Listen to the audio and watch the video. Answer the user's question.",
                            types.Part.from_bytes(data=audio_bytes, mime_type="audio/wav"),
                            types.Part.from_bytes(data=video_bytes, mime_type="video/mp4"),
                        ],
                        config=types.GenerateContentConfig(
                            system_instruction=bot_settings["system_instruction"],
                            thinking_config=types.ThinkingConfig(thinking_level="low")
                        )
                    )

                    async for chunk in stream:
                        chunk_text = (chunk.text or "")
                        if not chunk_text:
                            continue

                        # Some SDKs yield cumulative text; others yield deltas.
                        # We try to derive a delta safely.
                        if chunk_text.startswith(full_text):
                            delta = chunk_text[len(full_text):]
                        else:
                            delta = chunk_text

                        if delta:
                            full_text += delta
                            await manager.broadcast({
                                "type": "ai_response_stream_delta",
                                "stream_id": stream_id,
                                "data": delta
                            })

                    print(f"\n>>> GEMINI SAYS: {full_text}")

                    await manager.broadcast({
                        "type": "ai_response_stream_end",
                        "stream_id": stream_id,
                        "data": full_text
                    })

                    # Send Response back to ESP32 via same WebSocket
                    await websocket.send_text(json.dumps({
                        "status": "success",
                        "reply": full_text
                    }))
                    
                except Exception as e:
                    print(f"\nAPI Error: {e}")
                    error_msg = str(e)
                    await manager.broadcast({"type": "error", "data": error_msg})
                    await websocket.send_text(json.dumps({"status": "error", "message": error_msg}))
                
                # Clear buffers for next interaction
                session["audio_buffer"].clear()
                session["video_frames"].clear()
                
    except WebSocketDisconnect:
        print("ESP32 disconnected from streaming endpoint.")
        if websocket in active_streams:
            del active_streams[websocket]
    except Exception as e:
        print(f"Stream error: {e}")
        if websocket in active_streams:
            del active_streams[websocket]

@app.get("/ping")
async def ping():
    """A simple health check. Using this also notifies the UI that ESP32 is online."""
    await manager.broadcast({
        "type": "esp32_connected",
        "data": "ESP32 is online"
    })
    return {"status": "ready"}

# ==========================================
#          MAIN RUNNER
# ==========================================
if __name__ == "__main__":
    print(f"--- AUDIO/VISUAL BRAIN SERVER STARTED ---")
    print("Listening on http://0.0.0.0:8000")
    print("Waiting for ESP32 to connect to /ws/stream ...")
    uvicorn.run(app, host="0.0.0.0", port=8000)