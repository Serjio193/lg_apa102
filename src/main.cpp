#include <Arduino.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <USB.h>
#include <USBCDC.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "../include/lb_common.h"
#include "../include/lb_version.h"
#include "../include/ui_app_js.h"
#include "../include/ui_page.h"

static LBConfig cfg;
static WebServer server(80);
static USBCDC hyperHdrUsb;
static bool apActive = false;
static bool apWindowExpired = false;
static constexpr unsigned long AP_WINDOW_MS = 10UL * 60UL * 1000UL;
static bool spiReady = false;
static bool powerState = false;
static bool outputsEnabled = false;
static uint8_t currentBrightness = 24;
static uint8_t lastR = 0;
static uint8_t lastG = 0;
static uint8_t lastB = 0;
static uint8_t effectMode = 0;
static uint8_t effectSpeed = 128;
static uint8_t effectIntensity = 128;
static uint32_t effectFrame = 0;
static unsigned long lastEffectMs = 0;
static unsigned long lastWifiCheckMs = 0;
static unsigned long powerOnSinceMs = 0;
static unsigned long lastHyperHdrFrameMs = 0;
static unsigned long hyperHdrFpsWindowMs = 0;
static uint32_t hyperHdrFrameCount = 0;
static uint32_t hyperHdrBadFrames = 0;
static uint16_t hyperHdrWindowFrames = 0;
static uint16_t hyperHdrFps = 0;
static constexpr uint32_t HYPERHDR_TIMEOUT_MS = 2000;
static constexpr uint32_t LED_DETECT_INTERVAL_MS = 2000;
static constexpr uint16_t MAX_HYPERHDR_PIXELS = 2048;
static uint8_t hyperHdrFrame[MAX_HYPERHDR_PIXELS * 3U];
static bool ledDetectActive = false;
static uint16_t ledDetectStart = 0;
static uint16_t ledDetectIndex = 0;
static unsigned long lastLedDetectMs = 0;
static constexpr uint8_t LOG_CAPACITY = 24;
static constexpr uint8_t LOG_MESSAGE_SIZE = 64;

struct LogEntry {
  uint32_t timeMs;
  char message[LOG_MESSAGE_SIZE];
};

static LogEntry logEntries[LOG_CAPACITY]{};
static uint8_t logStart = 0;
static uint8_t logCount = 0;

enum SourceOwner : uint8_t {
  SOURCE_IDLE = 0,
  SOURCE_HYPERHDR,
  SOURCE_MANUAL
};

static SourceOwner sourceOwner = SOURCE_IDLE;

static void addLog(const char *message) {
  const uint8_t index = static_cast<uint8_t>((logStart + logCount) % LOG_CAPACITY);
  logEntries[index].timeMs = millis();
  strlcpy(logEntries[index].message, message, sizeof(logEntries[index].message));
  if (logCount < LOG_CAPACITY) {
    ++logCount;
  } else {
    logStart = static_cast<uint8_t>((logStart + 1U) % LOG_CAPACITY);
  }
}

static void addAwaError() {
  ++hyperHdrBadFrames;
  if (hyperHdrBadFrames == 1 || hyperHdrBadFrames % 10 == 0) {
    char message[LOG_MESSAGE_SIZE];
    snprintf(message, sizeof(message), "AWA frame errors: %lu", static_cast<unsigned long>(hyperHdrBadFrames));
    addLog(message);
  }
}

struct LedLayout {
  uint16_t top;
  uint16_t right;
  uint16_t bottom;
  uint16_t left;
  uint8_t firstLed;
  uint8_t direction;
};

static LedLayout ledLayout{19, 11, 19, 11, 0, 0};

struct LedHardware {
  uint32_t magic;
  uint8_t stripType;
  uint8_t colorOrder;
  uint32_t spiHz;
};

static constexpr uint32_t LED_HARDWARE_MAGIC = 0x4C454448UL;
static LedHardware ledHardware{LED_HARDWARE_MAGIC, 0, 5, 8000000UL};
static constexpr int8_t S2_MINI_OUTPUT_PINS[] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 18,
  21, 33, 34, 35, 36, 37, 38, 39, 40
};

struct PowerSettings {
  uint32_t magic;
  uint16_t autoOffMinutes;
};

static constexpr uint32_t POWER_SETTINGS_MAGIC = 0x50575232UL;
static PowerSettings powerSettings{POWER_SETTINGS_MAGIC, 0};

enum EffectMode : uint8_t {
  EFFECT_SOLID = 0,
  EFFECT_BLINK,
  EFFECT_BREATHE,
  EFFECT_WIPE,
  EFFECT_RAINBOW,
  EFFECT_COLORLOOP,
  EFFECT_CHASE,
  EFFECT_TWINKLE,
  EFFECT_COUNT
};

static void ensurePowerAndOutput();

