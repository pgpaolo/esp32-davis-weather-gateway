# ESP32 Davis Weather Gateway

**Italiano** | [English](README_EN.md)

[![Build firmware](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml/badge.svg)](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)

Gateway autonomo **ESP32/LILYGO 868 MHz** per ricevere direttamente una **Davis Vantage Pro2 / Pro2 Plus wireless europea**, aggiungere sensori locali e distribuire i dati via Web, MQTT, HTTP e microSD.

`develop` è il branch di integrazione attivo. `main` contiene gli stati promossi tramite Pull Request e CI.

## Stato del progetto

Versione di sviluppo corrente: **`0.4.0-dev`**.

La ricezione Davis EU 868 MHz/FHSS è stata collaudata con successo sull'attuale configurazione hardware reale e il gateway acquisisce i dati della stazione. La 0.4.0-dev mantiene invariato il motore RF e introduce un nuovo livello applicativo: microSD, nuova Web UI e predisposizione sicura per collegamento remoto.

Le nuove funzioni SD e remote-ready devono essere validate sul dispositivo prima della promozione a release stabile.

## Hardware

- LILYGO / TTGO LoRa32 T3 V1.6.1 + SX1276/RFM95 868 MHz
- LILYGO T3-S3 + SX1276/RFM95 868 MHz
- Davis Vantage Pro2 / Pro2 Plus wireless EU
- OLED SSD1306 128x64 I²C `0x3C`
- BME280 opzionale/locale per pressione e T/H lato ricevitore
- AS3935 opzionale per rilevamento fulmini
- microSD onboard tramite bus SPI dedicato

## Architettura

```text
Davis ISS EU 868 MHz FHSS
          |
          v
   SX1276 / RFM95
          |
          v
       ESP32
          |
          +-- decoder Davis / FHSS
          +-- OLED 128x64
          +-- BME280
          +-- AS3935
          +-- microSD datalogger
          +-- Web UI / diagnostica
          +-- MQTT
          +-- HTTP upload
          +-- profilo remote-ready TLS
          +-- NVS / provisioning Wi-Fi
```

Il percorso RF Davis conserva la massima priorità nel loop. Nessuna funzione applicativa modifica frequenze, CRC, packet mapping o logica FHSS.

## Web UI 0.4

La dashboard è stata riorganizzata graficamente con:

- panoramica meteo più leggibile;
- stato RF e ultimo aggiornamento in evidenza;
- sezioni separate per vento, pioggia, pressione, UV/solare e sensori locali;
- badge live `NET / RF / BME / SD / MQTT / AS3935 / REMOTE`;
- pagina Hardware più compatta;
- Configurazione divisa per Stazione, Rete, HTTP, MQTT, microSD, AS3935, Remote e Sistema;
- diagnostica RF completa mantenuta.

L'HTML non è più incorporato come stringa C++ grezza: `web/dashboard.html` viene compresso in gzip durante la build tramite `scripts/generate_web_ui.py`.

## microSD datalogger

Il datalogger è **disabilitato di default** e usa SdFat su un bus SPI dedicato, separato dalla radio.

Caratteristiche principali:

- coda RAM fissa per evitare scritture filesystem nel percorso RF;
- file CSV giornalieri UTC in `/weather/YYYY/MM/YYYY-MM-DD.csv`;
- `/weather/unsynced.csv` prima della sincronizzazione dell'orologio;
- logging opzionale di frame Davis RAW/NORMALIZED, snapshot meteo, BME280 e AS3935;
- retry automatico non bloccante se la scheda manca o il mount fallisce;
- mount a 4 MHz con fallback a 400 kHz;
- diagnostica SdFat con capacità, spazio usato, errori, coda e file corrente;
- formattazione FAT solo tramite azione Web esplicita e doppia conferma.

Pin microSD:

| Board | MOSI | MISO | SCLK | CS |
|---|---:|---:|---:|---:|
| T3 V1.6.1 | 15 | 2 | 14 | 13 |
| T3-S3 V1.2/V1.3 | 11 | 2 | 14 | 13 |

