#include "network_manager.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

#include "board_config.h"
#include "config.h"
#include "runtime_config.h"

namespace {
WebServer portal(80);
DNSServer dnsServer;
const IPAddress kApIp(192,168,4,1);
const IPAddress kApMask(255,255,255,0);

uint32_t connectStartMs = 0;
uint32_t lastRetryMs = 0;
uint32_t buttonDownMs = 0;
bool mdnsStarted = false;
bool provisioning = false;
bool manualProvisioning = false;
bool portalStarted = false;
String apSsid;

String htmlEscape(const String &s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i=0;i<s.length();++i) {
    const char c=s[i];
    if(c=='&') out += F("&amp;");
    else if(c=='<') out += F("&lt;");
    else if(c=='>') out += F("&gt;");
    else if(c=='\"') out += F("&quot;");
    else out += c;
  }
  return out;
}

String buildApSsid() {
  const uint64_t mac = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", (uint16_t)(mac & 0xFFFFU));
  return String(PROVISION_AP_PREFIX) + "-" + suffix;
}

bool parseIp(const String &text, IPAddress &out) {
  return out.fromString(text);
}

bool applyStaticNetwork() {
  if(runtimeConfig.useDhcp) return true;
  IPAddress ip, gw, mask, dns;
  if(!parseIp(runtimeConfig.staticIp, ip) ||
     !parseIp(runtimeConfig.gateway, gw) ||
     !parseIp(runtimeConfig.netmask, mask) ||
     !parseIp(runtimeConfig.dns, dns)) return false;
  return WiFi.config(ip, gw, mask, dns);
}

void startStaAttempt() {
  if(!runtimeWifiConfigured()) return;
  if(provisioning) WiFi.mode(WIFI_AP_STA);
  else WiFi.mode(WIFI_STA);
  WiFi.setHostname(runtimeConfig.hostname.c_str());
  WiFi.setAutoReconnect(true);
  applyStaticNetwork();
  WiFi.begin(runtimeConfig.wifiSsid.c_str(), runtimeConfig.wifiPassword.c_str());
  connectStartMs = millis();
  lastRetryMs = connectStartMs;
  Serial.print(F("[NET] Connessione a SSID: "));
  Serial.println(runtimeConfig.wifiSsid);
}

String scanOptions() {
  String out;
  int n = WiFi.scanNetworks(false, true);
  if(n <= 0) return out;
  for(int i=0;i<n;i++) {
    String ssid = WiFi.SSID(i);
    if(ssid.isEmpty()) continue;
    out += F("<option value=\""); out += htmlEscape(ssid); out += F("\">");
    out += htmlEscape(ssid); out += F(" ("); out += String(WiFi.RSSI(i)); out += F(" dBm)</option>");
  }
  WiFi.scanDelete();
  return out;
}

