#include "remote_access.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>
#include <time.h>
#include <vector>

#include "board_config.h"
#include "config.h"
#include "network_manager.h"
#include "remote_trust.h"
#include "runtime_config.h"
#include "web_manager.h"

namespace {
constexpr const char *NVS_NS = "remote";
constexpr const char *NVS_URL = "url";
constexpr const char *NVS_TOKEN = "token";
constexpr time_t VALID_TLS_EPOCH = 1700000000;
constexpr uint32_t PENDING_RETRY_MS = 30000UL;
constexpr uint32_t ERROR_RETRY_MS = 15000UL;
constexpr uint32_t APPROVED_REFRESH_MS = 60000UL;
constexpr uint32_t WS_RECONNECT_MS = 5000UL;
constexpr uint32_t LOCAL_HTTP_TIMEOUT_MS = 6000UL;
constexpr size_t MAX_REQUEST_BODY = 12288U;
constexpr size_t MAX_RESPONSE_BODY = 24576U;
constexpr size_t MAX_WS_MESSAGE = 38000U;

struct UrlParts {
  String host;
  String path;
  uint16_t port = 0;
};

struct LocalHttpResponse {
  int status = 502;
  String contentType = "text/plain; charset=utf-8";
  String contentEncoding;
  String location;
  String cacheControl;
  String contentDisposition;
  std::vector<uint8_t> body;
};

RemoteAccessConfig cfg;
RemoteAccessStatus status;
String deviceToken;
SemaphoreHandle_t stateMutex = nullptr;
TaskHandle_t remoteTaskHandle = nullptr;
volatile uint32_t configGeneration = 1;
volatile bool forceRetry = false;

WebSocketsClient ws;
String wsAuthHeader;
String activeWsUrl;
bool wsConfigured = false;

bool lockState(TickType_t wait = pdMS_TO_TICKS(250)) {
  return stateMutex && xSemaphoreTake(stateMutex, wait) == pdTRUE;
}
void unlockState() { if (stateMutex) xSemaphoreGive(stateMutex); }

String jsonEscape(const String &s) {
  String o; o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if (c == '\\' || c == '"') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else if (c == '\t') o += "\\t";
    else if (c != '\r') o += c;
  }
  return o;
}

String lowerCopy(String s) { s.toLowerCase(); return s; }

