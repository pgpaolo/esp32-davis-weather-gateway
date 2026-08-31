# ESP32 Davis Weather Gateway

Gateway autonomo **ESP32/LILYGO 868 MHz** per ricevere direttamente una **Davis Vantage Pro2 / Pro2 Plus wireless europea**, senza console Davis e senza Meteobridge.

Il progetto riceve la Davis ISS via **868.0–868.6 MHz FHSS**, decodifica i dati meteo, completa la pressione con un **BME280 locale**, offre una dashboard web e invia un record compatibile Meteobridge/Weather34 a un endpoint `mb.php` configurabile.

## Hardware target

- LILYGO / TTGO LoRa32 T3 V1.6.1 con **SX1276/RFM95 868 MHz**
- opzionale LILYGO T3-S3 + SX1276 868 MHz
- Davis Vantage Pro2 / Pro2 Plus wireless EU
- BME280 su I2C per pressione (e T/H locale)

> Una scheda SX1278 433 MHz non è adatta: serve la variante radio 868 MHz (SX1276/RFM95 o equivalente supportata dal profilo).

## Sensori

Dalla Davis ISS:

- temperatura esterna
- umidità esterna
- velocità e direzione vento
- raffica
- pioggia / rain rate
- UV
- radiazione solare
- stato batteria trasmettitore

Dal gateway:

- pressione atmosferica BME280 ridotta al livello del mare
- temperatura/umidità locale BME280

## RF Davis EU

Il ricevitore usa FSK a 19.2 kbps, deviazione 4.8 kHz, Gaussian shaping BT=0.5, sync `CB 89` e pacchetto fisso di 10 byte con CRC16-CCITT Davis.

Hop set EU usato dal firmware:

1. 868.066711 MHz
2. 868.297119 MHz
3. 868.527466 MHz
4. 868.181885 MHz
5. 868.412292 MHz

Dopo l'acquisizione il firmware segue i salti con periodo nominale di circa 2.555 s; in caso di perdita esegue hop-ahead e poi riacquisizione.

## `mb.php` / Aurora / DIGA

La Web UI permette di impostare, ad esempio:

```text
https://meteostz05013.ddns.net/diga/mbridge/mb.php
```

Il firmware aggiunge:

```text
?d=<record Meteobridge/Weather34 URL-encoded>
```

L'invio è considerato riuscito soltanto se il server risponde HTTP `200` e corpo `success`.

Il payload ha 192 posizioni per rimanere compatibile con le estensioni Weather34/Aurora; i campi Davis/BME disponibili sono valorizzati, quelli non applicabili restano `--`/compatibili.

## Configurazione

```bash
cp src/config_private.example.h src/config_private.h
```

Impostare almeno Wi-Fi. Tutte le impostazioni operative di `mb.php`, intervallo upload, ID ISS e rain-tip possono poi essere modificate dalla Web UI e sono salvate in NVS.

## Build PlatformIO

```bash
pio run -e t3-v161-868
```

oppure:

```bash
pio run -e t3-s3-868
```

## Web API

- `/` dashboard/configurazione
- `/api/status` stato JSON
- `/api/meteobridge` anteprima del record che verrebbe inviato a `mb.php`
- pulsante **TEST INVIO** nella dashboard

## Stato del progetto

`0.1.0-alpha1`: prima implementazione Davis-only. Il decoder e la logica RF sono progettati per test sul campo con ISS europea reale; prima di considerare stabile il firmware vanno verificati ricezione, sincronizzazione hop e valori UV/solare/pioggia sull'hardware definitivo.

## Riferimenti tecnici

Il protocollo è stato implementato a partire da informazioni pubbliche/reverse-engineered sul collegamento Davis ISS, incluse le specifiche RF Davis e i progetti storici DavisRFM69. Il codice di questo repository è una nuova implementazione per ESP32/SX1276 e RadioLib.

## Licenza

GPL-3.0, in continuità con il progetto ESP32 Oregon/Technoline da cui deriva l'architettura generale del gateway.
