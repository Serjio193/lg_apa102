#include <Arduino.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include "../include/lb_common.h"
#include "../include/lb_public_key.h"

static LBConfig cfg;
static WebServer server(80);
static bool apActive = false;
static char lastError[128] = "idle";

static const char RECOVERY_HTML[] PROGMEM = R"rawliteral(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>lg_apa102 Recovery</title>
<style>
body{margin:0;font-family:system-ui,sans-serif;background:radial-gradient(circle at top,#3a1520,#10070a 60%);color:#ffeef2}
.w{max-width:900px;margin:0 auto;padding:20px}
.c{background:rgba(28,10,16,.92);border:1px solid #4d2430;border-radius:18px;padding:16px;box-shadow:0 16px 44px rgba(0,0,0,.28);margin-top:14px}
label{display:block;margin-top:10px;margin-bottom:5px;color:#f0a8b8;font-size:13px}
input{width:100%;box-sizing:border-box;background:#1f0f16;border:1px solid #5f2e3c;color:#fff0f4;border-radius:10px;padding:10px 11px}
button{border:0;border-radius:10px;padding:10px 14px;background:#ff7f96;color:#2b0010;font-weight:800;cursor:pointer}
button.s{background:#3a2430;color:#ffeef2}
.r{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px}
.m{color:#d6a9b4;font-size:13px;line-height:1.5}
.p{display:inline-block;padding:4px 8px;border-radius:999px;background:#311521;color:#ffd9e1;font-size:12px;margin-right:6px}
</style></head><body><div class="w">
<div class="c">
  <div style="font-size:28px;font-weight:800">lg_apa102 Recovery</div>
  <div class="m">Signed OTA recovery for ESP32-S2 Mini. The bootloader stays untouched.</div>
</div>
<div class="c">
  <b>Update source</b>
  <label>Pack base URL</label><div class="p">https://serjio193.github.io/lg_apa102/latest/</div>
  <div class="r">
    <button onclick="flash()">Flash signed release</button>
    <button class="s" onclick="bootMain()">Boot main</button>
    <button class="s" onclick="resetCfg()">Reset settings</button>
  </div>
</div>
<div class="c">
  <b>Status</b>
  <div id="status" style="margin-top:8px"></div>
  <div class="m" id="msg" style="margin-top:10px"></div>
</div>
</div>
<script>
const msg = document.getElementById('msg');
const statusEl = document.getElementById('status');
async function loadStatus(){
  const r = await fetch('/api/status');
  const j = await r.json();
  statusEl.innerHTML = `<span class="p">${j.mode}</span><span class="p">${j.ip}</span><span class="p">${j.host}</span><div style="margin-top:8px">${j.packBaseUrl}</div><div style="margin-top:8px">${j.lastError}</div>`;
}
async function flash(){
  const r = await fetch('/api/recovery/flash', {method:'POST'});
  const j = await r.json();
  msg.textContent = j.ok ? 'flashing started' : ('error: ' + j.error);
}
async function bootMain(){
  const r = await fetch('/api/recovery/boot_main', {method:'POST'});
  const j = await r.json();
  msg.textContent = j.ok ? 'rebooting' : ('error: ' + j.error);
}
async function resetCfg(){
  const r = await fetch('/api/recovery/reset_settings', {method:'POST'});
  const j = await r.json();
  msg.textContent = j.ok ? 'settings cleared' : ('error: ' + j.error);
}
loadStatus();
</script>
</body></html>
)rawliteral";

struct ReleaseInfo {
  char version[32];
  char firmwareUrl[192];
  char firmwareSigUrl[192];
};

static void setLastError(const char *msg) {
  strlcpy(lastError, msg ? msg : "error", sizeof(lastError));
}

static void startAp() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(LB_DEFAULT_AP_SSID, LB_DEFAULT_AP_PASS);
  apActive = true;
}

static void startMdns() {
  MDNS.end();
  String host = lbHostnameFromName(cfg.deviceName);
  MDNS.begin(host.c_str());
}

static String currentIp() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  if (apActive) return WiFi.softAPIP().toString();
  return "0.0.0.0";
}

static bool fetchText(const String &url, String &out) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  out = http.getString();
  http.end();
  return true;
}

static bool parseManifest(const String &text, ReleaseInfo &info) {
  memset(&info, 0, sizeof(info));
  String base = cfg.packBaseUrl;
  if (!base.endsWith("/")) base += "/";
  int start = 0;
  while (start < static_cast<int>(text.length())) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String line = text.substring(start, end);
    line.trim();
    start = end + 1;
    if (!line.length() || line[0] == '#') continue;
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    String key = line.substring(0, eq);
    String value = line.substring(eq + 1);
    key.trim();
    value.trim();
    if (key == "version") {
      strlcpy(info.version, value.c_str(), sizeof(info.version));
      continue;
    }
    if (key == "firmware" || key == "firmware_url") {
      String full = value.startsWith("http://") || value.startsWith("https://") ? value : base + value;
      strlcpy(info.firmwareUrl, full.c_str(), sizeof(info.firmwareUrl));
    } else if (key == "firmware_sig" || key == "firmware_sig_url") {
      String full = value.startsWith("http://") || value.startsWith("https://") ? value : base + value;
      strlcpy(info.firmwareSigUrl, full.c_str(), sizeof(info.firmwareSigUrl));
    }
  }
  return info.firmwareUrl[0] != '\0' && info.firmwareSigUrl[0] != '\0';
}

static bool fetchBinary(const String &url, uint8_t *buffer, size_t bufferSize, size_t &outLen) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  int len = http.getSize();
  if (len <= 0 || static_cast<size_t>(len) > bufferSize) {
    http.end();
    return false;
  }
  WiFiClient *stream = http.getStreamPtr();
  size_t total = 0;
  while (total < static_cast<size_t>(len)) {
    int avail = stream->available();
    if (avail <= 0) {
      delay(1);
      continue;
    }
    size_t want = static_cast<size_t>(avail);
    if (want > bufferSize - total) want = bufferSize - total;
    int read = stream->readBytes(buffer + total, want);
    if (read <= 0) break;
    total += static_cast<size_t>(read);
  }
  http.end();
  outLen = total;
  return total == static_cast<size_t>(len);
}

static const esp_partition_t *mainPartition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
}

static bool verifySignatureHash(const uint8_t *hash, size_t hashLen, const uint8_t *sig, size_t sigLen) {
  if (hashLen != 32) return false;
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  if (mbedtls_pk_parse_public_key(&pk, reinterpret_cast<const unsigned char *>(LB_RELEASE_PUBLIC_KEY_PEM),
                                  sizeof(LB_RELEASE_PUBLIC_KEY_PEM)) != 0) {
    mbedtls_pk_free(&pk);
    return false;
  }
  int ok = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, hashLen, sig, sigLen);
  mbedtls_pk_free(&pk);
  return ok == 0;
}

static bool flashFirmwareFromUrl(const String &url, const String &sigUrl) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    setLastError("firmware begin failed");
    return false;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    setLastError("firmware GET failed");
    return false;
  }
  int size = http.getSize();
  if (size <= 0) {
    http.end();
    setLastError("firmware size unavailable");
    return false;
  }
  const esp_partition_t *part = mainPartition();
  if (!part) {
    http.end();
    setLastError("ota_0 partition not found");
    return false;
  }
  esp_ota_handle_t handle = 0;
  esp_err_t err = esp_ota_begin(part, static_cast<size_t>(size), &handle);
  if (err != ESP_OK) {
    http.end();
    setLastError("esp_ota_begin failed");
    return false;
  }
  uint8_t sig[512];
  size_t sigLen = 0;
  if (!fetchBinary(sigUrl, sig, sizeof(sig), sigLen)) {
    esp_ota_abort(handle);
    http.end();
    setLastError("signature download failed");
    return false;
  }
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts_ret(&sha, 0);
  WiFiClient *stream = http.getStreamPtr();
  uint8_t chunk[1024];
  size_t total = 0;
  while (total < static_cast<size_t>(size)) {
    int avail = stream->available();
    if (avail <= 0) {
      delay(1);
      continue;
    }
    size_t want = static_cast<size_t>(avail);
    if (want > sizeof(chunk)) want = sizeof(chunk);
    if (want > static_cast<size_t>(size) - total) want = static_cast<size_t>(size) - total;
    int read = stream->readBytes(chunk, want);
    if (read <= 0) {
      mbedtls_sha256_free(&sha);
      esp_ota_abort(handle);
      http.end();
      setLastError("firmware stream failed");
      return false;
    }
    mbedtls_sha256_update_ret(&sha, chunk, static_cast<size_t>(read));
    err = esp_ota_write(handle, chunk, static_cast<size_t>(read));
    if (err != ESP_OK) {
      mbedtls_sha256_free(&sha);
      esp_ota_abort(handle);
      http.end();
      setLastError("esp_ota_write failed");
      return false;
    }
    total += static_cast<size_t>(read);
  }
  uint8_t hash[32];
  mbedtls_sha256_finish_ret(&sha, hash);
  mbedtls_sha256_free(&sha);
  http.end();
  if (!verifySignatureHash(hash, sizeof(hash), sig, sigLen)) {
    esp_ota_abort(handle);
    setLastError("signature mismatch");
    return false;
  }
  err = esp_ota_end(handle);
  if (err != ESP_OK) {
    setLastError("esp_ota_end failed");
    return false;
  }
  err = esp_ota_set_boot_partition(part);
  if (err != ESP_OK) {
    setLastError("boot switch failed");
    return false;
  }
  return true;
}