bool validTokenHex(const String &s) {
  if (s.length() != 64U) return false;
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

String generateTokenHex() {
  uint8_t raw[32];
  esp_fill_random(raw, sizeof(raw));
  char out[65];
  for (size_t i = 0; i < sizeof(raw); ++i) snprintf(out + (i * 2U), 3U, "%02x", raw[i]);
  out[64] = '\0';
  return String(out);
}

String buildDeviceId() {
  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    const uint64_t efuse = ESP.getEfuseMac();
    for (uint8_t i = 0; i < 6U; ++i) mac[5U - i] = static_cast<uint8_t>((efuse >> (i * 8U)) & 0xFFU);
  }
  char out[32];
  snprintf(out, sizeof(out), "esp32-%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(out);
}

bool parseSecureUrl(const String &input, const char *scheme, UrlParts &out) {
  const String prefix = String(scheme) + "://";
  if (!input.startsWith(prefix)) return false;
  String rest = input.substring(prefix.length());
  if (rest.isEmpty() || rest.indexOf('@') >= 0 || rest.indexOf('\r') >= 0 || rest.indexOf('\n') >= 0) return false;
  const int slash = rest.indexOf('/');
  String authority = slash >= 0 ? rest.substring(0, slash) : rest;
  out.path = slash >= 0 ? rest.substring(slash) : "/";
  if (authority.isEmpty() || authority.indexOf('[') >= 0 || authority.indexOf(']') >= 0) return false;
  const int colon = authority.lastIndexOf(':');
  out.port = (strcmp(scheme, "https") == 0 || strcmp(scheme, "wss") == 0) ? 443U : 0U;
  if (colon > 0) {
    const long p = authority.substring(colon + 1).toInt();
    if (p <= 0 || p > 65535) return false;
    out.port = static_cast<uint16_t>(p);
    out.host = authority.substring(0, colon);
  } else out.host = authority;
  return !out.host.isEmpty() && out.path.startsWith("/");
}

bool normalizePortalUrl(String &url) {
  url.trim();
  while (url.endsWith("/")) url.remove(url.length() - 1U);
  if (url.isEmpty()) return true;
  if (url.length() > 220U || url.indexOf('?') >= 0 || url.indexOf('#') >= 0) return false;
  UrlParts parts;
  return parseSecureUrl(url, "https", parts);
}

bool tlsClockReady() { return time(nullptr) >= VALID_TLS_EPOCH; }

void setState(const String &name, const String &error = String()) {
  if (!lockState()) return;
  status.state = name;
  status.lastError = error;
  status.configured = !cfg.portalUrl.isEmpty();
  status.deviceId = remoteDefaultDeviceId();
  unlockState();
}

void noteActivity() {
  if (!lockState()) return;
  status.lastActivityMs = millis();
  unlockState();
}

void loadIdentityAndConfig() {
  cfg = RemoteAccessConfig{};
  Preferences p;
  if (!p.begin(NVS_NS, false)) {
    deviceToken = generateTokenHex();
    return;
  }
  cfg.portalUrl = p.getString(NVS_URL, "");
  normalizePortalUrl(cfg.portalUrl);
  deviceToken = p.getString(NVS_TOKEN, "");
  if (!validTokenHex(deviceToken)) {
    deviceToken = generateTokenHex();
    p.putString(NVS_TOKEN, deviceToken);
  }
  // Remove obsolete 0.4.0-dev manual-credential fields. The stable token is
  // deliberately retained; installers no longer edit device identity or CA.
  p.remove("device"); p.remove("ca"); p.remove("heartbeat");
  p.remove("admin"); p.remove("enabled");
  p.end();
}

bool decodeBase64(const String &in, std::vector<uint8_t> &out) {
  out.clear();
  if (in.isEmpty()) return true;
  const size_t capacity = (in.length() * 3U) / 4U + 4U;
  if (capacity > MAX_REQUEST_BODY + 4U) return false;
  out.resize(capacity);
  size_t written = 0;
  const int rc = mbedtls_base64_decode(out.data(), out.size(), &written,
                                       reinterpret_cast<const unsigned char *>(in.c_str()), in.length());
  if (rc != 0 || written > MAX_REQUEST_BODY) { out.clear(); return false; }
  out.resize(written);
  return true;
}

String encodeBase64(const uint8_t *data, size_t len) {
  if (!data || !len) return String();
  const size_t cap = 4U * ((len + 2U) / 3U) + 1U;
  std::vector<unsigned char> encoded(cap);
  size_t written = 0;
  if (mbedtls_base64_encode(encoded.data(), encoded.size(), &written, data, len) != 0) return String();
  String out;
  if (!out.reserve(written + 1U)) return String();
  out.concat(reinterpret_cast<const char *>(encoded.data()), written);
  return out;
}

bool safeLocalPath(const String &path) {
  return !path.isEmpty() && path.length() <= 768U && path[0] == '/' &&
         path.indexOf("//") != 0 && path.indexOf("\r") < 0 && path.indexOf("\n") < 0 &&
         path.indexOf("://") < 0;
}

String incomingHeader(const JsonObjectConst &headers, const char *wanted) {
  if (headers.isNull()) return String();
  const String target = lowerCopy(String(wanted));
  for (JsonPairConst kv : headers) {
    String key = kv.key().c_str(); key.toLowerCase();
    if (key == target) {
      String value = kv.value().as<String>();
      value.replace("\r", ""); value.replace("\n", "");
      if (value.length() > 256U) value.remove(256U);
      return value;
    }
  }
  return String();
}

bool connectLocal(WiFiClient &client) {
  client.setTimeout(LOCAL_HTTP_TIMEOUT_MS / 1000U);
  IPAddress local = WiFi.localIP();
  if (local != IPAddress(0, 0, 0, 0) && client.connect(local, 80)) return true;
  client.stop();
  return client.connect(IPAddress(127, 0, 0, 1), 80);
}

bool proxyLocalHttp(const String &method, const String &path, const JsonObjectConst &headers,
                    const std::vector<uint8_t> &requestBody, LocalHttpResponse &response, String &error) {
  if (!webStarted()) { error = "Web UI locale non ancora pronta"; return false; }
  if (!safeLocalPath(path)) { error = "Path remoto non valido"; return false; }
  if (!(method == "GET" || method == "POST" || method == "HEAD")) {
    response.status = 405;
    const char msg[] = "Method not allowed";
    response.body.assign(msg, msg + sizeof(msg) - 1U);
    return true;
  }

  WiFiClient client;
  if (!connectLocal(client)) { error = "Connessione alla Web UI locale fallita"; return false; }

  client.print(method); client.print(' '); client.print(path); client.print(F(" HTTP/1.0\r\n"));
  client.print(F("Host: 127.0.0.1\r\nConnection: close\r\n"));
  const String contentType = incomingHeader(headers, "content-type");
  const String accept = incomingHeader(headers, "accept");
  if (!contentType.isEmpty()) { client.print(F("Content-Type: ")); client.print(contentType); client.print(F("\r\n")); }
  if (!accept.isEmpty()) { client.print(F("Accept: ")); client.print(accept); client.print(F("\r\n")); }
  if (!requestBody.empty()) { client.print(F("Content-Length: ")); client.print(requestBody.size()); client.print(F("\r\n")); }
  client.print(F("\r\n"));
  if (!requestBody.empty()) client.write(requestBody.data(), requestBody.size());

  const uint32_t waitStart = millis();
  while (!client.available() && client.connected() && static_cast<uint32_t>(millis() - waitStart) < LOCAL_HTTP_TIMEOUT_MS) vTaskDelay(pdMS_TO_TICKS(2));
  if (!client.available()) { client.stop(); error = "Timeout Web UI locale"; return false; }

  String statusLine = client.readStringUntil('\n'); statusLine.trim();
  if (!statusLine.startsWith("HTTP/")) { client.stop(); error = "Risposta HTTP locale non valida"; return false; }
  const int firstSpace = statusLine.indexOf(' ');
  const int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
  response.status = firstSpace > 0 ? statusLine.substring(firstSpace + 1, secondSpace > firstSpace ? secondSpace : statusLine.length()).toInt() : 502;

  size_t contentLength = 0;
  bool haveLength = false;
  while (client.connected() || client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0U) break;
    line.trim();
    const int colon = line.indexOf(':');
    if (colon <= 0) continue;
    String key = line.substring(0, colon); key.toLowerCase();
    String value = line.substring(colon + 1); value.trim();
    if (key == "content-type") response.contentType = value;
    else if (key == "content-encoding") response.contentEncoding = value;
    else if (key == "location") response.location = value;
    else if (key == "cache-control") response.cacheControl = value;
    else if (key == "content-disposition") response.contentDisposition = value;
    else if (key == "content-length") { contentLength = static_cast<size_t>(value.toInt()); haveLength = true; }
  }

  if (haveLength && contentLength > MAX_RESPONSE_BODY) {
    client.stop(); error = "Risposta locale troppo grande"; return false;
  }
  response.body.clear();
  response.body.reserve(haveLength ? contentLength : 2048U);
  const uint32_t bodyStart = millis();
  while ((client.connected() || client.available()) && static_cast<uint32_t>(millis() - bodyStart) < LOCAL_HTTP_TIMEOUT_MS) {
    while (client.available()) {
      const int c = client.read();
      if (c < 0) break;
      if (response.body.size() >= MAX_RESPONSE_BODY) { client.stop(); error = "Risposta locale oltre limite"; return false; }
      response.body.push_back(static_cast<uint8_t>(c));
      if (haveLength && response.body.size() >= contentLength) break;
    }
    if (haveLength && response.body.size() >= contentLength) break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  client.stop();
  if (haveLength && response.body.size() < contentLength) { error = "Risposta locale incompleta"; return false; }
  return true;
}

void sendHttpResponse(const String &id, const LocalHttpResponse &response) {
  const String b64 = encodeBase64(response.body.data(), response.body.size());
  String out;
  const size_t estimate = b64.length() + 700U;
  if (estimate > MAX_WS_MESSAGE || !out.reserve(estimate)) return;
  out = "{\"type\":\"http_response\",\"id\":\"" + jsonEscape(id) + "\",\"status\":" + String(response.status) + ",\"headers\":{";
  out += "\"content-type\":\"" + jsonEscape(response.contentType) + "\"";
  if (!response.contentEncoding.isEmpty()) out += ",\"content-encoding\":\"" + jsonEscape(response.contentEncoding) + "\"";
  if (!response.location.isEmpty()) out += ",\"location\":\"" + jsonEscape(response.location) + "\"";
  if (!response.cacheControl.isEmpty()) out += ",\"cache-control\":\"" + jsonEscape(response.cacheControl) + "\"";
  if (!response.contentDisposition.isEmpty()) out += ",\"content-disposition\":\"" + jsonEscape(response.contentDisposition) + "\"";
  out += "},\"body_b64\":\""; out += b64; out += "\"}";
  if (out.length() <= MAX_WS_MESSAGE && ws.sendTXT(out)) {
    if (lockState()) { status.responses++; status.lastActivityMs = millis(); unlockState(); }
  }
}

void sendErrorResponse(const String &id, int httpStatus, const String &message) {
  LocalHttpResponse r; r.status = httpStatus;
  r.body.assign(message.c_str(), message.c_str() + message.length());
  sendHttpResponse(id, r);
}

void handleWsText(uint8_t *payload, size_t length) {
  if (!payload || !length || length > MAX_WS_MESSAGE) return;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;
  const String type = doc["type"] | "";
  const String id = doc["id"] | "";
  noteActivity();

  if (type == "ping") {
    String pong = "{\"type\":\"pong\"";
    if (!id.isEmpty()) pong += ",\"id\":\"" + jsonEscape(id) + "\"";
    pong += "}"; ws.sendTXT(pong); return;
  }
  if (type != "http_request" || id.isEmpty()) return;

  if (lockState()) { status.requests++; unlockState(); }
  String method = doc["method"] | "GET"; method.toUpperCase();
  const String path = doc["path"] | "/";
  const String bodyB64 = doc["body_b64"] | "";
  std::vector<uint8_t> body;
  if (!decodeBase64(bodyB64, body)) { sendErrorResponse(id, 413, "Request body non valido o troppo grande"); return; }
  const JsonObjectConst headers = doc["headers"].as<JsonObjectConst>();
  LocalHttpResponse response; String proxyError;
  if (!proxyLocalHttp(method, path, headers, body, response, proxyError)) {
    sendErrorResponse(id, 502, proxyError); return;
  }
  sendHttpResponse(id, response);
}

void onWebSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      if (lockState()) {
        status.transportActive = true; status.approved = true; status.state = "ONLINE";
        status.wsConnects++; status.lastActivityMs = millis(); status.lastError = ""; unlockState();
      }
      Serial.println(F("[REMOTE] AdminSensor tunnel ONLINE"));
      break;
    case WStype_DISCONNECTED:
      if (lockState()) {
        const bool wasActive = status.transportActive;
        status.transportActive = false;
        if (status.configured && status.approved) status.state = "RECONNECT";
        if (wasActive) status.wsDisconnects++;
        unlockState();
      }
      break;
    case WStype_TEXT: handleWsText(payload, length); break;
    case WStype_PING:
    case WStype_PONG: noteActivity(); break;
    default: break;
  }
}

