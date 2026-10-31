# FilamentRecycler – CrowPanel V0.3 (Live-Temperatur)

**Ziel:** Mega/RAMPS liest drei echte NTC-Sensoren; Pi reicht `TEMP,t1,t2,t3` über USB an die native LVGL-Anzeige des CrowPanels DIS07050H **V3.0** weiter. Keine Motor-/Heizausgänge aktiv.

## Dateien auf GitHub aktualisieren

Die Dateien dieses Pakets ins bestehende `FilamentRecycler`-Repository kopieren, bestehende `firmware/crowpanel/filament_temp_ui.h` ersetzen; neue Skripte hinzufügen. Die bereits vorhandene Mega-Firmware und `raspberry/temp_bridge.py` unverändert lassen. **Die versteckten Build-Ordner `.vendor` und `.build` gehören nicht auf GitHub**. Für Git `.gitignore` um diese Zeilen ergänzen:

```
.vendor/
.build/
```

## Installation auf dem Raspberry Pi

```bash
cd ~/FilamentRecycler
git pull --ff-only
sudo apt update
sudo apt install -y git python3-serial
~/arduino-tools/bin/arduino-cli core install esp32:esp32@3.3.0
bash scripts/flash_crowpanel.sh
```

Beim ersten Lauf lädt `prepare_crowpanel.sh` das **offizielle Elecrow-V3.0-Referenzprojekt** herunter, kopiert den kompletten Beispielordner mit `ui*.c` und Grafiken und ersetzt die Demooberfläche durch unser Header-Modul. Es verändert nicht die Herstellerquelle. Durch Core-/Bibliotheksunterschiede kann die erste Kompilierung Anpassungen erfordern; bei einem Fehler die komplette Ausgabe aufbewahren.

**Achtung:** Vor dem Flashen das Original-Flashbackup erstellen, falls noch nicht geschehen. Das CrowPanel verfügt laut deinem `esptool`-Test über 4 MB Flash und 8 MB PSRAM. Der erste Core-Build benötigt möglicherweise viele Downloads und Arbeitsspeicher.

## Brücke starten

Nach dem Flashen, falls die Mega-Temperaturfirmware weiterhin `GET_TEMP` beantwortet:

```bash
cd ~/FilamentRecycler
python3 raspberry/temp_bridge.py
```

Die Daten sollten etwa so erscheinen: `TEMP,20.7,20.4,20.1`. Auf dem CrowPanel wechseln die Felder von `--.- C` zu Messwerten und Status `USB ONLINE`. Wird die Übertragung unterbrochen, erscheinen nach rund 1,8 s `--.- C` und `USB OFFLINE`.

Falls das Display schwarz bleibt, zuerst die Toolchain-/Bibliotheksversionen, die PSRAM-Auswahl und die Hardwareinitialisierung prüfen; das Projekt wurde hier nicht auf einem realen CrowPanel kompiliert oder geflasht.

## Hinweise

- Beim Flashen muss `temp_bridge.py` beendet sein, sonst ist `/dev/ttyUSB0` belegt.
- ESP32-S3 nutzt die CH340-UART0-Verbindung auf GPIO44/43. Kein `Serial.print()` für Debug auf dieser Schnittstelle; es würde das Temperaturprotokoll stören.
- Displayversorgung: 5 V mit ausreichender Leistungsreserve, bei Bedarf aktiver USB-Hub.
- Quelle Hardwaretreiber: https://github.com/Elecrow-RD/CrowPanel-5.0-HMI-ESP32-Display-800x480
- Dies ist ein reiner Labortest, noch keine sichere Maschinensteuerung.
