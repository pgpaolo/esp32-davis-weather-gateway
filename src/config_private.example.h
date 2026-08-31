#pragma once

// Copiare in src/config_private.h (ignorato da Git) e personalizzare.
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"
#define DEVICE_HOSTNAME "davis-gateway"

// 0 = DHCP, 1 = IP statico definito in config.h o qui sotto.
#define WIFI_USE_STATIC_IP 0

// Endpoint Aurora/DIGA. Il firmware aggiunge automaticamente ?d=<payload>.
#define MB_DEFAULT_URL "https://example.net/diga/mbridge/mb.php"

// 0 = auto-lock sul primo ISS valido; 1..8 = ID trasmettitore Davis.
#define DAVIS_ISS_ID 0

// Metric rain collector: normalmente 0.2 mm/tip. Impostare 0.254 per 0.01 inch/tip.
#define DAVIS_RAIN_MM_PER_TIP 0.2f

// Altitudine del BME280 per pressione ridotta al livello del mare.
#define BME280_ALTITUDE_M 584.0f
