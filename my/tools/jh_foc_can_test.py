"""
JH_FOC_42V3 电机控制测试脚本
用法: python jh_foc_can_test.py

支持多种 USB-CAN 适配器，自动检测。
首次使用请先运行: python jh_foc_can_test.py --scan
"""

import can
import time
import sys
import struct

# ============================================================
# CAN 协议常量 (来自 JH_FOC_42V3 固件)
# ============================================================

DEFAULT_CAN_ID = 0x200  # 驱动器默认 CAN ID
SYNC_ID = 0x200         # 同步广播 ID

# 模式切换命令
MODE_IDLE    = (0x23, 0x11, 0x00)  # 空闲
MODE_CURRENT = (0x23, 0x22, 0x00)  # 电流闭环
MODE_SPEED   = (0x23, 0x33, 0x00)  # 速度闭环
MODE_POS     = (0x23, 0x44, 0x00)  # 位置闭环
MODE_ANGLE   = (0x23, 0x55, 0x00)  # 角度闭环 (0-360°)

# ============================================================
# 命令封装
# ============================================================

def build_cmd_speed(rpm, current_ma=1500, accel=500):
    """设置速度模式的速度"""
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
    """设置电流模式的目标电流 (A)"""
    iq_raw = int(iq_a * 100)
    return [0x44, 0x00, 0, 0, 0, 0,
            (iq_raw >> 8) & 0xFF, iq_raw & 0xFF]

def build_cmd_abs_position(angle_deg, current_ma=1500, max_speed=120, sync=False):
    """绝对位置运动 (度)"""
    sub = 0xAA if sync else 0x00
    angle_raw = int(angle_deg * 100)
    return [
        0x57, sub,
        (current_ma >> 8) & 0xFF, current_ma & 0xFF,
        (angle_raw >> 24) & 0xFF, (angle_raw >> 16) & 0xFF,
        (angle_raw >> 8) & 0xFF, angle_raw & 0xFF
    ]

def build_cmd_rel_position(delta_deg, current_ma=1500, sync=False):
    """相对位置运动 (度)"""
    sub = 0xAA if sync else 0x00
    delta_raw = int(delta_deg * 100)
    return [
        0x55, sub,
        (current_ma >> 8) & 0xFF, current_ma & 0xFF,
        (delta_raw >> 24) & 0xFF, (delta_raw >> 16) & 0xFF,
        (delta_raw >> 8) & 0xFF, delta_raw & 0xFF
    ]

def build_cmd_angle(angle_deg, current_ma=1500, sync=False):
    """角度模式 - 转到指定角度 (0-360°)"""
    sub = 0xAA if sync else 0x00
    angle_raw = int(angle_deg * 100)
    return [
        0x66, sub,
        (current_ma >> 8) & 0xFF, current_ma & 0xFF,
        0, 0,
        (angle_raw >> 8) & 0xFF, angle_raw & 0xFF
    ]

def build_cmd_sync():
    """同步触发 - 所有缓存命令同时执行"""
    return [0x99, 0x9A, 0, 0, 0, 0, 0, 0]

def build_cmd_stop():
    """紧急停止"""
    return [0x77, 0x78, 0, 0, 0, 0, 0, 0]

def build_cmd_query_speed():
    """查询速度"""
    return [0xC0, 0xA0, 0, 0, 0, 0, 0, 0]

def build_cmd_query_position():
    """查询位置"""
    return [0xC3, 0xA3, 0, 0, 0, 0, 0, 0]

def build_cmd_query_mode():
    """查询当前模式"""
    return [0xC9, 0xA9, 0, 0, 0, 0, 0, 0]

# ============================================================
# 响应解析
# ============================================================

def parse_int32_response(data):
    """解析7字节 int32 回复 (字节4-7)"""
    return int.from_bytes(data[4:8], 'big', signed=True)

def parse_speed_response(data):
    """解析速度回复 (rpm * 100)"""
    return parse_int32_response(data) / 100.0

def parse_position_response(data):
    """解析位置回复 (度 * 100)"""
    return parse_int32_response(data) / 100.0

MODE_NAMES = {0: 'IDLE', 0x11: 'CURRENT', 0x22: 'SPEED',
              0x33: 'POSITION', 0x44: 'ANGLE', 0xFF: 'UNKNOWN'}

# ============================================================
# 驱动器控制类
# ============================================================

