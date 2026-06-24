export const navItems = [
  { id: "home", icon: "⌂", label: "Главная" },
  { id: "led", icon: "▦", label: "LED" },
  { id: "power", icon: "⏻", label: "Питание" },
  { id: "settings", icon: "⚙", label: "Настройки" },
];

export const effects = [
  "Статичный",
  "Мигание",
  "Дыхание",
  "Заполнение",
  "Радуга",
  "Смена цвета",
  "Бегущий огонь",
  "Мерцание",
];

export const stripTypes = [
  { name: "APA102", maxHz: 20000000 },
  { name: "SK9822", maxHz: 12000000 },
  { name: "HD107S", maxHz: 24000000 },
  { name: "APA102-2020", maxHz: 20000000 },
];

export const spiFrequencies = [
  [1000000, "1 MHz"],
  [2000000, "2 MHz"],
  [4000000, "4 MHz"],
  [8000000, "8 MHz"],
  [12000000, "12 MHz"],
  [16000000, "16 MHz"],
  [20000000, "20 MHz"],
  [24000000, "24 MHz"],
];

export const colorOrders = ["RGB", "RBG", "GRB", "GBR", "BRG", "BGR"];

export const s2MiniOutputPins = [
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 18,
  21, 33, 34, 35, 36, 37, 38, 39, 40,
];
