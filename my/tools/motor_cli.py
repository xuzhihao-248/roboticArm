# -*- coding: utf-8 -*-
"""
JH_FOC_42V3 6-Axis Motor Control CLI
=====================================
Terminal-based interactive control tool for the robotic arm joint motors.

Usage:
    python motor_cli.py                         # Interactive mode
    python motor_cli.py --scan                  # Scan adapters + motors, then exit
    python motor_cli.py -i pcan -c PCAN_USBBUS1 # Specify CAN interface
    python motor_cli.py -j 1 --cmd "abs 45"     # Single command, non-interactive

Dependencies: python-can (installed in .venv)
"""

import sys
import os
import time
import argparse
import signal
from cmd import Cmd

# Fix Windows encoding
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass


def sprint(*args, **kwargs):
    """Safe print that survives GBK codec on Windows."""
    try:
        print(*args, **kwargs)
    except UnicodeEncodeError:
        ascii_args = []
        for a in args:
            s = str(a)
            ascii_args.append(s.encode('ascii', errors='replace').decode('ascii'))
        print(*ascii_args, **kwargs)


# Import driver layer (relative to tools dir)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from jh_foc_driver import (
    JHFOC42Bus, JHFOC42Driver,
    scan_can_adapters, connect_can, scan_motors,
    sprint as _driver_sprint,
)
from jh_foc_protocol import (
    MODE_CMD_IDLE, MODE_CMD_SPEED, MODE_CMD_POS, MODE_CMD_ANGLE,
    MODE_NAMES, JOINT_NAMES, joint_to_can_id,
    CAN_ID_BASE,
)

# ============================================================
# Helpers
# ============================================================

def parse_joint_selection(arg):
    """
    Parse joint selection string.
    '1' -> [1]
    '1-3' -> [1, 2, 3]
    'all' -> [1,2,3,4,5,6]
    '1,3,5' -> [1, 3, 5]
    Returns list of ints.
    """
    arg = arg.strip().lower()
    if arg == 'all':
        return list(range(1, 7))
    if '-' in arg and ',' not in arg:
        parts = arg.split('-')
        if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit():
            return list(range(int(parts[0]), int(parts[1]) + 1))
    if ',' in arg:
        return [int(x.strip()) for x in arg.split(',') if x.strip().isdigit()]
    if arg.isdigit():
        n = int(arg)
        return [n] if 1 <= n <= 6 else []
    return []


def fuzzy_number(s):
    """Try to parse a string as float, return None if not a number."""
    try:
        return float(s)
    except ValueError:
        return None


# ============================================================
# CLI Application
# ============================================================

