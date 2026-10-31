# FilamentRecycler V0.6 – E0 Motortest 1600 STEP/s

Kleines Update für das vorhandene V0.6-Projekt. Die Obergrenze der E0-Motortestfrequenz wurde von 800 auf **1600 STEP/s** erhöht.

## Geändert
- `firmware/crowpanel/filament_temp_ui.h`: Benutzeroberfläche erlaubt bis 1600 STEP/s.
- `raspberry/machine_controller.py`: Eingabevalidierung erlaubt bis 1600 STEP/s.
- `firmware/ramps/mega_temp_usb/mega_temp_usb.ino`: E0-Impulserzeugung erlaubt bis 1600 STEP/s.

Unverändert: 50 STEP/s Minimum; Schritte in 50-STEP/s-Schritten; 15 Sekunden Motorlauf-Limit; 1300-ms-Watchdog im Mega; Startbestätigung; manuelle STOP-Taste; Heizausgänge bleiben aus.

## Installation
Nur diese drei geänderten Dateien in den entsprechenden Pfaden im GitHub-Repository ersetzen. Auf dem Pi:

```bash
cd ~/FilamentRecycler
git pull --ff-only
# Laufenden Controller vorher mit Strg+C beenden
bash scripts/flash_ramps.sh
bash scripts/flash_crowpanel.sh
bash scripts/run_controller.sh
```

## Hinweis
Ohne Beschleunigungsrampe kann der NEMA17 bei höheren STEP-Frequenzen Schritte verlieren oder stehenbleiben. Zunächst 200–400 STEP/s testen, dann langsam erhöhen. Die tatsächliche Ausgangsdrehzahl hängt von Microstepping und der Getriebeübersetzung ab. Beim Betrieb einer heißen oder gefüllten Schnecke gelten zusätzliche mechanische und thermische Risiken.
