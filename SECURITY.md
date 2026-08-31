# Security Policy

## Supported branch

Security fixes are applied first to `develop` and promoted to `main` after review and CI validation. Critical fixes may be prepared directly against `main` when necessary.

## Reporting a vulnerability

Please avoid publishing credentials, private endpoints, device identifiers, network dumps containing secrets, or exploitable details in a public issue.

For a suspected vulnerability, contact the repository maintainer privately through the contact options available on the maintainer's GitHub profile. Provide the affected version/commit, hardware target, impact, and reproducible steps where possible.

## Secrets and configuration

The public repository must not contain real Wi-Fi credentials, private certificates, tokens, passwords, or installation-specific receiver URLs. Runtime credentials belong in NVS or an ignored private configuration file.

`src/config_private.h` is intentionally ignored by Git. Do not rename or copy a real private configuration into a tracked path.

## Device exposure

The embedded Web UI and captive provisioning portal are intended for a trusted local network and initial/recovery setup. Do not expose the device Web UI directly to the public Internet without an additional authenticated reverse proxy or equivalent access control.

The temporary provisioning AP may be open when no compile-time `PROVISION_AP_PASSWORD` is configured. Use a private 8+ character provisioning password when the installation environment requires protection against nearby clients.

ESP32 NVS persistence is not equivalent to encrypted secret storage unless flash/NVS encryption is explicitly enabled in the device platform configuration.

## Transport security

HTTPS certificate validation is the default project policy. `MB_TLS_INSECURE_DEFAULT` is `0`; disabling certificate validation must be an explicit installation choice. For HTTPS uploads, configure a trusted CA certificate whenever practical.

## CI and supply chain

GitHub Actions workflows use least-privilege token permissions. Third-party GitHub Actions are pinned to immutable commit SHAs, while PlatformIO and documentation tool versions are pinned for reproducible CI builds. Dependency updates should be reviewed on `develop` and validated by CI before promotion to `main`.