class JHFOC42Driver:
    """JH_FOC_42V3 单电机驱动器"""

    def __init__(self, bus, can_id=DEFAULT_CAN_ID):
        self.bus = bus
        self.can_id = can_id

    def send(self, data: list):
        """发送 CAN 帧"""
        msg = can.Message(
            arbitration_id=self.can_id,
            data=data[:8],
            is_extended_id=False
        )
        self.bus.send(msg)

    def send_and_wait(self, data: list, timeout=0.5):
        """发送并等待回复"""
        self.send(data)
        start = time.time()
        while time.time() - start < timeout:
            msg = self.bus.recv(timeout=0.05)
            if msg and msg.arbitration_id == self.can_id:
                return msg.data
        return None

    # --- 模式切换 ---
    def set_mode(self, mode):
        """切换模式 (IDLE/SPEED/POS/ANGLE)"""
        self.send(list(mode))

    def set_idle(self):
        self.set_mode(MODE_IDLE)

    def set_speed_mode(self):
        self.set_mode(MODE_SPEED)

    def set_position_mode(self):
        self.set_mode(MODE_POS)

    def set_angle_mode(self):
        self.set_mode(MODE_ANGLE)

    # --- 控制命令 ---
    def set_speed(self, rpm, current_ma=1500, accel=500):
        """设置速度 (rpm)"""
        self.send(build_cmd_speed(rpm, current_ma, accel))

    def set_current(self, iq_a):
        """设置 Iq 电流 (A)"""
        self.send(build_cmd_current(iq_a))

    def go_absolute(self, angle_deg, current_ma=1500, sync=False):
        """绝对位置运动"""
        self.send(build_cmd_abs_position(angle_deg, current_ma, sync=sync))

    def go_relative(self, delta_deg, current_ma=1500, sync=False):
        """相对位置运动"""
        self.send(build_cmd_rel_position(delta_deg, current_ma, sync=sync))

    def go_angle(self, angle_deg, current_ma=1500, sync=False):
        """角度模式转到指定角度"""
        self.send(build_cmd_angle(angle_deg, current_ma, sync=sync))

    def stop(self):
        """紧急停止"""
        self.send(build_cmd_stop())

    # --- 查询 ---
    def get_speed(self) -> float:
        """读取当前速度 (rpm)"""
        resp = self.send_and_wait(build_cmd_query_speed())
        if resp:
            return parse_speed_response(resp)
        return None

    def get_position(self) -> float:
        """读取当前位置 (度)"""
        resp = self.send_and_wait(build_cmd_query_position())
        if resp:
            return parse_position_response(resp)
        return None

    def get_mode(self) -> str:
        """读取当前模式"""
        resp = self.send_and_wait(build_cmd_query_mode())
        if resp:
            mode_code = resp[7]
            return MODE_NAMES.get(mode_code, f'UNKNOWN(0x{mode_code:02X})')
        return None


class JHFOC42Bus:
    """CAN 总线管理器 (多电机)"""

    def __init__(self, bus):
        self.bus = bus
        self.motors = {}

    def add_motor(self, joint_num, can_id=None):
        if can_id is None:
            can_id = 0x200 + joint_num
        self.motors[joint_num] = JHFOC42Driver(self.bus, can_id)
        return self.motors[joint_num]

    def sync_trigger(self):
        """同步触发所有缓存命令"""
        msg = can.Message(
            arbitration_id=SYNC_ID,
            data=build_cmd_sync(),
            is_extended_id=False
        )
        self.bus.send(msg)

    def stop_all(self):
        """停止所有电机"""
        for m in self.motors.values():
            m.stop()

    def move_all_absolute(self, angles: dict, sync=True):
        """
        多轴同时绝对运动
        angles = {1: 45.0, 2: -30.0, 3: 90.0, ...}
        """
        for joint, angle in angles.items():
            if joint in self.motors:
                self.motors[joint].go_absolute(angle, sync=sync)
        if sync:
            time.sleep(0.01)  # 等所有命令发完
            self.sync_trigger()


# ============================================================
# CAN 总线扫描与初始化
# ============================================================

def scan_can_adapters():
    """扫描可用的 CAN 适配器"""
    print("扫描 USB-CAN 适配器...\n")

    configs = []

    # 常见的 python-can 接口配置
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
        ('zcan', '0'),
        ('zcan', '1'),
        ('kvaser', '0'),
        ('socketcan', 'can0'),
    ]

    for interface, channel in interfaces:
        try:
            bus = can.interface.Bus(
                channel=channel,
                interface=interface,
                bitrate=500000,
                timeout=0.1
            )
            print(f"  [OK] {interface}:{channel} -- available")
            configs.append((interface, channel))
            bus.shutdown()
        except Exception as e:
            err = str(e).split('\n')[0][:80]
            print(f"  [--] {interface}:{channel} -- {err}")

    print()
    return configs


