# Bambulab X1C — Tessera Plugin

Shows the live status and temperatures of a **Bambulab X1C** 3D printer on a Tessera screen via Home Assistant sensors.

## Tiles

| Tile | Sizes | Shows |
|---|---|---|
| Printer status | 1×1 – 4×2 | Progress ring, status, job name, layer, time remaining, all three temperatures; what does not fit a small tile gives way |
| Printer temperatures | 1×1 – 2×2 | Nozzle / bed / chamber temperatures |

## Setup

Each tile needs you to map the matching Home Assistant sensor in its inspector:

| Option | Typical HA entity |
|---|---|
| Print status | `sensor.x1c_00m09d481700483_druckstatus` |
| Progress | `sensor.x1c_00m09d481700483_druckfortschritt` |
| Current layer | `sensor.x1c_00m09d481700483_aktuelle_schicht` |
| Total layers | `sensor.x1c_00m09d481700483_gesamtzahl_der_schichten` |
| Time remaining | `sensor.x1c_00m09d481700483_verbleibende_zeit` |
| Nozzle temperature | `sensor.x1c_00m09d481700483_temperatur_der_duse` |
| Bed temperature | `sensor.x1c_00m09d481700483_druckbetttemperatur` |
| Chamber temperature | `sensor.x1c_00m09d481700483_temperatur_im_druckraum` |

The exact entity names depend on how the Bambulab integration was set up in Home Assistant.
All options are independent — leave any you don't have empty.
