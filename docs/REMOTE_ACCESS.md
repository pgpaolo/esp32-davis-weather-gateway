# Remote Access Architecture

Firmware reference: **`0.4.0-dev`**.

Version 0.4 introduces the configuration and security foundation for future remote access. It deliberately does **not** expose the local Web server directly to the Internet and does not yet create an active remote tunnel.

## Security model

The intended architecture is **outbound-only** from the gateway:

```text
Browser / operator
       |
       v
Remote relay / portal
       ^
       | TLS / WSS outbound session
       |
ESP32 Davis Weather Gateway
       |
       +-- local Web UI on trusted LAN
```

The router/firewall should not need an inbound port-forward to the ESP32.

## Why a relay

Directly publishing the embedded HTTP server would expose a management interface that is designed for a trusted LAN. A relay allows remote identity, session authorization, rate limiting, audit logging and TLS termination to be implemented outside the constrained device.

## Configuration prepared in 0.4.0-dev

NVS namespace: `remote`.

Stored fields:

- enabled/disabled;
- HTTPS relay URL;
- unique Device ID;
- per-device authentication token;
- trusted relay CA certificate;
- future heartbeat interval;
- separate `allow remote admin` flag.

A default Device ID is generated from the ESP32 unique hardware identifier, for example:

```text
davis-A1B2C3
```

The Device ID can be changed, but only alphanumeric characters, `-` and `_` are accepted.

## Secret handling

The Web API never returns the stored token or CA contents. It only returns `has_token` and `has_ca` boolean indicators.

The token and CA are stored in NVS. Standard NVS persistence is not equivalent to encrypted secret storage; installations requiring resistance to physical extraction should evaluate ESP32 flash encryption / secure boot.

## Current states

- `OFF` - remote profile disabled;
- `CONFIG_REQUIRED` - enabled but one or more mandatory values are missing/invalid;
- `READY` - HTTPS relay URL, Device ID, token and CA are present;
- `transport_active=false` - expected in 0.4.0-dev because no tunnel transport is started yet.

`READY` therefore means **configuration ready for the relay implementation**, not that the device is remotely reachable.

## Web API

- `GET /api/remote/config` - non-secret configuration summary;
- `POST /api/remote/config` - save configuration;
- `GET /api/remote/status` - readiness/status;
- `POST /api/remote/reset` - remove relay URL, token, CA and settings.

Token and CA are replaced only when the POST request explicitly includes their replacement flags. Leaving their input fields blank preserves existing values.

## Planned transport

The recommended next implementation stage is a persistent outbound TLS/WebSocket session or a short-lived authenticated HTTPS control channel. The relay should authenticate every device and every operator session independently.

A future relay protocol should include at least:

- per-device token or stronger device credential;
- TLS certificate validation on the ESP32;
- server-side device allow-list;
- short-lived operator sessions;
- explicit read-only versus administration capability;
- command allow-list instead of an arbitrary shell;
- request/response correlation IDs;
- rate limiting and replay protection;
- audit log on the relay;
- remote actions disabled by default until explicitly enabled by the owner.

## Remote administration boundary

The `allow_remote_admin` flag is already stored separately from the basic remote profile. This allows the future design to support telemetry/status access without automatically granting configuration changes or restart/reset actions.

A future implementation should treat operations such as network reset, microSD format, credential replacement, firmware update and factory reset as privileged actions requiring stronger authorization and explicit confirmation.

## What is not implemented yet

Version 0.4.0-dev does not yet provide:

- a public relay server;
- WebSocket/tunnel transport;
- remote browser proxying of the local Web UI;
- remote firmware update;
- arbitrary remote terminal/shell;
- automatic Internet exposure.

These are intentionally deferred until the relay-side security contract is defined and tested.

## Deployment recommendation

Until the relay is implemented, remote access to an installed gateway should use a trusted VPN or an authenticated network-level solution rather than direct port forwarding to TCP/80 on the ESP32.
