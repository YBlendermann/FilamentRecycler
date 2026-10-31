# FilamentRecycler V0.5 – Dashboard-Layout 800×480

**Status:** Layout-Kandidat; noch nicht auf echter Hardware kompiliert oder getestet.

Dieses Paket baut auf der funktionsfähigen V0.4-Architektur auf. Das Dashboard ist auf 800×480 px skaliert und orientiert sich am dunklen Schwarz/Orange-Referenzdesign.

## Was sich ändert

- 108 px breite linke Navigationsleiste mit LVGL-Symbolen (Home, Temperatur, Motor, Filament, Setup)
- **Eine** Temperaturkarte für Eingang, Mitte, Düse mit drei Miniatur-Trends
- Eine Förderkarte mit Schnecken-Sollwert, Fördermenge und Filamentzug (Istwerte ohne Sensor/Rückmeldung als `--`)
- Kühlkarte mit Lüfterstatus `-- %` (noch kein Kühlmodul)
- Filamentkarte mit Istwert, Sollwert und Graph
- Systemkarte für Mega/Pi/Sensorkommunikation
- Start/Pause/Stopp mit den bestehenden Bestätigungsdialogen; weiterhin **nur Simulation**
- Detailseiten (Temperaturen, Filament, Einstellungen) bleiben erhalten

Die ikonischen Formen aus der Designvorlage werden in LVGL durch eingebaute Symbolglyphen und einfache gezeichnete Piktogramme angenähert. Eine pixelgenaue Kopie der Desktop-Vorlage wäre auf 800×480 nicht lesbar.

## Update über GitHub

Die Dateien auf deinem **Windows-PC** in `YBlendermann/FilamentRecycler` hochladen; Pi lädt nur herunter:

```bash
cd ~/FilamentRecycler
git pull --ff-only
```

**Bevor das CrowPanel geflasht wird:** laufenden `run_controller.sh`/`machine_controller.py`-Prozess beenden, damit `/dev/ttyUSB0` frei ist.

```bash
bash scripts/flash_crowpanel.sh
bash scripts/run_controller.sh
```

Zum schnellen Rückweg zur bewährten V0.4-Oberfläche die Datei `firmware/crowpanel/filament_temp_ui.h` aus der vorherigen Git-Version wiederherstellen und erneut flashen.

## Wichtige Einschränkungen

- Nicht am RAMPS-Heiz- oder Motorstromkreis mit angeschlossener Last testen; Aktoren müssen aus bleiben.
- Alle Motor-, Kühl- und Filament-**Istwerte**, für die keine Sensorik vorhanden ist, werden als `--` angezeigt. Die Schneckendrehzahl wird nur als **Sollwert** dargestellt.
- Der echte Pi-Controller, die Mega-Temperaturmessung und das vorhandene ASCII-USB-Protokoll bleiben unverändert.
- Charts zeigen zunächst nur 60 letzte Datenpunkte (~30 s), noch keine persistenten Langzeitkurven.
- Es sind bewusst keine dynamischen Icons/Bilddateien extern notwendig; das Layout wird vollständig durch LVGL gezeichnet.
