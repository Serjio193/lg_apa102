import React, { useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import { PowerButton } from "./components/ui.jsx";
import {
  effects, navItems, s2MiniOutputPins, spiFrequencies, stripTypes,
} from "./lib/constants.js";
import { clamp, hexToWheelPosition, hsvToHex } from "./lib/color.js";
import { HomePage } from "./pages/HomePage.jsx";
import { LedPage } from "./pages/LedPage.jsx";
import { PowerPage } from "./pages/PowerPage.jsx";
import { SettingsPage } from "./pages/SettingsPage.jsx";

const initialForm = {
  deviceName: "lg_apa102",
  wifiSsid: "",
  wifiPass: "",
  packBaseUrl: "https://serjio193.github.io/lg_apa102/latest/",
  dataPin: 11,
  clockPin: 7,
  oePin: 18,
  powerPin: 9,
  ledCount: 60,
  ledBrightness: 24,
  oeActiveLow: 1,
  powerActiveHigh: 1,
  powerOnDelayMs: 50,
  powerOffDelayMs: 100,
};

function App() {
  const [info, setInfo] = useState(null);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState("");
  const [section, setSection] = useState("home");
  const [powerOn, setPowerOn] = useState(true);
  const [brightness, setBrightness] = useState(24);
  const [selectedColor, setSelectedColor] = useState("#3b8cff");
  const [effect, setEffect] = useState(0);
  const [effectSpeed, setEffectSpeed] = useState(128);
  const [effectIntensity, setEffectIntensity] = useState(128);
  const [ledSides, setLedSides] = useState({ top: 19, right: 11, bottom: 19, left: 11 });
  const [firstLed, setFirstLed] = useState("top-left");
  const [ledDirection, setLedDirection] = useState("clockwise");
  const [stripType, setStripType] = useState(0);
  const [spiHz, setSpiHz] = useState(8000000);
  const [colorOrder, setColorOrder] = useState(5);
  const [autoOffMinutes, setAutoOffMinutes] = useState(0);
  const [autoOffRemaining, setAutoOffRemaining] = useState(0);
  const [relayEnabled, setRelayEnabled] = useState(true);
  const [sourceOwner, setSourceOwner] = useState(0);
  const [wifiNetworks, setWifiNetworks] = useState([]);
  const [wifiScanning, setWifiScanning] = useState(false);
  const [updateInfo, setUpdateInfo] = useState(null);
  const [updateChecking, setUpdateChecking] = useState(false);
  const [updateInstalling, setUpdateInstalling] = useState(false);
  const [logEntries, setLogEntries] = useState([]);
  const [logLoading, setLogLoading] = useState(false);
  const [ledDetectActive, setLedDetectActive] = useState(false);
  const [ledDetectIndex, setLedDetectIndex] = useState(0);
  const [ledDetectStep, setLedDetectStep] = useState(0);
  const [ledDetectCounts, setLedDetectCounts] = useState({});
  const [apiPin, setApiPin] = useState(() => sessionStorage.getItem("lg_api_pin") || "");
  const [newApiPin, setNewApiPin] = useState("");
  const [form, setForm] = useState(initialForm);

  const wifiRssi = info?.wifiRssi ?? 0;
  const wifi = info?.mode === "AP"
    ? { label: "AP", dbm: "точка доступа" }
    : wifiRssi >= -55
      ? { label: "GOOD", dbm: `${wifiRssi} dBm` }
      : wifiRssi >= -70
        ? { label: "FAIR", dbm: `${wifiRssi} dBm` }
        : { label: info?.mode === "STA" ? "WEAK" : "OFF", dbm: info?.mode === "STA" ? `${wifiRssi} dBm` : "нет сети" };
  const uiBrightness = Math.round((brightness / 31) * 255);
  const geometryLedCount = ledSides.top + ledSides.right + ledSides.bottom + ledSides.left;
  const availableSpiFrequencies = spiFrequencies.filter(([hz]) => hz <= stripTypes[stripType].maxHz);
  const dataPinOptions = s2MiniOutputPins.filter((pin) => pin !== form.clockPin && pin !== form.oePin && pin !== form.powerPin);
  const clockPinOptions = s2MiniOutputPins.filter((pin) => pin !== form.dataPin && pin !== form.oePin && pin !== form.powerPin);
  const oePinOptions = s2MiniOutputPins.filter((pin) => pin !== form.dataPin && pin !== form.clockPin && pin !== form.powerPin);
  const powerPinOptions = s2MiniOutputPins.filter((pin) => pin !== form.dataPin && pin !== form.clockPin && pin !== form.oePin);
  const wheelPosition = hexToWheelPosition(selectedColor);
  const cornerIndex = ["top-left", "top-right", "bottom-right", "bottom-left"].indexOf(firstLed);
  const clockwiseSides = ["top", "right", "bottom", "left"];
  const counterclockwiseSides = ["left", "bottom", "right", "top"];
  const ledDetectOrder = Array.from({ length: 4 }, (_, index) => {
    const sequence = ledDirection === "clockwise" ? clockwiseSides : counterclockwiseSides;
    const sequenceIndex = ledDirection === "clockwise"
      ? (cornerIndex + index) % 4
      : (index - cornerIndex + 4) % 4;
    return sequence[sequenceIndex];
  });

  const refresh = async (hydrateConfig = false) => {
    const data = await (await fetch("/api/info")).json();
    setInfo(data);
    setPowerOn(Boolean(data.power));
    setAutoOffRemaining(data.autoOffRemaining ?? 0);
    setSourceOwner(data.sourceOwner ?? 0);
    if (hydrateConfig) {
      setForm((previous) => ({
        ...previous,
        deviceName: data.deviceName ?? previous.deviceName,
        wifiSsid: data.wifiSsid ?? previous.wifiSsid,
        packBaseUrl: data.packBaseUrl ?? previous.packBaseUrl,
        dataPin: data.dataPin ?? previous.dataPin,
        clockPin: data.clockPin ?? previous.clockPin,
        oePin: data.oePin ?? previous.oePin,
        powerPin: data.powerPin ?? previous.powerPin,
        ledCount: data.ledCount ?? previous.ledCount,
        ledBrightness: data.ledBrightness ?? previous.ledBrightness,
        oeActiveLow: data.oeActiveLow ? 1 : 0,
        powerActiveHigh: data.powerActiveHigh ? 1 : 0,
        powerOnDelayMs: data.powerOnDelayMs ?? previous.powerOnDelayMs,
        powerOffDelayMs: data.powerOffDelayMs ?? previous.powerOffDelayMs,
      }));
      setBrightness(data.ledBrightness ?? 24);
      setEffect(data.effect ?? 0);
      setEffectSpeed(data.effectSpeed ?? 128);
      setEffectIntensity(data.effectIntensity ?? 128);
      setLedSides({
        top: data.ledTop ?? 19,
        right: data.ledRight ?? 11,
        bottom: data.ledBottom ?? 19,
        left: data.ledLeft ?? 11,
      });
      setFirstLed(["top-left", "top-right", "bottom-right", "bottom-left"][data.firstLed ?? 0]);
      setLedDirection((data.ledDirection ?? 0) === 0 ? "clockwise" : "counterclockwise");
      setStripType(data.stripType ?? 0);
      setSpiHz(data.spiHz ?? 8000000);
      setColorOrder(data.colorOrder ?? 5);
      setAutoOffMinutes(data.autoOffMinutes ?? 0);
      setRelayEnabled((data.powerPin ?? -1) >= 0);
    }
  };

  useEffect(() => {
    refresh(true);
    const timer = setInterval(refresh, 2000);
    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    if (!message) return undefined;
    const timer = setTimeout(() => setMessage(""), 4500);
    return () => clearTimeout(timer);
  }, [message]);

  useEffect(() => {
    if (!ledDetectActive) return undefined;
    const refreshDetection = async () => {
      const json = await apiFetch("/api/led/detect/status");
      if (json.ok) {
        setLedDetectActive(Boolean(json.active));
        setLedDetectIndex(json.index ?? 0);
      }
    };
    const timer = setInterval(refreshDetection, 500);
    return () => clearInterval(timer);
  }, [ledDetectActive, apiPin]);

  const updateSessionPin = (value) => {
    setApiPin(value);
    if (value) sessionStorage.setItem("lg_api_pin", value);
    else sessionStorage.removeItem("lg_api_pin");
  };

  const apiFetch = async (url, options = {}) => {
    const headers = new Headers(options.headers || {});
    if (apiPin) headers.set("X-API-PIN", apiPin);
    const response = await fetch(url, { ...options, headers });
    const json = await response.json();
    if (response.status === 401) setMessage("Ошибка: введите API PIN в настройках");
    return json;
  };

  const postForm = async (url, body) => {
    return apiFetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams(body),
    });
  };

  const saveConfig = async () => {
    setBusy(true);
    try {
      const json = await postForm("/api/config", {
        deviceName: form.deviceName,
        wifiSsid: form.wifiSsid,
        wifiPass: form.wifiPass,
        packBaseUrl: form.packBaseUrl,
      });
      setMessage(json.ok ? "Настройки сохранены. Устройство перезагружается." : `Ошибка: ${json.error}`);
    } finally {
      setBusy(false);
    }
  };

  const applyBrightness = async (value = brightness) => {
    const next = clamp(Number(value), 1, 31);
    setBrightness(next);
    setForm((previous) => ({ ...previous, ledBrightness: next }));
    const json = await postForm("/api/brightness", { value: next });
    setMessage(json.ok ? `Яркость: ${next}` : `Ошибка: ${json.error}`);
  };

  const sendColor = async (red, green, blue) => {
    const json = await postForm("/api/test/color", { r: red, g: green, b: blue });
    setMessage(json.ok ? "Цвет применён" : `Ошибка: ${json.error}`);
  };

  const applyEffect = async (nextEffect = effect) => {
    const json = await postForm("/api/effect", {
      effect: nextEffect,
      speed: effectSpeed,
      intensity: effectIntensity,
    });
    setMessage(json.ok ? `Эффект: ${effects[nextEffect]}` : `Ошибка: ${json.error}`);
  };

  const applyLedGeometry = async () => {
    setForm((previous) => ({ ...previous, ledCount: geometryLedCount }));
    const json = await postForm("/api/led/config", {
      dataPin: form.dataPin,
      clockPin: form.clockPin,
      oePin: form.oePin,
      oeActiveLow: form.oeActiveLow,
      ledCount: geometryLedCount,
      ledTop: ledSides.top,
      ledRight: ledSides.right,
      ledBottom: ledSides.bottom,
      ledLeft: ledSides.left,
      firstLed: ["top-left", "top-right", "bottom-right", "bottom-left"].indexOf(firstLed),
      direction: ledDirection === "clockwise" ? 0 : 1,
      stripType,
      spiHz,
      colorOrder,
    });
    setMessage(json.ok ? `LED: ${geometryLedCount}. Устройство перезагружается.` : `Ошибка: ${json.error}`);
  };

  const startLedDetection = async () => {
    const start = Object.values(ledDetectCounts).reduce((sum, value) => sum + value, 0);
    const json = await postForm("/api/led/detect/start", { start });
    if (json.ok) {
      setLedDetectActive(true);
      setLedDetectIndex(json.index ?? start + 1);
      setSourceOwner(2);
    }
    setMessage(json.ok ? `Поиск стороны «${ledDetectOrder[ledDetectStep]}» запущен` : `Ошибка: ${json.error}`);
  };

  const stopLedDetection = async () => {
    const json = await apiFetch("/api/led/detect/stop", { method: "POST" });
    if (!json.ok) {
      setMessage(`Ошибка: ${json.error}`);
      return;
    }
    const side = ledDetectOrder[ledDetectStep];
    const count = json.segmentCount ?? 0;
    const nextCounts = { ...ledDetectCounts, [side]: count };
    setLedDetectCounts(nextCounts);
    setLedSides((previous) => ({ ...previous, [side]: count }));
    setLedDetectActive(false);
    setLedDetectIndex(json.index ?? 0);
    if (ledDetectStep < 3) {
      setLedDetectStep((previous) => previous + 1);
      setMessage(`Сторона «${side}»: ${count} LED`);
    } else {
      const total = Object.values(nextCounts).reduce((sum, value) => sum + value, 0);
      setForm((previous) => ({ ...previous, ledCount: total }));
      setLedDetectStep(4);
      await apiFetch("/api/source/release", { method: "POST" });
      setSourceOwner(0);
      setPowerOn(false);
      setMessage(`Определение завершено: ${total} LED`);
    }
  };

  const resetLedDetection = async () => {
    if (ledDetectActive) await apiFetch("/api/led/detect/stop", { method: "POST" });
    setLedDetectActive(false);
    setLedDetectIndex(0);
    setLedDetectStep(0);
    setLedDetectCounts({});
    setMessage("Автоопределение сброшено");
  };

  const savePowerConfig = async () => {
    const json = await postForm("/api/power/config", {
      relayEnabled: relayEnabled ? 1 : 0,
      powerPin: relayEnabled ? form.powerPin : -1,
      powerActiveHigh: form.powerActiveHigh,
      powerOnDelayMs: form.powerOnDelayMs,
      powerOffDelayMs: form.powerOffDelayMs,
      autoOffMinutes,
    });
    setMessage(json.ok ? "Настройки питания сохранены" : `Ошибка: ${json.error}`);
    if (json.ok) await refresh();
  };

  const claimManualSource = async () => {
    const json = await apiFetch("/api/source/manual", { method: "POST" });
    if (json.ok) setSourceOwner(2);
    setMessage(json.ok ? "Ручной режим включён" : `Ошибка: ${json.error}`);
  };

  const releaseActiveSource = async () => {
    const json = await apiFetch("/api/source/release", { method: "POST" });
    if (json.ok) {
      setSourceOwner(0);
      setPowerOn(false);
    }
    setMessage(json.ok ? "Подсветка выключена" : `Ошибка: ${json.error}`);
  };

  const scanWifi = async () => {
    setWifiScanning(true);
    try {
      const json = await apiFetch("/api/wifi/scan");
      setWifiNetworks(json.ok ? json.networks : []);
      setMessage(json.ok ? `Найдено сетей: ${json.networks.length}` : `Ошибка: ${json.error}`);
    } finally {
      setWifiScanning(false);
    }
  };

  const refreshLog = async () => {
    setLogLoading(true);
    try {
      const json = await apiFetch("/api/log");
      setLogEntries(json.ok ? json.entries : []);
    } finally {
      setLogLoading(false);
    }
  };

  const clearLog = async () => {
    const json = await apiFetch("/api/log/clear", { method: "POST" });
    setMessage(json.ok ? "Журнал очищен" : `Ошибка: ${json.error}`);
    if (json.ok) await refreshLog();
  };

  const togglePower = async () => {
    const next = !powerOn;
    try {
      const json = await postForm("/api/power", { state: next ? 1 : 0 });
      if (json.ok) setPowerOn(Boolean(json.power));
      setMessage(json.ok ? (json.power ? "Питание включено" : "Питание выключено") : `Ошибка: ${json.error}`);
    } catch {
      setMessage("Ошибка связи с устройством");
    }
  };

  const checkUpdate = async () => {
    setUpdateChecking(true);
    try {
      const json = await apiFetch("/api/update/check");
      setUpdateInfo(json.ok ? json : null);
      setMessage(json.ok ? (json.available ? `Доступна версия ${json.latest}` : "Установлена актуальная версия") : `Ошибка: ${json.error}`);
    } finally {
      setUpdateChecking(false);
    }
  };

  const installUpdate = async () => {
    setUpdateInstalling(true);
    const json = await apiFetch("/api/update/install", { method: "POST" });
    setMessage(json.ok ? "Переход в Recovery для обновления" : `Ошибка: ${json.error}`);
    if (!json.ok) setUpdateInstalling(false);
  };

  const saveApiPin = async (disable = false) => {
    const nextPin = disable ? "" : newApiPin.trim();
    if (!disable && (nextPin.length < 4 || nextPin.length > 16)) {
      setMessage("Ошибка: PIN должен содержать 4-16 символов");
      return;
    }
    const json = await postForm("/api/security/pin", { newPin: nextPin });
    if (json.ok) {
      updateSessionPin(nextPin);
      setNewApiPin("");
      setInfo((previous) => ({ ...previous, authEnabled: Boolean(nextPin) }));
    }
    setMessage(json.ok ? (nextPin ? "API PIN сохранён" : "Защита API отключена") : `Ошибка: ${json.error}`);
  };

  const selectWheelColor = (event, commit = false) => {
    const rect = event.currentTarget.getBoundingClientRect();
    const radius = Math.min(rect.width, rect.height) / 2;
    let x = event.clientX - rect.left - rect.width / 2;
    let y = event.clientY - rect.top - rect.height / 2;
    const distance = Math.sqrt((x * x) + (y * y));
    if (distance > radius) {
      x = (x / distance) * radius;
      y = (y / distance) * radius;
    }
    const saturation = clamp(Math.sqrt((x * x) + (y * y)) / radius, 0, 1);
    const hue = (Math.atan2(y, x) * 180 / Math.PI + 90 + 360) % 360;
    const color = hsvToHex(hue, saturation);
    setSelectedColor(color);
    if (commit) sendColor(
      parseInt(color.slice(1, 3), 16),
      parseInt(color.slice(3, 5), 16),
      parseInt(color.slice(5, 7), 16),
    );
  };

  const vm = {
    info, busy, section, setSection, powerOn, wifi, brightness, setBrightness,
    selectedColor, effect, setEffect, effectSpeed, setEffectSpeed,
    effectIntensity, setEffectIntensity, ledSides, firstLed, setFirstLed,
    ledDirection, setLedDirection, stripType, setStripType, spiHz, setSpiHz,
    colorOrder, setColorOrder, autoOffMinutes, setAutoOffMinutes,
    autoOffRemaining, relayEnabled, setRelayEnabled, sourceOwner,
    wifiNetworks, wifiScanning, form, setForm,
    updateInfo, updateChecking, updateInstalling,
    logEntries, logLoading, refreshLog, clearLog,
    ledDetectActive, ledDetectIndex, ledDetectStep, ledDetectCounts, ledDetectOrder,
    apiPin, updateSessionPin, newApiPin, setNewApiPin, saveApiPin,
    uiBrightness, geometryLedCount, availableSpiFrequencies, dataPinOptions,
    clockPinOptions, oePinOptions, powerPinOptions, wheelPosition, applyBrightness, applyEffect,
    applyLedGeometry, savePowerConfig, claimManualSource, releaseActiveSource,
    startLedDetection, stopLedDetection, resetLedDetection,
    scanWifi, saveConfig, selectWheelColor,
    checkUpdate, installUpdate,
    updateLedSide: (side, value) => setLedSides((previous) => ({ ...previous, [side]: value })),
    enterRecovery: async () => {
      const json = await apiFetch("/api/recovery/enter", { method: "POST" });
      setMessage(json.ok ? "Переход в Recovery" : `Ошибка: ${json.error}`);
    },
    rebootDevice: async () => {
      const json = await apiFetch("/api/reboot", { method: "POST" });
      setMessage(json.ok ? "Перезагрузка устройства" : `Ошибка: ${json.error}`);
    },
  };

  const pages = {
    home: <HomePage vm={vm} />,
    led: <LedPage vm={vm} />,
    power: <PowerPage vm={vm} />,
    settings: <SettingsPage vm={vm} />,
  };

  return (
    <div className="app shell">
      {section === "home" && (
        <section className="topline">
          <div className="brand">
            <PowerButton on={powerOn} onToggle={togglePower} />
            <div>
              <h1>{form.deviceName || "lg_apa102"}</h1>
              <p>HyperHDR-compatible light controller</p>
            </div>
          </div>
        </section>
      )}
      {message && <div className={message.startsWith("Ошибка") ? "app-message error" : "app-message"}>{message}</div>}
      <div className="grid">{pages[section]}</div>
      <div className="footer-nav">
        <nav className="nav" aria-label="main">
          {navItems.map((item) => (
            <button key={item.id} type="button" className={`nav-item ${section === item.id ? "active" : ""}`} onClick={() => setSection(item.id)}>
              <div className="nav-ico">{item.icon}</div>
              <div style={{ fontSize: 11 }}>{item.label}</div>
            </button>
          ))}
        </nav>
      </div>
    </div>
  );
}

createRoot(document.getElementById("root")).render(<App />);