String setupPage(const String &message = String()) {
  const String options = scanOptions();
  String h;
  h.reserve(6500);
  h += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  h += F("<title>Davis Gateway Setup</title><style>body{font-family:Arial,sans-serif;max-width:760px;margin:24px auto;padding:0 14px;color:#222}fieldset{margin:16px 0;padding:14px;border:1px solid #bbb;border-radius:8px}label{display:block;margin-top:10px;font-weight:600}input,select{box-sizing:border-box;width:100%;padding:9px;margin-top:4px}input[type=checkbox]{width:auto}button{padding:11px 18px;font-weight:700}small{color:#666}.msg{padding:10px;background:#eef6ee;border-radius:6px}.warn{padding:10px;background:#fff4dc;border-radius:6px}</style></head><body>");
  h += F("<h1>ESP32 Davis Weather Gateway</h1><p>Configurazione iniziale rete e gateway. Le credenziali vengono salvate in NVS e non fanno parte del firmware pubblico.</p>");
  if(!message.isEmpty()){ h += F("<p class='msg'>"); h += htmlEscape(message); h += F("</p>"); }
  h += F("<form method='POST' action='/save'>");
  h += F("<fieldset><legend><b>Wi-Fi</b></legend>");
  if(!options.isEmpty()) { h += F("<label>Reti rilevate</label><select name='scan' onchange=\"if(this.value)document.getElementsByName('ssid')[0].value=this.value\"><option value=''>-- seleziona --</option>"); h += options; h += F("</select>"); }
  h += F("<label>SSID</label><input name='ssid' maxlength='32' required value='"); h += htmlEscape(runtimeConfig.wifiSsid); h += F("'>");
  // Never send the stored credential back to the browser/DOM.
  h += F("<label>Password Wi-Fi</label><input name='pass' type='password' maxlength='63' value=''><small>La password salvata non viene mai mostrata. Lascia vuoto per mantenerla se l'SSID non cambia.</small>");
  h += F("<label><input type='checkbox' name='clearpass' value='1'> Cancella password / rete Wi-Fi aperta</label>");
  h += F("<label>Hostname</label><input name='host' maxlength='31' value='"); h += htmlEscape(runtimeConfig.hostname); h += F("'>");
  h += F("</fieldset>");

  h += F("<fieldset><legend><b>Indirizzamento LAN</b></legend>");
  h += F("<label><input type='checkbox' name='dhcp' value='1' "); if(runtimeConfig.useDhcp) h += F("checked"); h += F("> Usa DHCP</label>");
  h += F("<p class='warn'>Se si sceglie IP statico, il profilo suggerito usa <b>192.168.1.120</b>. Verificare che l'indirizzo sia libero e coerente con la propria LAN.</p>");
  h += F("<label>IP statico</label><input name='ip' value='"); h += htmlEscape(runtimeConfig.staticIp); h += F("'>");
  h += F("<label>Gateway</label><input name='gw' value='"); h += htmlEscape(runtimeConfig.gateway); h += F("'>");
  h += F("<label>Netmask</label><input name='mask' value='"); h += htmlEscape(runtimeConfig.netmask); h += F("'>");
  h += F("<label>DNS</label><input name='dns' value='"); h += htmlEscape(runtimeConfig.dns); h += F("'>");
  h += F("</fieldset>");

  h += F("<fieldset><legend><b>Stazione</b></legend>");
  h += F("<label>Endpoint receiver (opzionale)</label><input name='mburl' placeholder='https://server.example/path/mb.php' value='"); h += htmlEscape(runtimeConfig.mbUrl); h += F("'>");
  h += F("<label>ID trasmettitore Davis (0=auto, 1..8=manuale)</label><input name='issid' type='number' min='0' max='8' value='"); h += String(runtimeConfig.issId); h += F("'>");
  h += F("<label>Scatto pluviometro (mm)</label><input name='raintip' type='number' min='0.05' max='1' step='0.001' value='"); h += String(runtimeConfig.rainMmPerTip,3); h += F("'>");
  h += F("<label>Quota BME280 (m)</label><input name='altm' type='number' min='-500' max='9000' step='0.1' value='"); h += String(runtimeConfig.bmeAltitudeM,1); h += F("'>");
  h += F("<label>Timezone POSIX</label><input name='tz' value='"); h += htmlEscape(runtimeConfig.tzInfo); h += F("'><small>Esempio Italia: CET-1CEST,M3.5.0,M10.5.0/3</small>");
  h += F("</fieldset><button type='submit'>SALVA E RIAVVIA</button></form>");
  h += F("<p><small>Access point di configurazione: "); h += htmlEscape(apSsid); h += F(" - portale: http://192.168.4.1</small></p></body></html>");
  return h;
}

void configurePortalRoutes() {
  portal.on("/", HTTP_GET, [](){ portal.send(200, "text/html; charset=utf-8", setupPage()); });
  portal.on("/generate_204", HTTP_GET, [](){ portal.sendHeader("Location", "http://192.168.4.1/", true); portal.send(302, "text/plain", ""); });
  portal.on("/hotspot-detect.html", HTTP_GET, [](){ portal.sendHeader("Location", "http://192.168.4.1/", true); portal.send(302, "text/plain", ""); });
  portal.on("/save", HTTP_POST, [](){
    const String ssid = portal.arg("ssid");
    if(ssid.isEmpty()) { portal.send(400, "text/html; charset=utf-8", setupPage("SSID obbligatorio.")); return; }

    const String previousSsid = runtimeConfig.wifiSsid;
    const String submittedPassword = portal.arg("pass");
    const bool clearPassword = portal.hasArg("clearpass");

    runtimeConfig.wifiSsid = ssid;
    if(clearPassword) runtimeConfig.wifiPassword = "";
    else if(!submittedPassword.isEmpty()) runtimeConfig.wifiPassword = submittedPassword;
    else if(ssid != previousSsid) runtimeConfig.wifiPassword = "";
    // Same SSID + blank field keeps the credential already present in RAM/NVS.

    runtimeConfig.hostname = portal.arg("host");
    if(runtimeConfig.hostname.isEmpty()) runtimeConfig.hostname = DEVICE_HOSTNAME_DEFAULT;
    runtimeConfig.useDhcp = portal.hasArg("dhcp");
    runtimeConfig.staticIp = portal.arg("ip");
    runtimeConfig.gateway = portal.arg("gw");
    runtimeConfig.netmask = portal.arg("mask");
    runtimeConfig.dns = portal.arg("dns");

    if(!runtimeConfig.useDhcp) {
      IPAddress tmp;
      if(!parseIp(runtimeConfig.staticIp,tmp) || !parseIp(runtimeConfig.gateway,tmp) ||
         !parseIp(runtimeConfig.netmask,tmp) || !parseIp(runtimeConfig.dns,tmp)) {
        portal.send(400, "text/html; charset=utf-8", setupPage("Profilo IP statico non valido.")); return;
      }
    }

    runtimeConfig.mbUrl = portal.arg("mburl");
    runtimeConfig.issId = (uint8_t)constrain(portal.arg("issid").toInt(),0,8);
    const float tip = portal.arg("raintip").toFloat();
    if(tip >= 0.05f && tip <= 1.0f) runtimeConfig.rainMmPerTip = tip;
    const float alt = portal.arg("altm").toFloat();
    if(alt >= -500.0f && alt <= 9000.0f) runtimeConfig.bmeAltitudeM = alt;
    runtimeConfig.tzInfo = portal.arg("tz");
    if(runtimeConfig.tzInfo.isEmpty()) runtimeConfig.tzInfo = TZ_INFO_DEFAULT;
    saveRuntimeConfig();

    portal.send(200, "text/html; charset=utf-8", "<html><body><h2>Configurazione salvata.</h2><p>Riavvio del gateway...</p></body></html>");
    delay(800);
    ESP.restart();
  });
  portal.onNotFound([](){ portal.sendHeader("Location", "http://192.168.4.1/", true); portal.send(302, "text/plain", ""); });
}