Vedere [docs/SD_DATALOGGER.md](docs/SD_DATALOGGER.md).

## Accesso remoto: predisposizione 0.4

La 0.4.0-dev introduce la configurazione per un futuro **relay outbound TLS**, disabilitata di default.

Il gateway non apre porte Internet e non espone automaticamente la Web UI. Vengono predisposti in NVS:

- URL relay HTTPS;
- Device ID;
- token per dispositivo;
- CA del relay;
- intervallo heartbeat futuro;
- flag separato per eventuale amministrazione remota.

Token e CA non vengono restituiti alla Web UI. In questa versione lo stato `READY` significa che il profilo è completo; **il tunnel remoto non è ancora attivo**. Il trasporto verrà implementato insieme al servizio relay, preferibilmente con connessione outbound TLS/WSS e sessioni autorizzate lato server.

Vedere [docs/REMOTE_ACCESS.md](docs/REMOTE_ACCESS.md).

## OLED e diagnostica Davis

Durante la ricerca l'OLED alterna `DAVIS SEARCH` e `DAVIS RX RAW`. Dopo la sincronizzazione ruota fra meteo, vento/pioggia, barometro, RF, AS3935 e stato gateway.

La Web diagnostica espone i cinque canali Davis, RAW/NORMALIZED, CRC ricevuto/calcolato, history degli ultimi frame, RSSI, timing/jitter, IRQ e report scaricabile.

Vedere [docs/OLED_DISPLAY.md](docs/OLED_DISPLAY.md) e [docs/DIAGNOSTICS_IT.md](docs/DIAGNOSTICS_IT.md).

## Pressione

Per la Vantage Pro2 Sensor Suite 6322/6322M la pressione barometrica è una misura lato ricevitore. Il gateway usa un BME280 locale e gestisce pressione assoluta, riduzione al livello del mare, trend e indicazione barometrica.

## MQTT e HTTP

MQTT è opzionale e disabilitato di default. Supporta plain MQTT, TLS con CA verificata e TLS insecure solo come scelta esplicita.

L'upload HTTP usa un endpoint configurabile; il firmware pubblico non contiene URL specifici di installazione. HTTPS verifica il certificato per default.

## Primo avvio

Senza Wi-Fi salvato il gateway crea `DavisGateway-XXXX` e apre il captive portal su `http://192.168.4.1`. DHCP è il default; `192.168.1.120` è solo un profilo statico suggerito.

## Build

```bash
pio run -e t3-v161-868
pio run -e t3-s3-868
```

La CI compila entrambi i target con radio Davis, OLED, BME280, AS3935, MQTT, microSD/SdFat e Web UI gzip.

## Documentazione

Indice corrente: [docs/README.md](docs/README.md)

Documenti principali:

- [Architettura 0.4](docs/ARCHITECTURE_0.4.md)
- [microSD datalogger](docs/SD_DATALOGGER.md)
- [Accesso remoto](docs/REMOTE_ACCESS.md)
- [Diagnostica Davis](docs/DIAGNOSTICS_IT.md)
- [OLED](docs/OLED_DISPLAY.md)
- [MQTT](docs/MQTT.md)
- [AS3935](docs/AS3935.md)
- [Protocollo RF IT](docs/RF_PROTOCOL_IT.md)
- [RF protocol EN](docs/RF_PROTOCOL_EN.md)
- [Licenza e attribuzioni](docs/LICENSING_IT.md)

## Licenza

Copyright (C) 2026 **Gianpaolo P. (`@pgpaolo`) e contributori**.

Il materiale originale del progetto è distribuito sotto **GNU LGPL v3.0 only (`LGPL-3.0-only`)**, salvo diversa indicazione. Il testo ufficiale GNU resta invariato; copyright, attribuzioni, riferimenti di terzi e disclaimer sono mantenuti separatamente in `NOTICE.md` e `THIRD_PARTY_NOTICES.md`.

Davis Instruments, Vantage Pro2 e gli altri marchi citati appartengono ai rispettivi titolari. Questo progetto è indipendente e non è affiliato o approvato da Davis Instruments.
