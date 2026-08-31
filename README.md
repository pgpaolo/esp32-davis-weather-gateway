# ESP32 Davis Weather Gateway

Gateway autonomo **ESP32/LILYGO 868 MHz** per ricevere direttamente una **Davis Vantage Pro2 / Pro2 Plus wireless europea**, senza console Davis e senza Meteobridge.

Il progetto riceve la Davis ISS via **868 MHz FHSS**, decodifica i dati meteo, completa la pressione con un **BME280 locale**, offre configurazione e diagnostica web e può inviare un record compatibile Meteobridge/Weather34 a un endpoint HTTP configurabile.

> Branch `develop`: sviluppo attivo. `main` viene mantenuto separato fino alla validazione della nuova release.

## Hardware target

- LILYGO / TTGO LoRa32 T3 V1.6.1 con **SX1276/RFM95 868 MHz**
- opzionale LILYGO T3-S3 + SX1276 868 MHz
- Davis Vantage Pro2 / Pro2 Plus wireless EU
- BME280 su I2C per pressione atmosferica e T/H locale

Una scheda radio destinata esclusivamente a 433 MHz non è adatta: serve una variante hardware corretta per la banda 868 MHz.

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

## RF Davis EU

Il ricevitore usa 2-FSK a 19.2 kbps, deviazione 4.8 kHz, shaping Gaussian BT=0.5, sync `CB 89` e pacchetto fisso di 10 byte con CRC16-CCITT Davis.

Hop set EU implementato:

1. 868.066711 MHz
2. 868.297119 MHz
3. 868.527466 MHz
4. 868.181885 MHz
5. 868.412292 MHz

Dopo l'acquisizione il firmware segue i salti con periodo nominale di circa 2.555 s; in caso di perdita effettua il recupero della sequenza e torna alla fase di acquisizione.

## Primo avvio e configurazione Wi-Fi

SSID e password **non vengono inseriti nel sorgente pubblico**.

Al primo avvio, o quando non esiste una configurazione valida, il gateway crea un access point temporaneo:

```text
DavisGateway-XXXX
```

Per impostazione predefinita l'AP di setup è aperto; può essere protetto definendo `PROVISION_AP_PASSWORD` in `config_private.h`.

Il portale captive è disponibile su:

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

`192.168.1.120` è solo il valore suggerito: va verificato che sia libero e compatibile con la LAN dell'installazione.

Se una rete già configurata non è raggiungibile per 60 secondi, il gateway attiva automaticamente il portale di recovery mantenendo i parametri salvati. Sul profilo T3-S3 è inoltre previsto il richiamo manuale del provisioning tramite pressione prolungata del pulsante utente.

Tutta la configurazione è salvata in **NVS**.

## Endpoint HTTP / `mb.php`

L'endpoint non contiene alcun indirizzo predefinito e viene deciso dall'utente. Esempio puramente generico:

```text
https://server.example/weather/mb.php
```

Il firmware aggiunge il parametro:

```text
?d=<record Meteobridge/Weather34 URL-encoded>
```

Un invio è considerato riuscito solo con risposta HTTP `200` e corpo `success`.

La dashboard offre:

- configurazione URL receiver
- intervallo upload
- test manuale dell'invio
- anteprima del record generato
- scelta modalità TLS

## Configurazione Davis e BME280

Dalla Web UI si impostano anche:

- ID trasmettitore Davis: `0` auto-lock oppure `1..8`
- dimensione scatto pluviometro in mm
- quota del BME280 per la riduzione della pressione al livello del mare
- timezone POSIX

I cumulati pioggia giorno/mese/anno e il contatore RF vengono conservati in NVS per evitare azzeramenti anomali dopo un riavvio.

## Web API

A rete operativa:

- `/` dashboard
- `/config` configurazione completa
- `/api/status` stato JSON
- `/api/meteobridge` anteprima record
- `TEST UPLOAD` dalla pagina configurazione
- `RESET RETE / PORTALE SETUP` per cancellare solo la configurazione Wi-Fi

## Build PlatformIO

```bash
pio run -e t3-v161-868
```

oppure:

```bash
pio run -e t3-s3-868
```

Le build CI vengono eseguite su `main` e `develop` tramite GitHub Actions.

## Documentazione protocollo

Nel branch `develop` è presente la guida tecnica:

`docs/Guida_Codifiche_RF_Davis_Vantage_Pro2_EU_Edizione_1.pdf`

La guida descrive il livello RF, il frequency hopping europeo, il framing, il CRC, la mappa dei pacchetti e le conversioni implementate. È documentazione tecnica indipendente basata su informazioni pubbliche e reverse engineering e non è documentazione ufficiale Davis Instruments.

## Stato del progetto

`0.2.0-dev`: implementazione Davis-only in fase di validazione su hardware reale. Prima della promozione su `main` vanno verificati sul campo sincronizzazione FHSS, valori dei diversi packet type e continuità dell'upload HTTP.

## Licenza

GPL-3.0.