class MotorCLI(Cmd):
    """Interactive CLI for JH_FOC_42V3 motor control."""

    identchars = Cmd.identchars + '-'  # Allow hyphen in command names (test-ptp etc.)

    def parseline(self, line):
        """Override to translate hyphens to underscores in command names."""
        cmd, arg, line = super().parseline(line)
        if cmd:
            cmd = cmd.replace('-', '_')
        return cmd, arg, line

    intro = """
  JH_FOC_42V3 6-Axis Motor Control CLI
  Type 'help' or '?' to list commands.
  Type 'quit' to exit.
"""
    prompt = 'motor> '

    # --------------------------------------------------
    # State
    # --------------------------------------------------
    bus = None
    driver_bus = None       # JHFOC42Bus instance
    active_joints = [1]     # Currently selected joints
    running_test = False    # Set True during automated tests
    _interactive = True     # False when running --cmd (skip confirm prompts)

    # --------------------------------------------------
    # Connection
    # --------------------------------------------------

    def _connect(self, interface=None, channel=None):
        """Lazy connect to CAN bus."""
        if self.bus is not None:
            return True
        try:
            self.bus, iface, chan = connect_can(interface, channel)
            self.driver_bus = JHFOC42Bus(self.bus)
            return True
        except SystemExit:
            return False

    def _ensure_connected(self):
        if self.bus is None:
            sprint("Not connected. Use 'connect' or 'scan' first.")
            return False
        return True

    def _ensure_motors(self):
        if not self._ensure_connected():
            return False
        if not self.driver_bus.motors:
            sprint("No motors registered. Use 'scan' to find motors.")
            return False
        return True

    # --------------------------------------------------
    # cmd.Cmd hooks
    # --------------------------------------------------

    def emptyline(self):
        """Do nothing on empty line (don't repeat last command)."""
        pass

    def precmd(self, line):
        """
        Parse joint prefix notation before each command.
        '1 abs 90' -> 'sel 1; abs 90'
        '1-3 posmode' -> 'sel 1-3; posmode'
        'all estop' -> 'sel all; estop'
        """
        if not line.strip():
            return line
        parts = line.split(None, 1)
        first = parts[0].strip().lower()

        # Check if first token is a joint selector
        joints = parse_joint_selection(first)
        if joints and len(parts) > 1:
            # It's a joint prefix: "1 abs 90"
            self.active_joints = joints
            return 'sel ' + first + '; ' + parts[1]

        return line

    def default(self, line):
        """Handle unknown commands gracefully."""
        if line.strip():
            sprint(f"Unknown command: {line.strip()}")
            sprint("Type 'help' for available commands.")

    # --------------------------------------------------
    # Motor Selection
    # --------------------------------------------------

    def do_sel(self, arg):
        """
        Select active joint(s).
        Usage: sel <joint>   e.g., sel 1   sel 1-3   sel all   sel 1,3,5
        """
        if not arg.strip():
            sprint(f"Active joints: {self.active_joints}")
            return

        joints = parse_joint_selection(arg)
        if joints:
            self.active_joints = joints
            names = [f"J{j}" for j in joints]
            sprint(f"Selected: {', '.join(names)}")
        else:
            sprint(f"Invalid joint: {arg}. Use 1-6, 1-3, all, or 1,3,5")

    def do_scan(self, arg):
        """
        Scan CAN bus for connected motor drivers.
        Usage: scan
        """
        if not self._connect():
            sprint("Failed to connect to CAN bus.")
            return
        found = scan_motors(self.bus)
        if not found:
            sprint("No motors found. Check power and CAN wiring.")
            return
        # Register found motors
        for can_id in found:
            joint = can_id - 0x1FF
            if 1 <= joint <= 6 and joint not in self.driver_bus.motors:
                self.driver_bus.add_motor(joint, can_id)
                sprint(f"  Registered: Joint {joint} ({JOINT_NAMES.get(joint, '?')}) at 0x{can_id:03X}")
        sprint(f"Total motors: {len(self.driver_bus.motors)}")

    def do_list(self, arg):
        """List registered motors and their status."""
        if not self._ensure_motors():
            return
        sprint("Joint  CAN ID   Name       Mode      Position")
        sprint("-" * 52)
        for j, m in sorted(self.driver_bus.motors.items()):
            state = m.get_state()
            pos_str = f"{state['position']:.2f} deg" if state['position'] is not None else 'N/A'
            mode_str = state['mode'] or 'N/A'
            sprint(f"  {j}     0x{m.can_id:03X}   {JOINT_NAMES.get(j, '?'):<9} {mode_str:<9} {pos_str}")

    # --------------------------------------------------
    # Mode Control
    # --------------------------------------------------

    def do_idle(self, arg):
        """Set active joint(s) to IDLE mode."""
        if not self._ensure_motors(): return
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                m.set_idle()
                sprint(f"  J{j} -> IDLE")
        time.sleep(0.2)

    def do_posmode(self, arg):
        """Set active joint(s) to POSITION mode (with retry)."""
        if not self._ensure_motors(): return
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                ok = m.set_mode_with_retry(MODE_CMD_POS)
                sprint(f"  J{j} -> POSITION {'[OK]' if ok else '[FAIL]'}")

    def do_spdmode(self, arg):
        """Set active joint(s) to SPEED mode (with retry)."""
        if not self._ensure_motors(): return
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                ok = m.set_mode_with_retry(MODE_CMD_SPEED)
                sprint(f"  J{j} -> SPEED {'[OK]' if ok else '[FAIL]'}")

    # --------------------------------------------------
    # Position Commands
    # --------------------------------------------------

    def do_abs(self, arg):
        """
        Absolute position move.
        Usage: abs <degrees>   e.g., abs 45  abs -30.5
        """
        if not self._ensure_motors(): return
        deg = fuzzy_number(arg)
        if deg is None:
            sprint("Usage: abs <degrees>   e.g., abs 45.0")
            return
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                sprint(f"  J{j} abs -> {deg:.2f} deg")
                m.go_absolute(deg)

    def do_rel(self, arg):
        """
        Relative position move.
        Usage: rel <degrees>   e.g., rel 15  rel -90
        """
        if not self._ensure_motors(): return
        deg = fuzzy_number(arg)
        if deg is None:
            sprint("Usage: rel <degrees>   e.g., rel 30.0")
            return
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                sprint(f"  J{j} rel -> {deg:+.2f} deg")
                m.go_relative(deg)

    def do_sync(self, arg):
        """Trigger sync broadcast -- all buffered commands execute together."""
        if not self._ensure_connected(): return
        self.driver_bus.sync_trigger()
        sprint("Sync triggered.")

    # --------------------------------------------------
    # Speed Commands
    # --------------------------------------------------

    def do_speed(self, arg):
        """
        Set target speed (must be in SPEED mode).
        Usage: speed <rpm>   e.g., speed 300  speed -100
        """
        if not self._ensure_motors(): return
        rpm = fuzzy_number(arg)
        if rpm is None:
            sprint("Usage: speed <rpm>   e.g., speed 300")
            return
        rpm = int(rpm)
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                sprint(f"  J{j} speed -> {rpm} rpm")
                m.set_speed(rpm)

    # --------------------------------------------------
    # Query Commands
    # --------------------------------------------------

    def do_pos(self, arg):
        """Read current position of active joint(s)."""
        if not self._ensure_motors(): return
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                pos = m.get_position()
                if pos is not None:
                    sprint(f"  J{j} position: {pos:.2f} deg")
                else:
                    sprint(f"  J{j} position: N/A")

    def do_spd(self, arg):
        """Read current speed of active joint(s)."""
        if not self._ensure_motors(): return
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                spd = m.get_speed()
                if spd is not None:
                    sprint(f"  J{j} speed: {spd:.1f} rpm")
                else:
                    sprint(f"  J{j} speed: N/A")

    def do_mode(self, arg):
        """Read current mode of active joint(s)."""
        if not self._ensure_motors(): return
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                mode = m.get_mode()
                sprint(f"  J{j} mode: {mode or 'N/A'}")

    def do_state(self, arg):
        """Read full state (mode, position, speed) of active joint(s)."""
        if not self._ensure_motors(): return
        sprint("Joint  Mode       Position       Speed")
        sprint("-" * 42)
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                s = m.get_state()
                mode = s['mode'] or 'N/A'
                pos = f"{s['position']:.2f} deg" if s['position'] is not None else 'N/A'
                spd = f"{s['speed']:.1f} rpm" if s['speed'] is not None else 'N/A'
                sprint(f"  {j}    {mode:<10} {pos:<14} {spd}")

    # --------------------------------------------------
    # Special Commands
    # --------------------------------------------------

    def do_calibrate(self, arg):
        """
        Run encoder calibration on active joint(s).
        WARNING: Motor will spin 1 revolution each direction!
        """
        if not self._ensure_motors(): return
        if self._interactive:
            sprint("WARNING: Motor(s) will spin during calibration!")
            resp = input("Continue? (y/N): ").strip().lower()
            if resp != 'y':
                sprint("Cancelled.")
                return
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                sprint(f"\nCalibrating Joint {j} ({JOINT_NAMES.get(j, '?')})...")
                m.calibrate_encoder()
        time.sleep(0.5)
        sprint("\nCalibration complete. Check motor positions:")
        self.do_state('')

    def do_estop(self, arg):
        """Emergency stop ALL motors immediately."""
        if not self._ensure_connected(): return
        self.driver_bus.stop_all()
        sprint("[EMERGENCY STOP] All motors stopped.")

    def do_home(self, arg):
        """
        Move active joint(s) to 0 degrees (absolute).
        Requires POSITION mode.
        """
        if not self._ensure_motors(): return
        for j in self.active_joints:
            m = self.driver_bus.motor(j)
            if m:
                m.set_mode_with_retry(MODE_CMD_POS)
                sprint(f"  J{j} -> home (0 deg)")
                m.go_absolute(0.0)
        sprint("Home command sent.")

    # --------------------------------------------------
    # Test Suite
    # --------------------------------------------------

    def _test_countdown(self, test_name, seconds=3):
        """Print countdown before test, allow abort."""
        sprint(f"\n=== {test_name} ===")
        sprint(f"Active joints: {self.active_joints}")
        sprint(f"Starting in ", end='', flush=True)
        for i in range(seconds, 0, -1):
            sprint(f"{i}... ", end='', flush=True)
            time.sleep(1)
        sprint("GO!")

    def _wait_for_motion(self, motor, target, tolerance=1.0, timeout=5.0):
        """Poll until motor reaches target position (within tolerance) or timeout."""
        start = time.time()
        while time.time() - start < timeout:
            pos = motor.get_position()
            if pos is not None and abs(pos - target) < tolerance:
                return pos, True
            time.sleep(0.1)
        pos = motor.get_position()
        return pos, False

    def do_test_ptp(self, arg):
        """
        Point-to-Point position test.
        Each joint moves: 0 -> +45 -> -45 -> 0 degrees.
        """
        if not self._ensure_motors(): return
        self.running_test = True
        try:
            self._test_countdown("Point-to-Point Test")
            targets = [0.0, 45.0, -45.0, 0.0]

            for j in self.active_joints:
                m = self.driver_bus.motor(j)
                if not m:
                    continue
                sprint(f"\n--- Joint {j} ({JOINT_NAMES.get(j, '?')}) ---")
                if not m.set_mode_with_retry(MODE_CMD_POS):
                    sprint(f"  [FAIL] Mode switch failed, skipping J{j}")
                    continue

                for target in targets:
                    sprint(f"  Move to {target:>6.0f} deg... ", end='', flush=True)
                    m.go_absolute(target)
                    time.sleep(0.5)
                    final_pos, ok = self._wait_for_motion(m, target, tolerance=2.0, timeout=5.0)
                    if final_pos is not None:
                        err = abs(final_pos - target) if ok else (final_pos - target)
                        status = "[OK]" if ok else "[WARN]"
                        sprint(f"pos={final_pos:.2f} deg (err={abs(err):.2f}) {status}")
                    else:
                        sprint(f"N/A [FAIL]")

            sprint("\n--- PTP Test Complete ---")
        except KeyboardInterrupt:
            sprint("\n[ABORT] Emergency stop!")
            self.driver_bus.stop_all()
        finally:
            self.running_test = False
            self.driver_bus.set_all_idle()

    def do_test_swing(self, arg):
        """
        Reciprocating swing test.
        Each joint swings +/-30 deg relative for 5 cycles.
        """
        if not self._ensure_motors(): return
        self.running_test = True
        try:
            self._test_countdown("Reciprocating Swing Test")
            cycles = 5

            for j in self.active_joints:
                m = self.driver_bus.motor(j)
                if not m:
                    continue
                sprint(f"\n--- Joint {j} ({JOINT_NAMES.get(j, '?')}) ---")
                if not m.set_mode_with_retry(MODE_CMD_POS):
                    sprint(f"  [FAIL] Mode switch failed, skipping J{j}")
                    continue

                start_pos = m.get_position()
                sprint(f"  Start: {start_pos:.2f} deg" if start_pos is not None else "  Start: N/A")

                for cycle in range(cycles):
                    sprint(f"  Cycle {cycle+1}/{cycles}: +30 deg... ", end='', flush=True)
                    m.go_relative(30.0)
                    time.sleep(1.5)
                    pos1 = m.get_position()
                    sprint(f"{pos1:.2f} -> -30 deg... " if pos1 is not None else "N/A -> ", end='', flush=True)
                    m.go_relative(-30.0)
                    time.sleep(1.5)
                    pos2 = m.get_position()
                    sprint(f"{pos2:.2f}" if pos2 is not None else "N/A")

                end_pos = m.get_position()
                drift = (end_pos - start_pos) if (start_pos is not None and end_pos is not None) else None
                sprint(f"  Drift after {cycles} cycles: {drift:.2f} deg" if drift is not None else "  Drift: N/A")

            sprint("\n--- Swing Test Complete ---")
        except KeyboardInterrupt:
            sprint("\n[ABORT] Emergency stop!")
            self.driver_bus.stop_all()
        finally:
            self.running_test = False
            self.driver_bus.set_all_idle()

    def do_test_torque(self, arg):
        """
        Torque limiting test.
        Compares motion with low current (500mA) vs normal current (1500mA).
        """
        if not self._ensure_motors(): return
        self.running_test = True
        try:
            self._test_countdown("Torque Limiting Test")

            for j in self.active_joints:
                m = self.driver_bus.motor(j)
                if not m:
                    continue
                sprint(f"\n--- Joint {j} ({JOINT_NAMES.get(j, '?')}) ---")
                if not m.set_mode_with_retry(MODE_CMD_POS):
                    sprint(f"  [FAIL] Mode switch failed, skipping J{j}")
                    continue

                # Go to 0 first with normal current
                sprint("  Homing (1500mA)...")
                m.go_absolute(0.0, current_ma=1500)
                time.sleep(2)

                # Test with normal current
                sprint("  Move +45 deg @ 1500mA... ", end='', flush=True)
                m.go_absolute(45.0, current_ma=1500)
                time.sleep(0.5)
                pos, ok = self._wait_for_motion(m, 45.0, tolerance=2.0, timeout=4.0)
                if pos is not None:
                    sprint(f"pos={pos:.2f} deg {'[OK]' if ok else '[WARN]'}")
                else:
                    sprint("N/A")

                # Back to 0
                sprint("  Move to 0 deg @ 1500mA... ", end='', flush=True)
                m.go_absolute(0.0, current_ma=1500)
                time.sleep(2)

                # Test with low current (may stall)
                sprint("  Move +30 deg @ 500mA... ", end='', flush=True)
                m.go_absolute(30.0, current_ma=500)
                time.sleep(0.5)
                pos, ok = self._wait_for_motion(m, 30.0, tolerance=5.0, timeout=4.0)
                if pos is not None:
                    if ok:
                        sprint(f"pos={pos:.2f} deg [OK] (low torque sufficient)")
                    else:
                        sprint(f"pos={pos:.2f} deg [WARN] (possible stall at 500mA, target=30)")
                else:
                    sprint("N/A")

            sprint("\n--- Torque Test Complete ---")
        except KeyboardInterrupt:
            sprint("\n[ABORT] Emergency stop!")
            self.driver_bus.stop_all()
        finally:
            self.running_test = False
            self.driver_bus.set_all_idle()

    def do_test_all(self, arg):
        """Run all joint simulation tests (PTP + Swing + Torque)."""
        if not self._ensure_motors(): return
        sprint("\n" + "=" * 50)
        sprint("  FULL JOINT SIMULATION TEST SUITE")
        sprint("=" * 50)

        for test_name, test_method in [
            ("PTP (Point-to-Point)", self.do_test_ptp),
            ("Swing (Reciprocating)", self.do_test_swing),
            ("Torque Limiting", self.do_test_torque),
        ]:
            sprint(f"\n>>> Running: {test_name}")
            test_method('')
            time.sleep(1)
            if not self.driver_bus.is_all_alive():
                sprint("[ABORT] One or more motors stopped responding!")
                break

        self.driver_bus.set_all_idle()
        sprint("\n" + "=" * 50)
        sprint("  ALL TESTS COMPLETE")
        sprint("=" * 50)

    # --------------------------------------------------
    # System
    # --------------------------------------------------

    def do_connect(self, arg):
        """
        Connect to CAN bus.
        Usage: connect <interface> <channel>
        e.g., connect pcan PCAN_USBBUS1
        """
        parts = arg.split()
        if len(parts) >= 2:
            self._connect(parts[0], parts[1])
        else:
            self._connect()

    def do_disconnect(self, arg):
        """Disconnect from CAN bus."""
        if self.bus:
            self.driver_bus.set_all_idle()
            self.bus.shutdown()
            self.bus = None
            self.driver_bus = None
            sprint("Disconnected.")

    def do_quit(self, arg):
        """Exit the CLI."""
        sprint("Exiting...")
        if self.bus:
            try:
                self.driver_bus.set_all_idle()
            except Exception:
                pass
            try:
                self.bus.shutdown()
            except Exception:
                pass
        return True

    def do_exit(self, arg):
        """Exit the CLI."""
        return self.do_quit(arg)

    def do_EOF(self, arg):
        """Handle Ctrl+D."""
        sprint("")
        return self.do_quit(arg)


