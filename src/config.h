#pragma once

#if __has_include("config_private.h")
#include "config_private.h"
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.1.0-alpha1"
#endif
#ifndef DEVICE_HOSTNAME
#define DEVICE_HOSTNAME "davis-gateway"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "NomeReteWiFi"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "PasswordWiFi"
#endif
#ifndef WIFI_USE_STATIC_IP
#define WIFI_USE_STATIC_IP 0
#endif
#ifndef WIFI_IP_A
#define WIFI_IP_A 192
#define WIFI_IP_B 168
#define WIFI_IP_C 1
#define WIFI_IP_D 221
#endif
#ifndef WIFI_GW_A
#define WIFI_GW_A 192
#define WIFI_GW_B 168
#define WIFI_GW_C 1
#define WIFI_GW_D 1
#endif
#ifndef WIFI_MASK_A
#define WIFI_MASK_A 255
#define WIFI_MASK_B 255
#define WIFI_MASK_C 255
#define WIFI_MASK_D 0
#endif

#ifndef DAVIS_ISS_ID
#define DAVIS_ISS_ID 0       // 0=auto-lock; 1..8=ID Davis esplicito
#endif
#ifndef DAVIS_RAIN_MM_PER_TIP
#define DAVIS_RAIN_MM_PER_TIP 0.2f
#endif
#ifndef DAVIS_PACKET_INTERVAL_MS
#define DAVIS_PACKET_INTERVAL_MS 2555UL
#endif
#ifndef DAVIS_SCAN_DWELL_MS
#define DAVIS_SCAN_DWELL_MS 650UL
#endif
#ifndef DAVIS_MISS_GRACE_MS
#define DAVIS_MISS_GRACE_MS 250UL
#endif

#ifndef BME280_ENABLE
#define BME280_ENABLE 1
#endif
#ifndef BME280_ALTITUDE_M
#define BME280_ALTITUDE_M 584.0f
#endif

#ifndef MB_DEFAULT_URL
#define MB_DEFAULT_URL "https://meteostz05013.ddns.net/diga/mbridge/mb.php"
#endif
#ifndef MB_UPLOAD_INTERVAL_MS
#define MB_UPLOAD_INTERVAL_MS 10000UL
#endif
#ifndef MB_TLS_INSECURE_DEFAULT
#define MB_TLS_INSECURE_DEFAULT 1
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
#ifndef TZ_INFO
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
#endif
