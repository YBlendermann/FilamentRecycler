# FilamentRecycler – Temperaturtest V0.2

Zentrale USB-Steuerung: Raspberry Pi ↔ Mega 2560/RAMPS 1.6 und CrowPanel DIS07050H V3.0.

## Enthalten
- `firmware/ramps/mega_temp_usb/mega_temp_usb.ino`: reale NTC-Messung, keine Heiz-/Motoransteuerung
- `raspberry/temp_bridge.py`: fragt Mega ab und überträgt Daten an das CrowPanel
- `firmware/crowpanel/filament_temp_ui.h`: LVGL-9-Erweiterung, **kein allein kompilierbarer Sketch**
- `scripts/flash_ramps.sh`: kompiliert und flasht Mega mit Arduino CLI
- `scripts/run_bridge.sh`: startet die USB-Brücke

## Voraussetzungen auf dem Pi
`sudo apt install -y git python3-serial`
Arduino CLI und `arduino:avr` müssen installiert sein.

## Mega flashen
`bash scripts/flash_ramps.sh /dev/ttyACM0`

## Pi-USB-Brücke starten
`bash scripts/run_bridge.sh`

## CrowPanel
Die UI-Headerdatei wird in das V3.0-Referenzprojekt von Elecrow eingebunden, das LVGL 9, GT911, PCA9557 und die RGB-Displayinitialisierung übernimmt. Datei in den Sketchordner kopieren, nach LVGL-Include `#include "filament_temp_ui.h"` ergänzen, nach vollständiger Hardware- und LVGL-Initialisierung `filament_ui_begin();` und in `loop()` `filament_ui_poll();` ergänzen. Hersteller-Demo-UI dabei deaktivieren; LVGL-Tick und `lv_timer_handler()` beibehalten.

Herstellerprojekt: https://github.com/Elecrow-RD/CrowPanel-5.0-HMI-ESP32-Display-800x480

## Hardware und Sicherheit
NTC-Eingang A13, Mitte A15, Düse A14. Thermistorannahmen aus altem V0.4.1-Sketch: R25=100k, Beta=3950, Pull-up=4k7. `GET_TEMP` antwortet `TEMP,23.4,24.0,25.1`; ungültige Messwerte als `ERR`. Keine Heizungsfreigabe: Lastversorgung während Tests trennen.
