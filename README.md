# FilamentRecycler V0.4 — HMI + Machine Controller

**Experimentelles Testprojekt. Kein Aktor wird eingeschaltet.**

Hardware: Raspberry Pi 4, Arduino Mega 2560 + RAMPS 1.6, CrowPanel DIS07050H V3.0 (ESP32-S3, 4 MB Flash/8 MB PSRAM), drei 100-kΩ-NTC-Sensoren an A13/A15/A14.

## Funktionen

- Helles LVGL-9-Dashboard mit 3 Temperaturkarten, Motor-Sollwert, Filamentstatus und Systemkarte.
- Detailseiten mit Verlaufsgrafiken, bildschirmfüllende Graphen, editierbare Solltemperaturen.
- Einstellungen für je 3 PID-Parameter in 3 Zonen; Motor-Solldrehzahl und Filament-Solldurchmesser.
- Bestätigungsdialoge für **START / PAUSE / STOP**; **Statussimulation** am Pi (`READY`, `PREHEAT`, `PAUSED`) ohne Heiz-/Motor-Ausgänge.
- Raspberry Pi ist Konfigurations-Master. Änderungen in `raspberry/machine_config.json` (lokal, bewusst nicht in Git); beim Verbinden Übertragung an den Mega. Mega hält Kopie nur im RAM. Kein EEPROM-Schreiben.
- Temperatur-Istwerte kommen vom Mega; Durchmesser-Istwert bleibt `ERR`, bis ein echter Sensor integriert ist. Graphenhistorie liegt nur im CrowPanel-RAM (60 Werte × 500 ms, ca. 30 s) und wird bei Neustart gelöscht.
- Die Material-/Spulenseite ist vorerst eine Platzhalterseite.

## GitHub → Raspberry Pi (nur Download)

Die Dateien vom Windows-PC in dein Repository `YBlendermann/FilamentRecycler` hochladen und auf dem Raspberry Pi abrufen:

```bash
cd ~/FilamentRecycler
git pull --ff-only
```

Wichtig: Die gesamte Verzeichnisstruktur aus diesem Paket übernehmen, nicht nur einzelne Header. Vorhandene `firmware/ramps/...`- und `scripts/...`-Dateien werden ersetzt.

## 1. Mega V0.4 flashen

Vorher die laufende USB-Brücke mit Ctrl+C beenden. Heizung und Motor elektrisch spannungsfrei lassen.

```bash
cd ~/FilamentRecycler
bash scripts/flash_ramps.sh
```

Neue Mega-Firmware meldet `ID,RAMPS_CONFIG,0.4`. Die bisherige V0.2-Firmware ist für den neuen Controller **nicht kompatibel**. Siehe die Protokolländerung unten.

## 2. CrowPanel flashen

```bash
cd ~/FilamentRecycler
bash scripts/flash_crowpanel.sh
```

Das Script verwendet das von dir bereits erfolgreich verwendete **Elecrow-V3.0-LVGL-Beispiel** einschließlich LovyanGFX, GT911 und PCA9557 und ersetzt die Demo-UI durch `filament_temp_ui.h`. Es lädt Elecrow in `.vendor/` und erzeugt den Sketch in `.build/` (beides bewusst nicht in Git). Der erste Compile kann viele LVGL-Dateien übersetzen. Original-Flashbackup aufbewahren.

**Ungetestete Kombination:** Diese V0.4-HMI wurde hier nicht auf einem echten CrowPanel kompiliert oder gestartet. Fehlerausgaben und LVGL-API-Abweichungen sind möglich. Der bereits bewährte V0.3-Stand bleibt dein Rückfallpunkt.

## 3. Controller starten

```bash
sudo apt install -y python3-serial
cd ~/FilamentRecycler
bash scripts/run_controller.sh
```

Nicht gleichzeitig die alte `raspberry/temp_bridge.py` starten! Beide würden dieselben seriellen USB-Geräte öffnen. Ein erfolgreicher Start zeigt unter anderem `Mega connected; settings synchronized`. Die Ports werden per USB VID/PID identifiziert (Mega 2341:0042, CrowPanel CH340 1a86:7523).

Falls du auf einem zweiten Pi einen anderen Mega-Klon verwendest, kannst du Ports explizit übergeben:

```bash
bash scripts/run_controller.sh --mega /dev/ttyACM0 --display /dev/ttyUSB0
```

## Kommunikationsprotokoll (115200 8N1 ASCII, LF)

Mega: `HELLO` → `ID,RAMPS_CONFIG,0.4`; `GET_TEMP` → `TEMP,t1,t2,t3`; `CFG,TEMP,index,value`, `CFG,PID,index,KP|KI|KD,value`, `CFG,MOTOR,RPM,value`, `CFG,FILAMENT,TARGET,value` → `ACK,CFG`.

CrowPanel: `HELLO`, `ACTION,START|PAUSE|STOP`, `SET,TEMP,index,value`, `SET,PID,index,KP|KI|KD,value`, `SET,MOTOR,RPM,value`, `SET,FILAMENT,TARGET,value`.

Pi → CrowPanel:

```
STATE,PREHEAT,1,20.1,20.2,20.3,175.0,190.0,205.0,12.50,ERR,1.750
PID,0,5.000,0.100,15.000
PID,1,5.000,0.100,15.000
PID,2,5.000,0.100,15.000
```

Der Pi sendet `STATE` alle 500 ms. `ERR` bedeutet ungültigen oder fehlenden Sensor. Empfangs-Timeout nach 2,5 Sekunden markiert Pi OFFLINE. Ungültige Werte werden vom Pi verworfen, nicht gespeichert.

## Sicherheit und nächste Schritte

**START, PAUSE und STOP sind in V0.4 ausschließlich simulierte Prozesszustände.** Der Mega hat keine aktiven Last-Ausgänge. Für die echte Temperaturregelung, Heizer, Förderschneckenmotor und später Fördermenge sind unabhängige Übertemperaturabschaltung, Sensorfehlerüberwachung, sicherer Anlauf, Hardware-Not-Aus, Motor-Watchdog, Test der Ausgangsstufen sowie eine eindeutige Zustandsmaschine erforderlich. Die angezeigten PID-Werte sind momentan nur gespeicherte Parameter, **keine laufende PID-Regelung**. Die Drehzahl ist ein Sollwert; die Fördermenge in g/h erfordert Kalibrierung.
