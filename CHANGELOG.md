# Changelog

All notable project changes are documented here.

The project follows a development-first branch model. Versions marked `-dev` are not stable releases.

## [Unreleased]

- Continue real-hardware validation of Davis FHSS decoding and weather conversions.
- Select/validate a safe AS3935 IRQ GPIO for each supported T3-S3 hardware revision before enabling lightning support by default on that target.

## [0.3.1-dev] - 2026-09-04

### Added

- SSD1306 128x64 OLED support through pinned U8g2.
- Boot-stage visualization for NVS/configuration, network, BME280, AS3935/MQTT and Davis radio initialization.
- Live **DAVIS SEARCH** diagnostics while the receiver is acquiring or re-acquiring the ISS.
- Search display with FHSS channel/frequency, configured ISS filter, RSSI, valid/CRC/missed counters, BME280/AS3935 status and network address.
- Six-page runtime display cycle for Davis weather, wind/rain, barometer, Davis RF/FHSS, AS3935 and gateway/system state.
- Automatic return to the search page when Davis traffic becomes stale or synchronization is lost.
- **DAVIS RF ERROR** screen with RadioLib error code when SX1276 initialization fails.
- OLED documentation in `docs/OLED_DISPLAY.md`.

### Changed

- Firmware version advanced to `0.3.1-dev`.
- Shared OLED/BME280/AS3935 I2C bus is kept at 100 kHz.
- OLED refresh is serviced after the Davis RF path so display work never takes priority over packet reception.

### Validation

- `t3-v161-868`: CI build successful with U8g2/OLED support.
- `t3-s3-868`: CI build successful with U8g2/OLED support.
- T3 V1.6.1 build: 54,296 bytes RAM (16.6%) and 1,168,617 bytes flash (89.2%) in the current application partition.

## [0.3.0-dev] - 2026-09-04

### Added

- New dark/tabbed Web UI inspired by the application layer of the Oregon/Technoline 6.4 reference project and rewritten for Davis-only data.
- Dashboard pages for live weather, Davis RF/FHSS, hardware, configuration and diagnostics.
- Aggregated `/api/state` / `/api/status` JSON with weather, RF, BME280, MQTT, AS3935 and HTTP upload status.
- MQTT publisher with runtime NVS configuration, retained telemetry, LWT status, JSON snapshot and optional TLS.
- MQTT TLS modes: plain, CA-verified TLS and explicit insecure TLS.
- AS3935 lightning manager with NVS configuration, IRQ handling, calibration, noise/disturber/lightning counters, distance/energy and MQTT event publication.
- BME280 rediscovery and read-fault recovery, absolute pressure, sea-level pressure, trend estimation and indicative local forecast.
- On-demand I2C diagnostic scan.
- Documentation for application architecture, MQTT and AS3935.

### Changed

- Firmware version advanced to `0.3.0-dev`.
- Main loop keeps Davis RF service first, followed by BME280, rain persistence, AS3935, network, Web, MQTT and HTTP upload services.
- Web configuration now groups Davis/gateway, MQTT and AS3935 settings in one application UI.
- `RuntimeConfig::tlsInsecure` default aligned with secure HTTP TLS policy (`false`).
- PlatformIO dependencies now include pinned PubSubClient and AS3935MI revisions.

### RF scope guarantee

- **Davis Vantage Pro2 EU 868 MHz FHSS / 2-FSK remains the only weather RF engine.**
- No Oregon Scientific receiver/decoder was imported.
- No Technoline/LaCrosse receiver/decoder was imported.
- No 433 MHz weather RF mode was added.

### Validation

- `t3-v161-868`: CI build successful after the 0.3 application-layer integration.
- `t3-s3-868`: CI build successful after the 0.3 application-layer integration.
- Classic T3 V1.6.1 CI build: approximately 53 kB RAM used and 1.15 MB flash used with the new services enabled in firmware.
- Real Davis RF behaviour and AS3935 physical installation remain hardware-validation items.

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
- Initial Web dashboard, configuration page, JSON status and record preview.
- Rain total persistence across reboot.
- GitHub Actions PlatformIO builds for T3 V1.6.1 868 and T3-S3 868.
- Italian and English RF protocol documentation.

### Security and reproducibility

- HTTPS certificate validation made the default (`MB_TLS_INSECURE_DEFAULT=0`).
- GitHub Actions permissions hardened to least privilege and actions pinned to immutable commit SHAs.
- PlatformIO, Espressif32 and external library versions pinned to CI-validated revisions.
- Stored Wi-Fi passwords are never rendered back into provisioning/configuration HTML or printed to the serial log.
- HTML attribute escaping covers user/network-controlled values such as SSIDs, hostnames and receiver URLs.

### Documentation

- RF protocol guide updated to Edition 1.1 in Italian and English.
- Clarified receiver-side barometer architecture for Vantage Pro2 6322/6322M and the equivalent local BME280 design.

### Validation status

- CI compilation validated for both supported PlatformIO targets.
- RF protocol behaviour requires continued validation against real Davis hardware before declaring a stable release.
