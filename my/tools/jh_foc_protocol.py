# -*- coding: utf-8 -*-
"""
JH_FOC_42V3 CAN bus protocol layer.
Pure data: command builders, response parsers, constants.
Zero dependencies beyond stdlib.
"""

import struct

# ============================================================
# Constants
# ============================================================

# CAN ID range for 6-axis robotic arm
CAN_ID_BASE = 0x200       # Joint 1 (Base)
CAN_ID_MAX  = 0x205       # Joint 6 (Wrist3)
SYNC_ID     = 0x200       # Broadcast sync trigger ID

# Baud rate
DEFAULT_BITRATE = 500000

# Mode identifiers (byte[1] of mode-switch command)
MODE_IDLE    = 0x11
MODE_CURRENT = 0x22
MODE_SPEED   = 0x33
MODE_POS     = 0x44
MODE_ANGLE   = 0x55

# Mode switch tuples (3 bytes sent as [0x23, mode, 0x00])
MODE_CMD_IDLE    = (0x23, 0x11, 0x00)
MODE_CMD_CURRENT = (0x23, 0x22, 0x00)
MODE_CMD_SPEED   = (0x23, 0x33, 0x00)
MODE_CMD_POS     = (0x23, 0x44, 0x00)
MODE_CMD_ANGLE   = (0x23, 0x55, 0x00)

# Mode name lookup (response byte[7] -> name)
MODE_NAMES = {
    0x00: 'IDLE',
    0x11: 'CURRENT',
    0x22: 'SPEED',
    0x33: 'POSITION',
    0x44: 'ANGLE',
}

# Mode switch command byte[1] -> expected mode name (for retry verification)
# Note: switch byte != response byte for POSITION and ANGLE!
MODE_CMD_NAMES = {
    0x11: 'IDLE',
    0x22: 'CURRENT',
    0x33: 'SPEED',
    0x44: 'POSITION',
    0x55: 'ANGLE',
}

# Joint mapping
JOINT_NAMES = {
    1: 'Base',
    2: 'Shoulder',
    3: 'Elbow',
    4: 'Wrist1',
    5: 'Wrist2',
    6: 'Wrist3',
}


def joint_to_can_id(joint_num):
    """Convert joint number (1-6) to CAN ID (0x200-0x205)."""
    if not 1 <= joint_num <= 6:
        raise ValueError(f"Joint number must be 1-6, got {joint_num}")
    return 0x1FF + joint_num


# ============================================================
# Command Builders
# ============================================================

def build_cmd_speed(rpm, current_ma=1500, accel=500):
    """
    Build speed-mode setpoint command.
    rpm: target speed (-3000 .. 3000)
    current_ma: current limit in mA (0 .. 3000)
    accel: acceleration (1 .. 10000)
    Returns 8-byte list.
    """
    rpm = max(-3000, min(3000, rpm))
    current_raw = max(0, min(3000, current_ma))
    accel = max(1, min(10000, accel))
    return [
        0x33, 0x00,
        (accel >> 8) & 0xFF, accel & 0xFF,
        (current_raw >> 8) & 0xFF, current_raw & 0xFF,
        (rpm >> 8) & 0xFF, rpm & 0xFF
    ]


def build_cmd_current(iq_a):
    """Build current-mode Iq setpoint (A)."""
    iq_raw = int(iq_a * 100)
    return [0x44, 0x00, 0, 0, 0, 0,
            (iq_raw >> 8) & 0xFF, iq_raw & 0xFF]


def build_cmd_abs_position(angle_deg, current_ma=1500, max_speed=120, sync=False):
    """
    Build absolute position move command.
    angle_deg: target angle in degrees
    current_ma: current limit (mA)
    max_speed: max speed (rpm) -- NOTE: may not be supported by all firmware
    sync: if True, motor buffers command until sync trigger
    Returns 8-byte list.
    """
    sub = 0xAA if sync else 0x00
    angle_raw = int(angle_deg * 100)
    return [
        0x57, sub,
        (current_ma >> 8) & 0xFF, current_ma & 0xFF,
        (angle_raw >> 24) & 0xFF, (angle_raw >> 16) & 0xFF,
        (angle_raw >> 8) & 0xFF, angle_raw & 0xFF
    ]


def build_cmd_rel_position(delta_deg, current_ma=1500, sync=False):
    """
    Build relative position move command.
    delta_deg: relative degrees to move
    current_ma: current limit (mA)
    sync: if True, motor buffers command until sync trigger
    Returns 8-byte list.
    """
    sub = 0xAA if sync else 0x00
    delta_raw = int(delta_deg * 100)
    return [
        0x55, sub,
        (current_ma >> 8) & 0xFF, current_ma & 0xFF,
        (delta_raw >> 24) & 0xFF, (delta_raw >> 16) & 0xFF,
        (delta_raw >> 8) & 0xFF, delta_raw & 0xFF
    ]


def build_cmd_angle(angle_deg, current_ma=1500, sync=False):
    """
    Build angle-mode command (0-360 degree circular).
    angle_deg: target angle 0-360
    Returns 8-byte list.
    """
    sub = 0xAA if sync else 0x00
    angle_raw = int(angle_deg * 100)
    return [
        0x66, sub,
        (current_ma >> 8) & 0xFF, current_ma & 0xFF,
        0, 0,
        (angle_raw >> 8) & 0xFF, angle_raw & 0xFF
    ]


def build_cmd_sync():
    """Build sync trigger command -- fires all buffered sync commands."""
    return [0x99, 0x9A, 0, 0, 0, 0, 0, 0]


def build_cmd_stop():
    """Build emergency stop command."""
    return [0x77, 0x78, 0, 0, 0, 0, 0, 0]


def build_cmd_encoder_calibrate():
    """Build encoder calibration command (motor spins 1 rev each direction)."""
    return [0x11, 0x22, 0, 0, 0, 0, 0, 0]


# ============================================================
# Query Command Builders
# ============================================================

def build_cmd_query_speed():
    """Build speed query command."""
    return [0xC0, 0xA0, 0, 0, 0, 0, 0, 0]


def build_cmd_query_position():
    """Build position query command."""
    return [0xC3, 0xA3, 0, 0, 0, 0, 0, 0]


def build_cmd_query_mode():
    """Build mode query command."""
    return [0xC9, 0xA9, 0, 0, 0, 0, 0, 0]


# ============================================================
# Response Parsers
# ============================================================

def parse_int32_response(data):
    """Parse signed int32 from response bytes 4-7 (big-endian)."""
    return struct.unpack('>i', bytes(data[4:8]))[0]


def parse_speed_response(data):
    """Parse speed response -> rpm (float)."""
    return parse_int32_response(data) / 100.0


def parse_position_response(data):
    """Parse position response -> degrees (float)."""
    return parse_int32_response(data) / 100.0


def parse_mode_response(data):
    """Parse mode response -> mode name string."""
    mode_code = data[7]
    return MODE_NAMES.get(mode_code, f'0x{mode_code:02X}')
