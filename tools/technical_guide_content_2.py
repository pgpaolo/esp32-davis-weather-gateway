story += [h1('7. Packet type Davis decodificati','s7'),
          table([
              ['Type','Nome progetto','Dato','Stato'],
              ['0x4','UV','Indice UV','implementato; formula da validare'],
              ['0x5','RAIN_RATE','Tempo fra tip / rain rate','implementato; dettagli da validare'],
              ['0x6','SOLAR','W/m2','implementato'],
              ['0x8','TEMP','Temperatura esterna','implementato'],
              ['0x9','WIND_GUST','Raffica 10 minuti','implementato'],
              ['0xA','HUMIDITY','Umidita esterna','implementato'],
              ['0xE','RAIN','Contatore tip modulo 128','implementato'],
          ], [19*mm,34*mm,58*mm,CONTENT_W-111*mm]),
          h2('7.1 Conversioni implementate'),
          P('<b>Vento</b> - ogni frame trasporta la velocita; il byte 1 viene interpretato in mph e convertito:'),
          Preformatted('wind_kmh = byte1 * 1.609344', styles['Codex']),
          P('<b>Direzione</b> - formula corrente:'),
          Preformatted('wind_dir_deg = 9.0 + byte2 * (342.0 / 255.0)', styles['Codex']),
          P('<b>Temperatura (0x8)</b>:'),
          Preformatted('raw = ((byte3 << 8) | byte4) >> 4\ntemp_f = raw / 10.0\ntemp_c = (temp_f - 32.0) * 5.0 / 9.0', styles['Codex']),
          P('<b>Umidita (0xA)</b>:'),
          Preformatted('raw = ((byte4 >> 4) << 8) | byte3\nhumidity_pct = raw / 10.0', styles['Codex']),
          P('<b>Solare (0x6)</b>:'),
          Preformatted('raw10 = byte3 * 4 + (byte4 >> 6)\nsolar_wm2 = raw10', styles['Codex']),
          P('<b>UV (0x4)</b>:'),
          Preformatted('raw = ((byte3 << 8) | byte4) >> 4\nuv = (raw - 4.0) / 200.0', styles['Codex']),
          P('<b>Pioggia (0xE)</b> - i 7 bit bassi del byte 3 sono un contatore modulo 128. Il gateway calcola il delta fra contatori e moltiplica per la dimensione del tip configurata. Delta anomali elevati vengono scartati.'),
          P('<b>Rain rate (0x5)</b> - viene ricostruito il tempo dall\'ultimo scatto e quindi:'),
          Preformatted('rain_rate_mm_h = rain_mm_per_tip * 3600 / seconds_since_tip', styles['Codex']),
          InfoBox('Da validare sul campo', 'La direzione, la conversione UV e alcuni dettagli del rain rate restano esplicitamente marcati come <b>reverse-engineered</b>. La versione 1.2 non li promuove a specifica ufficiale.', 'warn'),
          PageBreak()]

