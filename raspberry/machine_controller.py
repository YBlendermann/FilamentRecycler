#!/usr/bin/env python3
"""FilamentRecycler V0.4 - Raspberry Pi machine controller (TEST / NO OUTPUTS).
USB ASCII/LF 115200: Mega and CrowPanel. Settings persisted only on Raspberry Pi.
"""
import argparse
import json
import math
import os
import time
from pathlib import Path

import serial
from serial.tools import list_ports

DEFAULT = {
    "temperature_setpoints": [175.0, 190.0, 205.0],
    "pid": [[5.0, 0.10, 15.0], [5.0, 0.10, 15.0], [5.0, 0.10, 15.0]],
    "motor_rpm": 12.5,
    "filament_target_mm": 1.75,
}
BAUD = 115200


def find_port(vid, pid):
    ports = [p.device for p in list_ports.comports() if (p.vid, p.pid) == (vid, pid)]
    if len(ports) != 1:
        raise RuntimeError(f"Expected exactly one {vid:04x}:{pid:04x}, got {ports}")
    return ports[0]


def load_config(path):
    config = json.loads(json.dumps(DEFAULT))
    if path.exists():
        source = json.loads(path.read_text(encoding="utf-8"))
        for key in DEFAULT:
            if key in source:
                config[key] = source[key]
    # Validate persisted data too. Never trust a modified/corrupted file.
    if len(config['temperature_setpoints']) != 3 or len(config['pid']) != 3:
        raise ValueError('Invalid configuration dimensions')
    for v in config['temperature_setpoints']:
        numeric(v, 0, 250)
    for zone in config['pid']:
        if len(zone) != 3: raise ValueError('Invalid PID zone')
        for v, hi in zip(zone, [100, 10, 200]): numeric(v, 0, hi)
    numeric(config['motor_rpm'], 0, 150)
    numeric(config['filament_target_mm'], 1, 3)
    return config


def numeric(value, low, high):
    v = float(value)
    if not math.isfinite(v) or not low <= v <= high:
        raise ValueError(f'value {value} outside {low}..{high}')
    return v


def save_config(path, config):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix('.tmp')
    with temp.open('w', encoding='utf-8') as fh:
        json.dump(config, fh, ensure_ascii=False, indent=2)
        fh.write('\n')
        fh.flush()
        os.fsync(fh.fileno())
    temp.replace(path)


def send(port, line):
    port.write((line + '\n').encode('ascii'))