static void loadLedLayout() {
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, true);
  LedLayout stored{};
  const size_t length = prefs.getBytes("ledlayout", &stored, sizeof(stored));
  prefs.end();
  const uint32_t total = static_cast<uint32_t>(stored.top) + stored.right + stored.bottom + stored.left;
  if (length == sizeof(stored) && total == cfg.ledCount && stored.firstLed < 4 && stored.direction < 2) {
    ledLayout = stored;
    return;
  }
  const uint16_t horizontal = static_cast<uint16_t>((static_cast<uint32_t>(cfg.ledCount) * 64U) / 100U);
  const uint16_t vertical = cfg.ledCount - horizontal;
  ledLayout = {
    static_cast<uint16_t>((horizontal + 1U) / 2U),
    static_cast<uint16_t>((vertical + 1U) / 2U),
    static_cast<uint16_t>(horizontal / 2U),
    static_cast<uint16_t>(vertical / 2U),
    0,
    0,
  };
}

static void saveLedLayout() {
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, false);
  prefs.putBytes("ledlayout", &ledLayout, sizeof(ledLayout));
  prefs.end();
}

static uint32_t maxSpiHzForStrip(uint8_t stripType) {
  switch (stripType) {
    case 1: return 12000000UL;  // SK9822
    case 2: return 24000000UL;  // HD107S
    case 3: return 20000000UL;  // APA102-2020
    default: return 20000000UL; // APA102
  }
}

static bool isS2MiniOutputPin(int16_t pin) {
  for (const int8_t allowed : S2_MINI_OUTPUT_PINS) {
    if (pin == allowed) return true;
  }
  return false;
}

static void loadLedHardware() {
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, true);
  LedHardware stored{};
  const size_t length = prefs.getBytes("ledhw", &stored, sizeof(stored));
  prefs.end();
  if (length == sizeof(stored) && stored.magic == LED_HARDWARE_MAGIC &&
      stored.stripType < 4 && stored.colorOrder < 6 &&
      stored.spiHz >= 1000000UL && stored.spiHz <= maxSpiHzForStrip(stored.stripType)) {
    ledHardware = stored;
  }
}

static void saveLedHardware() {
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, false);
  prefs.putBytes("ledhw", &ledHardware, sizeof(ledHardware));
  prefs.end();
}

static void loadPowerSettings() {
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, true);
  PowerSettings stored{};
  const size_t length = prefs.getBytes("powercfg", &stored, sizeof(stored));
  prefs.end();
  if (length == sizeof(stored) && stored.magic == POWER_SETTINGS_MAGIC && stored.autoOffMinutes <= 1440) {
    powerSettings = stored;
  }
}

static void savePowerSettings() {
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, false);
  prefs.putBytes("powercfg", &powerSettings, sizeof(powerSettings));
  prefs.end();
}

// Legacy HTML kept out of the build; the React UI is generated into ui_page.h.
#if 0
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
#endif

