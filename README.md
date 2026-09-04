# ESP32 Davis Weather Gateway

**Italiano** | [English](README_EN.md)

[![Build firmware](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml/badge.svg)](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)

Gateway autonomo **ESP32/LILYGO 868 MHz** per ricevere direttamente una **Davis Vantage Pro2 / Pro2 Plus wireless europea**, senza console Davis e senza Meteobridge.

> **Confine architetturale:** il motore radio resta esclusivamente **DAVIS Vantage Pro2 EU 868 MHz FHSS / 2-FSK**. Web UI, OLED, MQTT, BME280 e AS3935 sono servizi applicativi e non introducono decoder Oregon Scientific, Technoline/LaCrosse o modalità meteo RF 433 MHz.

`develop` è il branch di integrazione attivo. `main` contiene stati revisionati e promossi tramite Pull Request con CI verde.

## Stato del progetto

Versione di sviluppo corrente: **`0.3.1-dev`**.

La CI compila entrambi i target LILYGO supportati. Il decoder RF Davis continua a richiedere validazione sul campo contro una ISS reale prima di dichiarare una release stabile.

## Hardware target

- LILYGO / TTGO LoRa32 T3 V1.6.1 con **SX1276/RFM95 868 MHz**
- LILYGO T3-S3 + SX1276/RFM95 868 MHz
- OLED integrato **SSD1306 128x64 I2C, 0x3C**
- Davis Vantage Pro2 / Pro2 Plus wireless EU ISS
- **BME280** I2C per barometro lato ricevitore e T/H locale
- **AS3935** I2C opzionale per rilevamento fulmini

Il bus I2C condiviso OLED/BME280/AS3935 viene mantenuto a **100 kHz** per aumentare il margine di affidabilità.

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
          +-- Davis decoder / FHSS   <-- unico motore RF meteo
          +-- OLED 128x64            <-- dati + ricerca/diagnostica live
          +-- BME280                 <-- barometro lato ricevitore
          +-- AS3935                 <-- fulmini, I2C + IRQ
          +-- Web UI / diagnostica
          +-- MQTT opzionale
          +-- HTTP mb.php opzionale
          +-- NVS / provisioning Wi-Fi