def connect_can(interface=None, channel=None, bitrate=500000):
    """连接 CAN 总线"""
    # 如果指定了接口，直接连接
    if interface and channel:
        print(f"连接 {interface}:{channel} @ {bitrate}bps...")
        bus = can.interface.Bus(
            channel=channel,
            interface=interface,
            bitrate=bitrate,
            timeout=0.1
        )
        print("连接成功！\n")
        return bus

    # 否则自动扫描
    configs = scan_can_adapters()
    if not configs:
        print("\n未找到可用的 CAN 适配器！")
        print("请检查：")
        print("  1. USB-CAN 适配器是否已插入")
        print("  2. 驱动程序是否已安装")
        print("  3. 在设备管理器中确认设备名称")
        print("\n手动指定: python jh_foc_can_test.py --interface pcan --channel PCAN_USBBUS1")
        sys.exit(1)

    interface, channel = configs[0]
    print(f"自动选择: {interface}:{channel}")
    bus = can.interface.Bus(
        channel=channel,
        interface=interface,
        bitrate=bitrate,
        timeout=0.1
    )
    return bus


def scan_motors(bus):
    """扫描 CAN 总线上的电机驱动器"""
    print("扫描 CAN 总线上的驱动器 (ID 0x200-0x20F)...")
    found = []
    for can_id in range(0x200, 0x210):
        motor = JHFOC42Driver(bus, can_id)
        mode = motor.get_mode()
        if mode:
            print(f"  [OK] ID 0x{can_id:03X} - mode: {mode}")
            found.append(can_id)
        else:
            print(f"  [--] ID 0x{can_id:03X} - no response")
    print()
    return found


# ============================================================
# 测试程序
# ============================================================

def test_speed_mode(motor):
    """测试速度模式"""
    print("=" * 50)
    print("测试1: 速度模式")
    print("=" * 50)

    motor.set_speed_mode()
    time.sleep(0.2)
    mode = motor.get_mode()
    print(f"当前模式: {mode}")

    if mode != 'SPEED':
        print("切换到速度模式失败！检查编码器是否连接")
        return False

    # 逐步加速
    print("\n正转加速中...")
    for rpm in [60, 120, 300, 600, 1000]:
        print(f"  设置速度: {rpm} rpm")
        motor.set_speed(rpm, current_ma=1000, accel=200)
        time.sleep(1.5)
        speed = motor.get_speed()
        print(f"  实际速度: {speed:.1f} rpm")

    # 减速停止
    print("\n减速停止...")
    motor.set_speed(0, accel=500)
    time.sleep(1)

    # 反转
    print("\n反转...")
    motor.set_speed(-300, current_ma=1000, accel=200)
    time.sleep(2)
    speed = motor.get_speed()
    print(f"  实际速度: {speed:.1f} rpm")

    motor.set_speed(0, accel=500)
    time.sleep(0.5)
    motor.set_idle()
    print("速度模式测试完成 ✓\n")
    return True


def test_position_mode(motor):
    """测试位置模式"""
    print("=" * 50)
    print("测试2: 位置模式")
    print("=" * 50)

    motor.set_position_mode()
    time.sleep(0.2)
    mode = motor.get_mode()
    print(f"当前模式: {mode}")

    if mode != 'POSITION':
        print("切换到位置模式失败！")
        return False

    pos = motor.get_position()
    print(f"当前位置: {pos:.2f}°")

    # 相对运动测试
    print("\n相对运动: +90°")
    motor.go_relative(90.0, current_ma=1500)
    time.sleep(2)
    pos = motor.get_position()
    print(f"当前位置: {pos:.2f}°")

    print("相对运动: -45°")
    motor.go_relative(-45.0, current_ma=1500)
    time.sleep(1.5)
    pos = motor.get_position()
    print(f"当前位置: {pos:.2f}°")

    print("绝对运动: 回到 0°")
    motor.go_absolute(0.0, current_ma=1500)
    time.sleep(2)
    pos = motor.get_position()
    print(f"当前位置: {pos:.2f}°")

    motor.set_idle()
    print("位置模式测试完成 ✓\n")
    return True