bool enrollDevice(const String &portalUrl, String &wsUrl, bool &pending) {
  pending = false; wsUrl = "";
  String enrollUrl = portalUrl + "/api/device/enroll";
  WiFiClientSecure client;
  client.setCACert(REMOTE_TRUST_CA);
  client.setTimeout(8);
  HTTPClient http;
  http.setConnectTimeout(7000);
  http.setTimeout(8000);
  if (!http.begin(client, enrollUrl)) { setState("ERROR", "Impossibile inizializzare HTTPS enroll"); return false; }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  JsonDocument req;
  req["device_id"] = remoteDefaultDeviceId();
  req["device_token"] = deviceToken;
  req["name"] = runtimeConfig.hostname.isEmpty() ? "Stazione meteo" : runtimeConfig.hostname;
  req["model"] = "ESP32 Davis Weather Gateway";
  req["firmware_version"] = FIRMWARE_VERSION;
  String body; serializeJson(req, body);

  if (lockState()) { status.enrollAttempts++; unlockState(); }
  const int code = http.POST(reinterpret_cast<const uint8_t *>(body.c_str()), body.length());
  const String response = code > 0 ? http.getString() : String();
  http.end();
  if (lockState()) { status.lastEnrollHttpCode = code; status.lastActivityMs = millis(); unlockState(); }
  if (code < 200 || code >= 300) {
    setState("ERROR", code > 0 ? String("Enroll HTTP ") + code : String("Errore TLS/HTTP enroll"));
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, response);
  if (err) { setState("ERROR", "Risposta enroll JSON non valida"); return false; }
  const String s = doc["status"] | "";
  if (s == "pending") {
    pending = true;
    if (lockState()) { status.approved = false; status.transportActive = false; unlockState(); }
    setState("PENDING");
    return true;
  }
  if (s != "approved") {
    if (lockState()) { status.approved = false; status.transportActive = false; unlockState(); }
    setState("DENIED", s.isEmpty() ? "Stato enroll mancante" : String("Stato portale: ") + s);
    return false;
  }

  wsUrl = doc["websocket_url"] | "";
  UrlParts wsParts;
  if (!parseSecureUrl(wsUrl, "wss", wsParts)) { setState("ERROR", "websocket_url WSS non valido"); return false; }
  if (lockState()) { status.approved = true; status.lastError = ""; unlockState(); }
  setState("APPROVED");
  return true;
}

