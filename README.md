# Libpeer Demo

A minimal WebRTC DataChannel example for the ESP32-S3 using the open-source **libpeer** library. This demo connects the board to Wi-Fi, negotiates a PeerConnection over a public STUN server, and sends a `"pong"` message once per second over a reliable data-channel.

## Key Features

* **Wi-Fi Station Mode**: Connects to your AP via configurable SSID/password (via `menuconfig`).
* **WebRTC Stack**: Uses libpeer to handle ICE (Google STUN), DTLS-SRTP, and SCTP.
* **DataChannel Send Loop**: Once the channel is open, sends `"pong"` every second in its own FreeRTOS task.
* **Modular Components**:

## Components Overview

This project is organized into three main components, each handling a distinct layer of the WebRTC demo:

### 1. `wifi-manager` (Wi-Fi Manager)
- **Purpose**: Bootstraps the ESP32-S3’s network interface in Station mode and manages the connection to your home/office AP.
- **How it works**:
  1. Initializes NVS and the TCP/IP stack.
  2. Creates the default Wi-Fi STA network interface.
  3. Registers event handlers to automatically reconnect on disconnect and to log your assigned IP address.
  4. Exposes a single `wifi_init(ssid, password)` function you call from `app_main()`.
- **Why use it**: Encapsulates all the boilerplate (NVS, `esp_netif`, `esp_event`, `esp_wifi`) into a clean helper that you can drop into any ESP-IDF project.

### 2. `peer-manager` (PeerConnection Manager)
- **Purpose**: Orchestrates the WebRTC PeerConnection lifecycle, from SDP offer creation through ICE candidate exchange to DataChannel messaging.
- **Key responsibilities**:
  - **`peer_init()`**: Brings up the libpeer stack.
  - **Offer generation & fingerprint logging**: Creates an SDP offer, extracts and logs the DTLS-SRTP fingerprint so you can paste it into your browser’s RTCPeerConnection config.
  - **ICE‐candidate helpers**: Cleans and formats `a=candidate:` lines into JSON objects complete with `sdpMid`/`sdpMLineIndex`, ready for Chrome’s `addIceCandidate()`.
  - **Signaling loop**: Runs in its own FreeRTOS task, calling `peer_signaling_connect()` and `peer_signaling_loop()` to exchange SDP and ICE via your chosen signaling endpoint.
  - **Connection loop**: In a separate task, calls `peer_connection_loop()` to drive ICE checks, DTLS handshake, and SCTP negotiation.
  - **DataChannel sender**: Starts a `send_task()` that waits for `onopen` and then dispatches `"hello"` every second with `peer_connection_datachannel_send()`.
- **Why use it**: Centralizes all WebRTC operations—initialization, signaling, SDP/ICE handling, and messaging—into one reusable component so your `app_main()` stays concise.

### 3. `libpeer` (Core WebRTC Engine)
- **Purpose**: Provides the actual implementation of WebRTC protocols (ICE, DTLS-SRTP, SCTP) in a lightweight C library suitable for embedded devices.
- **Features**:
  - **ICE agent** with STUN support (RFC 5389)
  - **DTLS-SRTP** negotiation (RFC 5764) for encrypted media
  - **SCTP over DTLS** for DataChannels (RFC 8831)
- **Integration**:
  - Exposed via a clean C API (`peer_init`, `peer_connection_create`, callbacks, and send/receive functions).
  - Bundled as an ESP-IDF component under `components/libpeer/`.
- **Why use it**: Eliminates the need for massive browser engines or platform‐specific WebRTC stacks—libpeer gives you just the network and security layers you need to build P2P IoT applications.

By layering your application this way—Wi-Fi management separate from signaling/connection logic and both built on a rock-solid core engine—you get maximum clarity, reusability, and ease of customization. Happy hacking!


## Prerequisites

* **ESP-IDF v5.4** (or higher) installed and sourced (`IDF_PATH` set).
* **Python 3** (use the ESP-IDF virtual environment).
* ESP32-S3 "Goiaba" board (or any ESP32-S3) connected via USB.

## Project Structure

```text
.
├──libpeer-demo
│   ├── CMakeLists.txt        ← Top-level CMake project file
│   ├── sdkconfig.defaults    ← Default CONFIG_WIFI_CONNECT_* & SIGNALING_* values
│   ├── README.md             ← This file
│   ├── main/
│      ├── includes/
│      ├── main.c             ← app_main() calling wifi_init() and peer_manager_init()
│      ├── wifi-manager.c     ← Wi-Fi manager
│      └── peer-manager.c     ← PeerConnection manager
└── components/
   └── libpeer/               ← WebRTC engine component (from Git)
```

## Configuration

Run `idf.py menuconfig` and set:

- **Wi-Fi credentials**
  **Component config → Wi-Fi credentials**
  - `CONFIG_WIFI_CONNECT_SSID` — your network’s SSID
  - `CONFIG_WIFI_CONNECT_PASSWORD` — your network’s password

- **Signaling server URL / token**
  **Component config → Peer-manager configuration**
  - `CONFIG_SIGNALING_URL` — full URL of your signaling endpoint (e.g. `https://goiaba-io.github.io/goiaba/peer-demo/?id=your-room-id`)
  - `CONFIG_SIGNALING_TOKEN` — optional auth token

- **DTLS-SRTP support**
  **Component config → mbedTLS**
  - **Enable support for negotiation of DTLS-SRTP (RFC 5764)** (`CONFIG_MBEDTLS_SSL_DTLS_SRTP`)

Defaults in `sdkconfig.defaults`:

```ini
CONFIG_WIFI_CONNECT_SSID="your_ssid"
CONFIG_WIFI_CONNECT_PASSWORD="your_password"
CONFIG_SIGNALING_URL="mqtts://libpeer.com/public/striped-lazy-eagle"
CONFIG_SIGNALING_TOKEN=""
```

## Quick Start

```bash
# 1. Configure your board and enter your SSID/Password & Signaling URL
idf.py menuconfig

# 2. Set target and build
idf.py set-target esp32s3
idf.py build

# 3. Flash and monitor
idf.py flash monitor
```

To establish a peer-to-peer connection, open the demo page at

```
https://goiaba-io.github.io/goiaba/peer-demo/
```

and let it generate (or manually enter) a unique device ID. Once you click “Connect,” the full URL—including your chosen ID will appear (for example, `mqtts://libpeer.com/public/striped-lazy-eagle`). Past this SIGNALING_URL in Goiaba `libpeer-demo` example.

After boot, you’ll see:

```
I (###) peer-manager: Datachannel opened
I (###) peer-manager: Starting send_task – sending "pong" every second
```

On your browser (via manual signaling or the libpeer demo page), you should then receive `"<<< pong"` messages every second. And you can send any string to the device.

Press **Ctrl+]** to exit the serial monitor.

## License

MIT © 2025
