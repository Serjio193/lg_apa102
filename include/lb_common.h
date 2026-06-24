#pragma once

#include <Arduino.h>
#include <Preferences.h>

static constexpr uint32_t LB_CONFIG_MAGIC = 0x4C425332UL;
static constexpr uint16_t LB_CONFIG_VERSION = 1;
static constexpr const char *LB_PREFS_NS = "lb2";
static constexpr const char *LB_DEFAULT_AP_SSID = "LB-SETUP";
static constexpr const char *LB_DEFAULT_AP_PASS = "lb123456";
static constexpr const char *LB_DEFAULT_DEVICE_NAME = "lg_apa102";
static constexpr const char *LB_DEFAULT_PACK_BASE_URL =
    "https://serjio193.github.io/lg_apa102/latest/";

struct LBConfig {
  uint32_t magic;
  uint16_t version;
  char deviceName[33];
  char wifiSsid[33];
  char wifiPass[65];
  char packBaseUrl[161];
  int16_t dataPin;
  int16_t clockPin;
  int16_t oePin;
  int16_t powerPin;
  uint16_t ledCount;
  uint8_t ledBrightness;
  uint8_t oeActiveLow;
  uint8_t powerActiveHigh;
  uint16_t powerOnDelayMs;
  uint16_t powerOffDelayMs;
};

static inline void lbSetDefaults(LBConfig *cfg) {
  if (!cfg) return;
  memset(cfg, 0, sizeof(*cfg));
  cfg->magic = LB_CONFIG_MAGIC;
  cfg->version = LB_CONFIG_VERSION;
  strlcpy(cfg->deviceName, LB_DEFAULT_DEVICE_NAME, sizeof(cfg->deviceName));
  strlcpy(cfg->packBaseUrl, LB_DEFAULT_PACK_BASE_URL, sizeof(cfg->packBaseUrl));
  cfg->dataPin = 11;
  cfg->clockPin = 7;
  cfg->oePin = 18;
  cfg->powerPin = 9;
  cfg->ledCount = 60;
  cfg->ledBrightness = 24;
  cfg->oeActiveLow = 1;
  cfg->powerActiveHigh = 1;
  cfg->powerOnDelayMs = 50;
  cfg->powerOffDelayMs = 100;
}

static inline void lbNormalizeDefaults(LBConfig *cfg) {
  if (!cfg) return;
  cfg->magic = LB_CONFIG_MAGIC;
  cfg->version = LB_CONFIG_VERSION;
  if (cfg->deviceName[0] == '\0') {
    strlcpy(cfg->deviceName, LB_DEFAULT_DEVICE_NAME, sizeof(cfg->deviceName));
  }
  strlcpy(cfg->packBaseUrl, LB_DEFAULT_PACK_BASE_URL, sizeof(cfg->packBaseUrl));
  if (cfg->ledCount == 0) cfg->ledCount = 1;
  if (cfg->ledBrightness == 0) cfg->ledBrightness = 1;
}

static inline bool lbLoadConfig(LBConfig *cfg) {
  if (!cfg) return false;
  lbSetDefaults(cfg);
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, true);
  LBConfig stored{};
  size_t len = prefs.getBytes("cfg", &stored, sizeof(stored));
  prefs.end();
  if (len == sizeof(stored) && stored.magic == LB_CONFIG_MAGIC && stored.version == LB_CONFIG_VERSION) {
    *cfg = stored;
    lbNormalizeDefaults(cfg);
    return true;
  }
  return false;
}

static inline void lbSaveConfig(const LBConfig *cfg) {
  if (!cfg) return;
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, false);
  prefs.putBytes("cfg", cfg, sizeof(*cfg));
  prefs.end();
}

static inline bool lbIsValidOutputPin(int16_t pin) {
  return pin < 0 || GPIO_IS_VALID_OUTPUT_GPIO(static_cast<gpio_num_t>(pin));
}

static inline String lbHostnameFromName(const char *name) {
  String host = name ? String(name) : String(LB_DEFAULT_DEVICE_NAME);
  host.trim();
  if (host.length() == 0) host = LB_DEFAULT_DEVICE_NAME;
  host.toLowerCase();
  for (size_t i = 0; i < host.length(); ++i) {
    char c = host[i];
    if (!isalnum(static_cast<unsigned char>(c)) && c != '-') host.setCharAt(i, '-');
  }
  return host;
}

static inline bool lbValidateConfig(const LBConfig *cfg) {
  if (!cfg) return false;
  if (cfg->deviceName[0] == '\0') return false;
  if (cfg->ledCount < 1 || cfg->ledCount > 2048) return false;
  if (cfg->ledBrightness < 1 || cfg->ledBrightness > 31) return false;
  if (!lbIsValidOutputPin(cfg->dataPin) || !lbIsValidOutputPin(cfg->clockPin) ||
      !lbIsValidOutputPin(cfg->oePin) || !lbIsValidOutputPin(cfg->powerPin)) {
    return false;
  }
  if (cfg->dataPin >= 0 && cfg->clockPin >= 0 && cfg->dataPin == cfg->clockPin) return false;
  if (cfg->dataPin >= 0 && cfg->oePin >= 0 && cfg->dataPin == cfg->oePin) return false;
  if (cfg->dataPin >= 0 && cfg->powerPin >= 0 && cfg->dataPin == cfg->powerPin) return false;
  if (cfg->clockPin >= 0 && cfg->oePin >= 0 && cfg->clockPin == cfg->oePin) return false;
  if (cfg->clockPin >= 0 && cfg->powerPin >= 0 && cfg->clockPin == cfg->powerPin) return false;
  if (cfg->oePin >= 0 && cfg->powerPin >= 0 && cfg->oePin == cfg->powerPin) return false;
  return true;
}

static inline void lbCopyArg(char *dst, size_t dstSize, const String &value) {
  if (!dst || dstSize == 0) return;
  size_t n = value.length();
  if (n >= dstSize) n = dstSize - 1;
  memcpy(dst, value.c_str(), n);
  dst[n] = '\0';
}

static inline int16_t lbArgPin(WebServer &server, const char *name, int16_t fallback) {
  if (!server.hasArg(name)) return fallback;
  return static_cast<int16_t>(server.arg(name).toInt());
}

static inline uint16_t lbArgU16(WebServer &server, const char *name, uint16_t fallback, uint16_t minValue, uint16_t maxValue) {
  if (!server.hasArg(name)) return fallback;
  long v = server.arg(name).toInt();
  if (v < static_cast<long>(minValue)) v = minValue;
  if (v > static_cast<long>(maxValue)) v = maxValue;
  return static_cast<uint16_t>(v);
}

static inline uint8_t lbArgU8(WebServer &server, const char *name, uint8_t fallback, uint8_t minValue, uint8_t maxValue) {
  if (!server.hasArg(name)) return fallback;
  long v = server.arg(name).toInt();
  if (v < static_cast<long>(minValue)) v = minValue;
  if (v > static_cast<long>(maxValue)) v = maxValue;
  return static_cast<uint8_t>(v);
}
