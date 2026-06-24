import React, { useEffect } from "react";
import { Card, SectionHeader, SectionTitle, StatIcon } from "../components/ui.jsx";

export function SettingsPage({ vm }) {
  const {
    setSection, scanWifi, wifiScanning, wifiNetworks, form, setForm,
    rebootDevice, enterRecovery, saveConfig, busy, updateInfo, updateChecking, updateInstalling,
    checkUpdate, installUpdate, logEntries, logLoading, refreshLog, clearLog, info,
  } = vm;

  useEffect(() => {
    refreshLog();
  }, []);

  const formatLogTime = (timeMs) => {
    const totalSeconds = Math.floor(timeMs / 1000);
    const hours = Math.floor(totalSeconds / 3600);
    const minutes = Math.floor((totalSeconds % 3600) / 60);
    const seconds = totalSeconds % 60;
    return [hours, minutes, seconds].map((value) => String(value).padStart(2, "0")).join(":");
  };

  return (
    <>
      <SectionHeader title="Настройки" onBack={() => setSection("home")} />
      <Card className="settings-card">
        <SectionTitle icon={<StatIcon kind="wifi" size={18} />}>Wi-Fi</SectionTitle>
        <button type="button" className="settings-action" onClick={scanWifi} disabled={wifiScanning}>
          {wifiScanning ? "Поиск сетей..." : "Найти доступные сети"}
        </button>
        {wifiNetworks.length > 0 && (
          <>
            <label>Найденные сети</label>
            <select value={form.wifiSsid} onChange={(event) => setForm({ ...form, wifiSsid: event.target.value })}>
              <option value="">Выберите сеть</option>
              {wifiNetworks.map((network) => (
                <option key={network.ssid} value={network.ssid}>
                  {network.ssid} · {network.rssi} dBm{network.secure ? " · 🔒" : ""}
                </option>
              ))}
            </select>
          </>
        )}
        <label>SSID</label>
        <input type="text" value={form.wifiSsid} onChange={(event) => setForm({ ...form, wifiSsid: event.target.value })} />
        <label>Пароль</label>
        <input type="password" value={form.wifiPass} placeholder="Оставьте пустым, чтобы не менять" onChange={(event) => setForm({ ...form, wifiPass: event.target.value })} />
      </Card>

      <Card className="settings-card">
        <SectionTitle icon="◆">Устройство</SectionTitle>
        <label>Имя устройства</label>
        <input type="text" maxLength="32" value={form.deviceName} onChange={(event) => setForm({ ...form, deviceName: event.target.value })} />
        <label>Язык интерфейса</label>
        <div className="settings-readonly">Русский</div>
      </Card>

      <Card className="settings-card">
        <SectionTitle icon="↻">Обновление и восстановление</SectionTitle>
        <label>Адрес пакета обновления</label>
        <div className="settings-readonly">{form.packBaseUrl}</div>
        <label>Текущая версия</label>
        <div className="settings-readonly">{info?.firmwareVersion ?? "—"}</div>
        <button type="button" className="settings-action update-check" onClick={checkUpdate} disabled={updateChecking || updateInstalling}>
          {updateChecking ? "Проверка..." : "Проверить обновления"}
        </button>
        {updateInfo?.available && (
          <div className="update-available">
            <span>Доступна версия <strong>{updateInfo.latest}</strong></span>
            <button type="button" onClick={installUpdate} disabled={updateInstalling}>
              {updateInstalling ? "Запуск Recovery..." : "Обновить прошивку"}
            </button>
          </div>
        )}
        {updateInfo && !updateInfo.available && <div className="update-current">Установлена актуальная версия.</div>}
        <div className="settings-buttons">
          <button type="button" onClick={rebootDevice}>Перезагрузить</button>
          <button type="button" className="warning" onClick={enterRecovery}>Recovery</button>
        </div>
      </Card>

      <Card className="settings-card">
        <SectionTitle icon="≡">Журнал работы</SectionTitle>
        <div className="device-log">
          {logEntries.length === 0 && <div className="device-log-empty">Событий пока нет</div>}
          {[...logEntries].reverse().map((entry, index) => (
            <div className="device-log-row" key={`${entry.timeMs}-${index}`}>
              <time>{formatLogTime(entry.timeMs)}</time>
              <span>{entry.message}</span>
            </div>
          ))}
        </div>
        <div className="log-buttons">
          <button type="button" onClick={refreshLog} disabled={logLoading}>
            {logLoading ? "Обновление..." : "Обновить"}
          </button>
          <button type="button" className="warning" onClick={clearLog}>Очистить</button>
        </div>
      </Card>

      <button type="button" className="led-save" onClick={saveConfig} disabled={busy}>
        {busy ? "Сохранение..." : "Сохранить настройки"}
      </button>
    </>
  );
}