static void setPower(bool on) {
  if (cfg.powerPin >= 0) {
    digitalWrite(cfg.powerPin, cfg.powerActiveHigh ? (on ? HIGH : LOW) : (on ? LOW : HIGH));
  }
  powerState = on;
  powerOnSinceMs = on ? millis() : 0;
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
  SPI.beginTransaction(SPISettings(ledHardware.spiHz, MSBFIRST, SPI_MODE0));
  for (uint8_t i = 0; i < 4; ++i) SPI.transfer(0x00);
  const uint8_t frame = 0xE0 | (currentBrightness & 0x1F);
  for (uint16_t i = 0; i < cfg.ledCount; ++i) {
    SPI.transfer(frame);
    const uint8_t channels[3] = {r, g, b};
    static constexpr uint8_t orders[6][3] = {
      {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
    };
    SPI.transfer(channels[orders[ledHardware.colorOrder][0]]);
    SPI.transfer(channels[orders[ledHardware.colorOrder][1]]);
    SPI.transfer(channels[orders[ledHardware.colorOrder][2]]);
  }
  for (uint16_t i = 0, endBytes = (cfg.ledCount + 15U) / 16U; i < endBytes; ++i) SPI.transfer(0xFF);
  SPI.endTransaction();
}

static void writeApa102Start() {
  SPI.beginTransaction(SPISettings(ledHardware.spiHz, MSBFIRST, SPI_MODE0));
  for (uint8_t i = 0; i < 4; ++i) SPI.transfer(0x00);
}

static void writeApa102Pixel(uint8_t r, uint8_t g, uint8_t b) {
  const uint8_t channels[3] = {r, g, b};
  static constexpr uint8_t orders[6][3] = {
    {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
  };
  SPI.transfer(0xE0 | (currentBrightness & 0x1F));
  SPI.transfer(channels[orders[ledHardware.colorOrder][0]]);
  SPI.transfer(channels[orders[ledHardware.colorOrder][1]]);
  SPI.transfer(channels[orders[ledHardware.colorOrder][2]]);
}

static void writeApa102End() {
  for (uint16_t i = 0, endBytes = (cfg.ledCount + 15U) / 16U; i < endBytes; ++i) SPI.transfer(0xFF);
  SPI.endTransaction();
}

static void writeLedDetectFrame(uint16_t activeIndex) {
  if (!spiReady || activeIndex >= MAX_HYPERHDR_PIXELS) return;
  ensurePowerAndOutput();
  writeApa102Start();
  for (uint16_t i = 0; i <= activeIndex; ++i) {
    if (i == activeIndex) writeApa102Pixel(255, 255, 255);
    else writeApa102Pixel(0, 0, 0);
  }
  const uint16_t endBytes = static_cast<uint16_t>((activeIndex + 16U) / 16U);
  for (uint16_t i = 0; i < endBytes; ++i) SPI.transfer(0xFF);
  SPI.endTransaction();
  setOutputsEnabled(true);
}

static void clearLedDetectFrame(uint16_t lastIndex) {
  if (!spiReady || lastIndex >= MAX_HYPERHDR_PIXELS) return;
  writeApa102Start();
  for (uint16_t i = 0; i <= lastIndex; ++i) writeApa102Pixel(0, 0, 0);
  const uint16_t endBytes = static_cast<uint16_t>((lastIndex + 16U) / 16U);
  for (uint16_t i = 0; i < endBytes; ++i) SPI.transfer(0xFF);
  SPI.endTransaction();
  setOutputsEnabled(false);
}

static void writeHyperHdrFrame(uint16_t pixelCount) {
  if (!spiReady || cfg.ledCount == 0) return;
  ensurePowerAndOutput();
  writeApa102Start();
  const uint16_t activePixels = min(pixelCount, cfg.ledCount);
  for (uint16_t i = 0; i < activePixels; ++i) {
    const uint16_t offset = i * 3U;
    writeApa102Pixel(hyperHdrFrame[offset], hyperHdrFrame[offset + 1U], hyperHdrFrame[offset + 2U]);
  }
  for (uint16_t i = activePixels; i < cfg.ledCount; ++i) writeApa102Pixel(0, 0, 0);
  writeApa102End();
  setOutputsEnabled(true);
}

static bool acquireManualSource() {
  if (sourceOwner == SOURCE_HYPERHDR) return false;
  if (sourceOwner != SOURCE_MANUAL) addLog("Manual mode active");
  sourceOwner = SOURCE_MANUAL;
  return true;
}

static void releaseSource() {
  if (sourceOwner == SOURCE_HYPERHDR) addLog("HyperHDR stream released");
  else if (sourceOwner == SOURCE_MANUAL) addLog("Manual mode released");
  ledDetectActive = false;
  sourceOwner = SOURCE_IDLE;
  effectMode = EFFECT_SOLID;
}

enum class AwaState : uint8_t {
  HeaderA,
  HeaderW,
  HeaderA2,
  CountHi,
  CountLo,
  HeaderCrc,
  Red,
  Green,
  Blue,
  Fletcher1,
  Fletcher2
};

static void handleHyperHdrSerial() {
  static AwaState state = AwaState::HeaderA;
  static uint8_t headerCrc = 0;
  static uint16_t remaining = 0;
  static uint16_t pixelCount = 0;
  static uint16_t pixelIndex = 0;
  static uint16_t fletcher1 = 0;
  static uint16_t fletcher2 = 0;
  static uint8_t red = 0;
  static uint8_t green = 0;
  uint16_t budget = 768;

  while (budget-- > 0 && hyperHdrUsb.available() > 0) {
    const uint8_t input = static_cast<uint8_t>(hyperHdrUsb.read());
    switch (state) {
      case AwaState::HeaderA:
        if (input == 'A') state = AwaState::HeaderW;
        break;
      case AwaState::HeaderW:
        state = input == 'w' ? AwaState::HeaderA2 : AwaState::HeaderA;
        break;
      case AwaState::HeaderA2:
        state = input == 'a' ? AwaState::CountHi : AwaState::HeaderA;
        break;
      case AwaState::CountHi:
        remaining = static_cast<uint16_t>(input) << 8;
        headerCrc = input;
        state = AwaState::CountLo;
        break;
      case AwaState::CountLo:
        remaining |= input;
        pixelCount = remaining + 1U;
        headerCrc ^= input ^ 0x55;
        state = AwaState::HeaderCrc;
        break;
      case AwaState::HeaderCrc:
        if (input == headerCrc && pixelCount <= MAX_HYPERHDR_PIXELS) {
          pixelIndex = 0;
          fletcher1 = 0;
          fletcher2 = 0;
          state = AwaState::Red;
        } else {
          addAwaError();
          state = AwaState::HeaderA;
        }
        break;
      case AwaState::Red:
        red = input;
        fletcher1 = (fletcher1 + input) % 255U;
        fletcher2 = (fletcher2 + fletcher1) % 255U;
        state = AwaState::Green;
        break;
      case AwaState::Green:
        green = input;
        fletcher1 = (fletcher1 + input) % 255U;
        fletcher2 = (fletcher2 + fletcher1) % 255U;
        state = AwaState::Blue;
        break;
      case AwaState::Blue: {
        fletcher1 = (fletcher1 + input) % 255U;
        fletcher2 = (fletcher2 + fletcher1) % 255U;
        const uint16_t offset = pixelIndex * 3U;
        hyperHdrFrame[offset] = red;
        hyperHdrFrame[offset + 1U] = green;
        hyperHdrFrame[offset + 2U] = input;
        ++pixelIndex;
        if (remaining-- == 0) state = AwaState::Fletcher1;
        else state = AwaState::Red;
        break;
      }
      case AwaState::Fletcher1:
        if (input == static_cast<uint8_t>(fletcher1)) {
          state = AwaState::Fletcher2;
        } else {
          addAwaError();
          state = AwaState::HeaderA;
        }
        break;
      case AwaState::Fletcher2:
        if (input == static_cast<uint8_t>(fletcher2) && sourceOwner != SOURCE_MANUAL) {
          const unsigned long now = millis();
          if (sourceOwner != SOURCE_HYPERHDR) addLog("HyperHDR USB stream active");
          sourceOwner = SOURCE_HYPERHDR;
          lastHyperHdrFrameMs = now;
          ++hyperHdrFrameCount;
          ++hyperHdrWindowFrames;
          if (hyperHdrFpsWindowMs == 0) hyperHdrFpsWindowMs = now;
          const unsigned long windowElapsed = now - hyperHdrFpsWindowMs;
          if (windowElapsed >= 1000) {
            hyperHdrFps = static_cast<uint16_t>(
              (static_cast<uint32_t>(hyperHdrWindowFrames) * 1000UL) / windowElapsed
            );
            hyperHdrWindowFrames = 0;
            hyperHdrFpsWindowMs = now;
          }
          writeHyperHdrFrame(pixelCount);
        } else if (input != static_cast<uint8_t>(fletcher2)) {
          addAwaError();
        }
        state = AwaState::HeaderA;
        break;
    }
  }
}

static void hsvToRgb(uint8_t hue, uint8_t saturation, uint8_t value, uint8_t &r, uint8_t &g, uint8_t &b) {
  const uint8_t region = hue / 43;
  const uint8_t remainder = (hue - (region * 43)) * 6;
  const uint8_t p = (value * (255 - saturation)) >> 8;
  const uint8_t q = (value * (255 - ((saturation * remainder) >> 8))) >> 8;
  const uint8_t t = (value * (255 - ((saturation * (255 - remainder)) >> 8))) >> 8;
  switch (region) {
    case 0: r = value; g = t; b = p; break;
    case 1: r = q; g = value; b = p; break;
    case 2: r = p; g = value; b = t; break;
    case 3: r = p; g = q; b = value; break;
    case 4: r = t; g = p; b = value; break;
    default: r = value; g = p; b = q; break;
  }
}

static uint8_t scaleColor(uint8_t value, uint8_t scale) {
  return static_cast<uint8_t>((static_cast<uint16_t>(value) * scale) / 255U);
}

static uint16_t effectIntervalMs() {
  return static_cast<uint16_t>(18U + ((255U - effectSpeed) * 282U) / 255U);
}

static void renderEffectFrame() {
  if (!spiReady || effectMode == EFFECT_SOLID || cfg.ledCount == 0) return;
  ensurePowerAndOutput();
  writeApa102Start();
  const uint16_t wipePosition = effectFrame % (cfg.ledCount + 1U);
  const uint16_t chaseWidth = 1U + (static_cast<uint16_t>(effectIntensity) * 11U) / 255U;
  const uint16_t chasePeriod = chaseWidth * 4U;
  const uint8_t breathe = static_cast<uint8_t>(abs(255 - static_cast<int>((effectFrame * 8U) & 0x1FFU)));

  for (uint16_t i = 0; i < cfg.ledCount; ++i) {
    uint8_t r = lastR;
    uint8_t g = lastG;
    uint8_t b = lastB;
    switch (effectMode) {
      case EFFECT_BLINK:
        if ((effectFrame & 1U) == 0) r = g = b = 0;
        break;
      case EFFECT_BREATHE:
        r = scaleColor(r, breathe);
        g = scaleColor(g, breathe);
        b = scaleColor(b, breathe);
        break;
      case EFFECT_WIPE:
        if (i >= wipePosition) r = g = b = 0;
        break;
      case EFFECT_RAINBOW: {
        const uint8_t hue = static_cast<uint8_t>((i * (4U + effectIntensity / 16U)) + effectFrame * 3U);
        hsvToRgb(hue, 255, 255, r, g, b);
        break;
      }
      case EFFECT_COLORLOOP:
        hsvToRgb(static_cast<uint8_t>(effectFrame * 3U), effectIntensity, 255, r, g, b);
        break;
      case EFFECT_CHASE: {
        const uint16_t position = (i + effectFrame) % chasePeriod;
        if (position >= chaseWidth) r = g = b = 0;
        break;
      }
      case EFFECT_TWINKLE: {
        uint32_t hash = (static_cast<uint32_t>(i) * 1103515245UL) ^ (effectFrame * 2654435761UL);
        hash ^= hash >> 16;
        const uint8_t threshold = static_cast<uint8_t>(4U + effectIntensity / 3U);
        if (static_cast<uint8_t>(hash) > threshold) {
          r = scaleColor(r, 18);
          g = scaleColor(g, 18);
          b = scaleColor(b, 18);
        }
        break;
      }
      default:
        break;
    }
    writeApa102Pixel(r, g, b);
  }
  writeApa102End();
  setOutputsEnabled(true);
  ++effectFrame;
}

static void ensurePowerAndOutput() {
  setOutputsEnabled(false);
  if (cfg.powerPin >= 0 && !powerState) {
    setPower(true);
    delay(cfg.powerOnDelayMs);
  }
}

static void showSolid(uint8_t r, uint8_t g, uint8_t b) {
  effectMode = EFFECT_SOLID;
  lastR = r; lastG = g; lastB = b;
  ensurePowerAndOutput();
  writeApa102Solid(r, g, b);
  setOutputsEnabled(true);
}

static void outputOff(bool cutPower) {
  effectMode = EFFECT_SOLID;
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
  setPower(cfg.powerPin < 0);
  setOutputsEnabled(false);
}

static bool connectSta() {
  if (cfg.wifiSsid[0] == '\0') {
    addLog("Wi-Fi SSID is not configured");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(lbHostnameFromName(cfg.deviceName).c_str());
  WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
  unsigned long start = millis();
  while (millis() - start < 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      addLog("Wi-Fi station connected");
      return true;
    }
    delay(250);
  }
  WiFi.disconnect(true, true);
  addLog("Wi-Fi connection failed");
  return false;
}

static void startAp() {
  if (apWindowExpired || millis() >= AP_WINDOW_MS) {
    apWindowExpired = true;
    return;
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(LB_DEFAULT_AP_SSID, LB_DEFAULT_AP_PASS);
  apActive = true;
  addLog("Access point started");
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

static bool fetchReleaseVersion(String &version) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  const String url = String(LB_DEFAULT_PACK_BASE_URL) + "release.txt";
  if (!http.begin(client, url)) return false;
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const String manifest = http.getString();
  http.end();
  int start = manifest.indexOf("version=");
  if (start < 0) return false;
  start += 8;
  int end = manifest.indexOf('\n', start);
  if (end < 0) end = manifest.length();
  version = manifest.substring(start, end);
  version.trim();
  return version.length() > 0;
}

static void handleInfo() {
  char json[1300];
  uint32_t autoOffRemaining = 0;
  const int32_t wifiRssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  const uint32_t hyperHdrLastFrameAge = lastHyperHdrFrameMs > 0 ? millis() - lastHyperHdrFrameMs : 0;
  if (sourceOwner != SOURCE_MANUAL && powerState && powerSettings.autoOffMinutes > 0 && powerOnSinceMs > 0) {
    const uint32_t duration = static_cast<uint32_t>(powerSettings.autoOffMinutes) * 60000UL;
    const uint32_t elapsed = millis() - powerOnSinceMs;
    autoOffRemaining = elapsed < duration ? (duration - elapsed + 59999UL) / 60000UL : 0;
  }
  snprintf(json, sizeof(json),
           "{\"ok\":true,\"deviceName\":\"%s\",\"host\":\"%s\",\"firmwareVersion\":\"%s\",\"mode\":\"%s\",\"ip\":\"%s\",\"wifiSsid\":\"%s\",\"wifiRssi\":%ld,\"uptimeMs\":%lu,\"freeHeap\":%lu,\"packBaseUrl\":\"%s\",\"sourceOwner\":%u,\"hyperHdrActive\":%s,\"hyperHdrFps\":%u,\"hyperHdrLastFrameMs\":%lu,\"hyperHdrFrames\":%lu,\"hyperHdrErrors\":%lu,\"dataPin\":%d,\"clockPin\":%d,\"oePin\":%d,\"powerPin\":%d,\"powerOnDelayMs\":%u,\"powerOffDelayMs\":%u,\"autoOffMinutes\":%u,\"autoOffRemaining\":%lu,\"ledCount\":%u,\"ledBrightness\":%u,\"stripType\":%u,\"spiHz\":%lu,\"colorOrder\":%u,\"ledTop\":%u,\"ledRight\":%u,\"ledBottom\":%u,\"ledLeft\":%u,\"firstLed\":%u,\"ledDirection\":%u,\"effect\":%u,\"effectSpeed\":%u,\"effectIntensity\":%u,\"oeActiveLow\":%s,\"powerActiveHigh\":%s,\"power\":%s,\"outputs\":%s}",
           cfg.deviceName, lbHostnameFromName(cfg.deviceName).c_str(),
           LB_FIRMWARE_VERSION,
           WiFi.status() == WL_CONNECTED ? "STA" : (apActive ? "AP" : "BOOT"),
           currentIp().c_str(), cfg.wifiSsid, static_cast<long>(wifiRssi),
           static_cast<unsigned long>(millis()), static_cast<unsigned long>(ESP.getFreeHeap()),
           cfg.packBaseUrl, sourceOwner,
           sourceOwner == SOURCE_HYPERHDR ? "true" : "false", hyperHdrFps,
           static_cast<unsigned long>(hyperHdrLastFrameAge),
           static_cast<unsigned long>(hyperHdrFrameCount), static_cast<unsigned long>(hyperHdrBadFrames),
           cfg.dataPin, cfg.clockPin, cfg.oePin, cfg.powerPin,
           cfg.powerOnDelayMs, cfg.powerOffDelayMs, powerSettings.autoOffMinutes,
           static_cast<unsigned long>(autoOffRemaining), cfg.ledCount, currentBrightness,
           ledHardware.stripType, static_cast<unsigned long>(ledHardware.spiHz),
           ledHardware.colorOrder, ledLayout.top, ledLayout.right, ledLayout.bottom, ledLayout.left,
           ledLayout.firstLed, ledLayout.direction, effectMode, effectSpeed, effectIntensity, cfg.oeActiveLow ? "true" : "false",
           cfg.powerActiveHigh ? "true" : "false", powerState ? "true" : "false", outputsEnabled ? "true" : "false");
  server.send(200, "application/json", json);
}

static void handleConfig() {
  LBConfig next = cfg;
  lbCopyArg(next.deviceName, sizeof(next.deviceName), server.arg("deviceName"));
  lbCopyArg(next.wifiSsid, sizeof(next.wifiSsid), server.arg("wifiSsid"));
  if (server.hasArg("wifiPass") && server.arg("wifiPass").length() > 0) {
    lbCopyArg(next.wifiPass, sizeof(next.wifiPass), server.arg("wifiPass"));
  }
  strlcpy(next.packBaseUrl, LB_DEFAULT_PACK_BASE_URL, sizeof(next.packBaseUrl));
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
  currentBrightness = cfg.ledBrightness;
  lbSaveConfig(&cfg);
  addLog("System settings saved");
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

static void handleUpdateCheck() {
  String remoteVersion;
  if (!fetchReleaseVersion(remoteVersion)) {
    server.send(503, "application/json", "{\"ok\":false,\"error\":\"update check failed\"}");
    return;
  }
  const bool available = remoteVersion != LB_FIRMWARE_VERSION;
  String json = "{\"ok\":true,\"current\":\"";
  json += LB_FIRMWARE_VERSION;
  json += "\",\"latest\":\"";
  json += remoteVersion;
  json += "\",\"available\":";
  json += available ? "true}" : "false}";
  server.send(200, "application/json", json);
}

static void handleUpdateInstall() {
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, false);
  prefs.putBool("update_pending", true);
  prefs.end();
  const esp_partition_t *part = factoryPartition();
  if (!part || esp_ota_set_boot_partition(part) != ESP_OK) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"recovery unavailable\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
  delay(300);
  ESP.restart();
}

static void handleWifiScan() {
  const int count = WiFi.scanNetworks(false, true);
  String json;
  json.reserve(128 + max(count, 0) * 72);
  json = "{\"ok\":true,\"networks\":[";
  for (int i = 0; i < count; ++i) {
    if (i > 0) json += ',';
    String ssid = WiFi.SSID(i);
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");
    json += "{\"ssid\":\"";
    json += ssid;
    json += "\",\"rssi\":";
    json += WiFi.RSSI(i);
    json += ",\"secure\":";
    json += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true";
    json += '}';
  }
  json += "]}";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

static void handleLog() {
  String json;
  json.reserve(128 + static_cast<size_t>(logCount) * 104U);
  json = "{\"ok\":true,\"entries\":[";
  for (uint8_t i = 0; i < logCount; ++i) {
    if (i > 0) json += ',';
    const uint8_t index = static_cast<uint8_t>((logStart + i) % LOG_CAPACITY);
    json += "{\"timeMs\":";
    json += logEntries[index].timeMs;
    json += ",\"message\":\"";
    json += logEntries[index].message;
    json += "\"}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

static void handleClearLog() {
  logStart = 0;
  logCount = 0;
  addLog("Log cleared");
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleColor() {
  if (!acquireManualSource()) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"HyperHDR owns output\"}");
    return;
  }
  uint8_t r = lbArgU8(server, "r", lastR, 0, 255);
  uint8_t g = lbArgU8(server, "g", lastG, 0, 255);
  uint8_t b = lbArgU8(server, "b", lastB, 0, 255);
  showSolid(r, g, b);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleOff() {
  releaseSource();
  outputOff(true);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleBrightness() {
  uint8_t nextBrightness = lbArgU8(server, "value", currentBrightness, 1, 31);
  currentBrightness = nextBrightness;
  cfg.ledBrightness = nextBrightness;
  lbSaveConfig(&cfg);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handlePower() {
  bool on = server.hasArg("state") ? server.arg("state").toInt() != 0 : true;
  if (on) {
    setPower(true);
  } else {
    releaseSource();
    outputOff(true);
    setPower(false);
  }
  server.send(200, "application/json", on ? "{\"ok\":true,\"power\":true}" : "{\"ok\":true,\"power\":false}");
}

static void handlePowerConfig() {
  LBConfig next = cfg;
  const bool relayEnabled = !server.hasArg("relayEnabled") || server.arg("relayEnabled").toInt() != 0;
  next.powerPin = relayEnabled ? lbArgPin(server, "powerPin", next.powerPin) : -1;
  next.powerActiveHigh = lbArgU8(server, "powerActiveHigh", next.powerActiveHigh, 0, 1);
  next.powerOnDelayMs = lbArgU16(server, "powerOnDelayMs", next.powerOnDelayMs, 0, 5000);
  next.powerOffDelayMs = lbArgU16(server, "powerOffDelayMs", next.powerOffDelayMs, 0, 5000);
  const uint16_t autoOffMinutes = relayEnabled
    ? lbArgU16(server, "autoOffMinutes", powerSettings.autoOffMinutes, 0, 1440)
    : 0;
  if (relayEnabled && (!isS2MiniOutputPin(next.powerPin) || next.powerPin == next.dataPin ||
      next.powerPin == next.clockPin || next.powerPin == next.oePin)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"POWER GPIO unavailable or already used\"}");
    return;
  }
  const bool wasOn = powerState;
  if (wasOn) setPower(false);
  cfg = next;
  powerSettings.autoOffMinutes = autoOffMinutes;
  lbSaveConfig(&cfg);
  savePowerSettings();
  addLog(relayEnabled ? "Relay settings saved" : "Relay control disabled");
  if (cfg.powerPin >= 0) pinMode(cfg.powerPin, OUTPUT);
  setPower(relayEnabled ? wasOn : true);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleLedConfig() {
  LBConfig next = cfg;
  next.dataPin = lbArgPin(server, "dataPin", next.dataPin);
  next.clockPin = lbArgPin(server, "clockPin", next.clockPin);
  next.oePin = lbArgPin(server, "oePin", next.oePin);
  next.oeActiveLow = lbArgU8(server, "oeActiveLow", next.oeActiveLow, 0, 1);
  next.ledCount = lbArgU16(server, "ledCount", next.ledCount, 1, 2048);
  LedHardware nextHardware = ledHardware;
  nextHardware.stripType = lbArgU8(server, "stripType", nextHardware.stripType, 0, 3);
  nextHardware.colorOrder = lbArgU8(server, "colorOrder", nextHardware.colorOrder, 0, 5);
  if (server.hasArg("spiHz")) nextHardware.spiHz = static_cast<uint32_t>(server.arg("spiHz").toInt());
  LedLayout nextLayout{
    lbArgU16(server, "ledTop", ledLayout.top, 0, 2048),
    lbArgU16(server, "ledRight", ledLayout.right, 0, 2048),
    lbArgU16(server, "ledBottom", ledLayout.bottom, 0, 2048),
    lbArgU16(server, "ledLeft", ledLayout.left, 0, 2048),
    lbArgU8(server, "firstLed", ledLayout.firstLed, 0, 3),
    lbArgU8(server, "direction", ledLayout.direction, 0, 1),
  };
  const uint32_t layoutTotal = static_cast<uint32_t>(nextLayout.top) + nextLayout.right + nextLayout.bottom + nextLayout.left;
  if (layoutTotal != next.ledCount) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"LED side total mismatch\"}");
    return;
  }
  if (!isS2MiniOutputPin(next.dataPin) || !isS2MiniOutputPin(next.clockPin) ||
      next.dataPin == next.clockPin || next.dataPin == next.oePin || next.dataPin == next.powerPin ||
      next.clockPin == next.oePin || next.clockPin == next.powerPin) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"GPIO unavailable or already used\"}");
    return;
  }
  if (nextHardware.spiHz < 1000000UL || nextHardware.spiHz > maxSpiHzForStrip(nextHardware.stripType)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"SPI frequency unsupported for strip\"}");
    return;
  }
  if (!lbValidateConfig(&next)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid LED config or GPIO\"}");
    return;
  }
  cfg = next;
  ledLayout = nextLayout;
  ledHardware = nextHardware;
  lbSaveConfig(&cfg);
  saveLedLayout();
  saveLedHardware();
  addLog("LED settings saved");
  server.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
  delay(250);
  ESP.restart();
}

static void handleLedDetectStart() {
  if (!acquireManualSource()) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"HyperHDR owns output\"}");
    return;
  }
  const uint16_t start = lbArgU16(server, "start", 0, 0, MAX_HYPERHDR_PIXELS - 1U);
  ledDetectStart = start;
  ledDetectIndex = start;
  ledDetectActive = true;
  lastLedDetectMs = millis();
  writeLedDetectFrame(ledDetectIndex);
  addLog("LED segment detection started");
  char json[96];
  snprintf(json, sizeof(json), "{\"ok\":true,\"index\":%u}", ledDetectIndex + 1U);
  server.send(200, "application/json", json);
}

