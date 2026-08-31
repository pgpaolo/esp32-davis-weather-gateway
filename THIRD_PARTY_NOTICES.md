# Third-Party Notices and Protocol References

ESP32 Davis Weather Gateway uses third-party libraries through PlatformIO. Those dependencies remain under their respective licenses and are not relicensed by this repository.

## Public protocol/reverse-engineering references

The project documentation acknowledges historical public work that helped document Davis ISS interoperability, including:

- **DavisRFM69** by DeKay - historical Davis ISS/RFM69 reverse-engineering project, published under CC BY-SA 3.0 according to its repository notices.
- **ISS-MQTT-Gateway** by dcbo - ESP32/RFM69 Davis ISS receiver project, published under GPL-3.0 according to its repository metadata.

These projects are cited as technical/protocol references. This repository is intended to contain an independent ESP32/SX1276 implementation and does not intentionally vendor their source files.

Protocol parameters, observed frame layouts and interoperability facts are documented so that behaviour can be tested against real hardware. Where a formula or behaviour has not yet been confirmed on hardware, the project documentation labels it as reverse-engineered or requiring validation.

## Davis trademarks

Davis, Vantage Pro2, Vantage Vue and other Davis product names may be trademarks of Davis Instruments or their respective owners. Their use in this project is descriptive and for interoperability/documentation purposes only. This project is independent and is not affiliated with or endorsed by Davis Instruments.
