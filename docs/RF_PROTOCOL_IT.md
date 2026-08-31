# Note tecniche protocollo RF Davis Vantage Pro2 / Pro2 Plus EU

Edizione 1.1 - 31 agosto 2026

Questa pagina riassume il comportamento RF implementato da ESP32 Davis Weather Gateway. È documentazione indipendente di interoperabilità/reverse engineering e **non una specifica ufficiale Davis Instruments**. Alcune conversioni richiedono ancora validazione su hardware reale.

## Architettura

```text
Davis ISS EU
   | 868 MHz / 2-FSK / FHSS
   v
SX1276 / RFM95
   | SPI
   v
ESP32 / LILYGO
   |-- decoder Davis
   |-- pressione BME280 locale
   |-- Web UI / diagnostica
   `-- upload HTTP opzionale
```

Il progetto è Davis-only: questa variante non contiene decoder Oregon Scientific o Technoline.

## Profilo RF

| Parametro | Valore implementato |
|---|---|
| Banda | 868 MHz EU |
| Modulazione | 2-FSK |
| Bit rate | 19.2 kbps |
| Deviazione RX | 4.8 kHz |
| Shaping | Gaussian BT=0.5 |
| RX bandwidth | 25 kHz |
| AFC bandwidth | 50 kHz |
| Preambolo | 4 x `0xAA` |
| Sync | `0xCB 0x89` |
| Payload | 10 byte fissi |
| CRC | CRC16-CCITT in firmware |

Il CRC hardware SX1276 è disabilitato perché il frame Davis contiene il proprio CRC.

## Hop set europeo

| Hop | FRF | Frequenza |
|---:|---|---:|
| 1 | `D9 04 45` | 868.066711 MHz |
| 2 | `D9 13 04` | 868.297119 MHz |
| 3 | `D9 21 C2` | 868.527466 MHz |
| 4 | `D9 0B A4` | 868.181885 MHz |
| 5 | `D9 1A 63` | 868.412292 MHz |

Il firmware usa un intervallo nominale di circa 2555 ms fra pacchetti. Dopo un frame Davis-shaped passa all'hop successivo; dopo una sequenza prolungata di mancati pacchetti torna in acquisizione. Questa è una strategia del gateway e non una specifica ufficiale della console Davis.

## Frame da 10 byte

Prima della decodifica ogni byte RF viene normalizzato tramite inversione dei bit.

| Byte | Uso corrente |
|---:|---|
| 0 | tipo pacchetto + batteria + ID trasmettitore |
| 1 | velocità vento |
| 2 | direzione vento |
| 3 | payload specifico |
| 4 | payload specifico |
| 5 | dati/riservato, incluso nel CRC primario |
| 6 | CRC MSB |
| 7 | CRC LSB |
| 8 | campo aggiuntivo/retransmit, non validato nella alpha corrente |
| 9 | campo aggiuntivo/retransmit, non validato nella alpha corrente |

Il byte 0 è trattato come `TTTT B III`: nibble alto tipo, bit 3 batteria, bit 2..0 ID 0..7. La UI visualizza 1..8 e usa `0` per auto-lock.

## Packet type decodificati

| Type | Nome | Misura |
|---|---|---|
| `0x4` | UV | indice UV |
| `0x5` | RAIN_RATE | secondi dall'ultimo scatto / rain rate |
| `0x6` | SOLAR | radiazione solare |
| `0x8` | TEMP | temperatura esterna |
| `0x9` | WIND_GUST | raffica 10 minuti |
| `0xA` | HUMIDITY | umidità esterna |
| `0xE` | RAIN | contatore scatti pluviometro |

Gli altri type validi possono essere registrati come `OTHER` senza aggiornare un sensore specifico.

## CRC16-CCITT

Il CRC viene calcolato sui byte normalizzati 0..5 con polinomio `0x1021` e valore iniziale `0`, quindi confrontato con i byte 6..7 interpretati MSB first.

## Conversioni correnti

Vento:

```text
wind_kmh = byte1 * 1.609344
```

Direzione:

```text
wind_dir_deg = 9.0 + byte2 * (342.0 / 255.0)
```

La mappatura della direzione deve essere verificata con una ISS reale, soprattutto nel passaggio 0/360 gradi.

Temperatura (`0x8`):

```text
raw = ((byte3 << 8) | byte4) >> 4
temp_f = raw / 10.0
temp_c = (temp_f - 32.0) * 5.0 / 9.0
```

Umidità (`0xA`):

```text
raw = ((byte4 >> 4) << 8) | byte3
humidity_pct = raw / 10.0
```

Raffica (`0x9`):

```text
gust_kmh = byte3 * 1.609344
```

Solare (`0x6`):

```text
raw10 = byte3 * 4 + (byte4 >> 6)
solar_wm2 = raw10
```

UV (`0x4`):

```text
raw = ((byte3 << 8) | byte4) >> 4
uv = (raw - 4.0) / 200.0
```

La conversione UV richiede esplicitamente confronto sul campo con un riferimento Davis.

Il contatore pioggia (`0xE`) usa i 7 bit bassi del byte 3 come contatore modulo 128. La dimensione dello scatto è configurabile. Delta anomali elevati vengono scartati.

Il rain rate (`0x5`) ricostruisce i secondi dall'ultimo scatto da byte 3 e nibble alto di byte 4, quindi calcola:

```text
rain_rate_mm_h = rain_mm_per_tip * 3600 / seconds_since_tip
```

## Pressione atmosferica e architettura Davis

Per la **Wireless Vantage Pro2 Sensor Suite 6322/6322M** usata come riferimento, la pressione barometrica non è tra i sensori dell'ISS esterna. Davis elenca per la sensor suite temperatura e umidità esterne, velocità e direzione del vento e pioggia; UV e radiazione solare sono presenti sulle configurazioni Pro2 Plus o come sensori aggiuntivi.

Nel sistema Davis il barometro è invece collocato sul lato ricevente. **WeatherLink Live** dichiara sensori integrati per pressione barometrica, temperatura interna e umidità interna; anche **Weather Envoy** include barometro e temperatura/umidità interna. Di conseguenza, per l'ISS 6322/6322M, il gateway non deve cercare un packet type RF di pressione.

ESP32 Davis Weather Gateway replica la stessa separazione funzionale usando un **BME280 locale** sul gateway. Il valore misurato viene ridotto al livello del mare in funzione della quota configurata:

```text
P0 = Pstation / (1 - altitude_m / 44330) ^ 5.255
```

Il BME280 deve essere installato in modo da comunicare correttamente con la pressione ambiente e, per letture termiche locali attendibili, va tenuto lontano dalle principali sorgenti di calore della scheda ESP32.

## Diagnostica FHSS

Durante la validazione su hardware reale è utile controllare almeno:

- `packetsOk`
- `crcErrors`
- `packetsMissed`
- `resyncs`
- RSSI
- canale/frequenza corrente
- `lastRadioError`

Un lock apparentemente stabile con molti slot persi può indicare problemi di timing, offset RF, antenna o sequenza di hopping.

## Persistenza e provisioning

I cumulati pioggia e il contatore RF sono persistenti in NVS. Anche le credenziali Wi-Fi sono memorizzate in NVS e non inserite nel sorgente pubblico. Il primo avvio usa l'AP temporaneo `DavisGateway-XXXX` su `192.168.4.1`; DHCP è il default LAN, mentre `192.168.1.120` è soltanto l'indirizzo statico suggerito.

## Limiti noti Edizione 1.1

1. La guida descrive il decoder `0.2.0-dev`, non una release stabile.
2. I packet type non elencati non vengono decodificati.
3. I byte 8-9 non sono validati nella alpha corrente.
4. UV, direzione e alcuni dettagli rain-rate richiedono verifica sul campo.
5. La sincronizzazione SX1276 è logica del progetto, non un algoritmo Davis ufficiale.
6. La pressione è volutamente locale al gateway e non deriva dal frame RF della ISS 6322/6322M.

## Riferimenti tecnici pubblici

Riferimenti Davis ufficiali utilizzati per distinguere i sensori ISS dai sensori lato ricevitore:

- https://www.davisinstruments.com/products/wireless-vantage-pro2-integrated-sensor-suite
- https://www.davisinstruments.com/products/weatherlink-live
- https://www.davisinstruments.com/products/cabled-weather-envoy-3

Vedere inoltre [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) per attribuzioni e note di licenza relative ai progetti storici di reverse engineering Davis ISS.