static void handleLedDetectStop() {
  const uint16_t segmentCount = ledDetectActive
    ? static_cast<uint16_t>(ledDetectIndex - ledDetectStart + 1U)
    : 0;
  ledDetectActive = false;
  clearLedDetectFrame(ledDetectIndex);
  char json[128];
  snprintf(json, sizeof(json),
           "{\"ok\":true,\"index\":%u,\"segmentCount\":%u}",
           ledDetectIndex + 1U, segmentCount);
  server.send(200, "application/json", json);
  addLog("LED segment detection stopped");
}

static void handleLedDetectStatus() {
  char json[128];
  snprintf(json, sizeof(json),
           "{\"ok\":true,\"active\":%s,\"index\":%u,\"start\":%u}",
           ledDetectActive ? "true" : "false", ledDetectIndex + 1U, ledDetectStart);
  server.send(200, "application/json", json);
}

static void handleEffect() {
  if (!acquireManualSource()) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"HyperHDR owns output\"}");
    return;
  }
  const uint8_t nextMode = lbArgU8(server, "effect", effectMode, 0, EFFECT_COUNT - 1);
  effectSpeed = lbArgU8(server, "speed", effectSpeed, 0, 255);
  effectIntensity = lbArgU8(server, "intensity", effectIntensity, 0, 255);
  effectMode = nextMode;
  effectFrame = 0;
  lastEffectMs = 0;
  if (effectMode == EFFECT_SOLID) {
    showSolid(lastR, lastG, lastB);
  } else {
    ensurePowerAndOutput();
    renderEffectFrame();
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleManualSource() {
  if (!acquireManualSource()) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"HyperHDR owns output\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true,\"sourceOwner\":2}");
}