# 8 receiver strategy
story += [h1('8. Strategia di ricezione SX1276 e sincronizzazione','s8'),
          P('L\'SX1276/RFM95 e usato in modalita FSK. Poiche il trasmettitore Davis salta fra canali, il firmware applica una macchina a stati semplice: acquisizione, lock, hop successivo, contabilizzazione degli slot mancati e ritorno ad acquisizione quando la sincronizzazione non e piu credibile.'),
          table([
              ['Fase','Comportamento'],
              ['Acquisizione','Si parte da un hop noto e si attende un frame Davis-shaped con CRC valido.'],
              ['Lock','Si memorizzano ID e timing; l\'ID puo essere auto-lock oppure forzato 1..8.'],
              ['Tracking','Dopo un frame plausibile il ricevitore passa alla frequenza successiva della sequenza.'],
              ['Miss','Se non arriva un frame nella finestra attesa, viene incrementato packetsMissed e si anticipa il canale.'],
              ['Resync','Dopo molti miss il firmware azzera il lock e torna in acquisizione.'],
          ], [35*mm,CONTENT_W-35*mm]),
          h2('8.1 Parametri diagnostici'),
          bullets(['packetsOk - frame validi e decodificati;', 'crcErrors - frame che falliscono la verifica CRC;', 'packetsMissed - slot attesi senza frame valido;', 'resyncs - volte in cui il ricevitore ha abbandonato il lock;', 'RSSI - livello del segnale ricevuto;', 'canale/frequenza corrente - posizione nella sequenza hop;', 'lastRadioError - ultimo errore RadioLib/SX1276.']),
          h2('8.2 Interpretazione del segnale'),
          P('Un RSSI buono non garantisce da solo una ricezione corretta: bisogna osservare anche CRC e miss. Un segnale forte con molti CRC error puo indicare frequenza, shaping, bandwidth o timing non corretti. Un CRC quasi perfetto ma molti miss puo invece indicare una sequenza hop non allineata o un periodo nominale non adatto all\'ID trasmettitore.'),
          InfoBox('Validazione consigliata', 'Durante i primi test reali conviene registrare per alcuni minuti timestamp, hop, RSSI, packet type, ID, CRC e raw 10-byte. Questo permette di verificare timing e conversioni senza affidarsi soltanto ai valori meteo finali.', 'info'),
          PageBreak()]

# 9 Hardware
story += [h1('9. Hardware del gateway','s9'),
          table([
              ['Componente','Ruolo','Note'],
              ['LILYGO / TTGO T3 V1.6.1','MCU ESP32 + radio','target principale CI'],
              ['LILYGO T3-S3','MCU ESP32-S3 + radio','target alternativo CI'],
              ['SX1276 / RFM95 868 MHz','ricevitore Davis','necessario hardware reale per 868 MHz'],
              ['BME280','pressione + T/H locale','I2C; quota configurabile'],
              ['Wi-Fi ESP32','LAN e provisioning','DHCP default; statico opzionale'],
          ], [45*mm,53*mm,CONTENT_W-98*mm]),
          P('Una scheda radio 433 MHz non e adatta alla stazione europea Davis. La frequenza di lavoro dipende dall\'hardware RF fisico, non e sufficiente cambiare un parametro software.'),
          h2('9.1 Pinout e profili PlatformIO'),
          P('Il repository contiene un board_config dedicato ai due target e usa PlatformIO per separare i profili. La CI compila entrambi i target per evitare regressioni di pinout o incompatibilita delle librerie.'),
          Preformatted('pio run -e t3-v161-868\npio run -e t3-s3-868', styles['Codex']),
          h2('9.2 Dipendenze principali'),
          table([
              ['Libreria','Uso'],
              ['RadioLib 7.7.1','controllo SX1276 in FSK, frequenza, sync, interrupt, RSSI'],
              ['Adafruit BME280 2.3.0','lettura pressione, temperatura e umidita locale'],
              ['Adafruit Unified Sensor 1.1.15','astrazione sensori Adafruit'],
              ['Arduino ESP32 / espressif32 7.0.1','Wi-Fi, WebServer, NVS Preferences, networking'],
          ], [58*mm,CONTENT_W-58*mm]),
          PageBreak()]

