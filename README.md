# FilamentRecycler V0.8 – E0 Motortest: Fehlerbehebung

**Patch für V0.7.** Ersetzt genau diese drei Dateien im bestehenden GitHub-Repository:

- `firmware/ramps/mega_temp_usb/mega_temp_usb.ino`
- `firmware/crowpanel/filament_temp_ui.h`
- `raspberry/machine_controller.py`

Keine anderen Dateien werden benötigt. Bestehende Konfiguration auf dem Raspberry Pi wird beim Update nicht überschrieben. Heizungen bleiben deaktiviert.

## Korrigierte Fehler

1. **STEP/s springt auf 200 zurück:** Der Mega meldete im Stillstand seine interne Initialvorgabe (200 STEP/s). Der Pi übernahm sie in die gewünschte Geschwindigkeit und schickte sie alle 500 ms ans CrowPanel. Jetzt ist `motor_test_step_hz` als **Sollwert auf dem Pi gespeichert**. `MTEST,SET_HZ,<wert>` übernimmt und speichert jede Änderung über die Plus-/Minustasten. Mega-Leerlaufmeldungen dürfen diesen Sollwert nicht mehr überschreiben. Grenzwerte: 50–2500 in 50er-Schritten.
2. **Mögliche STOPs nach ca. 1 Sekunde:** Direkt nach START konnte ein verspäteter Leerlaufstatus des Mega den gerade aktivierten Motortest im Pi wieder zurücksetzen und damit die KEEP-Heartbeats unterbrechen. Der Pi ignoriert solche veralteten Leerlaufstatusmeldungen bis zur START-Bestätigung (max. 0,8 s) und protokolliert Ablehnungen bzw. Watchdog-Stopps.
3. **Unregelmäßige STEP-Impulse:** Bisher kamen alle STEP-Impulse direkt aus der AVR-`loop()` – dort laufen auch ADC-Messung, `Serial.print()` und Befehlsparser. Diese Aktivitäten können zu ungleichmäßigen Schrittintervallen führen. Jetzt wird der STEP-Takt mit **Timer1-Compare-Interrupt** (100 µs Zeitraster, 10 kHz) erzeugt und die Rate in einer separaten Beschleunigungsrampe aktualisiert. Das vermeidet lange STEP-Lücken durch die Hauptschleife, hat aber ein 100-µs-Zeitquantum; es ist kein messtechnisch validierter präziser Schritttakt.
4. **Stoppdiagnose:** Der Mega meldet `ACK,MTEST,LIMIT` (15-s-Limit), `ACK,MTEST,WATCHDOG` (Heartbeat fehlt) oder `ACK,MTEST,LOOP_STALL` (Hauptschleife hängt), bzw. `ERR,MTEST,START` (Start abgewiesen). Der Pi gibt diese Meldungen im Terminal aus.

## Beibehalten

- Keine automatische Motorfreigabe beim Booten.
- Touch-Tippbetrieb: beim Loslassen STOP und zusätzlicher 600-ms-Heartbeat-Watchdog auf dem Mega.
- Normaler zeitbegrenzter Test mit 1300-ms-Watchdog und 15-s-Abschaltung.
- Maximalgeschwindigkeit 2500 STEP/s, Anfahrrampe bis 5000 STEP/s².
- Lokale Ist-Anzeige der **kommandierten** STEP-Frequenz und berechnete Motor-/Getriebe-RPM. Keine Encoder-Messung.
- Heizungen weiterhin AUS.

## Installation

1. Die drei Dateien aus diesem ZIP am Windows-PC in das GitHub-Repository unter den genannten Pfaden übernehmen und committen.
2. Auf dem Raspberry Pi den laufenden Controller mit `Strg+C` beenden.
3. Im Pi-Terminal ausführen:

```bash
cd ~/FilamentRecycler
git pull --ff-only
bash scripts/flash_ramps.sh
bash scripts/flash_crowpanel.sh
bash scripts/run_controller.sh
```

**Beide Firmwares neu flashen**, sonst sind die Korrekturen nicht vollständig.

## Prüfschritte

1. CrowPanel-Menü `Einstellungen → Motor` öffnen. Im Stillstand mit `+50` mehrfach erhöhen, mindestens zwei Sekunden warten: der gewählte Wert muss stehen bleiben. Nach Pi-Neustart muss er weiterhin gespeichert sein.
2. Motor ohne Getriebe bei 100–200 STEP/s kurz per HALTEN-Button starten und wieder loslassen. Danach einen bestätigten 15-s-Test starten und hören, ob das rhythmische Klacken verschwunden ist.
3. Wenn es weiter klackt, im Pi-Terminal nach `Mega stopped motor: ACK,MTEST,WATCHDOG`, `LOOP_STALL` bzw. `LIMIT` sehen. Außerdem Mikrostepping-Jumper, VREF und Motorversorgung unter Last prüfen. Ein Klick pro Sekunde kann auch von Motor-Neustarts nach Watchdog-Auslösung kommen.
4. Nur bei ruhigem Lauf die Geschwindigkeit erhöhen. Stoppen muss immer sofort möglich sein. Testläufe beaufsichtigen.

## Entwicklungsstatus

Python-Code wurde mit Fake-Serial-Tests geprüft. **Mega- und CrowPanel-Firmware konnten in dieser Umgebung nicht mit der realen Arduino-/ESP32-Toolchain kompiliert und nicht auf Hardware getestet werden.** Insbesondere Timer1 wird exklusiv vom Motortest verwendet; spätere andere Timer1-Funktionen dürfen damit nicht kollidieren. Kein sicherheitsgerichteter Not-Halt. Unabhängige Möglichkeit zur Trennung der 12-V-Motorversorgung erforderlich.
