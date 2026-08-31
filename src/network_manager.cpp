#include "network_manager.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"

namespace {
uint32_t lastAttemptMs = 0;
bool mdnsStarted = false;

void beginWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
#if WIFI_USE_STATIC_IP
  IPAddress ip(WIFI_IP_A, WIFI_IP_B, WIFI_IP_C, WIFI_IP_D);
  IPAddress gw(WIFI_GW_A, WIFI_GW_B, WIFI_GW_C, WIFI_GW_D);
  IPAddress mask(WIFI_MASK_A, WIFI_MASK_B, WIFI_MASK_C, WIFI_MASK_D);
  WiFi.config(ip, gw, mask, gw);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastAttemptMs = millis();
}
}

void initNetwork() {
  beginWifi();
  configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
}

void serviceNetwork() {
  if (WiFi.status() != WL_CONNECTED) {
    if ((uint32_t)(millis() - lastAttemptMs) > 15000UL) {
      WiFi.disconnect(false, false);
      beginWifi();
    }
    mdnsStarted = false;
    return;
  }

  if (!mdnsStarted) {
    mdnsStarted = MDNS.begin(DEVICE_HOSTNAME);
    if (mdnsStarted) MDNS.addService("http", "tcp", 80);
  }
}

bool networkConnected() { return WiFi.status() == WL_CONNECTED; }
String networkIp() { return networkConnected() ? WiFi.localIP().toString() : String("offline"); }
