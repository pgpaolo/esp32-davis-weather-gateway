# ESP32 Davis Weather Gateway

**Italiano** | [English](README_EN.md)

[![Build firmware](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml/badge.svg)](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)

Gateway autonomo **ESP32/LILYGO 868 MHz** per ricevere direttamente una **Davis Vantage Pro2 / Pro2 Plus wireless europea**, senza console Davis e senza Meteobridge.

> **Confine architetturale:** il motore radio resta esclusivamente **DAVIS Vantage Pro2 EU 868 MHz FHSS / 2-FSK**. L'evoluzione `0.3.x` porta sul progetto Davis l'interfaccia e i servizi applicativi maturati nel progetto Oregon/Technoline 6.4, ma **non importa decoder, parser o modalità RF Oregon Scientific / Technoline**.

`develop` è il branch di integrazione attivo. `main` contiene stati revisionati e validati tramite Pull Request e CI.

## Stato del progetto

Versione di sviluppo corrente: **`0.3.0-dev`**.

La CI compila entrambi i target LILYGO supportati. Il decoder RF Davis continua a richiedere validazione sul campo contro una ISS reale prima di dichiarare una release stabile.

## Hardware target

- LILYGO / TTGO LoRa32 T3 V1.6.1 con **SX1276/RFM95 868 MHz**
- LILYGO T3-S3 + SX1276/RFM95 868 MHz
- Davis Vantage Pro2 / Pro2 Plus wireless EU ISS
- **BME280** I²C sul gateway per barometro e T/H locale
- **AS3935** I²C opzionale per rilevamento fulmini

Una radio destinata esclusivamente a 433 MHz non è adatta: il ricevitore Davis usa hardware per la banda europea 868 MHz.

## Architettura 0.3

```text
Davis ISS EU 868 MHz FHSS
          |
          v
   SX1276 / RFM95
          |
          v
       ESP32
          |
          +-- Davis decoder / FHSS   <-- unico motore RF meteo
          +-- BME280                 <-- barometro lato ricevitore
          +-- AS3935                 <-- fulmini, I2C + IRQ
          +-- Web UI / diagnostica
          +-- MQTT opzionale
          +-- HTTP mb.php opzionale
          +-- NVS / provisioning Wi-Fi
```

La priorità del loop resta assegnata a `serviceDavisRadio()`: MQTT, Web, BME280 e AS3935 sono servizi ausiliari e non cambiano modulazione, hop table o decoder Davis.

## Dati meteo

Dalla Davis ISS:

- temperatura e umidità esterne
- velocità e direzione vento
- raffica 10 minuti
- pioggia e rain rate
- UV e radiazione solare quando disponibili
- stato batteria trasmettitore

Dal gateway:

- pressione BME280 assoluta e ridotta al livello del mare
- temperatura/umidità locale BME280
- trend barometrico e previsione locale indicativa
- eventi AS3935: lightning, disturber, noise, distanza ed energia

### Pressione Davis

Per la **Vantage Pro2 Sensor Suite 6322/6322M** il barometro non è nell'ISS esterna. Nell'ecosistema Davis la misura barometrica è lato ricevente; questo gateway replica la stessa separazione con un BME280 locale. Il nuovo manager BME280 gestisce indirizzi `0x76/0x77`, rediscovery non bloccante, pressione assoluta/sea-level, trend e diagnostica I²C.

## RF Davis EU

Il ricevitore usa 2-FSK a 19.2 kbps, deviazione 4.8 kHz, Gaussian BT=0.5, sync `CB 89` e frame fisso di 10 byte con CRC Davis verificato in software.

Hop set EU implementato:

1. 868.066711 MHz
2. 868.297119 MHz
3. 868.527466 MHz
4. 868.181885 MHz
5. 868.412292 MHz

**Non sono presenti** `oregon_receiver`, decoder Oregon Scientific, decoder Technoline/LaCrosse o modalità RF 433 MHz.

## Web UI

La dashboard `0.3.0-dev` adotta una grafica dark/tabbed coerente con il progetto 6.4 di riferimento, adattata ai dati Davis. Le sezioni sono:

- **Dashboard**: valori meteo, barometro, previsione e fulmini
- **Hardware**: Davis RF/FHSS, BME280/I²C, AS3935, rete e MQTT
- **Configurazione**: gateway/Davis, HTTP, MQTT e AS3935
- **Diagnostica**: JSON live dei sottosistemi

Indicatori rapidi mostrano stato **NET / RF / BME / MQTT / AS3935**.

API principali:

