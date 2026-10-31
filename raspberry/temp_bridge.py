#!/usr/bin/env python3
"""FilamentRecycler V0.2 - USB-Telemetriebruecke, nur Anzeige.
Mega: GET_TEMP -> TEMP,t1,t2,t3; CrowPanel: TEMP,t1,t2,t3; OFFLINE.
"""
import argparse
import glob
import time
import serial
from serial.tools import list_ports

BAUD = 115200

def port_for(vid, pid):
    matches = [p.device for p in list_ports.comports() if (p.vid, p.pid) == (vid, pid)]
    if len(matches) != 1:
        raise RuntimeError(f"USB-Gerät {vid:04x}:{pid:04x}: {len(matches)} Treffer {matches} (genau 1 erwartet)")
    return matches[0]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mega', default=None)
    parser.add_argument('--display', default=None)
    args = parser.parse_args()
    mega_port = args.mega or port_for(0x2341, 0x0042)
    display_port = args.display or port_for(0x1A86, 0x7523)
    print(f'Mega: {mega_port} / CrowPanel: {display_port}', flush=True)
    with serial.Serial(mega_port, BAUD, timeout=0.15, write_timeout=1) as mega, \
         serial.Serial(display_port, BAUD, timeout=0.02, write_timeout=1) as display:
        # Mega wird beim Oeffnen der seriellen Schnittstelle i.d.R. resettet.
        time.sleep(2.5)
        mega.reset_input_buffer()
        display.reset_input_buffer()
        mega.write(b'HELLO\n')
        ident = mega.readline().decode('ascii', errors='replace').strip()
        if ident != 'ID,RAMPS_TEMP,0.2':
            raise RuntimeError('Falsche Mega-Firmware oder kein HELLO: ' + repr(ident))
        print('Mega-Firmware erkannt:', ident, flush=True)
        next_poll = 0
        last_valid = 0
        offline_sent = False
        while True:
            now = time.monotonic()
            if now >= next_poll:
                mega.write(b'GET_TEMP\n')
                next_poll = now + 0.5
            # Lies eine komplette Antwort; fremde Zeilen ignorieren.
            raw = mega.readline()
            if raw:
                msg = raw.decode('ascii', errors='replace').strip()
                parts = msg.split(',')
                if len(parts) == 4 and parts[0] == 'TEMP':
                    try:
                        if any(v != 'ERR' for v in parts[1:]):
                            for v in parts[1:]:
                                if v != 'ERR':
                                    f = float(v)
                                    if not -20 <= f <= 300: raise ValueError('Bereich')
                        last_valid = time.monotonic()
                        display.write((msg + '\n').encode('ascii'))
                        print(msg, flush=True)
                        offline_sent = False
                    except (ValueError, OverflowError):
                        print('Ungültige Temperaturzeile:', repr(msg), flush=True)
            if (not last_valid or time.monotonic() - last_valid > 2.0) and not offline_sent:
                display.write(b'OFFLINE\n')
                offline_sent = True
            time.sleep(0.01)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('\nBeendet')
