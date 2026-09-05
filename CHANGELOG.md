# Changelog

All notable project changes are documented here.

Versions marked `-dev` are development baselines and are not stable releases.

## [Unreleased]

- Physical microSD validation on both supported board families.
- Define and implement the authenticated outbound remote-relay transport.
- Continue long-duration RF continuity and sensor-value validation.

## [0.4.0-dev] - 2026-09-05

### Added

- Fail-safe microSD datalogger using **SdFat 2.3.1** on a dedicated SPI bus.
- Board-specific onboard microSD pin profiles for T3 V1.6.1 and T3-S3.
- Fixed 16-record RAM queue and batched filesystem writes outside the Davis RF decoder path.
- UTC daily CSV files under `/weather/YYYY/MM/YYYY-MM-DD.csv` plus `/weather/unsynced.csv` before valid time.
- Optional logging of Davis RF RAW/NORMALIZED frames, weather snapshots, BME280 fields and AS3935 lightning events.
- Non-blocking microSD mount retry sequence and 4 MHz / 400 kHz SPI fallback.
- microSD Web configuration, remount/reset/format actions and live storage diagnostics.
- Secure **remote-ready** NVS profile with HTTPS relay URL, Device ID, per-device token, CA certificate, heartbeat setting and separate future remote-admin permission.
- `docs/SD_DATALOGGER.md` and `docs/REMOTE_ACCESS.md`.
- New standalone `ARCHITECTURE_0.4.md`.

### Changed

- Firmware version advanced to `0.4.0-dev`.
- Home dashboard redesigned around live Davis weather, RF state, storage and service status.
- Configuration page reorganized into separate Station, Network, HTTP, MQTT, microSD, AS3935, Remote and System cards.
- Status bar expanded to `NET / RF / BME / SD / MQTT / AS3935 / REMOTE`.
- Dashboard source moved to `web/dashboard.html` and deterministically gzip-compressed at build time by `scripts/generate_web_ui.py`.
- `web_manager.cpp` now serves the compressed UI and exposes microSD/remote APIs.
- Main loop services Davis RF again immediately after SD work to preserve RF responsiveness.
- Documentation rewritten as a self-contained ESP32 Davis Weather Gateway project.

### RF integrity

- Davis RF frequencies, FSK parameters, CRC, packet mapping and FHSS acquisition/tracking were **not changed**.
- The current real-hardware setup successfully acquires Davis station data.

### Remote-access scope

- 0.4.0-dev only prepares identity/credential/configuration state.
- No inbound Internet listener or remote tunnel is activated.
- `READY` means relay configuration is complete, not that the gateway is remotely reachable.

### Validation

- PlatformIO CI successful on `t3-v161-868` and `t3-s3-868` for the 0.4 firmware foundation.
- microSD physical-media behaviour and browser rendering require on-device validation.

## [0.3.2-dev] - 2026-09-04

### Added

- Extended Davis RF/FHSS Web diagnostics.
- Separate statistics for all five EU channels.
- 24-frame RAM ring buffer with RAW, normalized bytes and CRC comparison.
- Packet timing/jitter diagnostics and SX1276 path counters.
- `ERROR / SEARCH / CANDIDATE / SYNC` acquisition phases.
- System-health diagnostics and downloadable `davis-diagnostic.txt`.
- 60-second diagnostic capture workflow.

### Changed

- Firmware version advanced to `0.3.2-dev`.
- Diagnostics page prioritizes RF channel distribution, CRC, timing and frame history.

### RF integrity

- Decoder, CRC algorithm, five-frequency hop set and acquisition strategy remained unchanged.

## [0.3.1-dev] - 2026-09-04

### Added

- SSD1306 128x64 OLED support through pinned U8g2.
- Boot-stage visualization.
- `DAVIS SEARCH` and `DAVIS RX RAW` acquisition diagnostics.
- Six-page runtime display cycle for weather, wind/rain, barometer, RF, AS3935 and system state.
- Automatic return to acquisition view when Davis traffic becomes stale.

### Changed

- Firmware version advanced to `0.3.1-dev`.
- Shared OLED/BME280/AS3935 I2C bus kept at 100 kHz.

## [0.3.0-dev] - 2026-09-04

### Added

- Dark/tabbed local Web UI.
- Live weather, hardware, configuration and diagnostic views.
- Aggregated `/api/state` / `/api/status` JSON.
- MQTT publisher with NVS configuration, LWT, retained telemetry and optional TLS.
- AS3935 lightning manager with NVS configuration, IRQ/event handling and MQTT event publication.
- BME280 rediscovery, absolute/sea-level pressure, trend and local barometric indication.
- On-demand I2C diagnostic scan.

### Changed

- Firmware version advanced to `0.3.0-dev`.
- Davis RF service kept first in the main loop.
- TLS defaults aligned with secure certificate validation.

## [0.2.0-dev] - 2026-08-31

### Added

- Davis-only European 868 MHz receiver profile for LILYGO/ESP32 with SX1276/RFM95.
- Five-channel Davis EU FHSS acquisition and synchronization.
- Davis packet normalization, CRC16-CCITT validation and weather decoding.
- BME280 pressure integration with configurable altitude.
- Captive-portal Wi-Fi provisioning with NVS persistence.
- DHCP-first network configuration with optional static profile.
- Generic configurable HTTP receiver compatible with `mb.php`-style uploads.
- Rain total persistence across reboot.
- GitHub Actions PlatformIO builds for both supported targets.
- Italian and English RF protocol documentation.

### Security and reproducibility

- HTTPS certificate validation enabled by default.
- GitHub Actions permissions hardened to least privilege.
- PlatformIO/framework/library revisions pinned.
- Stored Wi-Fi passwords excluded from browser HTML and serial logs.
- User-controlled HTML attributes escaped.
