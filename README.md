# Reliable UDP Networking Project

This project implements a Reliable Data Transfer (RDT) protocol over UDP in C++. It features different modes of reliability and congestion control, and includes a modern web-based dashboard for real-time visualization and configuration of network scenarios.

## Features
- **Protocols:** Stop-and-Wait (SAW), Go-Back-N (GBN), and Selective Repeat (SR).
- **Network Simulation:** Built-in fault injection for testing packet loss, duplication, corruption, and delays.
- **Advanced Control:** Adaptive RTO (RFC 6298) and Congestion Control (TCP Reno).
- **Web Dashboard:** A real-time, interactive frontend built with FastAPI and Tailwind CSS to easily configure and observe transfers.

## Getting Started

### 1. The Web Dashboard (Recommended)
The easiest way to run tests and interact with the simulation is through the visual dashboard.

#### Prerequisites
- Python 3.10+
- The C++ executables (`sender.exe` and `receiver.exe`) must be present in the root directory.

#### Setup & Run
1. Open a terminal and navigate to the `frontend` directory:
   ```powershell
   cd frontend
   ```
2. Install the required Python packages:
   ```powershell
   pip install -r requirements.txt
   ```
3. Start the local web server:
   ```powershell
   python -m uvicorn main:app --host 127.0.0.1 --port 8000
   ```
4. Open your web browser and navigate to:
   **http://127.0.0.1:8000**

From the dashboard, you can select the `.bin` file to transfer, configure protocol parameters (window size, timeouts, and fault rates), and watch the real-time terminal logs and metrics as the transfer executes.

---

### 2. The Command-Line Interface
If you prefer running the simulation manually, you can use the core binaries directly.

**Start the Receiver:**
```powershell
.\receiver.exe <port> [output_dir] [options]
```

**Start the Sender:**
```powershell
.\sender.exe <host> <port> <file> [options]
```

Use the `--help` or run the binaries without arguments to see the full list of supported command-line options.
