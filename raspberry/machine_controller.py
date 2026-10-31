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
    "motor_microsteps": 16,
    "motor_steps_per_rev": 200,
    "motor_gear_ratio": 20.0,
    "screw_pitch_mm": 0.0,  # 0: unknown; calculated axial advance hidden
    "motor_ramp_hz_s": 500,
    "motor_test_step_hz": 200,
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
    if int(config['motor_microsteps']) not in (1, 2, 4, 8, 16, 32, 64, 128, 256):
        raise ValueError('Invalid microstep configuration')
    numeric(config['motor_steps_per_rev'], 100, 400)
    numeric(config['motor_gear_ratio'], 1, 200)
    numeric(config['screw_pitch_mm'], 0, 30)
    numeric(config['motor_ramp_hz_s'], 100, 5000)
    if int(config['motor_test_step_hz']) not in range(50, 2501, 50):
        raise ValueError('Invalid motor test step frequency')
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
        self.motor_step_hz = int(self.cfg["motor_test_step_hz"])
        self.motor_forward = True
        self.motor_current_hz = 0
        self.motor_jog = False
        self.last_motor_keep = 0.0
        self.last_motor_status = 0.0
        self.awaiting_motor_start_until = 0.0

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
        self.motor_current_hz = 0
        self.motor_jog = False
        self.awaiting_motor_start_until = 0.0

    def publish_motor(self):
        rpm = self.motor_current_hz * 60.0 / (self.cfg['motor_steps_per_rev'] * self.cfg['motor_microsteps'] * self.cfg['motor_gear_ratio'])
        pitch = self.cfg['screw_pitch_mm']
        advance = rpm * pitch if pitch > 0 else None
        extra = f'{rpm:.4f},{advance:.3f}' if advance is not None else f'{rpm:.4f},ERR'
        send(self.hmi, f'MOTOR,TEST,{int(self.motor_test_active)},{self.motor_step_hz},{int(self.motor_forward)},{self.motor_current_hz},{int(self.motor_jog)},{self.cfg["motor_ramp_hz_s"]},{extra}')

    def publish(self):
        t = ','.join('ERR' if x is None else f'{x:.1f}' for x in self.temps)
        sp = ','.join(f'{x:.1f}' for x in self.cfg['temperature_setpoints'])
        send(self.hmi, f'STATE,{self.mode},{int(self.mega_online)},{t},{sp},{self.cfg["motor_rpm"]:.2f},ERR,{self.cfg["filament_target_mm"]:.3f}')
        for i, values in enumerate(self.cfg['pid']):
            send(self.hmi, f'PID,{i},{values[0]:.3f},{values[1]:.3f},{values[2]:.3f}')
        send(self.hmi, f'MOTOR,CFG,{self.cfg["motor_microsteps"]},{self.cfg["motor_steps_per_rev"]},{self.cfg["motor_gear_ratio"]:.2f},{self.cfg["screw_pitch_mm"]:.2f},{self.cfg["motor_ramp_hz_s"]}')
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
            if len(p) == 8:
                try:
                    running = (p[2] == '1')
                    # A queued pre-START idle report must not cancel a just-sent
                    # START. Otherwise periodic STATUS traffic drops KEEP heartbeats.
                    if not running and time.monotonic() < self.awaiting_motor_start_until:
                        return
                    if running:
                        self.awaiting_motor_start_until = 0.0
                        self.motor_step_hz = int(p[3])
                        self.motor_forward = (p[4] == '1')
                    # Idle status from Mega must never overwrite HMI target STEP/s.
                    self.motor_test_active = running
                    self.motor_current_hz = int(p[5])
                    self.motor_jog = (p[6] == '1')
                    self.last_motor_status = time.monotonic()
                except ValueError:
                    pass
        elif line.startswith(('ACK,MTEST,TIMEOUT', 'ACK,MTEST,LIMIT', 'ACK,MTEST,WATCHDOG', 'ACK,MTEST,LOOP_STALL', 'ERR,MTEST,START')):
            self.motor_test_active = False
            self.motor_current_hz = 0
            self.awaiting_motor_start_until = 0.0
            print('Mega stopped motor:', line, flush=True)
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
                elif command in ('KEEP', 'HOLD') and len(parts) == 2:
                    if self.motor_test_active and ((command == 'HOLD') == self.motor_jog):
                        self.last_motor_keep = time.monotonic()
                        send(self.mega, 'MTEST,' + command)
                elif command == 'SET_HZ' and len(parts) == 3:
                    if self.motor_test_active:
                        raise ValueError('cannot change speed during run')
                    hz = int(parts[2])
                    if not 50 <= hz <= 2500 or hz % 50:
                        raise ValueError('invalid STEP/s')
                    self.motor_step_hz = hz
                    self.cfg['motor_test_step_hz'] = hz
                    save_config(self.path, self.cfg)
                    send(self.hmi, 'ACK,MTEST,SET_HZ')
                elif command == 'START' and len(parts) == 6:
                    if not self.mega_online or self.mode != 'READY' or self.motor_test_active:
                        raise ValueError('test not allowed')
                    hz = int(parts[2]); direction = int(parts[3])
                    accel = int(parts[4]); jog = int(parts[5])
                    if not 50 <= hz <= 2500 or direction not in (0, 1) or not 100 <= accel <= 5000 or jog not in (0, 1):
                        raise ValueError('invalid test setting')
                    self.motor_step_hz = hz
                    self.cfg['motor_test_step_hz'] = hz
                    save_config(self.path, self.cfg)
                    self.motor_forward = bool(direction)
                    self.motor_test_active = True
                    self.motor_jog = bool(jog)
                    self.motor_current_hz = 0
                    self.last_motor_keep = time.monotonic()
                    self.last_motor_status = time.monotonic()
                    self.awaiting_motor_start_until = time.monotonic() + 0.8
                    send(self.mega, f'MTEST,START,{hz},{direction},{accel},{jog}')
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
                elif parts[1:3] == ['MOTOR', 'RAMP'] and len(parts) == 4:
                    self.cfg['motor_ramp_hz_s'] = int(numeric(parts[3], 100, 5000))
                elif parts[1:3] == ['MOTOR', 'MICROSTEPS'] and len(parts) == 4:
                    val = int(parts[3])
                    if val not in (1, 2, 4, 8, 16, 32, 64, 128, 256): raise ValueError('invalid microsteps')
                    self.cfg['motor_microsteps'] = val
                elif parts[1:3] == ['MOTOR', 'PITCH'] and len(parts) == 4:
                    self.cfg['screw_pitch_mm'] = numeric(parts[3], 0, 30)
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
            if self.motor_test_active and (now - self.last_motor_keep > (0.65 if self.motor_jog else 1.1) or now - self.last_motor_status > 2.0):
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