static void handleReleaseSource() {
  releaseSource();
  outputOff(true);
  server.send(200, "application/json", "{\"ok\":true,\"sourceOwner\":0}");
}

static void setupWeb() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", UI_HTML); });
  server.on("/app.js", HTTP_GET, []() { server.send_P(200, "application/javascript", UI_APP_JS); });
  server.on("/api/info", HTTP_GET, handleInfo);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/recovery/enter", HTTP_POST, handleEnterRecovery);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/update/check", HTTP_GET, handleUpdateCheck);
  server.on("/api/update/install", HTTP_POST, handleUpdateInstall);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/log/clear", HTTP_POST, handleClearLog);
  server.on("/api/test/color", HTTP_POST, handleColor);
  server.on("/api/test/off", HTTP_POST, handleOff);
  server.on("/api/brightness", HTTP_POST, handleBrightness);
  server.on("/api/power", HTTP_POST, handlePower);
  server.on("/api/power/config", HTTP_POST, handlePowerConfig);
  server.on("/api/led/config", HTTP_POST, handleLedConfig);
  server.on("/api/led/detect/start", HTTP_POST, handleLedDetectStart);
  server.on("/api/led/detect/stop", HTTP_POST, handleLedDetectStop);
  server.on("/api/led/detect/status", HTTP_GET, handleLedDetectStatus);
  server.on("/api/effect", HTTP_POST, handleEffect);
  server.on("/api/source/manual", HTTP_POST, handleManualSource);
  server.on("/api/source/release", HTTP_POST, handleReleaseSource);
  server.begin();
}