void beginPortalServer() {
  if(portalStarted) return;
  configurePortalRoutes();
  portal.begin();
  portalStarted = true;
}
}

void startNetworkProvisioning(bool manual) {
  if(provisioning) { if(manual) manualProvisioning = true; return; }
  manualProvisioning = manual;
  provisioning = true;
  mdnsStarted = false;
  apSsid = buildApSsid();
  WiFi.mode(runtimeWifiConfigured() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAPConfig(kApIp, kApIp, kApMask);
  const String apPass = String(PROVISION_AP_PASSWORD);
  bool ok = apPass.length() >= 8 ? WiFi.softAP(apSsid.c_str(), apPass.c_str()) : WiFi.softAP(apSsid.c_str());
  if(!ok) Serial.println(F("[NET] Errore avvio AP provisioning"));
  dnsServer.start(53, "*", kApIp);
  beginPortalServer();
  Serial.println(F("[NET] Modalita provisioning attiva"));
  Serial.print(F("[NET] SSID AP: ")); Serial.println(apSsid);
  Serial.println(F("[NET] Portale: http://192.168.4.1"));
  if(apPass.length() >= 8) Serial.println(F("[NET] AP provisioning protetto"));
}

void stopNetworkProvisioning() {
  if(!provisioning) return;
  dnsServer.stop();
  if(portalStarted) { portal.stop(); portalStarted = false; }
  WiFi.softAPdisconnect(true);
  provisioning = false;
  manualProvisioning = false;
  if(networkConnected()) WiFi.mode(WIFI_STA);
}

void initNetwork() {
#if PROVISION_BUTTON_ENABLE
  pinMode(PROVISION_BUTTON_PIN, INPUT_PULLUP);
#endif
  if(!runtimeWifiConfigured()) {
    startNetworkProvisioning(false);
  } else {
    startStaAttempt();
  }
  configTzTime(runtimeConfig.tzInfo.c_str(), NTP_SERVER_1, NTP_SERVER_2);
}

void serviceNetwork() {
  const uint32_t now = millis();

#if PROVISION_BUTTON_ENABLE
  const bool pressed = digitalRead(PROVISION_BUTTON_PIN) == PROVISION_BUTTON_ACTIVE;
  if(pressed) {
    if(buttonDownMs == 0) buttonDownMs = now;
    else if(!provisioning && (uint32_t)(now-buttonDownMs) >= PROVISION_BUTTON_HOLD_MS) startNetworkProvisioning(true);
  } else buttonDownMs = 0;
#endif

  if(provisioning) {
    dnsServer.processNextRequest();
    if(portalStarted) portal.handleClient();
  }

  if(networkConnected()) {
    if(provisioning && !manualProvisioning) stopNetworkProvisioning();
    if(!mdnsStarted) {
      mdnsStarted = MDNS.begin(runtimeConfig.hostname.c_str());
      if(mdnsStarted) MDNS.addService("http", "tcp", 80);
    }
    return;
  }

  mdnsStarted = false;
  if(!runtimeWifiConfigured()) return;

  if(!provisioning && connectStartMs != 0 && (uint32_t)(now-connectStartMs) >= WIFI_CONNECT_TIMEOUT_MS) {
    Serial.println(F("[NET] Wi-Fi non raggiungibile: attivo il portale di recovery"));
    startNetworkProvisioning(false);
  }

  if((uint32_t)(now-lastRetryMs) >= WIFI_RETRY_INTERVAL_MS) {
    lastRetryMs = now;
    WiFi.disconnect(false, false);
    startStaAttempt();
  }
}

bool networkConnected() { return WiFi.status() == WL_CONNECTED; }
String networkIp() {
  if(networkConnected()) return WiFi.localIP().toString();
  if(provisioning) return kApIp.toString();
  return String("offline");
}
String networkSsid() { return networkConnected() ? WiFi.SSID() : String(); }
String networkModeName() {
  if(provisioning && networkConnected()) return "STA+SETUP";
  if(provisioning) return "SETUP";
  if(networkConnected()) return "STA";
  return "OFFLINE";
}
bool networkProvisioningActive() { return provisioning; }
String networkProvisioningSsid() { return apSsid; }