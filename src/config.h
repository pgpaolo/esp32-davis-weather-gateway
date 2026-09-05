#pragma once

#if __has_include("config_private.h")
#include "config_private.h"
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.4.0-dev"
#endif

#ifndef DEVICE_HOSTNAME_DEFAULT
#define DEVICE_HOSTNAME_DEFAULT "davis-gateway"
#endif

#ifndef PROVISION_AP_PREFIX
#define PROVISION_AP_PREFIX "DavisGateway"
#endif
#ifndef PROVISION_AP_PASSWORD
// Empty by default: the setup AP is temporary and open. Define an 8+ char
// password in config_private.h if a protected provisioning AP is preferred.
#define PROVISION_AP_PASSWORD ""
#endif
#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 60000UL
#endif
#ifndef WIFI_RETRY_INTERVAL_MS
#define WIFI_RETRY_INTERVAL_MS 30000UL
#endif
#ifndef PROVISION_BUTTON_HOLD_MS
#define PROVISION_BUTTON_HOLD_MS 5000UL
#endif

// Suggested static LAN profile. DHCP remains the first-boot default.
#ifndef STATIC_IP_DEFAULT
#define STATIC_IP_DEFAULT "192.168.1.120"
#endif
#ifndef STATIC_GATEWAY_DEFAULT
#define STATIC_GATEWAY_DEFAULT "192.168.1.1"
#endif
#ifndef STATIC_MASK_DEFAULT
#define STATIC_MASK_DEFAULT "255.255.255.0"
#endif
#ifndef STATIC_DNS_DEFAULT
#define STATIC_DNS_DEFAULT "192.168.1.1"
#endif

#ifndef DAVIS_ISS_ID
#define DAVIS_ISS_ID 0       // 0=auto-lock; 1..8=explicit Davis transmitter ID
#endif
#ifndef DAVIS_RAIN_MM_PER_TIP
#define DAVIS_RAIN_MM_PER_TIP 0.2f
#endif
#ifndef DAVIS_PACKET_INTERVAL_MS
#define DAVIS_PACKET_INTERVAL_MS 2555UL
#endif
#ifndef DAVIS_MISS_GRACE_MS
#define DAVIS_MISS_GRACE_MS 250UL
#endif

#ifndef BME280_ENABLE
#define BME280_ENABLE 1
#endif
#ifndef BME280_ALTITUDE_M_DEFAULT
#define BME280_ALTITUDE_M_DEFAULT 0.0f
#endif

// Intentionally empty: every installation supplies its own receiver endpoint.
#ifndef MB_DEFAULT_URL
#define MB_DEFAULT_URL ""
#endif
#ifndef MB_UPLOAD_INTERVAL_MS
#define MB_UPLOAD_INTERVAL_MS 10000UL
#endif
#ifndef MB_TLS_INSECURE_DEFAULT
#define MB_TLS_INSECURE_DEFAULT 0
#endif
#ifndef MB_ROOT_CA
#define MB_ROOT_CA ""
#endif

#ifndef NTP_SERVER_1
#define NTP_SERVER_1 "pool.ntp.org"
#endif
#ifndef NTP_SERVER_2
#define NTP_SERVER_2 "time.google.com"
#endif
#ifndef TZ_INFO_DEFAULT
#define TZ_INFO_DEFAULT "UTC0"
#endif
