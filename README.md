# ESP32 Davis Weather Gateway

**Italiano** | [English](README_EN.md)

[![Build firmware](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml/badge.svg)](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)

Gateway autonomo **ESP32/LILYGO 868 MHz** per ricevere direttamente una **Davis Vantage Pro2 / Pro2 Plus wireless europea**, senza console Davis e senza Meteobridge.

> **Confine architetturale:** il motore radio resta esclusivamente **DAVIS Vantage Pro2 EU 868 MHz FHSS / 2-FSK**. Web UI, OLED, MQTT, BME280, AS3935 e diagnostica sono servizi applicativi e non introducono decoder Oregon Scientific, Technoline/LaCrosse o modalità meteo RF 433 MHz.

`develop` è il branch di integrazione attivo. `main` contiene stati revisionati e promossi tramite Pull Request con CI verde.

## Stato del progetto

Versione di sviluppo corrente: **`0.3.2-dev`**.

La CI compila entrambi i target LILYGO supportati. Il decoder RF Davis continua a richiedere validazione sul campo contro una ISS reale prima di dichiarare una release stabile.

La 0.3.2 aggiunge diagnostica RF/FHSS estesa senza modificare il decoder Davis o la sequenza dei cinque hop.

## Hardware target

- LILYGO / TTGO LoRa32 T3 V1.6.1 con **SX1276/RFM95 868 MHz**
- LILYGO T3-S3 + SX1276/RFM95 868 MHz
- OLED integrato **SSD1306 128x64 I2C, 0x3C**
- Davis Vantage Pro2 / Pro2 Plus wireless EU ISS
- **BME280** I2C per barometro lato ricevitore e T/H locale
- **AS3935** I2C opzionale per rilevamento fulmini

Il bus I2C condiviso OLED/BME280/AS3935 viene mantenuto a **100 kHz**.

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
          +-- Web UI / diagnostica estesa
          +-- MQTT opzionale
          +-- HTTP mb.php opzionale
          +-- NVS / provisioning Wi-Fi
```

`serviceDavisRadio()` conserva la prima priorità nel loop. Display, Web, MQTT e sensori ausiliari non modificano hopping, modulazione o decoder Davis.

Vedere [docs/ARCHITECTURE_0.3.md](docs/ARCHITECTURE_0.3.md).

## OLED e ricerca Davis

All'avvio l'OLED mostra configurazione/NVS, rete, BME280, AS3935/MQTT e inizializzazione radio. Durante la ricerca alterna **DAVIS SEARCH** e **DAVIS RX RAW** mostrando canale/frequenza, RSSI, contatori, ultimo frame grezzo e stato CRC.

Dopo il lock ruota fra meteo, vento/pioggia, barometro, RF/FHSS, AS3935 e stato gateway. Se il traffico diventa stale o il lock viene perso, ritorna automaticamente alla ricerca.

Vedere [docs/OLED_DISPLAY.md](docs/OLED_DISPLAY.md).

## Diagnostica RF estesa 0.3.2

La pagina **Diagnostica** serve a capire dove si interrompe la catena di ricezione prima di modificare il decoder.

Espone:

- stati `ERROR / SEARCH / CANDIDATE / SYNC`;
- statistiche separate per tutti i cinque canali Davis EU;
- RAW, NORMALIZED, CRC ricevuto e CRC calcolato;
- ring buffer RAM degli ultimi **24 frame**;
- RSSI ultimo/medio/min/max per canale;
- timing, min/max/media e jitter rispetto ai circa 2555 ms attesi;
- IRQ DIO0, `readData()`, tune, hop, miss streak e codici RadioLib;
- health ESP32: uptime, heap libero/minimo, CPU, Wi-Fi RSSI e reset reason;
- pulsante **Diagnostic capture 60 s**;
- report testuale scaricabile `davis-diagnostic.txt`.

Interpretazione rapida:

- `RAW = 0`: nessun frame candidato arriva al percorso radio;
- `RAW > 0` ma `CRC KO`: segnale presente, verificare framing/bit order/CRC;
- `CRC OK` ma nessun `SYNC`: verificare ID ISS e continuità FHSS;
- `SYNC`: ricezione Davis sostanzialmente acquisita.

Vedere [docs/DIAGNOSTICS_IT.md](docs/DIAGNOSTICS_IT.md) e [docs/DIAGNOSTICS_EN.md](docs/DIAGNOSTICS_EN.md).

## Dati meteo e pressione

Dalla Davis ISS: temperatura/umidità esterne, vento, direzione, raffica, pioggia/rain rate, UV, radiazione solare e flag batteria trasmettitore.

Dal gateway: pressione BME280 assoluta e ridotta al livello del mare, T/H locale, trend barometrico, previsione indicativa e dati/eventi AS3935.

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

## Web UI e API

La dashboard dark/tabbed espone Dashboard, Hardware, Configurazione e Diagnostica, con stato **NET / RF / BME / MQTT / AS3935**.

API principali:

- `/api/state` e `/api/status`
- `/api/rf`
- `/api/rf/diagnostics`
- `POST /api/rf/reset`
- `/api/diag/report`
- `/api/system`
- `/api/bme` e `/api/i2c/scan`
- API AS3935 e MQTT
- `/api/config`
- `/api/meteobridge`

La Web UI è destinata a una **LAN fidata** e non deve essere esposta direttamente su Internet senza autenticazione/proxy appropriato.

## MQTT e AS3935

MQTT è disabilitato di default e supporta plain MQTT, TLS con CA verificata e insecure TLS solo come opt-in esplicito.

AS3935 è opzionale e configurabile via Web/NVS. Sul T3 V1.6.1 il default usa `0x03` e IRQ GPIO34; sul T3-S3 resta disabilitato di default finché non viene validato un GPIO IRQ libero.

Vedere [docs/MQTT.md](docs/MQTT.md) e [docs/AS3935.md](docs/AS3935.md).

## Primo avvio, Wi-Fi e reset

Se non esiste una configurazione valida il gateway crea `DavisGateway-XXXX` e apre il captive portal su `http://192.168.4.1`. DHCP è il default; `192.168.1.120` è solo il profilo statico suggerito. Dopo 60 s senza rete viene attivato il recovery portal.

