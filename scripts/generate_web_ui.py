Import("env")

from pathlib import Path
import gzip
import re

project_dir = Path(env.subst("$PROJECT_DIR"))
pioenv = env.subst("$PIOENV")
build_root = Path(env.subst("$BUILD_DIR"))
generated_dir = build_root / pioenv / "generated"
generated_dir.mkdir(parents=True, exist_ok=True)

source_path = project_dir / "web" / "dashboard.html"
html = source_path.read_text(encoding="utf-8")

# Normalize the project footer at build time. This operation is intentionally
# idempotent: both an older dashboard and a dashboard already carrying the
# current attribution must compile successfully.
old_footer = '<div class="footer">ESP32 Davis Weather Gateway · firmware open source · interfaccia locale</div>'
new_footer = '<div class="footer">ESP32 Davis Weather Gateway · Gianpaolo P. · firmware open source · interfaccia locale</div>'
if new_footer not in html:
    if old_footer in html:
        html = html.replace(old_footer, new_footer, 1)
    else:
        pattern = r'<div class="footer">ESP32 Davis Weather Gateway[^<]*</div>'
        html, replaced = re.subn(pattern, new_footer, html, count=1)
        if replaced != 1:
            raise RuntimeError("Dashboard footer element not found")

# Add the current-log viewer only when the source dashboard does not already
# contain it. This prevents duplicate buttons when UI changes are promoted
# back into web/dashboard.html.
viewer = '<a class="btn secondary" href="/api/sd/current" target="_blank" rel="noopener">Visualizza log SD</a>'
if '/api/sd/current' not in html:
    hardware_anchor = '<button onclick="sdRemount()">Rimonta</button>'
    config_anchor = '<button class="secondary" onclick="sdRemount()">Rimonta</button>'
    if hardware_anchor not in html or config_anchor not in html:
        raise RuntimeError("microSD action anchor not found")
    html = html.replace(hardware_anchor, hardware_anchor + viewer, 1)
    html = html.replace(config_anchor, config_anchor + viewer, 1)

