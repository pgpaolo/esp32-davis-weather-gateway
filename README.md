# ESP32 Davis Weather Gateway

**Italiano** | [English](README_EN.md)

[![Build firmware](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml/badge.svg)](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)

Gateway autonomo **ESP32/LILYGO 868 MHz** per ricevere direttamente una **Davis Vantage Pro2 / Pro2 Plus wireless europea**, senza console Davis e senza Meteobridge.

Il progetto riceve la Davis ISS via **868 MHz FHSS**, decodifica i dati meteo, completa la pressione con un **BME280 locale**, offre configurazione e diagnostica web e può inviare un record compatibile Meteobridge/Weather34 a un endpoint HTTP configurabile dall'utente.

> `develop` è il branch di integrazione attivo. `main` contiene stati del progetto revisionati e validati dalla CI.

## Stato del progetto

Versione di sviluppo corrente: `0.2.0-dev`.

La compilazione CI è validata per entrambi i target LILYGO supportati. La parte RF resta in validazione sul campo contro hardware Davis reale; i dettagli derivati da reverse engineering pubblico sono documentati come tali.

## Hardware target

- LILYGO / TTGO LoRa32 T3 V1.6.1 con **SX1276/RFM95 868 MHz**
- profilo opzionale LILYGO T3-S3 + SX1276 868 MHz
- Davis Vantage Pro2 / Pro2 Plus wireless EU ISS
- BME280 su I2C per pressione atmosferica e T/H locale

Una scheda radio destinata esclusivamente a 433 MHz non è adatta: serve hardware corretto per la banda europea 868 MHz.

## Sensori

Dalla Davis ISS:

- temperatura esterna
- umidità esterna
- velocità e direzione vento
- raffica 10 minuti
- pioggia e rain rate
- UV
- radiazione solare
- stato batteria trasmettitore

Dal gateway:

- pressione atmosferica BME280 ridotta al livello del mare
- temperatura/umidità locale BME280

### Nota sulla pressione Davis

Per la **Vantage Pro2 Sensor Suite 6322/6322M** la pressione barometrica non fa parte dei sensori dell'ISS esterna e non viene ricavata dal frame RF ISS. Nell'ecosistema Davis il barometro è sul lato ricevente: WeatherLink Live e Weather Envoy includono un barometro locale insieme ai sensori T/H interni. Il gateway segue la stessa architettura usando il BME280 locale. La distinzione è documentata nella guida RF Edizione 1.1.

## RF Davis EU

Il ricevitore usa 2-FSK a 19.2 kbps, deviazione 4.8 kHz, shaping Gaussian BT=0.5, sync `CB 89` e pacchetto fisso di 10 byte con CRC16-CCITT Davis verificato in software.

Hop set EU implementato:

1. 868.066711 MHz
2. 868.297119 MHz
3. 868.527466 MHz
4. 868.181885 MHz
5. 868.412292 MHz

Dopo l'acquisizione il firmware segue i salti con periodo nominale di circa 2.555 s; in caso di perdita torna alla fase di acquisizione.

## Primo avvio e configurazione Wi-Fi

SSID e password **non vengono inseriti nel sorgente pubblico**.

Al primo avvio, o quando non esiste una configurazione valida, il gateway crea un access point temporaneo:

```text
DavisGateway-XXXX
```

Per impostazione predefinita l'AP di setup è aperto perché temporaneo; può essere protetto definendo `PROVISION_AP_PASSWORD` in un file di configurazione privato. In ambienti non fidati è raccomandato impostare una password di almeno 8 caratteri.

Il captive portal è disponibile su:

```text
http://192.168.4.1
```

Da qui si configurano SSID, password, hostname e indirizzamento LAN. **DHCP è il default**; scegliendo IP statico il profilo proposto è:

```text
IP       192.168.1.120
Gateway  192.168.1.1
Netmask  255.255.255.0
DNS      192.168.1.1
```

`192.168.1.120` è soltanto il valore suggerito e va verificato rispetto alla LAN dell'installazione.

Se una rete già configurata non è raggiungibile per 60 secondi, il gateway attiva automaticamente il portale di recovery mantenendo i parametri salvati. Tutta la configurazione runtime è salvata in **NVS**.

## Endpoint HTTP / `mb.php`

Il firmware pubblico non contiene **alcun endpoint specifico di installazione**. Ogni utente configura il proprio receiver dalla Web UI, ad esempio:

```text
https://server.example/weather/mb.php
```

Il gateway aggiunge:

```text
?d=<record Meteobridge/Weather34 URL-encoded>
```

Un invio è considerato riuscito solo con risposta HTTP `200` e corpo `success`.

La dashboard offre configurazione URL receiver, intervallo upload, test manuale dell'invio, anteprima del record generato e scelta modalità TLS. La verifica del certificato HTTPS è il default; la modalità TLS insicura deve essere abilitata esplicitamente dall'installatore quando non è disponibile una CA configurata.

## Configurazione Davis e BME280

Dalla Web UI si impostano anche:

- ID trasmettitore Davis: `0` auto-lock oppure `1..8`
- dimensione scatto pluviometro in mm
- quota del BME280 per la riduzione della pressione al livello del mare
- timezone POSIX

I cumulati pioggia giorno/mese/anno e il contatore RF sono conservati in NVS per evitare azzeramenti anomali dopo un riavvio.

## Web API

A rete operativa:

- `/` - dashboard
- `/config` - configurazione completa
- `/api/status` - stato JSON
- `/api/meteobridge` - anteprima record
- **TEST UPLOAD** dalla pagina configurazione
- **RESET RETE / PORTALE SETUP** per cancellare solo la configurazione Wi-Fi

La Web UI è progettata per una LAN fidata e non deve essere esposta direttamente su Internet senza un livello di autenticazione/proxy aggiuntivo.

## Build PlatformIO

```bash
pio run -e t3-v161-868
pio run -e t3-s3-868
```

GitHub Actions compila entrambi i target su `main` e `develop`. Le versioni PlatformIO, piattaforma Espressif32 e librerie esterne sono bloccate a revisioni validate dalla CI per rendere le build riproducibili.

## Documentazione protocollo

- [Guida RF Davis - PDF italiano v1.1](docs/Davis_RF_Protocol_Guide_IT_v1.1.pdf)
- [Davis RF protocol guide - English PDF v1.1](docs/Davis_RF_Protocol_Guide_EN_v1.1.pdf)
- [Note protocollo RF - Italiano](docs/RF_PROTOCOL_IT.md)
- [RF protocol notes - English](docs/RF_PROTOCOL_EN.md)

La documentazione è tecnica e indipendente, basata sull'implementazione del firmware e su informazioni pubbliche/reverse-engineered. **Non è documentazione ufficiale Davis Instruments.**

## Branch e contributi

Vedere [CONTRIBUTING.md](CONTRIBUTING.md). Lo sviluppo ordinario parte da `develop`; la promozione a `main` avviene tramite pull request dopo revisione e CI verde.

## Sicurezza

Vedere [SECURITY.md](SECURITY.md). Il repository pubblico non deve contenere credenziali, certificati privati, token o endpoint specifici dell'installazione.

## Riferimenti e terze parti

Vedere [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) per attribuzioni, riferimenti storici di interoperabilità e note di licenza delle dipendenze/progetti citati.

## Licenza

Il codice e la documentazione originali del progetto sono distribuiti sotto **GNU Lesser General Public License v3.0 only (`LGPL-3.0-only`)**, salvo diversa indicazione nel singolo file.

Vedere [LICENSE](LICENSE) e [COPYING](COPYING).