bool configureWebSocket(const String &wsUrl) {
  UrlParts p;
  if (!parseSecureUrl(wsUrl, "wss", p)) return false;
  ws.disconnect();
  ws.beginSslWithCA(p.host.c_str(), p.port, p.path.c_str(), REMOTE_TRUST_CA, "");
  wsAuthHeader = "Authorization: Bearer " + deviceToken;
  ws.setExtraHeaders(wsAuthHeader.c_str());
  ws.setReconnectInterval(WS_RECONNECT_MS);
  ws.enableHeartbeat(30000UL, 5000UL, 2U);
  activeWsUrl = wsUrl;
  wsConfigured = true;
  setState("CONNECTING");
  return true;
}

void remoteTask(void *) {
  uint32_t seenGeneration = 0;
  uint32_t nextEnrollMs = 0;
  String approvedWsUrl;
  for (;;) {
    RemoteAccessConfig localCfg;
    if (lockState()) { localCfg = cfg; unlockState(); }
    const uint32_t generation = configGeneration;
    const bool retryNow = forceRetry;
    if (retryNow) forceRetry = false;

    if (generation != seenGeneration || retryNow) {
      seenGeneration = generation;
      if (wsConfigured) ws.disconnect();
      wsConfigured = false; activeWsUrl = ""; approvedWsUrl = ""; nextEnrollMs = 0;
      if (lockState()) { status.transportActive = false; status.approved = false; unlockState(); }
    }

    if (localCfg.portalUrl.isEmpty()) {
      if (wsConfigured) { ws.disconnect(); wsConfigured = false; }
      setState("OFF");
      vTaskDelay(pdMS_TO_TICKS(300));
      continue;
    }
    if (!networkConnected() || networkProvisioningActive()) {
      if (wsConfigured) { ws.disconnect(); wsConfigured = false; }
      setState("WAIT_NETWORK");
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    if (!tlsClockReady()) {
      setState("WAIT_TIME");
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    const uint32_t now = millis();
    bool transport = false;
    if (lockState()) { transport = status.transportActive; unlockState(); }
    if (nextEnrollMs == 0 || (!transport && static_cast<int32_t>(now - nextEnrollMs) >= 0)) {
      bool pending = false; String newWs;
      setState("ENROLLING");
      const bool ok = enrollDevice(localCfg.portalUrl, newWs, pending);
      if (ok && pending) {
        if (wsConfigured) { ws.disconnect(); wsConfigured = false; }
        approvedWsUrl = "";
        nextEnrollMs = millis() + PENDING_RETRY_MS;
      } else if (ok && !newWs.isEmpty()) {
        approvedWsUrl = newWs;
        if (!wsConfigured || activeWsUrl != approvedWsUrl) configureWebSocket(approvedWsUrl);
        nextEnrollMs = millis() + APPROVED_REFRESH_MS;
      } else nextEnrollMs = millis() + ERROR_RETRY_MS;
    }

    if (wsConfigured) ws.loop();
    vTaskDelay(pdMS_TO_TICKS(8));
  }
}
} // namespace

String remoteDefaultDeviceId() {
  static String id = buildDeviceId();
  return id;
}

void initRemoteAccess() {
  if (!stateMutex) stateMutex = xSemaphoreCreateMutex();
  loadIdentityAndConfig();
  if (lockState()) {
    status = RemoteAccessStatus{};
    status.initialized = true;
    status.configured = !cfg.portalUrl.isEmpty();
    status.deviceId = remoteDefaultDeviceId();
    status.state = cfg.portalUrl.isEmpty() ? "OFF" : "WAIT_NETWORK";
    unlockState();
  }
  ws.onEvent(onWebSocketEvent);
  if (!remoteTaskHandle) {
    if (xTaskCreate(remoteTask, "adminsensor", 12288, nullptr, 1, &remoteTaskHandle) != pdPASS) {
      remoteTaskHandle = nullptr;
      setState("ERROR", "Impossibile avviare task remoto");
    }
  }
  Serial.print(F("[REMOTE] Device ID: ")); Serial.println(remoteDefaultDeviceId());
  Serial.println(F("[REMOTE] Token dispositivo presente in NVS (valore non mostrato)"));
}

RemoteAccessConfig getRemoteAccessConfig() {
  RemoteAccessConfig out;
  if (lockState()) { out = cfg; unlockState(); }
  return out;
}

RemoteAccessStatus getRemoteAccessStatus() {
  RemoteAccessStatus out;
  if (lockState()) { out = status; unlockState(); }
  return out;
}

bool saveRemoteAccessPortalUrl(const String &portalUrl) {
  String normalized = portalUrl;
  if (!normalizePortalUrl(normalized)) return false;
  Preferences p;
  if (!p.begin(NVS_NS, false)) return false;
  const bool ok = p.putString(NVS_URL, normalized) == normalized.length();
  p.end();
  if (!ok && !normalized.isEmpty()) return false;
  if (lockState()) {
    cfg.portalUrl = normalized;
    status.configured = !normalized.isEmpty();
    status.approved = false; status.transportActive = false;
    status.state = normalized.isEmpty() ? "OFF" : "WAIT_NETWORK";
    status.lastError = "";
    unlockState();
  }
  configGeneration++;
  return true;
}

bool resetRemoteAccessConfig() {
  // Preserve NVS_TOKEN: a portal disable/re-enable must not create a new sensor
  // identity. A true factory erase is the operation that rotates this secret.
  return saveRemoteAccessPortalUrl("");
}

void retryRemoteAccessNow() { forceRetry = true; }

String remoteAccessConfigJson() {
  const RemoteAccessConfig c = getRemoteAccessConfig();
  String j; j.reserve(360);
  j = "{\"portal_url\":\"" + jsonEscape(c.portalUrl) + "\",\"device_id\":\"" + jsonEscape(remoteDefaultDeviceId()) + "\",\"has_token\":";
  j += validTokenHex(deviceToken) ? "true" : "false";
  j += ",\"identity_managed_by_firmware\":true}";
  return j;
}

String remoteAccessStatusJson() {
  const RemoteAccessStatus s = getRemoteAccessStatus();
  String j; j.reserve(700);
  j = "{\"initialized\":"; j += s.initialized ? "true" : "false";
  j += ",\"configured\":"; j += s.configured ? "true" : "false";
  j += ",\"approved\":"; j += s.approved ? "true" : "false";
  j += ",\"transport_active\":"; j += s.transportActive ? "true" : "false";
  j += ",\"state\":\"" + jsonEscape(s.state) + "\",\"device_id\":\"" + jsonEscape(s.deviceId) + "\"";
  j += ",\"enroll_attempts\":" + String(s.enrollAttempts) + ",\"last_enroll_http_code\":" + String(s.lastEnrollHttpCode);
  j += ",\"ws_connects\":" + String(s.wsConnects) + ",\"ws_disconnects\":" + String(s.wsDisconnects);
  j += ",\"requests\":" + String(s.requests) + ",\"responses\":" + String(s.responses);
  j += ",\"last_activity_age_ms\":" + (s.lastActivityMs ? String(static_cast<uint32_t>(millis() - s.lastActivityMs)) : String("null"));
  j += ",\"last_error\":\"" + jsonEscape(s.lastError) + "\"}";
  return j;
}