void setup() {
  addLog("Firmware boot");
  lbLoadConfig(&cfg);
  if (!lbValidateConfig(&cfg)) lbSetDefaults(&cfg);
  loadLedLayout();
  loadLedHardware();
  loadPowerSettings();
  currentBrightness = cfg.ledBrightness;
  hyperHdrUsb.begin(2000000);
  USB.begin();
  configureHardware();
  if (!connectSta()) {
    startAp();
  }
  startMdns();
  setupWeb();
}

void loop() {
  handleHyperHdrSerial();
  server.handleClient();
  const unsigned long now = millis();
  if (apActive && now >= AP_WINDOW_MS) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apActive = false;
    apWindowExpired = true;
    addLog("Access point stopped after 10 min");
  }
  if (ledDetectActive && now - lastLedDetectMs >= LED_DETECT_INTERVAL_MS) {
    lastLedDetectMs = now;
    if (ledDetectIndex + 1U < MAX_HYPERHDR_PIXELS) {
      ++ledDetectIndex;
      writeLedDetectFrame(ledDetectIndex);
    }
  }
  if (!ledDetectActive && effectMode != EFFECT_SOLID && now - lastEffectMs >= effectIntervalMs()) {
    lastEffectMs = now;
    renderEffectFrame();
  }
  if (sourceOwner == SOURCE_HYPERHDR && now - lastHyperHdrFrameMs >= HYPERHDR_TIMEOUT_MS) {
    releaseSource();
    hyperHdrFps = 0;
    hyperHdrWindowFrames = 0;
    hyperHdrFpsWindowMs = 0;
    outputOff(true);
    addLog("HyperHDR timeout: output off");
  }
  if (sourceOwner != SOURCE_MANUAL && powerState && powerSettings.autoOffMinutes > 0 && powerOnSinceMs > 0) {
    const uint32_t timeoutMs = static_cast<uint32_t>(powerSettings.autoOffMinutes) * 60000UL;
    if (now - powerOnSinceMs >= timeoutMs) outputOff(true);
  }
  if (now - lastWifiCheckMs > 10000) {
    lastWifiCheckMs = now;
    if (WiFi.status() != WL_CONNECTED && !apActive && !apWindowExpired) {
      startAp();
      startMdns();
    }
  }
}
