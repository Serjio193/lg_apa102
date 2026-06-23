#include <Arduino.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "../include/lb_common.h"

static LBConfig cfg;
static WebServer server(80);
static SPISettings spiSettings(8000000, MSBFIRST, SPI_MODE0);
static bool apActive = false;
static bool spiReady = false;
static bool powerState = false;
static bool outputsEnabled = false;
static uint8_t lastR = 0;
static uint8_t lastG = 0;
static uint8_t lastB = 0;
static unsigned long lastWifiCheckMs = 0;

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>lg_apa102</title>
<style>
body{margin:0;font-family:system-ui,sans-serif;background:radial-gradient(circle at top,#1a2240,#0b1020 60%);color:#e5eefc}
.w{max-width:1060px;margin:0 auto;padding:20px}
.g{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}
.c{background:rgba(16,24,44,.92);border:1px solid #243253;border-radius:18px;padding:16px;box-shadow:0 16px 44px rgba(0,0,0,.28)}
.h{display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap}
.t{font-size:28px;font-weight:800}
.m{color:#93a7cc;font-size:13px}
label{display:block;margin-top:10px;margin-bottom:5px;color:#90a4c8;font-size:13px}
input{width:100%;box-sizing:border-box;background:#121b33;border:1px solid #314363;color:#eef6ff;border-radius:10px;padding:10px 11px}
button{border:0;border-radius:10px;padding:10px 14px;background:#5ed0ff;color:#00111d;font-weight:800;cursor:pointer}
button.s{background:#243352;color:#e5eefc}
button.d{background:#ff7a7a;color:#200}
.r{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px}
.p{display:inline-block;padding:4px 8px;border-radius:999px;background:#1a2642;color:#d7e7ff;font-size:12px;margin-right:6px}
.st{font-size:14px;line-height:1.5}
.sw{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}
.sw i{display:block;width:40px;height:40px;border-radius:12px;border:1px solid rgba(255,255,255,.14);cursor:pointer}
.full{grid-column:1/-1}
@media(max-width:760px){.g{grid-template-columns:1fr}.w{padding:12px}.t{font-size:24px}}
</style></head><body><div class="w">
<div class="c">
  <div class="h">
    <div><div class="t">lg_apa102</div><div class="m">ESP32-S2 Mini protected OTA + recovery</div></div>
    <div class="st" id="status">Loading...</div>
  </div>
</div>
<div class="g" style="margin-top:14px">
  <div class="c">
    <b>Network</b>
    <label>Device name</label><input id="deviceName">
    <label>Wi-Fi SSID</label><input id="wifiSsid">
    <label>Wi-Fi password</label><input id="wifiPass" type="password">
    <div class="m" style="margin-top:8px">If STA fails, AP mode starts automatically.</div>
  </div>
  <div class="c">
    <b>GPIO</b>
    <div class="g" style="grid-template-columns:repeat(2,minmax(0,1fr));gap:10px">
      <div><label>DATA</label><input id="dataPin" type="number" min="-1" max="48"></div>
      <div><label>CLOCK</label><input id="clockPin" type="number" min="-1" max="48"></div>
      <div><label>OE</label><input id="oePin" type="number" min="-1" max="48"></div>
      <div><label>POWER</label><input id="powerPin" type="number" min="-1" max="48"></div>
      <div><label>LED count</label><input id="ledCount" type="number" min="1" max="2048"></div>
      <div><label>Brightness 1..31</label><input id="ledBrightness" type="number" min="1" max="31"></div>
      <div><label>OE active low</label><input id="oeActiveLow" type="number" min="0" max="1"></div>
      <div><label>POWER active high</label><input id="powerActiveHigh" type="number" min="0" max="1"></div>
    </div>
  </div>
  <div class="c">
    <b>Update site</b>
    <label>Pack base URL</label><input id="packBaseUrl" placeholder="https://.../latest/">
    <div class="m" style="margin-top:8px">Recovery downloads release.txt + signed binaries from this URL.</div>
    <div class="r">
      <button onclick="saveCfg()">Save</button>
      <button class="s" onclick="enterRecovery()">Enter recovery</button>
      <button class="s" onclick="reboot()">Reboot</button>
    </div>
  </div>
  <div class="c">
    <b>Test output</b>
    <div class="g" style="grid-template-columns:repeat(3,minmax(0,1fr));gap:10px">
      <div><label>R</label><input id="r" type="number" min="0" max="255" value="255"></div>
      <div><label>G</label><input id="g" type="number" min="0" max="255" value="32"></div>
      <div><label>B</label><input id="b" type="number" min="0" max="255" value="0"></div>
    </div>
    <div class="r">
      <button onclick="sendColor()">Send color</button>
      <button class="s" onclick="sendOff()">Off</button>
    </div>
    <div class="sw">
      <i style="background:#ff4040" onclick="preset(255,64,64)"></i>
      <i style="background:#40ff80" onclick="preset(64,255,128)"></i>
      <i style="background:#4080ff" onclick="preset(64,128,255)"></i>
      <i style="background:#fff" onclick="preset(255,255,255)"></i>
      <i style="background:#000" onclick="preset(0,0,0)"></i>
    </div>
  </div>
  <div class="c full">
    <b>Status</b>
    <div class="m" style="margin-top:8px">GPIO46 is rejected on ESP32-S2.</div>
    <div class="m" id="msg" style="margin-top:8px"></div>
  </div>
</div>
</div>
<script>
const msg = document.getElementById('msg');
const statusEl = document.getElementById('status');
async function loadInfo(){
  const r = await fetch('/api/info');
  const j = await r.json();
  statusEl.innerHTML = `<span class="p">${j.mode}</span><span class="p">${j.ip}</span><span class="p">${j.host}</span><div style="margin-top:8px">${j.ledCount} LEDs | DATA ${j.dataPin} CLOCK ${j.clockPin} OE ${j.oePin} POWER ${j.powerPin}</div><div>${j.power ? 'power on' : 'power off'} | ${j.outputs ? 'outputs enabled' : 'outputs off'}</div>`;
  deviceName.value = j.deviceName; wifiSsid.value = j.wifiSsid; wifiPass.value = '';
  dataPin.value = j.dataPin; clockPin.value = j.clockPin; oePin.value = j.oePin; powerPin.value = j.powerPin;
  ledCount.value = j.ledCount; ledBrightness.value = j.ledBrightness; oeActiveLow.value = j.oeActiveLow ? 1 : 0; powerActiveHigh.value = j.powerActiveHigh ? 1 : 0;
  packBaseUrl.value = j.packBaseUrl;
}
async function saveCfg(){
  const body = new URLSearchParams({
    deviceName: deviceName.value, wifiSsid: wifiSsid.value, wifiPass: wifiPass.value,
    packBaseUrl: packBaseUrl.value, dataPin: dataPin.value, clockPin: clockPin.value, oePin: oePin.value,
    powerPin: powerPin.value, ledCount: ledCount.value, ledBrightness: ledBrightness.value,
    oeActiveLow: oeActiveLow.value, powerActiveHigh: powerActiveHigh.value
  });
  const r = await fetch('/api/config', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
  const j = await r.json();
  msg.textContent = j.ok ? 'saved' : ('error: ' + j.error);
  if (j.ok) setTimeout(()=>location.reload(), 1500);
}
async function enterRecovery(){
  const r = await fetch('/api/recovery/enter', {method:'POST'});
  const j = await r.json();
  msg.textContent = j.ok ? 'switching to recovery' : ('error: ' + j.error);
}
async function reboot(){ await fetch('/api/reboot', {method:'POST'}); msg.textContent = 'rebooting'; }
async function sendColor(){
  const body = new URLSearchParams({r:r.value, g:g.value, b:b.value});
  const res = await fetch('/api/test/color', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
  const j = await res.json(); msg.textContent = j.ok ? 'color sent' : ('error: ' + j.error);
}
async function sendOff(){ const r = await fetch('/api/test/off', {method:'POST'}); const j = await r.json(); msg.textContent = j.ok ? 'off' : ('error: ' + j.error); }
function preset(rr,gg,bb){ r.value=rr; g.value=gg; b.value=bb; sendColor(); }
loadInfo();
</script>
</body></html>
)rawliteral";

static void setPower(bool on) {
  if (cfg.powerPin < 0) return;
  digitalWrite(cfg.powerPin, cfg.powerActiveHigh ? (on ? HIGH : LOW) : (on ? LOW : HIGH));
  powerState = on;
}

static void setOutputsEnabled(bool on) {
  if (cfg.oePin < 0) {
    outputsEnabled = on;
    return;
  }
  digitalWrite(cfg.oePin, cfg.oeActiveLow ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
  outputsEnabled = on;
}

static void writeApa102Solid(uint8_t r, uint8_t g, uint8_t b) {
  if (!spiReady || cfg.dataPin < 0 || cfg.clockPin < 0 || cfg.ledCount == 0) return;
  SPI.beginTransaction(spiSettings);
  for (uint8_t i = 0; i < 4; ++i) SPI.transfer(0x00);
  const uint8_t frame = 0xE0 | (cfg.ledBrightness & 0x1F);
  for (uint16_t i = 0; i < cfg.ledCount; ++i) {
    SPI.transfer(frame);
    SPI.transfer(b);
    SPI.transfer(g);
    SPI.transfer(r);
  }
  for (uint16_t i = 0, endBytes = (cfg.ledCount + 15U) / 16U; i < endBytes; ++i) SPI.transfer(0xFF);
  SPI.endTransaction();
}

static void ensurePowerAndOutput() {
  setOutputsEnabled(false);
  if (cfg.powerPin >= 0 && !powerState) {
    setPower(true);
    delay(cfg.powerOnDelayMs);
  }
}

static void showSolid(uint8_t r, uint8_t g, uint8_t b) {
  lastR = r; lastG = g; lastB = b;
  ensurePowerAndOutput();
  writeApa102Solid(r, g, b);
  setOutputsEnabled(true);
}

static void outputOff(bool cutPower) {
  setOutputsEnabled(false);
  if (cfg.dataPin >= 0 && cfg.clockPin >= 0 && cfg.ledCount > 0) {
    writeApa102Solid(0, 0, 0);
  }
  if (cutPower && cfg.powerPin >= 0 && powerState) {
    delay(cfg.powerOffDelayMs);
    setPower(false);
  }
}

static void configureHardware() {
  spiReady = false;
  outputOff(false);
  if (cfg.oePin >= 0) pinMode(cfg.oePin, OUTPUT);
  if (cfg.powerPin >= 0) pinMode(cfg.powerPin, OUTPUT);
  if (cfg.dataPin >= 0 && cfg.clockPin >= 0) {
    SPI.end();
    SPI.begin(cfg.clockPin, -1, cfg.dataPin, -1);
    spiReady = true;
  }
  setPower(false);
  setOutputsEnabled(false);
}

static bool connectSta() {
  if (cfg.wifiSsid[0] == '\0') return false;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(lbHostnameFromName(cfg.deviceName).c_str());
  WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
  unsigned long start = millis();
  while (millis() - start < 15000) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(250);
  }
  WiFi.disconnect(true, true);
  return false;
}

static void startAp() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(LB_DEFAULT_AP_SSID, LB_DEFAULT_AP_PASS);
  apActive = true;
}

static void startMdns() {
  MDNS.end();
  MDNS.begin(lbHostnameFromName(cfg.deviceName).c_str());
}

static String currentIp() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  if (apActive) return WiFi.softAPIP().toString();
  return "0.0.0.0";
}

static void handleInfo() {
  char json[560];
  snprintf(json, sizeof(json),
           "{\"ok\":true,\"deviceName\":\"%s\",\"host\":\"%s\",\"mode\":\"%s\",\"ip\":\"%s\",\"wifiSsid\":\"%s\",\"packBaseUrl\":\"%s\",\"dataPin\":%d,\"clockPin\":%d,\"oePin\":%d,\"powerPin\":%d,\"ledCount\":%u,\"ledBrightness\":%u,\"oeActiveLow\":%s,\"powerActiveHigh\":%s,\"power\":%s,\"outputs\":%s}",
           cfg.deviceName, lbHostnameFromName(cfg.deviceName).c_str(),
           WiFi.status() == WL_CONNECTED ? "STA" : (apActive ? "AP" : "BOOT"),
           currentIp().c_str(), cfg.wifiSsid, cfg.packBaseUrl, cfg.dataPin, cfg.clockPin, cfg.oePin, cfg.powerPin,
           cfg.ledCount, cfg.ledBrightness, cfg.oeActiveLow ? "true" : "false",
           cfg.powerActiveHigh ? "true" : "false", powerState ? "true" : "false", outputsEnabled ? "true" : "false");
  server.send(200, "application/json", json);
}

static void handleConfig() {
  LBConfig next = cfg;
  lbCopyArg(next.deviceName, sizeof(next.deviceName), server.arg("deviceName"));
  lbCopyArg(next.wifiSsid, sizeof(next.wifiSsid), server.arg("wifiSsid"));
  lbCopyArg(next.wifiPass, sizeof(next.wifiPass), server.arg("wifiPass"));
  lbCopyArg(next.packBaseUrl, sizeof(next.packBaseUrl), server.arg("packBaseUrl"));
  next.dataPin = lbArgPin(server, "dataPin", next.dataPin);
  next.clockPin = lbArgPin(server, "clockPin", next.clockPin);
  next.oePin = lbArgPin(server, "oePin", next.oePin);
  next.powerPin = lbArgPin(server, "powerPin", next.powerPin);
  next.ledCount = lbArgU16(server, "ledCount", next.ledCount, 1, 2048);
  next.ledBrightness = lbArgU8(server, "ledBrightness", next.ledBrightness, 1, 31);
  next.oeActiveLow = lbArgU8(server, "oeActiveLow", next.oeActiveLow, 0, 1);
  next.powerActiveHigh = lbArgU8(server, "powerActiveHigh", next.powerActiveHigh, 0, 1);
  lbNormalizeDefaults(&next);
  if (!lbValidateConfig(&next)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid config or GPIO\"}");
    return;
  }
  cfg = next;
  lbSaveConfig(&cfg);
  server.send(200, "application/json", "{\"ok\":true}");
  delay(250);
  ESP.restart();
}

static const esp_partition_t *factoryPartition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
}

static const esp_partition_t *mainPartition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
}

static void handleEnterRecovery() {
  const esp_partition_t *part = factoryPartition();
  if (!part) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"factory recovery partition not found\"}");
    return;
  }
  esp_err_t err = esp_ota_set_boot_partition(part);
  if (err != ESP_OK) {
    char json[96];
    snprintf(json, sizeof(json), "{\"ok\":false,\"error\":\"esp_ota_set_boot_partition:%d\"}", (int)err);
    server.send(500, "application/json", json);
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
  delay(300);
  ESP.restart();
}

static void handleReboot() {
  server.send(200, "application/json", "{\"ok\":true}");
  delay(250);
  ESP.restart();
}

static void handleColor() {
  uint8_t r = lbArgU8(server, "r", lastR, 0, 255);
  uint8_t g = lbArgU8(server, "g", lastG, 0, 255);
  uint8_t b = lbArgU8(server, "b", lastB, 0, 255);
  showSolid(r, g, b);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleOff() {
  outputOff(true);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void setupWeb() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/info", HTTP_GET, handleInfo);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/recovery/enter", HTTP_POST, handleEnterRecovery);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/test/color", HTTP_POST, handleColor);
  server.on("/api/test/off", HTTP_POST, handleOff);
  server.begin();
}

void setup() {
  lbLoadConfig(&cfg);
  if (!lbValidateConfig(&cfg)) lbSetDefaults(&cfg);
  configureHardware();
  if (!connectSta()) {
    startAp();
  }
  startMdns();
  setupWeb();
}

void loop() {
  server.handleClient();
  const unsigned long now = millis();
  if (now - lastWifiCheckMs > 10000) {
    lastWifiCheckMs = now;
    if (WiFi.status() != WL_CONNECTED && !apActive) {
      startAp();
      startMdns();
    }
  }
}
