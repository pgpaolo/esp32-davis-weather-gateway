story += [h1('13. Sicurezza del progetto e del repository','s13'),
          SecurityDiagram(),
          P('Figura 5 - Principi di sicurezza applicati al firmware e al repository.', 'Captionx'),
          table([
              ['Area','Controllo corrente','Rischio residuo'],
              ['Segreti','nessuna credenziale reale nel repository; config privata ignorata','segreti possono comunque essere estratti da un dispositivo fisico senza protezioni flash'],
              ['Password Web','non renderizzata nel DOM; non stampata in seriale','Web UI senza login applicativo'],
              ['TLS upload','verifica certificato di default','modalita insecure disponibile come opt-in'],
              ['Input HTML','escaping di & < > " e apostrofo','validazione semantica URL/hostname migliorabile'],
              ['GitHub Actions','permessi minimi e action fissate a SHA','dipendenze Python/PlatformIO richiedono manutenzione'],
              ['main','promozione via PR + required checks','protezione dipende dal ruleset GitHub configurato'],
          ], [34*mm,68*mm,CONTENT_W-102*mm], font=7.0),
          h2('13.1 Raccomandazioni per una release stabile'),
          bullets(['aggiungere autenticazione amministrativa per /config e per le azioni POST;',
                   'aggiungere token CSRF o sessione locale per le operazioni mutative;',
                   'considerare flash encryption / secure boot se il dispositivo contiene credenziali di valore;',
                   'mantenere l\'endpoint di upload HTTPS con certificato verificabile;',
                   'non pubblicare dump NVS, log contenenti password o endpoint privati nelle issue GitHub.']),
          PageBreak()]

story += [h1('14. Piano di validazione su stazione Davis reale','s14'),
          P('Prima di dichiarare una release stabile, la parte RF deve essere verificata con una stazione reale e, quando possibile, confrontata in parallelo con una console o WeatherLink ufficiale.'),
          table([
              ['Test','Metodo','Criterio'],
              ['Lock RF','accensione a diverse fasi della sequenza','acquisizione ripetibile senza intervento manuale'],
              ['Hop timing','log timestamp/frequenza per >=10 min','miss limitati e resync rari'],
              ['CRC','conteggio frame validi/errori','errori coerenti con qualita RF, non sistematici'],
              ['Temperatura/umidita','confronto con console Davis','stessa tendenza e valori plausibili'],
              ['Direzione vento','orientamenti noti / confronto console','nessun offset o wrap errato 0/360'],
              ['Pioggia','tip manuali controllati','0,2 mm/tip metric e cumulati persistenti'],
              ['Rain rate','sequenze tip a intervalli noti','formula coerente col tempo fra tip'],
              ['UV/solare','confronto Pro2 Plus / console','conversioni coerenti'],
              ['Pressione','confronto BME280 vs ricevitore Davis','differenza compatibile dopo correzione quota'],
          ], [35*mm,65*mm,CONTENT_W-100*mm], font=7.0),
          h2('14.1 Log diagnostico minimo'),
          Preformatted('timestamp  hop  MHz        RSSI   ID  type  CRC  raw[10]\n12:04:31   3   868.527466 -71.5  1   0x8   OK   ...', styles['Codex']),
          InfoBox('Obiettivo della v1.2', 'Il documento descrive con precisione cio che il progetto sa oggi, ma lascia visibili i punti da dimostrare sul campo. La validazione reale dovra trasformare gradualmente le etichette [R] in comportamenti confermati [P-validated].', 'ok'),
          PageBreak()]

story += [h1('15. Diagnostica e troubleshooting','s15'),
          table([
              ['Sintomo','Controlli prioritari'],
              ['Nessun pacchetto Davis','hardware realmente 868 MHz; antenna; pin SX1276; sync FSK; frequenze EU'],
              ['RSSI presente ma CRC quasi sempre errato','bit rate/deviazione/bandwidth; bit reversal; sync; timing hop'],
              ['Molti packetsMissed','periodo trasmissione legato all\'ID; sequenza hop; latenza nel loop; interferenze'],
              ['Valori vento plausibili ma direzione errata','formula byte2, offset 9°, wrap 0/360; orientamento anemometro'],
              ['Pioggia non accumula','packet 0xE, contatore modulo 128, rain tip configurato'],
              ['Pressione fuori scala','quota errata; BME280 non ventilato; indirizzo I2C 0x76/0x77'],
              ['Upload fallisce HTTPS','CA non configurata/certificato; DNS; ora NTP; endpoint'],
              ['Portale non appare','modalita AP; IP 192.168.4.1; DNS captive portal; recovery timeout'],
          ], [52*mm,CONTENT_W-52*mm], font=7.2),
          h2('15.1 Indicatori di una ricezione sana'),
          bullets(['RSSI stabile e coerente con la distanza;', 'CRC error basso rispetto ai frame validi;', 'numero di miss contenuto;', 'resync sporadici o assenti in condizioni stabili;', 'alternanza dei packet type coerente nel tempo;', 'valori meteo senza salti irrealistici.']),
          PageBreak()]

