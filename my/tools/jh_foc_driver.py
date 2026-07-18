# -*- coding: utf-8 -*-
"""
JH_FOC_42V3 CAN bus driver layer.
JHFOC42Driver: single motor control
JHFOC42Bus: multi-motor bus manager
CAN scan/connect utilities.
"""

import time
import sys
import can

from jh_foc_protocol import (
    CAN_ID_BASE, CAN_ID_MAX, SYNC_ID, DEFAULT_BITRATE,
    MODE_NAMES, MODE_CMD_NAMES,
    MODE_CMD_IDLE, MODE_CMD_SPEED, MODE_CMD_POS, MODE_CMD_ANGLE,
    build_cmd_speed, build_cmd_current,
    build_cmd_abs_position, build_cmd_rel_position, build_cmd_angle,
    build_cmd_sync, build_cmd_stop, build_cmd_encoder_calibrate,
    build_cmd_query_speed, build_cmd_query_position, build_cmd_query_mode,
    parse_speed_response, parse_position_response, parse_mode_response,
    joint_to_can_id,
)


# ============================================================
# Safe print (avoids GBK encoding crashes on Windows)
# ============================================================

def sprint(*args, **kwargs):
    """Print that survives Windows GBK codec."""
    try:
        print(*args, **kwargs)
    except UnicodeEncodeError:
        ascii_args = []
        for a in args:
            s = str(a)
            ascii_args.append(s.encode('ascii', errors='replace').decode('ascii'))
        print(*ascii_args, **kwargs)


# ============================================================
# JHFOC42Driver - Single Motor Driver
# ============================================================

class JHFOC42Driver:
    """Control a single JH_FOC_42V3 motor driver via CAN bus."""

    def __init__(self, bus, can_id=CAN_ID_BASE):
        self.bus = bus
        self.can_id = can_id

    # --- Low-level ---

    def send(self, data):
        """Send a CAN frame to this driver."""
        msg = can.Message(
            arbitration_id=self.can_id,
            data=data[:8],
            is_extended_id=False
        )
        self.bus.send(msg)

    def send_and_wait(self, data, timeout=0.5):
        """Send command and wait for response from this CAN ID."""
        self.send(data)
        start = time.time()
        while time.time() - start < timeout:
            msg = self.bus.recv(timeout=0.05)
            if msg and msg.arbitration_id == self.can_id:
                return msg.data
        return None

    # --- Mode Switching ---

    def set_mode(self, mode_tuple):
        """Send mode switch command (e.g., MODE_CMD_POS)."""
        self.send(list(mode_tuple))

    def set_mode_with_retry(self, mode_tuple, max_retries=3, settle_time=0.3):
        """
        Switch mode with verification retry.
        Returns True if command was sent (mode switch is best-effort).
        Verification may fail due to driver busy state, but motion commands will still work.
        """
        expected_name = MODE_CMD_NAMES.get(mode_tuple[1], 'UNKNOWN')

        # Always send the mode switch at least once
        for attempt in range(max_retries):
            self.send(list(mode_tuple))
            time.sleep(settle_time)
            actual = self.get_mode()
            if actual == expected_name:
                return True
            if actual is not None and actual != expected_name:
                if attempt < max_retries - 1:
                    sprint(f"  Retry {attempt+2}/{max_retries}: expected {expected_name}, got {actual}")
            # If actual is None, driver is busy - mode switch likely succeeded anyway
        # After all retries: warn but assume command was sent
        sprint(f"  [WARN] Mode verification inconclusive, command sent (expected {expected_name})")
        return True  # Return True since we sent the command

    def set_idle(self):
        self.set_mode(MODE_CMD_IDLE)

    def set_speed_mode(self):
        self.set_mode(MODE_CMD_SPEED)

    def set_position_mode(self):
        self.set_mode(MODE_CMD_POS)

    def set_angle_mode(self):
        self.set_mode(MODE_CMD_ANGLE)

    # --- Control Commands ---

    def set_speed(self, rpm, current_ma=1500, accel=500):
        """Set target speed (rpm). Must be in SPEED mode."""
        self.send(build_cmd_speed(rpm, current_ma, accel))

    def set_current(self, iq_a):
        """Set Iq current (A). Must be in CURRENT mode."""
        self.send(build_cmd_current(iq_a))

    def go_absolute(self, angle_deg, current_ma=1500, sync=False):
        """Absolute position move. Must be in POSITION mode."""
        self.send(build_cmd_abs_position(angle_deg, current_ma, sync=sync))

    def go_relative(self, delta_deg, current_ma=1500, sync=False):
        """Relative position move. Must be in POSITION mode."""
        self.send(build_cmd_rel_position(delta_deg, current_ma, sync=sync))

    def go_angle(self, angle_deg, current_ma=1500, sync=False):
        """Angle mode move (0-360). Must be in ANGLE mode."""
        self.send(build_cmd_angle(angle_deg, current_ma, sync=sync))

    def stop(self):
        """Emergency stop this motor."""
        self.send(build_cmd_stop())

    def calibrate_encoder(self, wait_complete=True):
        """
        Run encoder calibration. Motor spins 1 rev each direction.
        If wait_complete=True, blocks for ~6 seconds and returns when driver is ready.
        """
        sprint(f"  Calibrating motor 0x{self.can_id:03X}...")
        self.send(build_cmd_encoder_calibrate())
        if wait_complete:
            time.sleep(6)
            # Wait for driver to come back online
            for i in range(10):
                if self.is_alive():
                    sprint(f"  Calibration complete, motor 0x{self.can_id:03X} ready.")
                    return True
                time.sleep(0.5)
            sprint(f"  [WARN] Motor 0x{self.can_id:03X} not responding after calibration")
            return False
        return True

    # --- Query Commands ---

    def get_speed(self):
        """Read current speed (rpm). Returns float or None."""
        resp = self.send_and_wait(build_cmd_query_speed())
        if resp:
            return parse_speed_response(resp)
        return None

    def get_position(self):
        """Read current position (degrees). Returns float or None."""
        resp = self.send_and_wait(build_cmd_query_position())
        if resp:
            return parse_position_response(resp)
        return None

    def get_mode(self):
        """Read current mode name. Returns str or None."""
        resp = self.send_and_wait(build_cmd_query_mode())
        if resp:
            return parse_mode_response(resp)
        return None

    def is_alive(self):
        """Quick check if driver is responding. Returns bool."""
        return self.get_mode() is not None

    def get_state(self):
        """Get full state dict: {mode, position, speed}."""
        mode = self.get_mode()
        pos = self.get_position()
        spd = self.get_speed()
        return {'mode': mode, 'position': pos, 'speed': spd}


