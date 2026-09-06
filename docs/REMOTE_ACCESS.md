# AdminSensor Remote

Firmware reference: **`0.4.8-opt-dev`**.

AdminSensor Remote provides an outbound-only remote administration channel for ESP32 Davis Weather Gateway. The local Web UI is never published directly on the Internet and no router port-forward is required.

## Installer configuration

The installer/user configures **one value only**:

```text
URL portale: https://portal.example/adminsensor
```

The URL must use HTTPS. No installation-specific hostname is compiled into the public firmware.

Device ID, device token, enrollment state and WebSocket URL are managed automatically by the firmware and are not editable secrets in the Web UI.

## Device identity

At first initialization the firmware:

1. reads the ESP32 Wi-Fi STA MAC address;
2. builds a stable ID in the form `esp32-a1b2c3d4e5f6`;
3. generates 32 cryptographically random bytes with the ESP32 hardware RNG;
4. stores the token as a 64-character hexadecimal value in NVS namespace `remote`, key `token`;
5. reuses the same token after every reboot.

Disabling/re-enabling the portal preserves the token. A full flash/NVS erase is the operation that rotates the device identity secret.

The token is never returned by the Web API and is never printed in logs.

## HTTPS enrollment

When Wi-Fi is available and system time is valid for TLS verification, the gateway performs:

```text
POST <URL-portale>/api/device/enroll
Content-Type: application/json
```

Payload:

```json
{
  "device_id": "esp32-a1b2c3d4e5f6",
  "device_token": "<firmware-managed token>",
  "name": "davis-gateway",
  "model": "ESP32 Davis Weather Gateway",
  "firmware_version": "0.4.8-opt-dev"
}
```

Typical portal states:

```json
{"status":"pending"}
```

or:

```json
{
  "status":"approved",
  "websocket_url":"wss://portal.example/adminsensor/ws/device/esp32-a1b2c3d4e5f6"
}
```

While pending, the gateway repeats enrollment periodically without generating a new token.

## WebSocket tunnel

After approval the firmware opens the `wss://` URL returned by the portal and authenticates with:

```text
Authorization: Bearer <device token>
```

TLS certificate validation remains enabled. The connection automatically retries after interruption and WebSocket heartbeat/ping handling is enabled.

Runtime states exposed by the Web UI include:

- `OFF`
- `WAIT_NETWORK`
- `WAIT_TIME`
- `ENROLLING`
- `PENDING`
- `APPROVED`
- `CONNECTING`
- `ONLINE`
- `RECONNECT`
- `ERROR`
- `DENIED`

## Remote HTTP proxy

The portal can send:

```json
{
  "type":"http_request",
  "id":"123456",
  "method":"GET",
  "path":"/",
  "headers":{},
  "body_b64":""
}
```

The remote task forwards the request to the existing local Web server on TCP/80. The same dashboard and API are therefore used locally and remotely; there is no second administration UI.

The response is returned as:

```json
{
  "type":"http_response",
  "id":"123456",
  "status":200,
  "headers":{"content-type":"text/html"},
  "body_b64":"..."
}
```

The proxy accepts only local relative paths and only `GET`, `POST` and `HEAD`. Request/response body sizes are bounded to protect ESP32 memory.

## Remote firmware update protocol

Firmware `0.4.8-opt-dev` adds a dedicated OTA protocol on the **same authenticated WSS connection**. OTA data is not sent through the HTTP proxy, so normal HTTP request limits do not need to be increased and the stable dashboard tunnel remains unchanged.

The portal must use a stop-and-wait flow: send one command and wait for the matching `firmware_response` before sending the next command. Every command must contain a unique `id`.

### 1. Begin

```json
{
  "type":"firmware_begin",
  "id":"ota-001",
  "size":1048576,
  "sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
}
```

`size` is mandatory and must fit the inactive OTA slot. `sha256` is mandatory and must contain 64 hexadecimal characters.

### 2. Firmware chunks

```json
{
  "type":"firmware_chunk",
  "id":"ota-002",
  "sequence":0,
  "data_b64":"<base64 firmware bytes>"
}
```

Decoded chunks are limited to **8192 bytes**. Sequence numbers start at `0` and must be strictly consecutive. A 4096-byte chunk size is recommended to reduce transient heap use.

### 3. End and verify

```json
{
  "type":"firmware_end",
  "id":"ota-999"
}
```

The ESP32 verifies the received byte count and SHA-256 before finalizing the inactive OTA partition. Only a successful `Update.end()` schedules the reboot.

### 4. Abort

```json
{
  "type":"firmware_abort",
  "id":"ota-abort",
  "reason":"operator cancelled"
}
```

An in-progress remote OTA is also aborted automatically if the AdminSensor WebSocket disconnects, the remote configuration changes, a manual reconnect is requested, or network connectivity is lost.

### OTA response

Every OTA command receives:

```json
{
  "type":"firmware_response",
  "id":"ota-002",
  "stage":"chunk",
  "ok":true,
  "sequence":0,
  "status":{
    "firmware_version":"0.4.8-opt-dev",
    "in_progress":true,
    "source":"remote",
    "received":4096,
    "expected":1048576,
    "percent":0.4
  }
}
```

On failure, `ok` is `false` and `message` contains the reason. The OTA engine rejects out-of-order chunks, oversized data, invalid ESP32 images, incomplete images and SHA-256 mismatches.

While an OTA update is in progress, new tunneled HTTP requests are rejected with HTTP status `423` to preserve heap and transport responsiveness. Native WebSocket ping/pong remains active.

## Ping / reconnect

JSON `ping` messages receive `pong`; native WebSocket ping/pong is also tracked. The WebSocket library performs reconnect attempts and the AdminSensor task periodically rechecks enrollment when the connection is not online.

## TLS trust

The public firmware contains generic public CA trust anchors, not an installation hostname or portal credential. The configured portal URL must present a certificate chain accepted by the embedded trust anchors.

A future release may provide a managed CA-bundle update mechanism if deployments require additional public/private PKI roots.

## Web API

- `GET /api/remote/config` — returns portal URL, generated Device ID and only a boolean indicating that the token exists;
- `POST /api/remote/config` — saves only the generic `url` field;
- `GET /api/remote/status` — enrollment/tunnel status and counters;
- `POST /api/remote/retry` — forces a new enrollment/reconnect attempt;
- `POST /api/remote/reset` — disables the portal while preserving the device token;
- `GET /api/firmware/status` — reports OTA slots, progress and last result.

## Security notes

AdminSensor is outbound-only and does not open an inbound Internet listener. Portal/operator authentication, authorization, session management, rate limiting and audit logging remain responsibilities of the server-side AdminSensor service.

The ESP32 NVS token is persistent but standard NVS storage is not equivalent to hardware-protected secret storage. Installations requiring resistance to physical extraction should additionally evaluate ESP32 Secure Boot and Flash Encryption.

The Davis RF receive/decoder path is independent from AdminSensor Remote and is not modified by the remote task.
