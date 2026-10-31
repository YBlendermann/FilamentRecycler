#!/usr/bin/env python3
"""Patcht ausschließlich das geprüfte Elecrow-DIS07050H-V3.0-Referenzbeispiel.

Hardwareinitialisierung, PSRAM/LVGL-Buffer und Touch/PCA9557 bleiben erhalten.
Bei geänderter Upstream-Struktur bricht das Skript explizit ab.
"""
import re
import sys
from pathlib import Path

source = Path(sys.argv[1])
s = source.read_text(encoding="utf-8")
if 'class LGFX' not in s or 'PCA9557 Out' not in s or 'touch_init();' not in s:
    sys.exit('FEHLER: Dies ist nicht das erwartete V3.0-Beispiel.')
if s.count('ui_init();') != 1:
    sys.exit('FEHLER: ui_init nicht genau einmal gefunden.')
if s.count('#include "touch.h"') != 1:
    sys.exit('FEHLER: touch.h nicht eindeutig gefunden.')
s = s.replace('#include "touch.h"', '#include "touch.h"\n#include "filament_temp_ui.h"')
s = s.replace('ui_init();', 'filament_ui_begin();')
# Touch-Callbacks dürfen keine Logmeldungen mehr auf UART0 ausgeben,
# da UART0 nun Daten des Raspberry Pi empfängt.
s = re.sub(r'^\s*Serial\.(?:print|println|printf)\s*\([^;]*\);', '', s, flags=re.MULTILINE)
s = re.sub(r'^\s*Serial\.begin\(115200\);', '', s, flags=re.MULTILINE)
# Die DHT20-Demo bleibt als unbenutzte Bibliothek eingebunden; keine
# Abhängigkeit vom Sensor und keine Änderung an der Displayinitialisierung.
s = s.replace('dht20.begin();', '')
# LED der Hersteller-Demo nicht schalten; wir benötigen keine Demo-Buttons.
# Ein sauberer, nichtblockierender Loop ersetzt die Demo-Schleife.
match = re.search(r'\bvoid\s+loop\s*\(\s*\)\s*\{', s)
if not match:
    sys.exit('FEHLER: loop() nicht gefunden.')
level = 1
end = match.end()
while end < len(s) and level:
    if s[end] == '{': level += 1
    elif s[end] == '}': level -= 1
    end += 1
if level: sys.exit('FEHLER: loop-Klammern nicht geschlossen.')
s = s[:match.start()] + """void loop() {
  filament_ui_poll();
  lv_timer_handler();
  delay(5);
}
""" + s[end:]
if 'filament_ui_begin();' not in s or 'lv_timer_handler();' not in s:
    sys.exit('FEHLER: Inkompletter Patch.')
source.write_text(s, encoding='utf-8')
print('Elecrow V3.0 mit FilamentRecycler-LVGL-UI gepatcht.')
