import React from "react";
import { Card, LedStepper, SectionHeader, SectionTitle } from "../components/ui.jsx";
import { colorOrders, stripTypes } from "../lib/constants.js";

export function LedPage({ vm }) {
  const {
    setSection, stripType, setStripType, spiHz, setSpiHz, availableSpiFrequencies,
    form, setForm, dataPinOptions, clockPinOptions, oePinOptions, colorOrder, setColorOrder,
    ledSides, updateLedSide, geometryLedCount, ledDirection, setLedDirection,
    firstLed, setFirstLed, applyLedGeometry, ledDetectActive, ledDetectIndex,
    ledDetectStep, ledDetectCounts, ledDetectOrder, startLedDetection,
    stopLedDetection, resetLedDetection,
  } = vm;
  const sideNames = { top: "Верх", right: "Право", bottom: "Низ", left: "Лево" };
  const currentDetectSide = ledDetectStep < 4 ? ledDetectOrder[ledDetectStep] : null;

  return (
    <>
      <SectionHeader title="LED" onBack={() => setSection("home")} />
      <Card className="led-card">
        <SectionTitle icon="▣">Конфигурация ленты</SectionTitle>
        <div className="led-config-grid">
          <div>
            <label>Тип ленты</label>
            <select
              value={stripType}
              onChange={(event) => {
                const nextType = Number(event.target.value);
                setStripType(nextType);
                if (spiHz > stripTypes[nextType].maxHz) setSpiHz(stripTypes[nextType].maxHz);
              }}
            >
              {stripTypes.map((type, index) => <option key={type.name} value={index}>{type.name}</option>)}
            </select>
          </div>
          <div>
            <label>SPI частота</label>
            <select value={spiHz} onChange={(event) => setSpiHz(Number(event.target.value))}>
              {availableSpiFrequencies.map(([hz, label]) => <option key={hz} value={hz}>{label}</option>)}
            </select>
          </div>
          <div>
            <label>GPIO DATA</label>
            <select value={form.dataPin} onChange={(event) => setForm({ ...form, dataPin: Number(event.target.value) })}>
              {dataPinOptions.map((pin) => <option key={pin} value={pin}>GPIO {pin}</option>)}
            </select>
          </div>
          <div>
            <label>GPIO CLOCK</label>
            <select value={form.clockPin} onChange={(event) => setForm({ ...form, clockPin: Number(event.target.value) })}>
              {clockPinOptions.map((pin) => <option key={pin} value={pin}>GPIO {pin}</option>)}
            </select>
          </div>
          <div>
            <label>Порядок цветов</label>
            <select value={colorOrder} onChange={(event) => setColorOrder(Number(event.target.value))}>
              {colorOrders.map((order, index) => <option key={order} value={index}>{order}</option>)}
            </select>
          </div>
          <div>
            <label>GPIO OE</label>
            <select value={form.oePin} onChange={(event) => setForm({ ...form, oePin: Number(event.target.value) })}>
              {oePinOptions.map((pin) => <option key={pin} value={pin}>GPIO {pin}</option>)}
            </select>
          </div>
          <div>
            <label>Активный уровень OE</label>
            <select value={form.oeActiveLow} onChange={(event) => setForm({ ...form, oeActiveLow: Number(event.target.value) })}>
              <option value="1">LOW</option>
              <option value="0">HIGH</option>
            </select>
          </div>
          <div><label>Питание ленты</label><div className="led-fixed-field">5 V</div></div>
        </div>
      </Card>

      <Card className="led-card led-detect-card">
        <SectionTitle icon="◎">Автоопределение отрезков</SectionTitle>
        <div className="led-detect-copy">
          Диоды переключаются по одному каждые 2 секунды. Нажмите «Стоп на повороте»,
          когда загорится последний диод текущей стороны.
        </div>
        <div className="led-detect-origin">
          <div>
            <label>С какого угла начинаем</label>
            <select
              value={firstLed}
              disabled={ledDetectStep > 0 || ledDetectActive}
              onChange={(event) => setFirstLed(event.target.value)}
            >
              <option value="top-left">Верхний левый</option>
              <option value="top-right">Верхний правый</option>
              <option value="bottom-right">Нижний правый</option>
              <option value="bottom-left">Нижний левый</option>
            </select>
          </div>
          <div>
            <label>Направление отсчёта</label>
            <select
              value={ledDirection}
              disabled={ledDetectStep > 0 || ledDetectActive}
              onChange={(event) => setLedDirection(event.target.value)}
            >
              <option value="clockwise">По часовой стрелке</option>
              <option value="counterclockwise">Против часовой</option>
            </select>
          </div>
        </div>
        <div className="led-detect-status">
          <div>
            <span>Текущая сторона</span>
            <strong>{currentDetectSide ? sideNames[currentDetectSide] : "Готово"}</strong>
          </div>
          <div>
            <span>Номер диода</span>
            <strong>{ledDetectIndex || "—"}</strong>
          </div>
        </div>
        <div className="led-detect-results">
          {["top", "right", "bottom", "left"].map((side) => (
            <span key={side}>{sideNames[side]}: <strong>{ledDetectCounts[side] ?? "—"}</strong></span>
          ))}
        </div>
        <div className="led-detect-actions">
          {!ledDetectActive && ledDetectStep < 4 && (
            <button type="button" className="start" onClick={startLedDetection}>
              {ledDetectStep === 0 ? "Начать автоопределение" : "Продолжить следующую сторону"}
            </button>
          )}
          {ledDetectActive && (
            <button type="button" className="stop" onClick={stopLedDetection}>Стоп на повороте</button>
          )}
          {(ledDetectStep > 0 || ledDetectActive) && (
            <button type="button" onClick={resetLedDetection}>Сбросить</button>
          )}
        </div>
      </Card>

      <Card className="led-card">
        <SectionTitle icon="⌗">Геометрия ленты</SectionTitle>
        <div className="led-steppers">
          <LedStepper label="Верх" value={ledSides.top} onChange={(value) => updateLedSide("top", value)} />
          <LedStepper label="Право" value={ledSides.right} onChange={(value) => updateLedSide("right", value)} />
          <LedStepper label="Низ" value={ledSides.bottom} onChange={(value) => updateLedSide("bottom", value)} />
          <LedStepper label="Лево" value={ledSides.left} onChange={(value) => updateLedSide("left", value)} />
        </div>
        <div className="led-total"><span>Всего LED</span><strong>{geometryLedCount}</strong></div>
      </Card>

      <Card className="led-card">
        <div className="led-map-title"><span>Схема ленты</span></div>
        <div className="led-map">
          <div className="led-map-label top">Top · {ledSides.top}</div>
          <div className="led-map-label right">Right<br /><strong>{ledSides.right}</strong></div>
          <div className="led-map-label bottom">Bottom · {ledSides.bottom}</div>
          <div className="led-map-label left">Left<br /><strong>{ledSides.left}</strong></div>
          <div className={`led-frame ${ledDirection}`}><span className={`led-origin ${firstLed}`} /></div>
        </div>
      </Card>
      <Card className="led-power-card">
        APA102 × {geometryLedCount} × 5V ≈ <strong>{Math.round(geometryLedCount * 0.3)}W</strong>
        <span> ({(geometryLedCount * 0.06).toFixed(1)}A, максимум)</span>
      </Card>
      <button type="button" className="led-save" onClick={applyLedGeometry}>Сохранить конфигурацию LED</button>
    </>
  );
}
