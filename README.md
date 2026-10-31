# FilamentRecycler V0.7 – E0 Motor über CrowPanel

**Update auf die bestehende V0.6-Installation (Mega + Pi + CrowPanel). Kein vollständiges neues Repository.** Nur die drei Dateien dieses Archivs im GitHub-Repository an ihren Pfaden ersetzen. Heizungen bleiben deaktiviert; Dashboard-Prozessstart bleibt Simulation.

## Neu

- E0-Motortest: **50–2500 STEP/s**, Schrittweite 50.
- **Anfahrrampe:** 100–5000 STEP/s², Startwert 500 STEP/s². Die Beschleunigung erfolgt lokal auf dem Mega (nicht von USB abhängig).
- **15s TEST:** Start mit Dialogbestätigung, nach maximal 15 Sekunden automatische Abschaltung, Motor STOP jederzeit ohne Dialog.
- **HALTEN:** Tastfläche gedrückt halten = Tippbetrieb (JOG). Beim Loslassen oder Verlassen der Seite sofort STOP-Befehl. Zusätzlich muss der Mega alle 600 ms ein `MTEST,HOLD`-Heartbeat empfangen, sonst schaltet er selbständig ab. Pi verwirft JOG nach 650 ms ohne UI-Heartbeat. Auch im Tippbetrieb maximal 15 s je Tastenbetätigung. Ein echter hardwareseitiger Not-Halt wird dadurch nicht ersetzt.
- **Drehzahlanzeige:** Motorwelle und Getriebeausgang in U/min, berechnet aus der aktuell kommandierten STEP-Frequenz, 200 Schritten/Umdrehung, *angenommenem* 1/16-Microstepping und 1:20-Getriebe. Keine reale Encoder-Messung, Schrittverluste sind nicht nachweisbar.
- **Microstep-Einstellung:** Button `uSteps` setzt nur den Berechnungsfaktor (gespeichert auf Pi). Er konfiguriert NICHT die Hardware-Jumper des RAMPS/TMC2208.
- **Theoretischer axialer Schneckenvorschub (mm/min):** nur bei eingetragener Schneckensteigung > 0. Die tatsächliche Schmelzefördermenge in g/min ist nicht daraus ableitbar und bleibt im Dashboard `-- g/min`.
- Dashboard-Förderkarte zeigt die berechnete aktuelle Schnecken-U/min und, falls die Steigung bekannt ist, den **theoretischen** axialen Vorschub. Filamentzug bleibt ohne Sensor `-- m/min`.
- Alle neuen Parameter werden durch den Pi in der vorhandenen `machine_config.json` gespeichert.

## Rechenbeispiel

Bei einem Motor mit 200 Vollschritten/Umdrehung, 1/16 Microstepping und Getriebe 1:20:

```
Motor-RPM    = STEP/s × 60 / (200 × 16)
Schnecken-RPM = Motor-RPM / 20
```

Bei 2500 STEP/s: **46,875 U/min Motor**, **2,34375 U/min Getriebeausgang**. Bei 5 mm Schneckensteigung wäre der reine kinematische Wert **11,72 mm/min**; das ist **nicht** der reale Filament- oder Schmelzevorschub.

## Installation

1. ZIP auf Windows entpacken und die drei Dateien mit derselben Verzeichnisstruktur in GitHub übernehmen.
2. Auf dem Pi laufenden Controller beenden (Strg+C).
3. Auf dem Pi:

```bash
cd ~/FilamentRecycler
git pull --ff-only
bash scripts/flash_ramps.sh
bash scripts/flash_crowpanel.sh
bash scripts/run_controller.sh
```

**Mega und CrowPanel müssen beide neu geflasht werden**, das USB-Motorprotokoll hat sich geändert. Die übrigen Dateien des bisherigen Projekts bleiben unverändert.

## Sicherheit und bekannte Grenzen

- Treiberausrichtung, E0-Anschluss, Motorstrom/VREF und RAMPS-Microstep-Jumper müssen zur echten Hardware passen.
- Für erste Versuche am besten **unbelastet**, mit kleiner STEP-Frequenz und langer Anfahrrampe beginnen. Bei 2500 STEP/s sind Schrittverluste, Blockieren oder Erwärmung unter Last möglich.
- Der Pi und das CrowPanel sind **keine sicherheitsgerichtete Steuerung**. Ein Touch-Ausfall, blockierte Firmware oder klebender Treiber lässt sich nicht vollständig durch softwarebasierte Stopps absichern. Ein unabhängiger Hardware-Abschalter für die 12-V-Motorversorgung ist notwendig.
- Stop/Loslassen deaktiviert den Motor **sofort**, also **ohne Bremsrampe**, um die Stoppzeit nicht künstlich zu verlängern. Die Anfahrrampe ist vorhanden. Eine separate geregelte Bremsrampe können wir für einen späteren Produktionsbetrieb hinzufügen.
- Keine ESP32/LVGL-Hardwarekompilierung und kein Test auf deinem echten Board in dieser Umgebung möglich; das Paket ist ein technisch geprüfter, aber noch nicht real kompilierter Kandidat.