# 10 Network
story += [h1('10. Provisioning Wi-Fi e configurazione runtime','s10'),
          P('Le credenziali Wi-Fi non sono compilate nel firmware pubblico. Al primo avvio, o quando non esiste una configurazione valida, il gateway crea un access point temporaneo con SSID DavisGateway-XXXX e captive portal su 192.168.4.1.'),
          table([
              ['Parametro','Comportamento'],
              ['Modalita iniziale','AP temporaneo / captive portal'],
              ['SSID setup','DavisGateway-XXXX'],
              ['IP portale','192.168.4.1'],
              ['LAN default','DHCP'],
              ['Profilo statico suggerito','192.168.1.120 / 255.255.255.0'],
              ['Gateway/DNS suggeriti','192.168.1.1'],
              ['Persistenza','NVS'],
              ['Recovery','portale automatico se la rete configurata non torna disponibile'],
          ], [58*mm,CONTENT_W-58*mm]),
          h2('10.1 Gestione password'),
          P('Il portale non rende la password Wi-Fi memorizzata nel sorgente HTML e non la stampa sulla seriale. Se l\'SSID rimane invariato e il campo password viene lasciato vuoto, la credenziale esistente viene mantenuta internamente. La cancellazione della password/rete aperta richiede una scelta esplicita.'),
          InfoBox('Raccomandazione', 'L\'AP di provisioning puo essere aperto se usato soltanto in installazione controllata; in ambienti non fidati e preferibile configurare una password AP privata di almeno 8 caratteri. L\'AP non deve restare attivo permanentemente.', 'warn'),
          PageBreak()]

# 11 Web/HTTP
story += [h1('11. Web UI, API locali e upload HTTP','s11'),
          P('A rete operativa il gateway espone una dashboard locale, la pagina di configurazione, stato JSON e anteprima del record. L\'URL del receiver non contiene valori specifici di installazione nel repository pubblico.'),
          table([
              ['Percorso','Funzione'],
              ['/','dashboard meteo e diagnostica'],
              ['/config','configurazione completa'],
              ['/api/status','stato JSON'],
              ['/api/meteobridge','anteprima record compatibile'],
              ['TEST UPLOAD','invio immediato al receiver'],
              ['RESET RETE','cancella configurazione Wi-Fi e riapre setup'],
          ], [48*mm,CONTENT_W-48*mm]),
          h2('11.1 Endpoint mb.php compatibile'),
          P('Il firmware costruisce un record di 192 posizioni, usa -- per i campi non disponibili e invia il record URL-encoded tramite parametro d. Un invio e considerato riuscito solo se il server risponde HTTP 200 con corpo success.'),
          Preformatted('GET https://server.example/weather/mb.php?d=<record-url-encoded>', styles['Codex']),
          h2('11.2 TLS'),
          P('La verifica del certificato HTTPS e il comportamento predefinito. La modalita senza verifica deve essere abilitata esplicitamente dall\'installatore quando l\'ambiente non consente una CA valida. In una installazione definitiva e preferibile usare un certificato attendibile anziche setInsecure.'),
          InfoBox('Superficie Web', 'La Web UI attuale e pensata per <b>LAN fidata</b> e non include ancora autenticazione applicativa completa. Non va pubblicata direttamente su Internet; se serve accesso remoto, usare VPN o reverse proxy con autenticazione.', 'warn'),
          PageBreak()]

# 12 persistence
story += [h1('12. Persistenza dati e stato meteorologico','s12'),
          P('La NVS viene usata per separare configurazione e stato persistente. Oltre alle credenziali e ai parametri runtime, il progetto conserva i cumulati pluviometrici e il contatore RF necessario a non introdurre salti dopo un riavvio.'),
          table([
              ['Dato','Persistenza','Scopo'],
              ['SSID/password Wi-Fi','NVS','riconnessione senza ricompilazione'],
              ['DHCP/statico/hostname','NVS','configurazione rete'],
              ['ID Davis / rain tip / quota / timezone','NVS','configurazione stazione'],
              ['URL receiver / intervallo / TLS','NVS','upload HTTP'],
              ['Pioggia giorno/mese/anno/ieri','NVS','continuita cumulati'],
              ['Ultimo contatore rain','NVS','delta corretto dopo reboot'],
          ], [55*mm,40*mm,CONTENT_W-95*mm]),
          h2('12.1 Rollover temporali'),
          P('Dopo la sincronizzazione NTP il firmware riconcilia il giorno, il mese e l\'anno salvati, spostando i cumulati quando cambia il calendario. Prima dell\'orario valido i dati vengono conservati senza forzare un rollover basato su una data non affidabile.'),
          PageBreak()]

# 13 security
