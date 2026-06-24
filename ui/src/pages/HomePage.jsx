import React from "react";
import { Card, SectionTitle, StatIcon } from "../components/ui.jsx";
import { effects } from "../lib/constants.js";
import { clamp } from "../lib/color.js";

export function HomePage({ vm }) {
  const {
    powerOn, info, wifi, sourceOwner, claimManualSource, releaseActiveSource,
    uiBrightness, brightness, setBrightness, form, setForm, applyBrightness,
    selectWheelColor, wheelPosition, selectedColor, effect, setEffect,
    effectSpeed, setEffectSpeed, effectIntensity, setEffectIntensity, applyEffect,
  } = vm;
  const hyperHdrActive = sourceOwner === 1;
  const manualActive = sourceOwner === 2;
  const formatUptime = (timeMs = 0) => {
    const totalMinutes = Math.floor(timeMs / 60000);
    const days = Math.floor(totalMinutes / 1440);
    const hours = Math.floor((totalMinutes % 1440) / 60);
    const minutes = totalMinutes % 60;
    return `${days}d ${hours}h ${minutes}m`;
  };

  return (
    <>
      <Card className="dashboard-status-card">
        <div className="dashboard-status">
          <div className="dashboard-status-cell">
            <span className={`status-dot ${hyperHdrActive ? "online" : "offline"}`} />
            <div className={`dashboard-glyph ${hyperHdrActive ? "online-text" : "offline-text"}`}>
              <StatIcon kind="usb" size={44} />
            </div>
            <div className="dashboard-status-copy">
              <div className="dashboard-status-label">USB (AWA)</div>
              <div className={`dashboard-status-value ${hyperHdrActive ? "online-text" : "offline-text"}`}>
                {hyperHdrActive ? "ACTIVE" : manualActive ? "BLOCKED" : "READY"}
              </div>
              <div className="dashboard-status-detail">
                {hyperHdrActive ? "приём кадров · 2.0 Mbaud" : manualActive ? "ручной режим" : "ожидание потока"}
              </div>
            </div>
          </div>
          <div className="dashboard-status-cell divided">
            <span className={`status-dot ${info?.mode === "STA" || info?.mode === "AP" ? "wifi-dot" : "offline"}`} />
            <div className="dashboard-glyph wifi-text"><StatIcon kind="wifi" size={38} /></div>
            <div className="dashboard-status-copy">
              <div className="dashboard-status-label">Wi-Fi</div>
              <div className="dashboard-status-value wifi-text">{wifi.label}</div>
              <div className="dashboard-status-detail">{info?.ip ?? "192.168.4.1"}</div>
              <div className="dashboard-status-detail wifi-text">{wifi.dbm}</div>
            </div>
          </div>
          <div className="dashboard-status-cell fps-cell">
            <div className="dashboard-status-label">FPS</div>
            <div className="dashboard-fps">{info?.hyperHdrFps ?? 0}</div>
            <div className="dashboard-status-detail">Кадр/сек</div>
          </div>
        </div>
      </Card>

      <Card className="dashboard-section source-card">
        <SectionTitle icon="◆">Режим управления</SectionTitle>
        <div className="source-buttons">
          <div className={`source-option ${hyperHdrActive ? "active" : ""} ${manualActive ? "disabled" : ""}`}>
            <span>HyperHDR</span>
            <small>{hyperHdrActive ? "Управляет" : "Автозахват"}</small>
          </div>
          <button type="button" className={manualActive ? "active" : ""} disabled={hyperHdrActive} onClick={claimManualSource}>
            <span>Ручной</span>
            <small>{manualActive ? "Управляет" : "Эффекты"}</small>
          </button>
        </div>
        <div className="source-status">
          <span>
            {sourceOwner === 1
              ? "HyperHDR получил управление. Ручной режим заблокирован."
              : sourceOwner === 2
                ? "Ручной режим получил управление. Кадры HyperHDR игнорируются."
                : "Ожидание первого источника: HyperHDR или ручной режим."}
          </span>
          {sourceOwner !== 0 && <button type="button" onClick={releaseActiveSource}>Выключить</button>}
        </div>
      </Card>

      <Card className="dashboard-section">
        <SectionTitle icon="☼">Яркость</SectionTitle>
        <div className="dashboard-brightness">
          <div className="dashboard-brightness-value">{uiBrightness} <span>/ 255</span></div>
          <input
            className="dashboard-range"
            type="range"
            min="0"
            max="255"
            value={uiBrightness}
            onChange={(event) => {
              const value = clamp(Math.round((Number(event.target.value) / 255) * 31), 1, 31);
              setBrightness(value);
              setForm({ ...form, ledBrightness: value });
            }}
            onPointerUp={() => applyBrightness(brightness)}
            onKeyUp={() => applyBrightness(brightness)}
          />
          <button type="button" onClick={() => applyBrightness(brightness)} className="dashboard-apply" aria-label="Apply brightness">›</button>
        </div>
      </Card>

      <Card className="dashboard-section color-effect-card">
        <div className="color-effect-section">
          <SectionTitle icon="◉">Цвет</SectionTitle>
          <div
            className={`color-wheel ${hyperHdrActive ? "disabled" : ""}`}
            role="slider"
            tabIndex="0"
            aria-label="Выбор цвета"
            aria-disabled={hyperHdrActive}
            onPointerDown={(event) => {
              if (hyperHdrActive) return;
              event.currentTarget.setPointerCapture(event.pointerId);
              selectWheelColor(event);
            }}
            onPointerMove={(event) => {
              if (hyperHdrActive) return;
              if (event.currentTarget.hasPointerCapture(event.pointerId)) selectWheelColor(event);
            }}
            onPointerUp={(event) => {
              if (hyperHdrActive) return;
              selectWheelColor(event, true);
              event.currentTarget.releasePointerCapture(event.pointerId);
            }}
          >
            <span className="color-wheel-marker" style={{ left: `${wheelPosition.left}%`, top: `${wheelPosition.top}%`, background: selectedColor }} />
          </div>
          <div className="color-copy">
            <div className="color-heading">Выбор цвета</div>
            <div className="dashboard-status-detail">Нажмите или перемещайте маркер по кругу для точного выбора цвета.</div>
            <div className="current-color">Текущий цвет: <strong style={{ color: selectedColor }}>{selectedColor}</strong></div>
          </div>
        </div>
        <div className="color-effect-section effect-card">
          <SectionTitle icon="✦">Эффект</SectionTitle>
          <label>Режим</label>
          <select className="effect-select" value={effect} disabled={hyperHdrActive} onChange={(event) => setEffect(Number(event.target.value))}>
            {effects.map((name, index) => <option key={name} value={index}>{name}</option>)}
          </select>
          <div className="effect-control">
            <div className="effect-label"><span>Скорость</span><strong>{effectSpeed}</strong></div>
            <input type="range" min="0" max="255" value={effectSpeed} disabled={hyperHdrActive} onChange={(event) => setEffectSpeed(Number(event.target.value))} />
          </div>
          <div className="effect-control">
            <div className="effect-label"><span>Интенсивность</span><strong>{effectIntensity}</strong></div>
            <input type="range" min="0" max="255" value={effectIntensity} disabled={hyperHdrActive} onChange={(event) => setEffectIntensity(Number(event.target.value))} />
          </div>
          <button type="button" className="effect-apply" disabled={hyperHdrActive} onClick={() => applyEffect(effect)}>Применить эффект</button>
        </div>
      </Card>

      <Card className="system-card">
        <div className="system-stat">Ошибки: <strong>{info?.hyperHdrErrors ?? 0}</strong></div>
        <div className="system-stat divided">Uptime: <strong>{formatUptime(info?.uptimeMs)}</strong></div>
        <div className="system-stat">Свободно: <strong>{info?.freeHeap ? `${Math.round(info.freeHeap / 1024)} KB` : "— KB"}</strong></div>
      </Card>
    </>
  );
}
