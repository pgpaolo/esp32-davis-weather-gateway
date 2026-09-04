# Diagnostica estesa Davis RF / FHSS

Versione firmware di riferimento: **0.3.2-dev**.

Questa diagnostica è progettata per il collaudo reale del ricevitore Davis Vantage Pro2 EU 868 MHz. Non modifica il decoder, le frequenze, il CRC o la logica di hopping: osserva e registra ciò che il ricevitore sta già facendo.

## Obiettivo

Permettere di distinguere rapidamente fra:

- assenza totale di traffico RF;
- traffico RF presente ma framing/bit order non corretto;
- frame Davis candidati con CRC errato;
- frame Davis validi senza lock stabile;
- lock FHSS stabile;
- perdita di sincronizzazione o problemi su uno specifico canale.

## Stati RF

- **ERROR**: SX1276 non inizializzato.
- **SEARCH**: radio pronta, nessun frame candidato ricevuto.
- **CANDIDATE**: almeno un frame da 10 byte ricevuto, ma nessun lock Davis stabile.
- **SYNC**: frame Davis validi e sincronizzazione FHSS acquisita.

## Diagnostica per i cinque canali

Per ciascun canale Davis EU vengono mantenuti:

- frequenza;
- frame RAW ricevuti;
- CRC validi;
- CRC errati;
- errori di lettura RadioLib;
- RSSI ultimo;
- RSSI medio;
- RSSI minimo e massimo;
- età dell'ultima ricezione.

Hop set implementato:

1. 868.066711 MHz
2. 868.297119 MHz
3. 868.527466 MHz
4. 868.181885 MHz
5. 868.412292 MHz

Durante SEARCH il firmware rimane sul canale di acquisizione previsto dalla strategia Davis del progetto. Dopo il lock le statistiche permettono di verificare la continuità dell'intera sequenza FHSS.

## RAW, NORMALIZED e CRC

Per ogni frame candidato vengono registrati i 10 byte ricevuti direttamente dall'SX1276 e i 10 byte dopo il bit reversal usato dal protocollo Davis.

La pagina diagnostica mostra separatamente:

- `RAW`: byte come ricevuti dal chip radio;
- `NORMALIZED`: byte dopo il bit reversal;
- `CRC RX`: CRC contenuto nel frame normalizzato;
- `CRC CALC`: CRC16-CCITT calcolato sui byte 0..5;
- esito `CRC OK` / `CRC KO`.

Questo consente di capire immediatamente se il problema è prima o dopo la normalizzazione.

## Buffer degli ultimi frame

Il firmware mantiene in RAM una memoria circolare degli ultimi **24 frame candidati**. Per ogni frame sono disponibili:

- numero progressivo;
- età;
- canale e frequenza;
- RSSI;
- CRC ricevuto e calcolato;
- RAW hex;
- NORMALIZED hex;
- codice RadioLib.

Il buffer è volutamente RAM-only: non viene effettuata scrittura continua sulla flash.

## Timing e jitter

Per i frame candidati vengono calcolati:

- intervallo atteso Davis: 2555 ms;
- ultimo intervallo;
- minimo;
- massimo;
- media;
- jitter medio assoluto rispetto all'intervallo atteso;
- numero di campioni.

Valori molto lontani dal timing nominale possono indicare perdita di hop, ricezione intermittente o framing non coerente.

## Radio path

Sono esposti anche i contatori interni del percorso SX1276:

- IRQ DIO0;
- `readData()` riuscite/fallite;
- tune eseguiti/falliti;
- hop eseguiti;
- errori `startReceive()`;
- miss streak corrente;
- ultimo codice RadioLib.

Questi dati aiutano a separare un problema hardware/radio da un problema di decodifica.

## System health

La Web UI mostra:

- uptime;
- heap libero;
- minimo heap libero osservato;
- frequenza CPU;
- RSSI Wi-Fi;
- motivo dell'ultimo reset ESP32.

Il report scaricabile include inoltre lo stato BME280, AS3935, MQTT e HTTP upload.

## Diagnostic capture 60 s

Il pulsante **Diagnostic capture 60 s** azzera la finestra diagnostica RF e lascia raccogliere un minuto di traffico. Con un intervallo Davis di circa 2,555 s, il buffer da 24 frame copre approssimativamente un minuto di ricezione.

Al termine usare **Scarica report**. Il file `davis-diagnostic.txt` è pensato per essere allegato a una issue o incollato durante il collaudo.

## Interpretazione rapida

### RAW = 0

La radio non sta consegnando frame candidati da 10 byte. Controllare in ordine:

1. ISS accesa e trasmittente;
2. modulo realmente 868 MHz;
3. antenna 868 MHz;
4. pinout SX1276;
5. distanza e schermature;
6. inizializzazione FSK e codice RadioLib.

### RAW aumenta, CRC sempre KO

Il ricevitore sente traffico candidato. Confrontare:

- RAW;
- NORMALIZED;
- CRC RX / CRC CALC;
- RSSI;
- timing;
- distribuzione sui canali.

Questo è lo scenario più utile per correggere framing, bit order o parametri FSK senza modificare alla cieca il decoder.

### CRC OK ma nessun SYNC

Verificare:

- ID ISS configurato;
- continuità degli hop;
- miss streak;
- timing;
- distribuzione dei frame validi sui cinque canali.

### SYNC stabile

La parte RF principale è operativa. A quel punto il collaudo passa alla correttezza delle conversioni meteo e alla continuità nel tempo.

## API

- `/api/rf` - stato RF senza history completa
- `/api/rf/diagnostics` - diagnostica RF completa con history
- `POST /api/rf/reset` - azzera la finestra diagnostica
- `/api/diag/report` - report testuale scaricabile
- `/api/system` - health ESP32
- `/api/state` - stato aggregato gateway

## Nota

Le statistiche estese sono strumenti di osservazione. Il motore meteo RF resta esclusivamente Davis Vantage Pro2 EU 868 MHz FHSS / 2-FSK e la diagnostica non introduce decoder Oregon Scientific, Technoline/LaCrosse o modalità 433 MHz.