- `/api/state` e `/api/status` - stato aggregato
- `/api/bme` - BME280/trend
- `/api/i2c/scan` - scansione I²C on-demand
- `/api/as3935/state` e `/api/as3935/config`
- `/api/mqtt/status` e `/api/mqtt/config`
- `/api/config` - configurazione gateway
- `/api/meteobridge` - anteprima record

La UI è destinata a una **LAN fidata** e non deve essere pubblicata direttamente su Internet senza autenticazione/proxy appropriato.

## AS3935 Lightning

Il sensore fulmini è opzionale e configurabile da Web UI/NVS: ambiente indoor/outdoor, address I²C, IRQ GPIO, noise floor, watchdog, spike rejection, minimo numero di strike, mask disturber, tuning capacitor e autotune.

Sul profilo **T3 V1.6.1** il default è address `0x03`, IRQ **GPIO34**. Sul profilo **T3-S3** AS3935 è volutamente **disabilitato di default** finché non viene scelto e verificato un GPIO IRQ libero sulla revisione hardware utilizzata.

Vedere [docs/AS3935.md](docs/AS3935.md).

## MQTT

MQTT è **disabilitato di default** ed è completamente configurabile dalla Web UI. Supporta:

- broker/porta
- username/password
- client ID e base topic
- intervallo pubblicazione
- MQTT non cifrato
- TLS con CA verificata
- TLS insecure, solo come scelta esplicita dell'installatore

La password MQTT e la CA memorizzate non vengono restituite al browser. Sono pubblicati topic per weather, BME, Davis RF, system e AS3935, oltre a uno snapshot JSON `state`.

Vedere [docs/MQTT.md](docs/MQTT.md).

## Primo avvio e Wi-Fi

SSID e password non vengono inseriti nel sorgente pubblico. Se non esiste una configurazione valida il gateway crea l'AP temporaneo:

```text
DavisGateway-XXXX
```

Captive portal: `http://192.168.4.1`.

DHCP è il default; il profilo statico proposto usa `192.168.1.120` come semplice valore suggerito. Dopo 60 secondi senza rete viene attivato il recovery portal. La configurazione è salvata in NVS e le password memorizzate non vengono renderizzate nuovamente nella Web UI.

## HTTP / mb.php

Il firmware pubblico non contiene endpoint specifici dell'installazione. Ogni utente configura il proprio URL receiver, ad esempio:

```text
https://server.example/weather/mb.php
```

Il gateway aggiunge `?d=<record Meteobridge/Weather34 URL-encoded>`. HTTPS verifica il certificato per default; la modalità insecure richiede opt-in esplicito.

## Build PlatformIO

```bash
pio run -e t3-v161-868
pio run -e t3-s3-868
```

La CI `0.3.0-dev` ha compilato con successo entrambe le board dopo l'integrazione Web/MQTT/BME/AS3935. Le dipendenze principali restano versionate/pinnate in `platformio.ini`.

## Documentazione

- [Architettura applicativa 0.3](docs/ARCHITECTURE_0.3.md)
- [MQTT](docs/MQTT.md)
- [AS3935 Lightning](docs/AS3935.md)
- [Note protocollo RF - Italiano](docs/RF_PROTOCOL_IT.md)
- [RF protocol notes - English](docs/RF_PROTOCOL_EN.md)
- [Guida RF Davis PDF italiano v1.1](docs/Davis_RF_Protocol_Guide_IT_v1.1.pdf)
- [Davis RF guide English PDF v1.1](docs/Davis_RF_Protocol_Guide_EN_v1.1.pdf)

La documentazione del protocollo è indipendente e basata su implementazione/interoperabilità/reverse engineering pubblico; non è una specifica ufficiale Davis Instruments.

## Sicurezza

Vedere [SECURITY.md](SECURITY.md). Il repository pubblico non deve contenere password, token, chiavi private, CA private o endpoint personali. MQTT e HTTP TLS sono secure-by-default dove applicabile; le modalità insecure sono esplicite.

## Branch e contributi

Vedere [CONTRIBUTING.md](CONTRIBUTING.md). Lo sviluppo ordinario avviene su `develop`; la promozione a `main` avviene esclusivamente via Pull Request con CI verde.

## Licenza

Il codice e la documentazione originali del progetto sono distribuiti sotto **GNU Lesser General Public License v3.0 only (`LGPL-3.0-only`)**, salvo diversa indicazione nel singolo file. Le dipendenze mantengono le proprie licenze.

Vedere [LICENSE](LICENSE), [COPYING](COPYING) e [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