story += [h1('16. Limiti noti e roadmap tecnica','s16'),
          table([
              ['Area','Limite corrente','Evoluzione prevista'],
              ['RF','timing nominale 2555 ms non adattivo per ID','profilo timing per ID / auto-stima periodo'],
              ['Frame','byte 8-9 non interpretati','cattura e correlazione con retransmit'],
              ['Direzione','formula da confermare','test con orientamenti noti'],
              ['UV','formula da confermare','confronto Pro2 Plus / WeatherLink'],
              ['Web security','LAN-only senza login','auth locale + CSRF'],
              ['Documentazione','v1.2 unico PDF italiano','eventuale versione inglese equivalente'],
              ['Release','0.2.0-dev','tag stabile dopo validazione hardware'],
          ], [36*mm,64*mm,CONTENT_W-100*mm], font=7.2),
          h2('16.1 Cosa non e parte del progetto'),
          P('Il firmware e Davis-only: non include decoder Oregon Scientific o Technoline. Non implementa una console grafica Davis completa, non emula protocolli proprietari lato console e non sostituisce le garanzie o le certificazioni del produttore.'),
          PageBreak()]

story += [h1('17. Changelog della documentazione','s17'),
          table([
              ['Versione','Data','Contenuto'],
              ['1.0','31/08/2026','prima guida tecnica RF: profilo radio, frame e packet type'],
              ['1.1','31/08/2026','chiarimento sulla pressione lato ricevitore e BME280 locale'],
              ['1.2','31/08/2026','manuale unificato: stazione, sensori, specifiche, RF, firmware, rete, sicurezza, validazione e troubleshooting'],
          ], [24*mm,32*mm,CONTENT_W-56*mm]),
          h2('17.1 Versione del firmware descritta'),
          P('Questa edizione descrive la baseline di sviluppo 0.2.0-dev del repository esp32-davis-weather-gateway. Le versioni del documento e del firmware sono indipendenti: una futura revisione del manuale puo documentare la stessa baseline con chiarimenti, oppure una nuova release firmware puo richiedere un aggiornamento della guida.'),
          PageBreak()]

story += [h1('18. Riferimenti tecnici e note legali','s18'),
          P('Le specifiche numeriche della stazione sono tratte dalla documentazione Davis indicata sotto. I dettagli di framing e conversione non pubblicati come specifica Davis sono invece classificati come interoperabilita/reverse engineering e confrontati con l\'implementazione del progetto.'),
          P('[D1] Davis Instruments - Wireless Vantage Pro2 Sensor Suite Weather Station, SKU 6322/6322M. https://www.davisinstruments.com/products/wireless-vantage-pro2-integrated-sensor-suite', 'Refx'),
          P('[D2] Davis Instruments - Wireless Vantage Pro2 & Vantage Pro2 Plus Stations Specification Sheet, rev. FF 15/08/2022. https://cdn.shopify.com/s/files/1/0515/5992/3873/files/6152_6162_6153_6163_Spec_SheetFF__Web.pdf', 'Refx'),
          P('[D3] Davis Instruments - WeatherLink Live. Include barometro e sensori T/H interni; ricezione fino a 8 trasmettitori. https://www.davisinstruments.com/products/weatherlink-live', 'Refx'),
          P('[D4] Davis Instruments - Wireless Vantage Pro2 Plus Sensor Suite, SKU 6327/6327M. https://www.davisinstruments.com/collections/wireless-vantage-pro2-and-vantage-pro2-plus-weather-stations/products/wireless-vantage-pro2-plus-iss', 'Refx'),
          P('[P1] ESP32 Davis Weather Gateway - README e sorgenti branch develop. https://github.com/pgpaolo/esp32-davis-weather-gateway', 'Refx'),
          P('[P2] ESP32 Davis Weather Gateway - RF_PROTOCOL_IT.md, Edizione 1.1. Repository develop.', 'Refx'),
          P('[R1] DavisRFM69 by DeKay - riferimento storico pubblico di reverse engineering Davis ISS/RFM69; citato per interoperabilita, non vendorizzato nel progetto.', 'Refx'),
          P('[R2] ISS-MQTT-Gateway by dcbo - riferimento pubblico storico ESP32/RFM69 Davis ISS; citato nella sezione THIRD_PARTY_NOTICES del repository.', 'Refx'),
          P('[L1] RadioLib - libreria radio multi-protocollo usata per SX1276. https://github.com/jgromes/RadioLib', 'Refx'),
          Spacer(1,3*mm),
          InfoBox('Marchi e indipendenza', 'Davis, Vantage Pro2, Vantage Vue, WeatherLink e altri nomi di prodotto possono essere marchi di Davis Instruments o dei rispettivi titolari. Il loro uso e descrittivo e finalizzato all\'interoperabilita. Il progetto non e affiliato, sponsorizzato o approvato da Davis Instruments.', 'info'),
          Spacer(1,3*mm),
          InfoBox('Licenza del documento', 'Salvo diversa indicazione, la documentazione originale del progetto e distribuita con il repository sotto GNU Lesser General Public License v3.0 only (LGPL-3.0-only). Le specifiche e i marchi dei terzi restano di proprieta dei rispettivi titolari.', 'ok')]

doc=MyDocTemplate(str(OUT), pagesize=A4, leftMargin=LEFT, rightMargin=RIGHT, topMargin=TOP, bottomMargin=BOTTOM,
                  title='ESP32 Davis Weather Gateway - Guida tecnica completa v1.2',
                  author='ESP32 Davis Weather Gateway', subject='Davis Vantage Pro2 EU 868 MHz interoperability technical guide',
                  creator='ESP32 Davis Weather Gateway')
doc.multiBuild(story)
print(OUT)
