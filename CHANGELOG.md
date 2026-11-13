## 0.4.0 — 2026-11-13

- Both tiles redrawn after the design concept: a dark ground, a full progress ring with the percentage in it, the status in its colour (printing cyan, paused yellow, error red, finished green), and the temperatures as chips, warm values in orange.
- The status tile arranges itself for every size from 1×1 to 4×2, so no value is cut off.
- The print status is read from the integration's raw state (`running`, `pause`, ...), and the time left in whatever unit Home Assistant reports.

## 0.1.0 — 2026-11-13

- Initial release.
- `printer_status` tile (2×1 to 4×2): print status text, animated progress bar, layer, time remaining, and all three temperatures.
- `printer_temps` tile (1×1 to 2×2): compact nozzle / bed / chamber temperature overview.
- English and German translations.
