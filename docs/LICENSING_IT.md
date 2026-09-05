# Licenza, attribuzioni e provenienza

Versione documentazione: **0.4.0-dev**  
Progetto: **ESP32 Davis Weather Gateway**

## Licenza principale

Il codice e la documentazione originali di questo repository sono distribuiti, salvo diversa indicazione nel singolo file, sotto **GNU Lesser General Public License v3.0 only (`LGPL-3.0-only`)**.

Il testo canonico della LGPLv3 è contenuto in `LICENSE`. Il file `COPYING` contiene il testo GPLv3 incorporato per riferimento dalla LGPLv3. Il testo GNU non viene modificato o personalizzato; le informazioni specifiche del progetto restano separate in `NOTICE.md` e `THIRD_PARTY_NOTICES.md`.

## Copyright

Copyright (C) 2026 **Gianpaolo P. (`@pgpaolo`) e contributori**.

Le ridistribuzioni del sorgente devono conservare le informazioni di copyright e licenza applicabili e le attribuzioni richieste dalle licenze dei componenti effettivamente redistribuiti.

## Componenti di terzi

Le librerie indicate in `platformio.ini` mantengono le rispettive licenze upstream. La LGPLv3 del progetto non modifica né sostituisce tali licenze.

I riferimenti pubblici utilizzati per interoperabilità Davis sono elencati in `THIRD_PARTY_NOTICES.md`. Essere citati come riferimento tecnico non significa che i relativi sorgenti siano incorporati nel repository.

## Modifiche e fork

Chi pubblica un fork o una versione modificata dovrebbe:

1. conservare gli avvisi di copyright/licenza richiesti;
2. indicare chiaramente le modifiche sostanziali;
3. non presentare il progetto come firmware o documentazione ufficiale Davis Instruments;
4. conservare le attribuzioni di terzi applicabili;
5. distinguere dati ufficiali del produttore, comportamento implementato dal progetto e informazioni di interoperabilità/reverse engineering.

## SPDX

Per i nuovi file sorgente originali può essere usata l'intestazione:

```text
SPDX-License-Identifier: LGPL-3.0-only
```

L'identificatore SPDX non sostituisce `LICENSE`, `NOTICE.md` o le attribuzioni obbligatorie di terzi.

## Marchi e non affiliazione

Davis Instruments, Vantage Pro2, Vantage Vue e gli altri nomi/prodotti citati appartengono ai rispettivi titolari. Il progetto è indipendente e non è affiliato, sponsorizzato o approvato da Davis Instruments. I nomi sono usati esclusivamente a fini descrittivi, di interoperabilità e documentazione tecnica.

## Garanzia e utilizzo dei dati

Valgono le esclusioni di garanzia previste dalla GNU LGPLv3/GPLv3. RF, meteorologia, pressione e rilevamento fulmini devono essere validati per il contesto d'uso prima di impieghi safety-critical o operativi.

## File di riferimento

- `LICENSE` - GNU LGPLv3
- `COPYING` - GNU GPLv3 incorporata dalla LGPLv3
- `NOTICE.md` - copyright, provenienza, marchi e disclaimer
- `THIRD_PARTY_NOTICES.md` - dipendenze e riferimenti di terzi
- `CONTRIBUTING.md` - regole per i contributi
- `SECURITY.md` - confine di sicurezza del firmware