def interactive_mode(motor):
    """交互式控制"""
    print("=" * 50)
    print("交互控制模式")
    print("=" * 50)
    print("命令:")
    print("  s <rpm>      — 设置速度 (先自动切速度模式)")
    print("  p <deg>      — 绝对位置 (先自动切位置模式)")
    print("  r <deg>      — 相对位置")
    print("  mode         — 查看当前模式")
    print("  pos          — 查看当前位置")
    print("  spd          — 查看当前速度")
    print("  idle         — 空闲模式")
    print("  stop         — 急停")
    print("  q            — 退出")
    print()

    current_mode = motor.get_mode()
    print(f"当前模式: {current_mode}")

    while True:
        try:
            cmd = input("> ").strip().lower()
            if not cmd:
                continue
            parts = cmd.split()
            if parts[0] == 'q':
                break
            elif parts[0] == 's' and len(parts) >= 2:
                motor.set_speed_mode()
                time.sleep(0.1)
                rpm = int(parts[1])
                motor.set_speed(rpm, current_ma=1000, accel=200)
                print(f"  速度 → {rpm} rpm")
            elif parts[0] == 'p' and len(parts) >= 2:
                motor.set_position_mode()
                time.sleep(0.1)
                deg = float(parts[1])
                motor.go_absolute(deg, current_ma=1500)
                print(f"  绝对位置 → {deg}°")
            elif parts[0] == 'r' and len(parts) >= 2:
                motor.set_position_mode()
                time.sleep(0.1)
                deg = float(parts[1])
                motor.go_relative(deg, current_ma=1500)
                print(f"  相对位置 → {deg}°")
            elif parts[0] == 'mode':
                print(f"  模式: {motor.get_mode()}")
            elif parts[0] == 'pos':
                print(f"  位置: {motor.get_position():.2f}°")
            elif parts[0] == 'spd':
                print(f"  速度: {motor.get_speed():.1f} rpm")
            elif parts[0] == 'idle':
                motor.set_idle()
                print("  → IDLE")
            elif parts[0] == 'stop':
                motor.stop()
                print("  → STOP")
            else:
                print(f"  未知命令: {cmd}")
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"  错误: {e}")

    motor.set_idle()
    motor.stop()


# ============================================================
# 主入口
# ============================================================

def main():
    import argparse

    parser = argparse.ArgumentParser(description='JH_FOC_42V3 电机控制测试')
    parser.add_argument('--scan', action='store_true', help='扫描 CAN 适配器和驱动器')
    parser.add_argument('--interface', '-i', type=str, help='CAN 接口类型 (pcan/slcan/zcan/kvaser)')
    parser.add_argument('--channel', '-c', type=str, help='CAN 通道 (PCAN_USBBUS1/COM3/...)')
    parser.add_argument('--id', type=str, default='0x200', help='驱动器 CAN ID (默认: 0x200)')
    parser.add_argument('--test', action='store_true', help='运行自动测试')
    parser.add_argument('--bitrate', type=int, default=500000, help='CAN 波特率 (默认: 500000)')
    args = parser.parse_args()

    can_id = int(args.id, 16)

    print("╔══════════════════════════════════════════════╗")
    print("║   JH_FOC_42V3 步进电机 FOC 驱动器测试工具    ║")
    print("╚══════════════════════════════════════════════╝")
    print()

    # 仅扫描
    if args.scan:
        bus = connect_can(None, None, args.bitrate)
        scan_motors(bus)
        bus.shutdown()
        return

    # 连接
    bus = connect_can(args.interface, args.channel, args.bitrate)
    motor = JHFOC42Driver(bus, can_id)

    # 检查驱动器
    mode = motor.get_mode()
    if mode:
        print(f"驱动器 0x{can_id:03X} 已连接，当前模式: {mode}\n")
    else:
        print(f"驱动器 0x{can_id:03X} 无响应！")
        print("\n请检查:")
        print("  1. 驱动板是否上电 (12-24V)")
        print("  2. CANH/CANL 接线是否正确")
        print("  3. CAN 终端电阻 (120Ω) 是否接好")
        print("  4. 驱动板是否处于 CAN 模式 (非 USB 模式)")
        bus.shutdown()
        sys.exit(1)

    # 运行测试或交互模式
    try:
        if args.test:
            print("开始自动测试...\n")
            if test_speed_mode(motor):
                time.sleep(1)
            if test_position_mode(motor):
                time.sleep(1)
            print("全部测试完成 ✓")
        else:
            interactive_mode(motor)
    finally:
        bus.shutdown()
        print("已断开 CAN 总线。")


if __name__ == '__main__':
    main()
