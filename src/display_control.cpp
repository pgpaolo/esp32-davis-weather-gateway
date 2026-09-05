#include "display_control.h"

#include <Preferences.h>
#include <Wire.h>

#include "board_config.h"

namespace {
constexpr const char *NVS_NS = "display";
constexpr const char *NVS_KEY = "enabled";
bool hwAvailable = false;
bool enabledState = true;

bool applyPanelState(bool enabled) {
#if OLED_ENABLE
  if (!hwAvailable) return false;
  Wire.beginTransmission(OLED_ADDRESS);
  Wire.write(static_cast<uint8_t>(0x00)); // SSD1306 command stream
  Wire.write(static_cast<uint8_t>(enabled ? 0xAF : 0xAE)); // display ON / OFF
  return Wire.endTransmission() == 0;
#else
  (void)enabled;
  return false;
#endif
}
}

void initDisplayControl(bool displayDetected) {
  hwAvailable = displayDetected;
  Preferences p;
  if (p.begin(NVS_NS, true)) {
    enabledState = p.getBool(NVS_KEY, true);
    p.end();
  }
  if (hwAvailable) applyPanelState(enabledState);
  Serial.print(F("[OLED] pannello "));
  Serial.println(enabledState ? F("ON") : F("OFF (NVS)"));
}

bool displayControlAvailable() { return hwAvailable; }
bool displayControlEnabled() { return enabledState; }

bool setDisplayControlEnabled(bool enabled) {
  Preferences p;
  if (!p.begin(NVS_NS, false)) return false;
  const size_t written = p.putBool(NVS_KEY, enabled);
  p.end();
  if (written == 0U) return false;
  enabledState = enabled;
  if (hwAvailable) return applyPanelState(enabledState);
  return true;
}

String displayControlJson() {
  String j;
  j.reserve(80);
  j = "{\"available\":";
  j += hwAvailable ? "true" : "false";
  j += ",\"enabled\":";
  j += enabledState ? "true" : "false";
  j += "}";
  return j;
}
