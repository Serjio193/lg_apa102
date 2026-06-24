import React from "react";

export function Card({ children, className = "" }) {
  return <section className={`panel dashboard-card ${className}`}>{children}</section>;
}

export function SectionTitle({ icon, children }) {
  return (
    <div className="dashboard-title">
      <span>{icon}</span>
      <span>{children}</span>
    </div>
  );
}

export function StatIcon({ kind, size = 12 }) {
  const common = {
    width: size,
    height: size,
    viewBox: "0 0 24 24",
    fill: "none",
    stroke: "currentColor",
    strokeWidth: 2,
    strokeLinecap: "round",
    strokeLinejoin: "round",
    style: { opacity: 0.9, flex: "0 0 auto" },
  };
  if (kind === "usb") {
    return (
      <svg width={size} height={size} viewBox="0 0 32 48" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
        <path d="M16 43V5" />
        <path d="M12 9l4-4 4 4" />
        <path d="M16 27l-8-7v-5" />
        <circle cx="8" cy="12" r="2.5" />
        <path d="M16 34l8-7v-6" />
        <rect x="21.5" y="15.5" width="5" height="5" rx=".6" />
        <circle cx="16" cy="43" r="3" />
      </svg>
    );
  }
  if (kind === "wifi") {
    return (
      <svg {...common}>
        <path d="M5 9a12 12 0 0 1 14 0" />
        <path d="M7.5 12a8 8 0 0 1 9 0" />
        <path d="M10 15a4 4 0 0 1 4 0" />
        <path d="M12 18h0" />
      </svg>
    );
  }
  return (
    <svg {...common}>
      <path d="M5 5h4v14H5z" />
      <path d="M10 5h4v14h-4z" />
      <path d="M15 5h4v14h-4z" />
    </svg>
  );
}

export function LedStepper({ label, value, onChange }) {
  return (
    <div className="led-stepper-row">
      <span>{label}</span>
      <div className="led-stepper">
        <button type="button" onClick={() => onChange(Math.max(0, value - 1))}>−</button>
        <strong>{value}</strong>
        <button type="button" onClick={() => onChange(Math.min(999, value + 1))}>+</button>
      </div>
    </div>
  );
}

export function PowerButton({ on, onToggle }) {
  return (
    <button type="button" className="power" aria-pressed={on} onClick={onToggle}>
      <div className="power-ring" />
      <div className="power-core" />
      <div className="power-mark">
        <svg viewBox="0 0 24 24" fill="none" stroke={on ? "#4ae18c" : "#ff6c74"} strokeWidth="2.3" strokeLinecap="round" strokeLinejoin="round">
          <path d="M12 3v8" />
          <path d="M7.2 5.8a7 7 0 1 0 9.6 0" />
        </svg>
      </div>
    </button>
  );
}

export function SectionHeader({ title, onBack }) {
  return (
    <div className="section-header">
      <button type="button" onClick={onBack} aria-label="Назад">‹</button>
      <h2>{title}</h2>
      <span />
    </div>
  );
}