# Wi-Fi management is injected into the configuration page at build time so
# older dashboard sources remain buildable. The API itself lives in
# network_web.cpp. Scanning is asynchronous and only starts on user request.
if 'id="wifiSsid"' not in html:
    network_anchor = '<div class="configCard"><h2>Rete locale</h2>'
    if network_anchor not in html:
        raise RuntimeError("Network configuration card anchor not found")
    wifi_card = '''<div class="configCard"><h2>Connessione Wi-Fi</h2><p class="desc">Cerca le reti disponibili, seleziona l'SSID e salva le credenziali. La password memorizzata non viene mai mostrata.</p><div class="formgrid"><label class="full">Reti rilevate<select id="wifiList" onchange="wifiPick()"><option value="">-- premi Cerca reti --</option></select></label><label class="full">SSID<input id="wifiSsid" maxlength="32" autocomplete="off"></label><label>Password Wi-Fi<input id="wifiPass" type="password" maxlength="63" autocomplete="new-password" placeholder="vuoto = mantiene se stesso SSID"></label><label class="check"><input type="checkbox" id="wifiClear">Rete aperta / cancella password</label></div><div class="actions"><button onclick="scanWifi()">Cerca reti</button><button onclick="saveWifi()">Salva Wi-Fi</button></div><div id="wifiMsg" class="hint">Stato Wi-Fi in caricamento...</div></div>\n'''
    html = html.replace(network_anchor, wifi_card + network_anchor, 1)

    nav_anchor = "if(b.dataset.page==='config')loadConfig();"
    if nav_anchor not in html:
        raise RuntimeError("Configuration navigation anchor not found")
    html = html.replace(nav_anchor, "if(b.dataset.page==='config'){loadConfig();loadWifiConfig();}", 1)

    save_anchor = 'async function saveGateway(){'
    if save_anchor not in html:
        raise RuntimeError("Gateway save JavaScript anchor not found")
    wifi_js = r'''async function loadWifiConfig(){try{const w=await (await fetch('/api/network/wifi',{cache:'no-store'})).json();window.savedWifiSsid=w.saved_ssid||'';$('wifiSsid').value=w.saved_ssid||w.current_ssid||'';$('wifiPass').value='';$('wifiClear').checked=false;$('wifiMsg').textContent=(w.connected?'Connesso a '+w.current_ssid+' · '+w.rssi_dbm+' dBm':'Wi-Fi non connesso')+' · password salvata: '+(w.password_set?'sì':'no')}catch(e){$('wifiMsg').textContent='Impossibile leggere stato Wi-Fi'}}
function wifiPick(){const s=$('wifiList'),o=s.options[s.selectedIndex];if(!o||!o.value)return;$('wifiSsid').value=o.value;$('wifiClear').checked=o.dataset.open==='1';$('wifiPass').value='';}
async function scanWifi(){const list=$('wifiList'),msg=$('wifiMsg');list.innerHTML='<option value="">Ricerca in corso...</option>';msg.textContent='Scansione Wi-Fi in corso...';try{await fetch('/api/network/scan/start',{method:'POST'});for(let i=0;i<20;i++){await new Promise(r=>setTimeout(r,700));const d=await (await fetch('/api/network/scan',{cache:'no-store'})).json();if(d.status==='complete'){const nets=d.networks||[];list.innerHTML='<option value="">-- seleziona rete --</option>'+nets.map(n=>'<option value="'+esc(n.ssid)+'" data-open="'+(n.open?'1':'0')+'">'+esc(n.ssid)+' · '+n.rssi_dbm+' dBm · CH '+n.channel+' · '+(n.open?'APERTA':'PROTETTA')+(n.current?' · CONNESSA':'')+'</option>').join('');msg.textContent=nets.length?nets.length+' reti rilevate':'Nessuna rete rilevata';return;}if(d.status==='error')throw new Error(d.message||'Errore scansione');}throw new Error('Timeout scansione Wi-Fi')}catch(e){list.innerHTML='<option value="">-- scansione fallita --</option>';msg.textContent=e.message||String(e)}}
async function saveWifi(){const ssid=$('wifiSsid').value.trim(),pass=$('wifiPass').value,clear=$('wifiClear').checked;if(!ssid){$('wifiMsg').textContent='Selezionare o inserire un SSID';return;}const sel=$('wifiList'),opt=sel.options[sel.selectedIndex],knownProtected=opt&&opt.value===ssid&&opt.dataset.open==='0';if(knownProtected&&ssid!==(window.savedWifiSsid||'')&&!pass){$('wifiMsg').textContent='Inserire la password della nuova rete protetta';return;}const p=new URLSearchParams({ssid,password:pass});if(clear)p.set('clearpass','1');try{$('wifiMsg').textContent=await post('/api/network/wifi',p);window.savedWifiSsid=ssid;if(confirm('Configurazione Wi-Fi salvata. Riavviare ora il gateway per applicarla?'))await post('/api/restart')}catch(e){$('wifiMsg').textContent=e.message||String(e)}}
'''
    html = html.replace(save_anchor, wifi_js + save_anchor, 1)

# Controller health is intentionally read-only. No NVS writes are generated by
# these metrics. The ESP32 internal temperature is explicitly labelled as a
# SoC diagnostic value, not ambient temperature.
if 'id="controllerHw"' not in html:
    hardware_grid = '<section id="hardware" class="page"><div class="hardwareGrid">'
    if hardware_grid not in html:
        raise RuntimeError("Hardware grid anchor not found")
    controller_card = '<div class="panel"><h2>Controller ESP32</h2><div id="controllerHw" class="kv"></div><p class="hint">La temperatura è quella interna del SoC ESP32: è un dato diagnostico e non rappresenta la temperatura ambiente.</p></div>'
    html = html.replace(hardware_grid, hardware_grid + controller_card, 1)

if 'async function loadControllerMetrics()' not in html:
    save_anchor = 'async function saveGateway(){'
    if save_anchor not in html:
        raise RuntimeError("Controller JavaScript anchor not found")
    controller_js = r'''async function loadControllerMetrics(){try{const c=await (await fetch('/api/system/controller',{cache:'no-store'})).json();const el=$('controllerHw');if(!el)return;el.innerHTML=kv({'Chip':(c.chip_model||'--')+' rev '+(c.chip_revision??'--'),'Core':c.cores||'--','Temperatura SoC':fmt(c.chip_temperature_c,1,' °C'),'CPU':(c.cpu_mhz||'--')+' MHz','RAM libera':bytes(c.heap_free),'RAM minima':bytes(c.heap_min),'Blocco heap max':bytes(c.heap_max_block),'Flash totale':bytes(c.flash_total),'Firmware':bytes(c.firmware_size),'Slot attivo':(c.running_slot||'--')+' · '+bytes(c.running_slot_size),'Slot OTA':(c.ota_slot||'--')+' · '+bytes(c.ota_slot_size),'PSRAM':c.psram_total?bytes(c.psram_free)+' liberi / '+bytes(c.psram_total):'non presente'})}catch(e){const el=$('controllerHw');if(el)el.textContent='Metriche controller non disponibili'}}
'''
    html = html.replace(save_anchor, controller_js + save_anchor, 1)