static void handleStatus() {
  char json[512];
  snprintf(json, sizeof(json),
           "{\"ok\":true,\"deviceName\":\"%s\",\"host\":\"%s\",\"mode\":\"%s\",\"ip\":\"%s\",\"packBaseUrl\":\"%s\",\"lastError\":\"%s\"}",
           cfg.deviceName, lbHostnameFromName(cfg.deviceName).c_str(),
           WiFi.status() == WL_CONNECTED ? "STA" : (apActive ? "AP" : "BOOT"),
           currentIp().c_str(), cfg.packBaseUrl, lastError);
  server.send(200, "application/json", json);
}

static void handleConfig() {
  server.send(403, "application/json", "{\"ok\":false,\"error\":\"update URL is fixed\"}");
}

static bool performFlash() {
  String manifest;
  String base = LB_DEFAULT_PACK_BASE_URL;
  if (!base.endsWith("/")) base += "/";
  if (!fetchText(base + "release.txt", manifest)) {
    setLastError("manifest download failed");
    return false;
  }
  ReleaseInfo info{};
  if (!parseManifest(manifest, info)) {
    setLastError("manifest parse failed");
    return false;
  }
  if (!flashFirmwareFromUrl(info.firmwareUrl, info.firmwareSigUrl)) {
    setLastError("download or signature failed");
    return false;
  }
  setLastError("flash ok");
  return true;
}

