import os
import asyncio
import glob
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
import subprocess

app = FastAPI()

# Make sure we're looking in the right parent directory
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

@app.get("/api/files")
def get_files():
    # Find all .bin files in the main directory
    files = glob.glob(os.path.join(BASE_DIR, "*.bin"))
    return [os.path.basename(f) for f in files]

class RunRequest(BaseModel):
    file: str
    mode: str = "saw"
    window: int = 16
    timeout: int = 200
    loss: float = 0.0
    dup: float = 0.0
    corrupt: float = 0.0
    delay: int = 0
    cc: bool = False
    adaptive: bool = False

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    
    try:
        while True:
            data = await websocket.receive_json()
            if data.get("action") == "start":
                req = data.get("params", {})
                
                # Extract params
                filename = req.get("file", "test_1mb.bin")
                mode = req.get("mode", "saw")
                window = int(req.get("window", 16))
                timeout = int(req.get("timeout", 200))
                loss = float(req.get("loss", 0.0))
                dup = float(req.get("dup", 0.0))
                corrupt = float(req.get("corrupt", 0.0))
                delay = int(req.get("delay", 0))
                cc = bool(req.get("cc", False))
                adaptive = bool(req.get("adaptive", False))
                
                filepath = os.path.join(BASE_DIR, filename)
                output_dir = os.path.join(BASE_DIR, "test_output")
                os.makedirs(output_dir, exist_ok=True)
                
                port = 5005
                
                receiver_cmd = [
                    os.path.join(BASE_DIR, "receiver.exe"),
                    str(port),
                    output_dir,
                    "--loss", str(loss),
                    "--dup", str(dup),
                    "--corrupt", str(corrupt),
                    "--delay", str(delay),
                    "--verbose"
                ]
                
                sender_cmd = [
                    os.path.join(BASE_DIR, "sender.exe"),
                    "127.0.0.1",
                    str(port),
                    filepath,
                    "--mode", mode,
                    "--window", str(window),
                    "--timeout", str(timeout),
                    "--loss", str(loss),
                    "--dup", str(dup),
                    "--corrupt", str(corrupt),
                    "--delay", str(delay),
                    "--verbose"
                ]
                
                if cc:
                    sender_cmd.append("--cc")
                if adaptive:
                    sender_cmd.append("--adaptive")
                    
                await websocket.send_json({"type": "status", "message": "Starting receiver..."})
                
                # Start receiver
                receiver_process = await asyncio.create_subprocess_exec(
                    *receiver_cmd,
                    stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.PIPE,
                    cwd=BASE_DIR
                )
                
                await asyncio.sleep(0.5)
                await websocket.send_json({"type": "status", "message": f"Starting sender with file {filename}..."})
                
                # Start sender
                sender_process = await asyncio.create_subprocess_exec(
                    *sender_cmd,
                    stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.PIPE,
                    cwd=BASE_DIR
                )
                
                async def read_stream(stream, source, std_type):
                    while True:
                        line = await stream.readline()
                        if not line:
                            break
                        line_str = line.decode('utf-8', errors='replace').strip()
                        if line_str:
                            await websocket.send_json({
                                "type": "log",
                                "source": source,
                                "stream": std_type,
                                "message": line_str
                            })
                
                # Run all streams concurrently
                await asyncio.gather(
                    read_stream(receiver_process.stdout, "receiver", "stdout"),
                    read_stream(receiver_process.stderr, "receiver", "stderr"),
                    read_stream(sender_process.stdout, "sender", "stdout"),
                    read_stream(sender_process.stderr, "sender", "stderr"),
                )
                
                sender_code = await sender_process.wait()
                # Kill receiver if still running
                try:
                    receiver_process.terminate()
                except Exception:
                    pass
                receiver_code = await receiver_process.wait()
                
                await websocket.send_json({
                    "type": "status",
                    "message": f"Transfer complete. Sender code: {sender_code}, Receiver code: {receiver_code}"
                })
                await websocket.send_json({"type": "done"})
                
    except WebSocketDisconnect:
        print("Client disconnected")

@app.get("/")
def index():
    html_path = os.path.join(os.path.dirname(__file__), "index.html")
    with open(html_path, "r", encoding="utf-8") as f:
        return HTMLResponse(f.read())