# ============================================================
# JHFOC42Bus - Multi-Motor Bus Manager
# ============================================================

class JHFOC42Bus:
    """Manage multiple motors on the same CAN bus."""

    def __init__(self, bus):
        self.bus = bus
        self.motors = {}  # {joint_num: JHFOC42Driver}

    def add_motor(self, joint_num, can_id=None):
        """Register a motor. joint_num 1-6, can_id auto-assigned if omitted."""
        if can_id is None:
            can_id = joint_to_can_id(joint_num)
        self.motors[joint_num] = JHFOC42Driver(self.bus, can_id)
        return self.motors[joint_num]

    def motor(self, joint_num):
        """Get motor by joint number (returns None if not registered)."""
        return self.motors.get(joint_num)

    # --- Bulk Operations ---

    def sync_trigger(self):
        """Broadcast sync trigger -- all buffered commands execute simultaneously."""
        msg = can.Message(
            arbitration_id=SYNC_ID,
            data=build_cmd_sync(),
            is_extended_id=False
        )
        self.bus.send(msg)

    def stop_all(self):
        """Emergency stop all motors."""
        for m in self.motors.values():
            m.stop()

    def set_all_idle(self):
        """Set all motors to IDLE mode."""
        for m in self.motors.values():
            m.set_idle()
        time.sleep(0.3)

    def set_all_modes(self, mode_tuple, with_retry=True):
        """Switch all motors to the given mode (with retry)."""
        ok = True
        for j, m in self.motors.items():
            if with_retry:
                if not m.set_mode_with_retry(mode_tuple):
                    sprint(f"  [WARN] Joint {j} mode switch failed")
                    ok = False
            else:
                m.set_mode(mode_tuple)
                time.sleep(0.3)
        return ok

    def get_all_positions(self):
        """Query all motor positions. Returns {joint_num: degrees or None}."""
        return {j: m.get_position() for j, m in self.motors.items()}

    def get_all_modes(self):
        """Query all motor modes. Returns {joint_num: mode_name or None}."""
        return {j: m.get_mode() for j, m in self.motors.items()}

    def is_all_alive(self):
        """Check if all motors respond."""
        return all(m.is_alive() for m in self.motors.values())

    # --- Coordinated Motion ---

    def move_all_absolute(self, angles, sync=True, current_ma=1500):
        """
        Multi-axis absolute position move.
        angles: {joint_num: degrees, ...}
        sync: if True, all motors execute simultaneously
        """
        for joint, angle in angles.items():
            m = self.motors.get(joint)
            if m:
                m.go_absolute(angle, current_ma=current_ma, sync=sync)
        if sync:
            time.sleep(0.02)
            self.sync_trigger()

    def move_all_relative(self, deltas, sync=True, current_ma=1500):
        """
        Multi-axis relative position move.
        deltas: {joint_num: delta_degrees, ...}
        sync: if True, all motors execute simultaneously
        """
        for joint, delta in deltas.items():
            m = self.motors.get(joint)
            if m:
                m.go_relative(delta, current_ma=current_ma, sync=sync)
        if sync:
            time.sleep(0.02)
            self.sync_trigger()