static void handleFlash() {
  if (!performFlash()) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"update failed\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
  delay(400);
  ESP.restart();
}

static void handleBootMain() {
  const esp_partition_t *part = mainPartition();
  if (!part) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"ota_0 partition not found\"}");
    return;
  }
  esp_err_t err = esp_ota_set_boot_partition(part);
  if (err != ESP_OK) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"boot switch failed\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
  delay(300);
  ESP.restart();
}

static void handleResetSettings() {
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, false);
  prefs.clear();
  prefs.end();
  lbSetDefaults(&cfg);
  lbSaveConfig(&cfg);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void setupWeb() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", RECOVERY_HTML); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/recovery/flash", HTTP_POST, handleFlash);
  server.on("/api/recovery/boot_main", HTTP_POST, handleBootMain);
  server.on("/api/recovery/reset_settings", HTTP_POST, handleResetSettings);
  server.begin();
}

void setup() {
  lbLoadConfig(&cfg);
  if (!lbValidateConfig(&cfg)) lbSetDefaults(&cfg);
  strlcpy(cfg.packBaseUrl, LB_DEFAULT_PACK_BASE_URL, sizeof(cfg.packBaseUrl));
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  String host = lbHostnameFromName(cfg.deviceName);
  WiFi.setHostname(host.c_str());
  if (cfg.wifiSsid[0] != '\0') {
    WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
    unsigned long start = millis();
    while (millis() - start < 12000) {
      if (WiFi.status() == WL_CONNECTED) break;
      delay(250);
    }
  }
  if (WiFi.status() != WL_CONNECTED) {
    startAp();
  }
  startMdns();
  setupWeb();
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, false);
  const bool updatePending = prefs.getBool("update_pending", false);
  if (updatePending) prefs.putBool("update_pending", false);
  prefs.end();
  if (updatePending && WiFi.status() == WL_CONNECTED && performFlash()) {
    delay(400);
    ESP.restart();
  }
}

void loop() {
  server.handleClient();
}