```

`serviceDavisRadio()` conserva la prima priorità nel loop. Il display viene aggiornato dopo il percorso RF e non modifica hopping, modulazione o decoder.

## OLED e diagnostica durante la ricerca

Il display integrato viene inizializzato all'avvio e mostra l'avanzamento di boot: configurazione/NVS, rete, BME280, AS3935/MQTT e inizializzazione radio Davis.

Finché non viene acquisito un pacchetto Davis valido e il ricevitore non è sincronizzato, l'OLED resta sulla pagina **DAVIS SEARCH** e mostra in tempo reale:

- canale FHSS e frequenza corrente
- filtro ISS `AUTO` oppure ID configurato
- RSSI quando disponibile
- pacchetti validi, errori CRC e pacchetti mancati
- stato BME280 e AS3935
- IP Web oppure IP del captive portal/recovery

Dopo il lock RF il display ruota automaticamente, circa ogni 6,5 s, tra:

1. dati meteo Davis
2. vento e pioggia
3. barometro BME280, trend e previsione
4. diagnostica RF Davis/FHSS
5. AS3935 fulmini
6. stato gateway, Wi-Fi, MQTT, heap e uptime

Se il flusso Davis diventa stale per circa 12 s o si perde la sincronizzazione, il display torna automaticamente alla schermata **DAVIS SEARCH**. Se l'SX1276 non si inizializza compare invece **DAVIS RF ERROR** con il codice RadioLib.

Vedere [docs/OLED_DISPLAY.md](docs/OLED_DISPLAY.md).

## Dati meteo

Dalla Davis ISS: temperatura/umidità esterne, vento, direzione, raffica, pioggia/rain rate, UV, radiazione solare e flag batteria trasmettitore.

Dal gateway: pressione BME280 assoluta e ridotta al livello del mare, T/H locale, trend barometrico, previsione indicativa e dati/eventi AS3935.

### Pressione Davis

Per la **Vantage Pro2 Sensor Suite 6322/6322M** il barometro non è nell'ISS esterna. Nell'ecosistema Davis la misura barometrica è lato ricevente; il gateway segue la stessa architettura usando un BME280 locale.

## RF Davis EU

Il ricevitore usa 2-FSK a 19.2 kbps, deviazione 4.8 kHz, Gaussian BT=0.5, sync `CB 89` e frame fisso di 10 byte con CRC Davis verificato in software.

Hop set EU implementato:

1. 868.066711 MHz
2. 868.297119 MHz
3. 868.527466 MHz
4. 868.181885 MHz
5. 868.412292 MHz

**Non sono presenti** `oregon_receiver`, decoder Oregon Scientific, decoder Technoline/LaCrosse o modalità RF meteo 433 MHz.

## Web UI e diagnostica

La dashboard dark/tabbed espone Dashboard, Hardware, Configurazione e Diagnostica, con stato **NET / RF / BME / MQTT / AS3935** e API JSON dedicate.

API principali: `/api/state`, `/api/status`, `/api/bme`, `/api/i2c/scan`, `/api/as3935/state`, `/api/as3935/config`, `/api/mqtt/status`, `/api/mqtt/config`, `/api/config`, `/api/meteobridge`.

La Web UI è destinata a una **LAN fidata** e non deve essere esposta direttamente su Internet senza autenticazione/proxy appropriato.

## MQTT e AS3935

MQTT è disabilitato di default e supporta plain MQTT, TLS con CA verificata e insecure TLS solo come opt-in esplicito. AS3935 è opzionale e configurabile via Web/NVS. Sul T3 V1.6.1 il default usa `0x03` e IRQ GPIO34; sul T3-S3 resta disabilitato di default finché non viene validato un GPIO IRQ libero.

Vedere [docs/MQTT.md](docs/MQTT.md) e [docs/AS3935.md](docs/AS3935.md).

## Primo avvio e Wi-Fi

Se non esiste una configurazione valida il gateway crea `DavisGateway-XXXX` e apre il captive portal su `http://192.168.4.1`. DHCP è il default; `192.168.1.120` è solo il profilo statico suggerito. Dopo 60 s senza rete viene attivato il recovery portal.

## HTTP / mb.php

Il firmware pubblico non contiene endpoint specifici. Ogni installazione configura il proprio receiver; HTTPS verifica il certificato per default e la modalità insecure richiede opt-in esplicito.

## Build PlatformIO

```bash
pio run -e t3-v161-868
pio run -e t3-s3-868
```

La CI `0.3.1-dev` compila entrambe le board con OLED/U8g2, Web, MQTT, BME280 e AS3935. Sul T3 V1.6.1 la build OLED usa circa **54,3 kB RAM** e **1,169 MB flash** nella partizione applicativa corrente.

## Documentazione

- [OLED display e ricerca Davis](docs/OLED_DISPLAY.md)
- [Architettura applicativa 0.3](docs/ARCHITECTURE_0.3.md)
- [MQTT](docs/MQTT.md)
- [AS3935 Lightning](docs/AS3935.md)
- [Note protocollo RF - Italiano](docs/RF_PROTOCOL_IT.md)
- [RF protocol notes - English](docs/RF_PROTOCOL_EN.md)

La documentazione RF è indipendente e basata su interoperabilità/reverse engineering pubblico; non è una specifica ufficiale Davis Instruments.

## Sicurezza, branch e licenza

Vedere [SECURITY.md](SECURITY.md), [CONTRIBUTING.md](CONTRIBUTING.md) e [docs/BRANCH_POLICY.md](docs/BRANCH_POLICY.md). Il codice e la documentazione originali sono distribuiti sotto **GNU LGPL v3.0 only (`LGPL-3.0-only`)**, salvo diversa indicazione.