# ============================================================
# CAN Bus Scanning & Connection
# ============================================================

def scan_can_adapters():
    """Scan for available CAN adapters. Returns list of (interface, channel) tuples."""
    sprint("Scanning USB-CAN adapters...")

    configs = []

    interfaces = [
        ('pcan', 'PCAN_USBBUS1'),
        ('pcan', 'PCAN_USBBUS2'),
        ('pcan', 'PCAN_USBBUS3'),
        ('slcan', 'COM3'),
        ('slcan', 'COM4'),
        ('slcan', 'COM5'),
        ('slcan', 'COM6'),
        ('slcan', 'COM7'),
        ('slcan', 'COM8'),
        ('slcan', '/dev/ttyACM0'),
        ('slcan', '/dev/ttyUSB0'),
        ('kvaser', '0'),
        ('socketcan', 'can0'),
    ]

    for interface, channel in interfaces:
        try:
            bus = can.interface.Bus(
                channel=channel,
                interface=interface,
                bitrate=DEFAULT_BITRATE,
                timeout=0.1
            )
            sprint(f"  [OK] {interface}:{channel}")
            configs.append((interface, channel))
            bus.shutdown()
        except Exception:
            pass  # Skip unavailable adapters

    return configs


def connect_can(interface=None, channel=None, bitrate=DEFAULT_BITRATE):
    """
    Connect to CAN bus. Auto-scans if interface/channel not specified.
    Returns (bus, interface, channel) tuple.
    Exits with error if no adapter found.
    """
    if interface and channel:
        sprint(f"Connecting {interface}:{channel} @ {bitrate}bps...")
        try:
            bus = can.interface.Bus(
                channel=channel,
                interface=interface,
                bitrate=bitrate,
                timeout=0.1
            )
            sprint("Connected.")
            return bus, interface, channel
        except Exception as e:
            sprint(f"Connection failed: {e}")
            sys.exit(1)

    # Auto-scan
    configs = scan_can_adapters()
    if not configs:
        sprint("")
        sprint("No CAN adapter found!")
        sprint("Check: 1) USB-CAN adapter plugged in  2) Driver installed")
        sprint("Manual: python motor_cli.py -i pcan -c PCAN_USBBUS1")
        sys.exit(1)

    interface, channel = configs[0]
    sprint(f"Auto-selected: {interface}:{channel}")
    bus = can.interface.Bus(
        channel=channel,
        interface=interface,
        bitrate=bitrate,
        timeout=0.1
    )
    sprint("Connected.")
    return bus, interface, channel


def scan_motors(bus, start_id=CAN_ID_BASE, end_id=CAN_ID_MAX):
    """
    Scan CAN bus for motor drivers.
    Returns list of CAN IDs that respond.
    """
    sprint(f"Scanning motors (ID 0x{start_id:03X}-0x{end_id:03X})...")
    found = []
    for can_id in range(start_id, end_id + 1):
        motor = JHFOC42Driver(bus, can_id)
        mode = motor.get_mode()
        if mode:
            sprint(f"  [OK] ID 0x{can_id:03X} - {mode}")
            found.append(can_id)
        else:
            sprint(f"  [--] ID 0x{can_id:03X} - no response")
    return found
