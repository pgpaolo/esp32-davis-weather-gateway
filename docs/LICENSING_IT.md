# Licenza, attribuzioni e provenienza

Versione documentazione: **0.3.2-dev**  
Progetto: **ESP32 Davis Weather Gateway**

## 1. Licenza principale

Il codice e la documentazione originali di questo repository sono distribuiti, salvo diversa indicazione nel singolo file, sotto:

**GNU Lesser General Public License v3.0 only (`LGPL-3.0-only`)**.

Il testo canonico della LGPLv3 è contenuto in `LICENSE`. Il file `COPYING` contiene il testo GPLv3 incorporato per riferimento dalla LGPLv3.

Il testo GNU non viene modificato o personalizzato. Le personalizzazioni del progetto sono mantenute separatamente in `NOTICE.md`, `THIRD_PARTY_NOTICES.md` e in questa guida.

## 2. Copyright e attribuzione del progetto

Copyright (C) 2026 **Gianpaolo P. (`@pgpaolo`) e contributori**.

Le ridistribuzioni del sorgente devono conservare le informazioni di copyright e licenza applicabili e tutte le attribuzioni richieste dalle licenze dei componenti effettivamente redistribuiti.

Il file `NOTICE.md` descrive autore/provenienza, marchi e natura indipendente del progetto. Non introduce condizioni aggiuntive alla LGPLv3.

## 3. Regola adottata dal progetto Oregon / Technoline

Il precedente progetto ESP32 Oregon Scientific + Technoline adottava una regola importante: **provenienza e attribuzioni non devono essere nascoste**. Quando una parte deriva o prende riferimento da lavoro di terzi, la documentazione e, quando necessario, il sorgente devono identificarne chiaramente origine e licenza.

Lo stesso criterio viene adottato qui, ma senza trasferire automaticamente la licenza GPL dell'altro progetto al gateway Davis.

Nel progetto Davis:

- il motore RF meteorologico è esclusivamente Davis EU 868 MHz FHSS;
- non sono inclusi decoder Oregon Scientific;
- non sono inclusi decoder Technoline/LaCrosse;
- non vengono importate le parti GPL derivate da `rtl_433` o PracticalArduino presenti/riferite nell'altro progetto;
- le parti applicative originali dello stesso maintainer possono essere adattate o reimplementate e distribuite qui sotto LGPLv3 quando il titolare del copyright ne possiede i diritti.

## 4. Riferimenti pubblici Davis

La decodifica Davis è una implementazione indipendente ESP32/SX1276 basata su informazioni pubbliche di interoperabilità e reverse engineering.

Fra i riferimenti storici documentati:

- **DavisRFM69** di DeKay;
- **ISS-MQTT-Gateway** di dcbo.

Questi progetti sono riferimenti tecnici. Il repository Davis non intende incorporarne direttamente i sorgenti. Le relative licenze e attribuzioni sono riportate in `THIRD_PARTY_NOTICES.md`.

## 5. Dipendenze software

Le librerie dichiarate in `platformio.ini` — RadioLib, U8g2, Adafruit BME280/Unified Sensor/BusIO, PubSubClient, AS3935MI e le dipendenze del framework ESP32/Arduino — mantengono le rispettive licenze upstream.

La LGPLv3 del progetto non modifica né sostituisce tali licenze.

## 6. Modifiche e fork

Chi pubblica un fork o una versione modificata dovrebbe:

1. conservare le informazioni di copyright/licenza richieste;
2. indicare chiaramente le modifiche sostanziali e, preferibilmente, la loro data;
3. non presentare il progetto come firmware o documentazione ufficiale Davis Instruments;
4. conservare le attribuzioni di terzi applicabili;
5. distinguere sempre fatti verificati, reverse engineering e ipotesi ancora da validare su hardware reale.

## 7. SPDX

Per i nuovi file sorgente originali del progetto può essere utilizzata l'intestazione:

```text
SPDX-License-Identifier: LGPL-3.0-only
```

L'uso di SPDX semplifica l'identificazione della licenza ma non sostituisce `LICENSE`, `NOTICE.md` o le attribuzioni obbligatorie di terzi.

## 8. Marchi e non affiliazione

Davis Instruments, Vantage Pro2, Vantage Vue e gli altri nomi/prodotti citati appartengono ai rispettivi titolari.

Il progetto è indipendente e non è affiliato, sponsorizzato o approvato da Davis Instruments. L'uso dei nomi è descrittivo e finalizzato a interoperabilità, studio e documentazione tecnica.

## 9. Garanzia e utilizzo dei dati

Valgono le esclusioni di garanzia previste dalla GNU LGPLv3/GPLv3. Il firmware è ancora in sviluppo e i dati RF, meteorologici, barometrici e di rilevamento fulmini devono essere validati prima di utilizzi operativi, commerciali o safety-critical.

## 10. File di riferimento

- `LICENSE` — GNU LGPLv3
- `COPYING` — GNU GPLv3 incorporata dalla LGPLv3
- `NOTICE.md` — copyright, provenienza, marchi e disclaimer progetto
- `THIRD_PARTY_NOTICES.md` — riferimenti e attribuzioni di terzi
- `CONTRIBUTING.md` — regole per contributi e provenance
- `SECURITY.md` — confine di sicurezza del firmware
