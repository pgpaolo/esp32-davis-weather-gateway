# AdminSensor Remote

Firmware reference: **`0.4.1-dev`**.

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
  "firmware_version": "0.4.1-dev"
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
- `POST /api/remote/reset` — disables the portal while preserving the device token.

## Security notes

AdminSensor is outbound-only and does not open an inbound Internet listener. Portal/operator authentication, authorization, session management, rate limiting and audit logging remain responsibilities of the server-side AdminSensor service.

The ESP32 NVS token is persistent but standard NVS storage is not equivalent to hardware-protected secret storage. Installations requiring resistance to physical extraction should additionally evaluate ESP32 Secure Boot and Flash Encryption.

The Davis RF receive/decoder path is independent from AdminSensor Remote and is not modified by the remote task.
