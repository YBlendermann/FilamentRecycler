# FilamentRecycler V0.6 – E0-Motortest über CrowPanel

**Testsoftware, keine fertige Maschinensteuerung.** Baut auf V0.5 auf.

## Funktionen
- CrowPanel → Einstellungen → Motor: STEP-Frequenz 50–800 Hz, Drehrichtung, MOTOR START mit Bestätigung, MOTOR STOP ohne Bestätigung.
- Alternativ Dashboard-Förderkarte antippen → Motor-Einstellungen.
- Raspberry Pi leitet `MTEST`-Befehle an Mega E0 weiter und benötigt Heartbeat von CrowPanel.
- Mega E0: STEP=26, DIR=28, ENABLE=24 (LOW aktiv). Max. 15 s Laufzeit, 1,3 s Watchdog ohne Heartbeat.
- NTC-Temperaturmessung, Dashboard und Settings bleiben erhalten. Heizungen bleiben softwareseitig aus.
- Normale Dashboard-Prozessbuttons sind weiterhin **Simulation** und starten NICHT den E0-Motor.

## WICHTIG vor 12 V
1. Netzteil aus und USB abziehen. TMC2208-Pinout/Ausrichtung auf E0 gegen **deine konkrete Platine** prüfen.
2. Motorphasen und Motoranschluss prüfen. Motor im Betrieb niemals stecken oder abziehen.
3. VREF/Strombegrenzung gemäß Treiberrevision und Sense-Widerständen prüfen; nicht einfach unter Last testen.
4. Sicherstellen, dass E0 nicht mechanisch blockiert. Für den Ersttest Fördergetriebe möglichst abkoppeln.
5. E0-`ENABLE` geht auf HIGH beim Boot und nach jedem Stop. Das ist kein Hardware-Not-Halt. Ein Ausfall der Mega-Firmware kann den Software-Watchdog unwirksam machen. 12-V-Versorgung bei Problemen hardwareseitig ausschalten.
6. **Kein automatischer Bewegungsstart** beim Pi-/Mega-/Display-Neustart.

## Installation
Nur auf Windows die ZIP-Inhalte in dein GitHub-Repository kopieren/pushen. Auf dem Raspberry Pi:

```bash
cd ~/FilamentRecycler
git pull --ff-only
# Controller vorher mit Strg+C beenden
bash scripts/flash_ramps.sh
bash scripts/flash_crowpanel.sh
bash scripts/run_controller.sh
```

Wichtig: **Mega und CrowPanel beide flashen**, sonst werden `MTEST`-Befehle nicht verstanden.

## Technische Notiz
STEP/s ist die Pulsrate, nicht Motordrehzahl. Erst nach Bestimmung der wirklichen TMC2208-Microstep-Konfiguration kann eine korrekte rpm-Ausgabe ergänzt werden. Motortest: bis 800 Hz ohne Beschleunigungsrampe. Vor Start stets 50–200 Hz wählen. Die Mikrocontroller-Ausgänge für Heizer sind weiterhin unangetastet.
