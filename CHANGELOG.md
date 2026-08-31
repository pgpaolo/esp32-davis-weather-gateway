# Changelog

All notable project changes are documented here.

The project follows a development-first branch model. Versions marked `-dev` are not stable releases.

## [0.2.0-dev] - 2026-08-31

### Added

- Davis-only European 868 MHz receiver profile for LILYGO/ESP32 with SX1276/RFM95.
- Five-channel Davis EU FHSS acquisition and synchronization logic.
- Davis packet normalization, CRC16-CCITT validation and weather decoding.
- BME280 pressure integration with configurable installation altitude.
- Captive-portal Wi-Fi provisioning with NVS persistence.
- DHCP-first configuration with optional static profile suggesting `192.168.1.120`.
- Automatic recovery provisioning after Wi-Fi connection failure.
- Generic configurable HTTP receiver compatible with `mb.php`-style `?d=` uploads.
- Web dashboard, configuration page, JSON status and record preview.
- Rain total persistence across reboot.
- GitHub Actions PlatformIO builds for T3 V1.6.1 868 and T3-S3 868.
- Italian and English protocol documentation.

### Changed

- Removed installation-specific endpoint defaults and environment-specific references.
- Public firmware configuration is now installation-neutral.
- Project license standardized as GNU LGPLv3 (`LGPL-3.0-only`) for project-original material.

### Validation status

- CI compilation: validated for both supported PlatformIO targets.
- RF protocol behaviour: requires continued validation against real Davis hardware before declaring a stable release.