class Controller:
    def __init__(self, mega, hmi, config_path):
        self.mega = mega
        self.hmi = hmi
        self.path = config_path
        self.cfg = load_config(config_path)
        self.temps = [None] * 3
        self.mega_online = False
        self.mode = 'READY'  # simulation only
        self.last_temp = 0.0
        self.last_poll = 0.0
        self.last_push = 0.0
        self.last_ping = 0.0
        self.pending = []
        self.last_ident = 0.0
        self.last_config_send = 0.0
        self.motor_test_active = False
        self.motor_step_hz = 200
        self.motor_forward = True
        self.last_motor_keep = 0.0
        self.last_motor_status = 0.0

    def sync_mega(self):
        # Mega firmware accepts configuration ONLY; transmit gradually to avoid AVR UART overflow.
        self.pending.clear()
        for i, sp in enumerate(self.cfg['temperature_setpoints']):
            self.pending.append(f'CFG,TEMP,{i},{sp:.1f}')
            for label, value in zip(['KP', 'KI', 'KD'], self.cfg['pid'][i]):
                self.pending.append(f'CFG,PID,{i},{label},{value:.3f}')
        self.pending.append(f'CFG,MOTOR,RPM,{self.cfg["motor_rpm"]:.2f}')
        self.pending.append(f'CFG,FILAMENT,TARGET,{self.cfg["filament_target_mm"]:.3f}')

    def stop_motor_test(self):
        # Stop regardless of our cached running state.
        send(self.mega, 'MTEST,STOP')
        self.motor_test_active = False

    def publish_motor(self):
        send(self.hmi, f'MOTOR,TEST,{int(self.motor_test_active)},{self.motor_step_hz},{int(self.motor_forward)}')

    def publish(self):
        t = ','.join('ERR' if x is None else f'{x:.1f}' for x in self.temps)
        sp = ','.join(f'{x:.1f}' for x in self.cfg['temperature_setpoints'])
        send(self.hmi, f'STATE,{self.mode},{int(self.mega_online)},{t},{sp},{self.cfg["motor_rpm"]:.2f},ERR,{self.cfg["filament_target_mm"]:.3f}')
        for i, values in enumerate(self.cfg['pid']):
            send(self.hmi, f'PID,{i},{values[0]:.3f},{values[1]:.3f},{values[2]:.3f}')
        self.publish_motor()

    def process_mega(self, line):
        if line.startswith('ID,RAMPS_CONFIG,0.4'):
            was_offline = not self.mega_online
            self.mega_online = True
            self.last_ident = time.monotonic()
            if was_offline:
                self.sync_mega()
                print('Mega connected; settings synchronized', flush=True)
        elif line.startswith('TEMP,'):
            parts = line.split(',')
            if len(parts) != 4: return
            vals = []
            for field in parts[1:]:
                if field == 'ERR': vals.append(None)
                else:
                    try: vals.append(numeric(field, -20, 300))
                    except ValueError: return
            self.temps = vals
            self.last_temp = time.monotonic()
            self.mega_online = True
        elif line.startswith('MOTOR,TEST,'):
            p = line.split(',')
            if len(p) == 5:
                try:
                    self.motor_test_active = (p[2] == '1')
                    self.motor_step_hz = int(p[3])
                    self.motor_forward = (p[4] == '1')
                    self.last_motor_status = time.monotonic()
                except ValueError:
                    pass
        elif line.startswith('ACK,MTEST,TIMEOUT'):
            self.motor_test_active = False
        elif line.startswith('ACK,') or line.startswith('ERR,'):
            print('Mega:', line, flush=True)

    def process_hmi(self, line):
        if line == 'HELLO' or line == 'GET_STATE':
            self.publish()
            return
        parts = line.split(',')
        try:
            if len(parts) >= 2 and parts[0] == 'MTEST':
                command = parts[1]
                if command == 'STOP' and len(parts) == 2:
                    self.stop_motor_test()
                    send(self.hmi, 'ACK,MTEST,STOP')
                elif command == 'KEEP' and len(parts) == 2:
                    if self.motor_test_active:
                        self.last_motor_keep = time.monotonic()
                        send(self.mega, 'MTEST,KEEP')
                elif command == 'START' and len(parts) == 4:
                    if not self.mega_online or self.mode != 'READY' or self.motor_test_active:
                        raise ValueError('test not allowed')
                    hz = int(parts[2]); direction = int(parts[3])
                    if not 50 <= hz <= 800 or direction not in (0, 1):
                        raise ValueError('invalid test setting')
                    self.motor_step_hz = hz
                    self.motor_forward = bool(direction)
                    self.motor_test_active = True
                    self.last_motor_keep = time.monotonic()
                    send(self.mega, f'MTEST,START,{hz},{direction}')
                    send(self.hmi, 'ACK,MTEST,START_SENT')
                else:
                    raise ValueError('unknown motor test command')
                self.publish_motor()
                return
            if len(parts) >= 2 and parts[0] == 'ACTION':
                action = parts[1]
                if action == 'START' and self.mode in ('READY', 'PAUSED') and not self.motor_test_active:
                    self.mode = 'PREHEAT'  # simulated status, no actuation
                elif action == 'PAUSE' and self.mode == 'PREHEAT':
                    self.mode = 'PAUSED'
                elif action == 'STOP':
                    self.mode = 'READY'
                else:
                    raise ValueError('action not allowed')
                send(self.hmi, 'ACK,ACTION')
            elif parts[0] == 'SET' and len(parts) >= 4:
                if parts[1] == 'TEMP' and len(parts) == 4:
                    i = int(parts[2]); assert 0 <= i < 3
                    self.cfg['temperature_setpoints'][i] = numeric(parts[3], 0, 250)
                elif parts[1] == 'PID' and len(parts) == 5:
                    i = int(parts[2]); assert 0 <= i < 3
                    name = parts[3]
                    axis = {'KP': 0, 'KI': 1, 'KD': 2}[name]
                    self.cfg['pid'][i][axis] = numeric(parts[4], 0, [100, 10, 200][axis])
                elif parts[1:3] == ['MOTOR', 'RPM'] and len(parts) == 4:
                    self.cfg['motor_rpm'] = numeric(parts[3], 0, 150)
                elif parts[1:3] == ['FILAMENT', 'TARGET'] and len(parts) == 4:
                    self.cfg['filament_target_mm'] = numeric(parts[3], 1, 3)
                else:
                    raise ValueError('invalid SET command')
                save_config(self.path, self.cfg)
                if self.mega_online: self.sync_mega()
                send(self.hmi, 'ACK,SAVED')
            else:
                raise ValueError('unknown command')
        except (AssertionError, ValueError, KeyError, IndexError) as exc:
            print('Rejected HMI command:', repr(line), repr(exc), flush=True)
            send(self.hmi, 'ERR,BAD_COMMAND')
        self.publish()

    def loop(self):
        self.stop_motor_test()
        send(self.mega, 'HELLO')
        send(self.hmi, 'HELLO')
        while True:
            now = time.monotonic()
            if now - self.last_ping >= 3:
                send(self.mega, 'HELLO')
                self.last_ping = now
            if now - self.last_poll >= .5:
                send(self.mega, 'GET_TEMP')
                send(self.mega, 'MTEST,STATUS')
                self.last_poll = now
            if self.motor_test_active and (now - self.last_motor_keep > 1.1 or now - self.last_motor_status > 2.0):
                self.stop_motor_test()
            if now - self.last_push >= .5:
                self.publish()
                self.last_push = now
            if self.pending and now - self.last_config_send >= .08:
                send(self.mega, self.pending.pop(0))
                self.last_config_send = now
            mega_line = self.mega.readline().decode('ascii', errors='replace').strip()
            if mega_line: self.process_mega(mega_line)
            hmi_line = self.hmi.readline().decode('ascii', errors='replace').strip()
            if hmi_line: self.process_hmi(hmi_line)
            if self.last_temp and now - self.last_temp > 2.0:
                self.temps = [None] * 3
                self.mega_online = False
                self.mode = 'READY'
                if self.motor_test_active: self.stop_motor_test()
            time.sleep(.01)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mega', default=None)
    parser.add_argument('--display', default=None)
    parser.add_argument('--config', default=str(Path(__file__).with_name('machine_config.json')))
    args = parser.parse_args()
    mega_port = args.mega or find_port(0x2341, 0x0042)
    hmi_port = args.display or find_port(0x1a86, 0x7523)
    print('Mega:', mega_port, 'Display:', hmi_port, flush=True)
    with serial.Serial(mega_port, BAUD, timeout=.02, write_timeout=1) as mega, \
         serial.Serial(hmi_port, BAUD, timeout=.02, write_timeout=1) as hmi:
        time.sleep(2.5)  # Mega bootloader/reset
        mega.reset_input_buffer()
        hmi.reset_input_buffer()
        Controller(mega, hmi, Path(args.config)).loop()


if __name__ == '__main__':
    try: main()
    except KeyboardInterrupt: print('\nStopped')
