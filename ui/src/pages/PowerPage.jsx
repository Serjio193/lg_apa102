import React from "react";
import { Card, SectionHeader, SectionTitle } from "../components/ui.jsx";

export function PowerPage({ vm }) {
  const {
    setSection, relayEnabled, setRelayEnabled, form, setForm, powerPinOptions,
    autoOffMinutes, setAutoOffMinutes, powerOn, autoOffRemaining, savePowerConfig,
  } = vm;

  return (
    <>
      <SectionHeader title="Питание" onBack={() => setSection("home")} />
      <Card className="power-card">
        <SectionTitle icon="⏻">Управление реле</SectionTitle>
        <div className="power-state">
          <div>
            <span>Управление реле</span>
            <strong className={relayEnabled ? "online-text" : "offline-text"}>{relayEnabled ? "ИСПОЛЬЗУЕТСЯ" : "ОТКЛЮЧЕНО"}</strong>
            {relayEnabled && <small>Питание сейчас: {powerOn ? "ВКЛ" : "ВЫКЛ"}</small>}
          </div>
          <button
            type="button"
            className={relayEnabled ? "power-toggle on" : "power-toggle"}
            onClick={() => {
              const next = !relayEnabled;
              setRelayEnabled(next);
              if (next && form.powerPin < 0) setForm({ ...form, powerPin: 9 });
            }}
            aria-pressed={relayEnabled}
            aria-label="Использовать реле"
          >
            <span />
          </button>
        </div>
        {relayEnabled && (
          <div className="relay-settings">
            <label>POWER GPIO</label>
            <select value={form.powerPin} onChange={(event) => setForm({ ...form, powerPin: Number(event.target.value) })}>
              {powerPinOptions.map((pin) => <option key={pin} value={pin}>GPIO {pin}</option>)}
            </select>
            <label>Активный уровень реле</label>
            <div className="logic-choice">
              <button type="button" className={form.powerActiveHigh === 0 ? "active" : ""} onClick={() => setForm({ ...form, powerActiveHigh: 0 })}>LOW</button>
              <button type="button" className={form.powerActiveHigh === 1 ? "active" : ""} onClick={() => setForm({ ...form, powerActiveHigh: 1 })}>HIGH</button>
            </div>
            <div className="power-delay-grid">
              <div>
                <label>Задержка включения, мс</label>
                <input type="number" min="0" max="5000" value={form.powerOnDelayMs} onChange={(event) => setForm({ ...form, powerOnDelayMs: Number(event.target.value) })} />
              </div>
              <div>
                <label>Задержка выключения, мс</label>
                <input type="number" min="0" max="5000" value={form.powerOffDelayMs} onChange={(event) => setForm({ ...form, powerOffDelayMs: Number(event.target.value) })} />
              </div>
            </div>
          </div>
        )}
      </Card>

      {relayEnabled && (
        <Card className="power-card">
          <SectionTitle icon="⏲">Автовыключение</SectionTitle>
          <label>Таймер отключения</label>
          <select value={autoOffMinutes} onChange={(event) => setAutoOffMinutes(Number(event.target.value))}>
            <option value="0">Отключено</option>
            {[5, 10, 15, 30, 45, 60, 90, 120, 180, 240].map((minutes) => (
              <option key={minutes} value={minutes}>{minutes} минут</option>
            ))}
          </select>
          <div className="timer-presets">
            {[5, 10, 15, 30, 45, 60, 90, 120].map((minutes) => (
              <button key={minutes} type="button" className={autoOffMinutes === minutes ? "active" : ""} onClick={() => setAutoOffMinutes(minutes)}>
                {minutes}m
              </button>
            ))}
          </div>
          <div className="power-note">
            {autoOffMinutes === 0
              ? "Автоматическое отключение питания выключено."
              : powerOn && autoOffRemaining > 0
                ? `До отключения осталось примерно ${autoOffRemaining} мин.`
                : `После включения питание отключится через ${autoOffMinutes} мин.`}
          </div>
        </Card>
      )}
      <button type="button" className="led-save" onClick={savePowerConfig}>Сохранить настройки питания</button>
    </>
  );
}