# ============================================================
# Main Entry Point
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description='JH_FOC_42V3 6-Axis Motor Control CLI',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python motor_cli.py                              Interactive mode
  python motor_cli.py --scan                       Scan and exit
  python motor_cli.py -i pcan -c PCAN_USBBUS1      Specify CAN interface
  python motor_cli.py -j 1 --cmd "abs 45"          Single command
        """
    )
    parser.add_argument('--scan', action='store_true',
                        help='Scan CAN adapters and motors, then exit')
    parser.add_argument('--interface', '-i', type=str,
                        help='CAN interface type (pcan, slcan, kvaser, etc.)')
    parser.add_argument('--channel', '-c', type=str,
                        help='CAN channel (PCAN_USBBUS1, COM3, etc.)')
    parser.add_argument('--bitrate', '-b', type=int, default=500000,
                        help='CAN bitrate (default: 500000)')
    parser.add_argument('--joint', '-j', type=str, default='1',
                        help='Joint selection for --cmd (default: 1)')
    parser.add_argument('--cmd', type=str,
                        help='Single command to execute (non-interactive)')
    args = parser.parse_args()

    sprint("=" * 50)
    sprint("  JH_FOC_42V3 6-Axis Motor Control CLI")

    # --scan mode
    if args.scan:
        bus, _, _ = connect_can(args.interface, args.channel, args.bitrate)
        found = scan_motors(bus)
        sprint(f"\nFound {len(found)} motor(s) on CAN bus.")
        bus.shutdown()
        return

    # --cmd mode (single command)
    if args.cmd:
        bus, _, _ = connect_can(args.interface, args.channel, args.bitrate)
        dbus = JHFOC42Bus(bus)
        joints = parse_joint_selection(args.joint)
        for j in joints:
            can_id = joint_to_can_id(j)
            dbus.add_motor(j, can_id)
            sprint(f"Registered Joint {j} at 0x{can_id:03X}")

        # Parse and execute the command
        cli = MotorCLI()
        cli.bus = bus
        cli.driver_bus = dbus
        cli.active_joints = joints
        cli._interactive = False

        cmd_line = args.cmd
        sprint(f"Running: {cmd_line}")
        try:
            cli.onecmd(cmd_line)
        finally:
            dbus.set_all_idle()
            bus.shutdown()
        return

    # Interactive mode
    cli = MotorCLI()

    # Connect on startup if interface specified
    if args.interface or args.channel:
        if not cli._connect(args.interface, args.channel):
            sprint("Connection failed. Use 'connect' command in CLI.")
    else:
        sprint("Type 'scan' to detect motors, or 'connect <iface> <chan>'")
        sprint("Type 'help' for all commands.")

    # Register signal handler for graceful exit
    def sigint_handler(sig, frame):
        sprint("\nInterrupted. Emergency stop...")
        if cli.driver_bus:
            try:
                cli.driver_bus.stop_all()
                cli.driver_bus.set_all_idle()
            except Exception:
                pass
        if cli.bus:
            try:
                cli.bus.shutdown()
            except Exception:
                pass
        sys.exit(0)

    signal.signal(signal.SIGINT, sigint_handler)

    # Start CLI
    try:
        cli.cmdloop()
    except KeyboardInterrupt:
        sprint("\nExiting...")
        if cli.bus:
            try:
                cli.driver_bus.set_all_idle()
                cli.bus.shutdown()
            except Exception:
                pass


if __name__ == '__main__':
    main()