# Surface the WebSocket diagnostics that are especially useful when the portal
# has approved a device but still reports it offline.
old_remote_hw = "$('remoteHw').innerHTML=kv({'Stato':rem.state||'OFF','Configurato':rem.configured?'SI':'NO','Approvato':rem.approved?'SI':'NO','Device ID':rem.device_id||'--','Tunnel':rem.transport_active?'ONLINE':'OFF','Enroll':rem.enroll_attempts||0,'Richieste':rem.requests||0});"
new_remote_hw = "$('remoteHw').innerHTML=kv({'Stato':rem.state||'OFF','Configurato':rem.configured?'SI':'NO','Approvato':rem.approved?'SI':'NO','Device ID':rem.device_id||'--','Tunnel':rem.transport_active?'ONLINE':'OFF','Enroll':rem.enroll_attempts||0,'Tentativi WSS':rem.ws_attempts||0,'Handshake KO':rem.ws_handshake_failures||0,'Ultimo evento':rem.last_ws_event||'--','WSS host':rem.ws_host||'--','WSS path':rem.ws_path||'--','Heartbeat':rem.heartbeats_sent||0,'Richieste':rem.requests||0});"
if old_remote_hw in html:
    html = html.replace(old_remote_hw, new_remote_hw, 1)

old_remote_diag = "$('remoteDiag').innerHTML=kv({'Stato':rem.state||'OFF','Device':rem.device_id||'--','HTTP enroll':rem.last_enroll_http_code||'--','WS connect':rem.ws_connects||0,'WS disconnect':rem.ws_disconnects||0,'Request/Response':(rem.requests||0)+' / '+(rem.responses||0),'Errore':rem.last_error||'--'});"
new_remote_diag = "$('remoteDiag').innerHTML=kv({'Stato':rem.state||'OFF','Device':rem.device_id||'--','HTTP enroll':rem.last_enroll_http_code||'--','Tentativi WSS':rem.ws_attempts||0,'WS connect':rem.ws_connects||0,'WS disconnect':rem.ws_disconnects||0,'Handshake KO':rem.ws_handshake_failures||0,'Ultimo evento':rem.last_ws_event||'--','Host / path':(rem.ws_host||'--')+' '+(rem.ws_path||''),'Heartbeat':rem.heartbeats_sent||0,'Request/Response':(rem.requests||0)+' / '+(rem.responses||0),'Errore':rem.last_error||'--'});"
if old_remote_diag in html:
    html = html.replace(old_remote_diag, new_remote_diag, 1)

# Refresh controller metrics at a much lower frequency than live weather data.
interval_anchor = "refresh();setInterval(refresh,1500);setInterval(()=>{if(diagVisible)refreshExtended();if(firmwareVisible)loadFirmwareStatus()},4000);"
if interval_anchor in html:
    html = html.replace(interval_anchor, "loadControllerMetrics();refresh();setInterval(refresh,1500);setInterval(()=>{loadControllerMetrics();if(diagVisible)refreshExtended();if(firmwareVisible)loadFirmwareStatus()},5000);", 1)

payload = gzip.compress(html.encode("utf-8"), compresslevel=9, mtime=0)

rows = []
for offset in range(0, len(payload), 16):
    chunk = payload[offset:offset + 16]
    rows.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")

header = (
    "#pragma once\n"
    "#include <Arduino.h>\n"
    "// Auto-generated from web/dashboard.html; deterministic gzip (mtime=0).\n"
    "static const uint8_t WEB_UI_GZ[] PROGMEM = {\n"
    + "\n".join(rows)
    + "\n};\n"
    + f"static constexpr size_t WEB_UI_GZ_LEN = {len(payload)}U;\n"
)

header_path = generated_dir / "web_ui_generated.h"
if not header_path.exists() or header_path.read_text(encoding="ascii") != header:
    header_path.write_text(header, encoding="ascii", newline="\n")

env.Append(CPPPATH=[str(generated_dir)])
print(f"Compressed Web UI: {len(html.encode('utf-8'))} -> {len(payload)} bytes")