Livelli di reset correnti:

- `POST /api/rf/reset` / capture 60 s: azzera solo la diagnostica RF;
- **Reset rete**: cancella il profilo Wi-Fi/rete e torna al provisioning;
- Reset MQTT: cancella solo la configurazione MQTT;
- Reset AS3935: cancella solo la configurazione fulmini;
- reset flash completo: `esptool erase_flash` seguito da nuovo upload firmware.

La 0.3.2-dev non include ancora un pulsante Web di factory reset totale. La procedura completa è documentata in [docs/DIAGNOSTICS_IT.md](docs/DIAGNOSTICS_IT.md).

## HTTP / mb.php

Il firmware pubblico non contiene endpoint specifici. Ogni installazione configura il proprio receiver; HTTPS verifica il certificato per default e la modalità insecure richiede opt-in esplicito.

## Build PlatformIO

```bash
pio run -e t3-v161-868
pio run -e t3-s3-868
```

La CI compila entrambe le board con OLED/U8g2, Web, diagnostica estesa, MQTT, BME280 e AS3935.

## Documentazione

L'indice aggiornato della documentazione di `develop` è in [docs/README.md](docs/README.md).

Documenti principali:

- [Diagnostica estesa Davis](docs/DIAGNOSTICS_IT.md)
- [Extended Davis diagnostics](docs/DIAGNOSTICS_EN.md)
- [OLED display e ricerca Davis](docs/OLED_DISPLAY.md)
- [Architettura applicativa 0.3.2](docs/ARCHITECTURE_0.3.md)
- [MQTT](docs/MQTT.md)
- [AS3935 Lightning](docs/AS3935.md)
- [Note protocollo RF - Italiano](docs/RF_PROTOCOL_IT.md)
- [RF protocol notes - English](docs/RF_PROTOCOL_EN.md)
- [Licenza e provenance - Italiano](docs/LICENSING_IT.md)
- [Licensing and provenance - English](docs/LICENSING_EN.md)

Le vecchie guide PDF RF v1.0/v1.1 sono mantenute come storico e non descrivono da sole l'intero firmware 0.3.2.

## Licenza, attribuzioni e provenance

Copyright (C) 2026 **Gianpaolo P. (`@pgpaolo`) e contributori**.

Il codice e la documentazione originali del progetto sono distribuiti, salvo diversa indicazione, sotto **GNU LGPL v3.0 only (`LGPL-3.0-only`)**.

La stessa regola di trasparenza adottata nell'altro progetto Oregon/Technoline viene mantenuta qui: provenienza e attribuzioni dei riferimenti tecnici sono documentate esplicitamente, senza importare nel motore Davis i decoder GPL Oregon/Technoline o le relative parti `rtl_433` / PracticalArduino.

Il testo ufficiale LGPL non viene modificato. Le personalizzazioni e attribuzioni del progetto sono separate in:

- [LICENSE](LICENSE)
- [NOTICE.md](NOTICE.md)
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- [docs/LICENSING_IT.md](docs/LICENSING_IT.md)
- [docs/LICENSING_EN.md](docs/LICENSING_EN.md)

Davis Instruments, Vantage Pro2 e gli altri marchi citati appartengono ai rispettivi titolari. Questo progetto è indipendente e non è affiliato o approvato da Davis Instruments.

Vedere anche [SECURITY.md](SECURITY.md), [CONTRIBUTING.md](CONTRIBUTING.md) e [docs/BRANCH_POLICY.md](docs/BRANCH_POLICY.md).
