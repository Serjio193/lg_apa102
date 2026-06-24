export function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

export function hsvToHex(hue, saturation, value = 1) {
  const chroma = value * saturation;
  const sector = hue / 60;
  const x = chroma * (1 - Math.abs((sector % 2) - 1));
  const [red, green, blue] = sector < 1 ? [chroma, x, 0]
    : sector < 2 ? [x, chroma, 0]
      : sector < 3 ? [0, chroma, x]
        : sector < 4 ? [0, x, chroma]
          : sector < 5 ? [x, 0, chroma]
            : [chroma, 0, x];
  const match = value - chroma;
  return `#${[red, green, blue].map((channel) => Math.round((channel + match) * 255).toString(16).padStart(2, "0")).join("")}`;
}

export function hexToWheelPosition(hex) {
  const red = parseInt(hex.slice(1, 3), 16) / 255;
  const green = parseInt(hex.slice(3, 5), 16) / 255;
  const blue = parseInt(hex.slice(5, 7), 16) / 255;
  const max = Math.max(red, green, blue);
  const min = Math.min(red, green, blue);
  const delta = max - min;
  let hue = 0;
  if (delta) {
    if (max === red) hue = 60 * (((green - blue) / delta) % 6);
    else if (max === green) hue = 60 * (((blue - red) / delta) + 2);
    else hue = 60 * (((red - green) / delta) + 4);
  }
  if (hue < 0) hue += 360;
  const saturation = max === 0 ? 0 : delta / max;
  const angle = (hue - 90) * Math.PI / 180;
  return {
    left: 50 + Math.cos(angle) * saturation * 50,
    top: 50 + Math.sin(angle) * saturation * 50,
  };
}
