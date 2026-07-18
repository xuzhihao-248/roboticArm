"""电机 USB 测试脚本"""
import serial, time

def wait_com7(timeout=30):
    """等待 COM7 重新出现"""
    import serial.tools.list_ports
    for i in range(timeout):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if 'COM7' in ports:
            return True
        time.sleep(1)
    return False

def connect():
    s = serial.Serial('COM7', 115200, timeout=2)
    time.sleep(2)
    s.reset_input_buffer()
    return s

def cmd(s, cmd_str, wait=0.5):
    s.write((cmd_str + '\n').encode())
    time.sleep(wait)
    r = s.read_all().decode(errors='replace').strip()
    if r:
        print(f'  <- {r}')
    return r

# ===== 主流程 =====
print("=== 1. 初始状态 ===")
ser = connect()
cmd(ser, 'JH0+Gmode')
cmd(ser, 'JH0+Gencoder')
cmd(ser, 'JH0+Gvol')

print("\n=== 2. 编码器校准 ===")
print("(电机应该正反转一圈)")
cmd(ser, 'JH0+Adjust', wait=4)
ser.close()

print("等待重启...")
wait_com7()
print("COM7 已恢复，等待驱动板初始化(8秒)...")
time.sleep(8)

print("\n=== 3. 校准后 ===")
ser = connect()
cmd(ser, 'JH0+Gencoder')

print("\n=== 4. 速度模式测试 ===")
cmd(ser, 'JH0+Mode+Speed')
time.sleep(0.3)
cmd(ser, 'JH0+SetCurr+2.0')
time.sleep(0.3)

for rpm, dur in [(30, 3), (100, 3), (300, 2), (-100, 2), (0, 1)]:
    print(f"设置 {rpm} rpm ...")
    cmd(ser, f'JH0+Speed+{rpm}')
    time.sleep(dur)
    cmd(ser, 'JH0+Gspeed')

cmd(ser, 'JH0+Mode+IDLE')
ser.close()
print("\n完成!")
