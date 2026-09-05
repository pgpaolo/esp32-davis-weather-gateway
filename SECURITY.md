# Security Policy

## Supported branch

Security fixes are applied first to `develop` and promoted to `main` after review and CI validation. Critical fixes may be prepared directly against `main` when necessary.

## Reporting a vulnerability

Do not publish credentials, private endpoints, device tokens, CA material, network captures containing secrets or immediately exploitable details in a public issue.

For a suspected vulnerability, contact the repository maintainer privately through the contact options available on the maintainer's GitHub profile. Include the affected version/commit, hardware target, impact and reproducible steps where possible.

## Secrets and configuration

The public repository must not contain real Wi-Fi credentials, private certificates, tokens, passwords or installation-specific receiver URLs. Runtime credentials belong in NVS or an ignored private configuration file.

`src/config_private.h` is intentionally ignored by Git.

The Web API does not return stored Wi-Fi/MQTT/remote token values. For MQTT and remote access it exposes only boolean indicators showing whether a secret/CA is present.

ESP32 NVS persistence is not equivalent to encrypted secret storage unless flash/NVS encryption is explicitly enabled.

## Local Web interface

The embedded Web UI and captive provisioning portal are intended for a trusted local network and initial/recovery setup. Do not expose TCP/80 directly to the public Internet.

The temporary provisioning AP may be open when no compile-time `PROVISION_AP_PASSWORD` is configured. Configure a private 8+ character AP password when the installation environment requires it.

## Remote-access model

Version 0.4.0-dev prepares a future remote relay profile but does not activate a remote tunnel.

The intended model is outbound-only TLS/WSS from the ESP32 to an authenticated relay. No router port-forward should be required.

The remote profile is disabled by default and requires:

- HTTPS relay URL;
- Device ID;
- per-device token;
- trusted CA certificate.

`allow_remote_admin` is a separate explicit permission and must remain false unless administrative remote actions are deliberately required.

A future relay implementation should use server-side device allow-lists, short-lived operator sessions, command allow-lists, replay protection, rate limiting and audit logging. Destructive actions such as microSD format, network reset, credential replacement, firmware update or factory reset should require stronger authorization and explicit confirmation.

Until that relay exists, use VPN/network-level authenticated access rather than direct Internet exposure.

## Transport security

HTTPS certificate validation is the default project policy. `MB_TLS_INSECURE_DEFAULT` is `0`; disabling certificate validation must be an explicit installation choice.

MQTT supports CA-verified TLS. The remote-ready profile intentionally requires an HTTPS URL and trusted CA before it can enter `READY` state.

## microSD

The microSD may contain weather records, RF diagnostics and station identifiers. Treat removable media as potentially sensitive operational data.

Formatting is destructive and requires an explicit `confirm=FORMAT` API value; the Web UI additionally asks for a confirmation dialog and literal `FORMAT` entry.

## CI and supply chain

GitHub Actions workflows use least-privilege token permissions. Third-party GitHub Actions are pinned to immutable commit SHAs, while PlatformIO and major library versions are pinned for reproducible CI builds. Dependency updates should be reviewed on `develop` and validated by CI before promotion to `main`.
