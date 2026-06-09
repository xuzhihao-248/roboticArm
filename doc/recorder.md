# 项目操作记录

> 记录每次对项目的改动，方便回溯和交接。

---

## 使用说明

每条记录按以下模板填写：

```
### YYYY/MM/DD  操作人

**类型**：[文档/代码/硬件/其他]

**内容**：做了什么

**原因**：为什么做（可选）

**相关文件**：改动了哪些文件（可选）
```

---

## 模板

### YYYY/MM/DD  姓名

**类型**：

**内容**：

**原因**：

**相关文件**：

---

## 记录

### 2026/05/23  xuzhihao-248

**类型**：文档

**内容**：引入原版 Dummy 开源模型，使用 AI 整理学习路径

**原因**：方便查找相关资料和学习

**相关文件**：`raw/`、`raw/dummy/AI项目解读/`

> **重要约定**：所有原始文件放在 `raw/` 中，不要修改。自己的修改另存到根目录下新建文件夹（如 `selfModel/`），命名方式采用小驼峰命名法。如有版本对比必要，文件名加时间。

---

### 2026/05/23  xuzhihao-248

**类型**：文档

**内容**：整理 GitHub SSH 协作指南

**原因**：统一团队 Git 工作流，解决 Clash Verge 端口冲突问题

**相关文件**：`doc/GitHub协作指南.md`

---

### 2026/05/23  xuzhihao-248

**类型**：文档

**内容**：撰写需求文档 proposal.md，规划五期开发目标（复刻→原理→视觉→SLAM→VLA）

**原因**：为项目建立清晰的长期路线图

**相关文件**：`doc/proposal.md`

---

### 2026/05/23  xuzhihao-248

**类型**：文档

**内容**：撰写任务拆分 tasks.md，将五期目标拆为 62 个可执行任务

**原因**：需求文档落地为具体任务，方便追踪进度

**相关文件**：`doc/tasks.md`

---

### 2026/05/23  xuzhihao-248

**类型**：文档

**内容**：撰写一期复刻技术文档，采用"先电子后机械"策略

**原因**：零基础可执行的开发手册；确定用嘉立创做 PCB、减速器走便宜路线、先桌面调通电子再装机械

**相关文件**：`doc/一期复刻技术文档.md`

---

### 2026/05/23  xuzhihao-248

**类型**：环境

**内容**：配置 Git SSH 连接、解决 Clash Verge 端口冲突、改用 HTTPS + 代理

**原因**：确保 GitHub 仓库可正常拉取和推送

**相关文件**：`~/.ssh/config`（已删除）、Git 全局代理配置

### 2026/05/24  xuzhihao-248

**类型**：文件

**内容**：增加了固件和软件

**原因**：为了方便学习相关程序

**相关文件**：raw\dummy\zhihui原版\Dummy-Robot-main

---

### 2026/06/05  xuzhihao-248

**类型**：环境

**内容**：完成 Ubuntu 22.04 虚拟机环境搭建 + ROS2 Humble 安装 + 开发工具链配置

**环境**：VMware Workstation Pro + Ubuntu 22.04 LTS

**完成事项**：

1. **系统初始化**：更新系统、安装基础工具（curl/wget/git/vim/build-essential）、更换清华镜像源
2. **VMware Tools**：安装 open-vm-tools，实现鼠标自由进出、剪贴板共享、拖拽文件、自适应分辨率
3. **SSH Server**：安装 openssh-server，启用开机自启，VSCode Remote-SSH 连接虚拟机
4. **ROS2 Humble 安装**：设置 UTF-8 Locale → 添加 ROS2 软件源和 GPG 密钥 → 安装 ros-humble-desktop → 安装 colcon/rosdep/ros-dev-tools → 初始化 rosdep → 写入 ~/.bashrc
5. **工作空间**：创建 `~/ros2_ws/src/`，配置 `source ~/ros2_ws/install/setup.bash`
6. **Python uv**：安装 uv 包管理器，安装 Python 3.10（与 ROS2 Humble 绑定版本一致），配置虚拟环境工作流
7. **仿真工具**：安装 Gazebo（ros-humble-gazebo-ros-pkgs）、Navigation2（ros-humble-navigation2）、MoveIt2（ros-humble-moveit）
8. **共享文件夹**：配置 VMware 共享文件夹 `D:\code\project\roboticArm` → VM `/mnt/hgfs/`，实现 Windows↔VM 高速文件传输

**验证**：`ros2 run demo_nodes_cpp talker` + `ros2 run demo_nodes_py listener` 通信成功

**相关文件**：`~/ros2_ws/`、`~/.bashrc`、`/etc/fstab`

---

### 2026/06/05  xuzhihao-248

**类型**：代码

**内容**：SolidWorks 机械臂 URDF → ROS2 RViz2 全流程打通

**前提**：SolidWorks 侧使用 SW URDF Exporter 插件导出 6 轴机械臂（1 底座 + 5 关节，末端还没有考虑），每个关节处手动建参考坐标系使 Z 轴对齐旋转轴

**完成事项**：

1. **文件传输**：通过 VMware 共享文件夹将导出的 URDF 包从 Windows 拷贝到 `~/ros2_ws/src/`（避免用 SCP，NAT 网络下极慢）
2. **ROS1→ROS2 转换**：
   - `package.xml`：format 改为 3，catkin → ament_cmake，添加 rviz2/robot_state_publisher/joint_state_publisher_gui 依赖
   - `CMakeLists.txt`：替换为 5 行 ament_cmake 标准模板（用 printf 写入，避免 Windows 换行符问题）
   - `launch/display.launch.py`：新建 ROS2 Python launch，启动 robot_state_publisher + joint_state_publisher_gui + rviz2 三个节点
   - 删除 ROS1 遗留文件（display.launch、gazebo.launch、export.log）
3. **URDF 修复**：Joint3 角度上下限相同（1.047/1.047）导致关节锁死，改为 -1.047/1.047
4. **编译运行**：`colcon build --symlink-install` → `source install/setup.bash` → `ros2 launch my_robot display.launch.py`
5. **RViz2 配置**：Fixed Frame → base_link，添加 RobotModel，Description Topic → /robot_description

**踩过的坑**：
- `src/` 根目录下多余的 CMakeLists.txt 导致编译解析失败（heredoc 跨平台写入出错）
- RViz2 只看到 TF 坐标轴没有 3D 模型 → Description Topic 未设置为 /robot_description
- 机械臂水平躺倒 → base_link 参考坐标系 Z 轴未竖直向上，需在 SolidWorks 中手动修正
- 不要从 Windows 直接复制 CMakeLists.txt 到 VM，隐藏字符会导致 CMake 解析失败

**相关文件**：`~/ros2_ws/src/my_robot/`（后改名为 newrobot）、`D:\code\project\roboticArm\my\urdf\`   
**参考**：https://mp.weixin.qq.com/s?__biz=MzU1NjEwMTY0Mw==&mid=2247605924&idx=1&sn=5dc503fe141886e4feffadc92bc9ef39&chksm=fa7e9dd839d804376f49be638c164ae6c0909022df00a8904f21af94e19e2bf68c4ddf3e7eaf&scene=27

---

### 2026/06/06 20:29  xuzhihao-248

**类型**：环境

**内容**：MoveIt2 Setup Assistant 导入 URDF 时闪退，报错 `PackageNotFoundError`

**问题描述**：
启动 MoveIt Setup Assistant 后，加载 URDF 文件时立即闪退，终端报错：
```
terminate called after throwing an instance of 'ament_index_cpp::PackageNotFoundError'
  what():  package 'newrobot' not found
```

**尝试过的错误方案**：
1. 把 URDF 中的 `package://newrobot/meshes/` 替换为绝对路径 → 仍然闪退，原因相同
2. 用 `LIBGL_ALWAYS_SOFTWARE=1` 软件渲染启动 → 无效，非图形问题

**最终解决方案**：
MoveIt Setup Assistant 要求 URDF 必须以 **ROS2 功能包** 的形式存在，不能直接加载裸 URDF 文件。具体步骤：

1. 确保 URDF 文件位于一个完整的 ROS2 功能包中（含 `package.xml`、`CMakeLists.txt`、`urdf/`、`meshes/` 目录），且 URDF 内使用 `package://包名/meshes/` 路径
2. 在工作空间根目录执行 `colcon build --packages-select <包名>` 编译
3. 在**启动 Setup Assistant 的同一个终端**中先执行 `source install/setup.bash`
4. 再启动 `ros2 launch moveit_setup_assistant setup_assistant.launch.py`
5. 此时加载 URDF 文件即可正常识别，不会闪退

**相关文件**：`~/ros2_ws/src/newrobot/`、`~/moveit_robot/urdf/newrobot.urdf`（绝对路径版备用）

**方案来源**：CSDN 博主「热心市民R先生」
https://blog.csdn.net/h542812556sdu/article/details/148025046

---

### 2026/06/06 20:46  xuzhihao-248

**类型**：环境

**内容**：MoveIt demo.launch.py 启动时报错 `package 'controller_manager' not found`，RViz 中无机器人模型显示

**问题描述**：
MoveIt Setup Assistant 配置完成并编译后，执行 `ros2 launch newrobot_moveit_config demo.launch.py` 时：
1. RViz 打开但无任何内容，Fixed Frame 报 `No TF data`
2. 终端报错 `package 'controller_manager' not found`

**原因**：
MoveIt Setup Assistant 中配置了 ROS2 Controller（`ros2_control` 相关），但系统未安装 `ros2_control` 及其依赖包。

**解决方案**：
安装缺失的 ROS2 控制相关包：

```bash
sudo apt install ros-humble-ros2-control ros-humble-ros2-controllers ros-humble-joint-state-broadcaster ros-humble-joint-trajectory-controller ros-humble-controller-manager
```

安装后重新启动即可正常显示机器人模型并进行运动规划。

**相关文件**：`~/ros2_ws/src/newrobot_moveit_config/`

---

### 2026/06/06 21:00  xuzhihao-248

**类型**：环境

**内容**：完整配置 MoveIt2 Setup Assistant 并成功启动 demo.launch.py

**前提条件**：
- URDF 功能包已编译（`colcon build --packages-select newrobot`）
- 启动前已 `source install/setup.bash`
- 已安装 MoveIt2（`sudo apt install ros-humble-moveit`）

**Setup Assistant 配置步骤**：

1. **启动**：`ros2 launch moveit_setup_assistant setup_assistant.launch.py`
2. **Start Screen** → Load Files → 选择编译好的 URDF（必须 source 过）

   ![Load Files](images/moveit_setup/01_load_urdf.png)

3. **Self-Collisions** → Generate Collision Matrix（自动计算，6 个 link，15 组碰撞对）

   ![碰撞检测](images/moveit_setup/02_self_collisions.png)

4. **Planning Joints** → Add Group：
   - Group Name: `arm`
   - Kin. Solver: `KDLKinematicPlugin`
   - Add Joints → 逐个勾选 joint1 ~ joint5

   ![规划组](images/moveit_setup/03_planning_group.png)

5. **Robot Poses** → Add Pose → `home`，所有关节角度填 0

   ![预设姿态](images/moveit_setup/04_robot_poses.png)

6. **ROS2 Control URDF Modifications** → 保持默认（command: position，state: position + velocity）
7. **ROS2 Controller** → Add Controller → `joint_trajectory_controller/JointTrajectoryController` → Add Planning Group Joints → 选 arm

   ![controller控制器设置](images/moveit_setup/05_ros2_controller.png)

8. **MoveIt Controllers** → Add Controller → `FollowJointTrajectory` → Add Planning Group Joints → 选 arm
9. **Configuration Files** → Generate Package → 保存到 `~/ros2_ws/src/newrobot_moveit_config`

   ![moveit设置总览](images/moveit_setup/06_overview.png)

   **成功后展示效果：**

   <video controls src="images/moveit_setup/07_demo_preview.mp4" title="成功后展示效果"></video>

**配置过程中遇到的问题**：

| 问题 | 原因 | 解决 |
|------|------|------|
| Planning Joints 标红，`Group 'arm' is empty` | Add Joints 时没有正确选中关节 | 重新 Add Group → Add Joints → 逐个勾选 joint1-5 → Save |
| Generate Package 时文件标红 | 之前有残留的配置文件 | 全部勾选覆盖即可 |
| `ros2 launch` 报 `Package not found` | 未编译新生成的配置包 | `colcon build --packages-select newrobot_moveit_config` |
| RViz 无模型，`No TF data` | 缺少 ros2_control 依赖包 | 安装 ros-humble-ros2-control 等（见上条记录） |

**编译与启动命令**：

```bash
cd ~/ros2_ws
colcon build --packages-select newrobot newrobot_moveit_config
source install/setup.bash
ros2 launch newrobot_moveit_config demo.launch.py
```

**最终效果**：RViz 中显示机器人模型，可通过 MotionPlanning 面板拖动末端并进行路径规划。

---

### 2026/06/06 21:15  xuzhihao-248

**类型**：文档

**内容**：MoveIt2 RViz 基本操作、Planning Options 参数说明、重影问题解决

**相关文件**：`~/ros2_ws/src/newrobot_moveit_config/`

#### MoveIt2 RViz 基本操作

| 操作 | 方法 |
|------|------|
| 拖动末端 | 鼠标左键拖动蓝色球（位置）或红绿蓝箭头（单轴） |
| 规划路径 | Planning 标签 → Plan（只看路径） |
| 规划并执行 | Planning 标签 → Plan & Execute（机器人动起来） |
| 随机姿态 | Planning 标签 → Random |
| 回零位 | Planning 标签 → Goal State → home |
| 添加障碍物 | Scene 标签 → Add Box / Sphere / Cylinder（规划时自动避障） |
| 单关节控制 | Joints 标签 → 拖动各 joint 滑块 |

#### Planning Options 参数说明

| 参数 | 含义 | 默认值 | 建议 |
|------|------|--------|------|
| Planning Time | 规划算法最大运行时间（秒） | 5 | 保持默认，复杂场景可调到 10 |
| Planning Attempts | 规划失败重试次数 | 10 | 保持默认 |
| Velocity Scaling | 速度缩放比例（0~1） | 0.1 | 刚上手用 0.1，熟悉后调到 0.5 |
| Acceleration Scaling | 加速度缩放比例（0~1） | 0.1 | 刚上手用 0.1，熟悉后调到 0.5 |

> Velocity / Acceleration Scaling = 0.1 表示最大速度/加速度的 10%，适合初学慢速观察。仿真中可调到 1.0 测试极限。

#### 问题：RViz 中机器人出现重影

**原因**：MotionPlanning 插件中多个显示层同时开启，导致多个机器人模型叠加显示。

**解决方案**：
在 Displays 面板中：
1. MotionPlanning → **Scene Robot** → 取消勾选 **Show Scene Robot** ❌
2. MotionPlanning → **Planned Path** → 取消勾选 **Show Robot Visual** ❌（或保持，仅规划时显示）
3. 只保留 **RobotModel** 勾选 ✅

#### 问题：Planning Time 设为 0 后卡死

**原因**：Planning Time 是规划算法的最大运行时间（秒）。设为 0 意味着给规划器 0 秒计算路径，无法完成规划。

**解决方案**：Planning Time 不能为 0，最小保持 1 秒，建议默认值 5。

---

#### 问题：move_group 进程崩溃（exit code -9），机器人无法运动

**现象**：终端报错 `process has died [pid xxx, exit code -9]`，RViz 中机器人无法再执行规划。

**原因**：exit code -9 表示进程被系统强制杀死（SIGKILL），通常是 VMware 虚拟机内存不足触发 OOM Killer。

**解决方案**：

```bash
# 杀掉所有残留进程
pkill -f moveit
pkill -f rviz

# 重新启动
source ~/ros2_ws/install/setup.bash
ros2 launch newrobot_moveit_config demo.launch.py
```

**预防措施**：
- 确保虚拟机分配足够内存（建议 8GB+）
- 关闭不必要的后台程序
- 如频繁出现，用 `free -h` 监控内存使用情况

---

### 2026/06/06 21:30  xuzhihao-248

**类型**：文档

**内容**：MoveIt 用途、主控板必要性、CAN 总线能力的知识点整理

#### Q1：MoveIt 有什么用？

MoveIt 是机器人的"大脑"，负责运动规划。核心流程：

```
目标位置 → 逆运动学求关节角度 → 轨迹规划（平滑路径） → 输出给电机
```

接入真实硬件后的架构：MoveIt 规划路径 → ros2_control 硬件接口 → 你的驱动节点 → CAN 总线 → STM32 驱动板 → 电机转动。

#### Q2：可以不要主控板，电脑直连驱动板吗？

可以。对比：

| 方案 | 优点 | 缺点 |
|------|------|------|
| 电脑直连 USB-CAN | 少一块板，调试方便，MoveIt 直接输出 | Linux 非实时，同步靠代码实现 |
| 用主控板（原版方案） | 实时性好，6 电机同步广播 | 多一块板（~100 元） |

建议：学习阶段电脑直连，以后需要独立运行时再加主控板。

#### Q3：CAN 总线能力

- 每个驱动板有唯一 CAN ID（1~6），可单独控制也可广播
- CAN 波特率：1 Mbps
- 实际控制频率：100~1000 Hz（取决于固件配置）
- MoveIt 输出频率：通常 100 Hz
- 数据量：5 电机 × 4 字节 = 20 字节/周期，远低于 CAN 带宽极限

---

### 2026/6/7  xuzhihao-248

**类型**：文档

**内容**：添加了三个新文档

**原因**：为了方便了解原版中的固件、软件和运行逻辑

**相关文件**：doc/固件解析.md 软件解析.md 电路板解析.md

---

### 2026/6/7  xuzhihao-248

**类型**：硬件

**内容**：安装 KiCad 8.0，从 Altium 工程文件导入 MotorDriver-42 原理图和 PCB，导出 BOM 表，MotorDriver-42 PCB 已下单嘉立创打样

**关键操作**：

1. **KiCad 导入**：文件 → 导入 → 非 KiCad 工程 → Altium Designer 工程 → 选择 `Motor-42.PrjPCB`，5 页原理图 + 4 层 PCB 导入成功
2. **BOM 导出**：工具 → 生成 BOM → 导出为 CSV（遇到 UTF-8 编码问题，Excel 打开乱码，已通过添加 BOM（UTF-8 with BOM）解决）
3. **BOM 核实（部分完成）**：
   - ✅ TB67H450FNG ×2（确认双芯片设计）
   - ✅ 晶振 12MHz（确认与固件 `hal_conf.h` 的 `HSE_VALUE 12000000U` 一致）
   - ✅ CAN 收发器 SN65HVD232DR
   - ✅ 电源方案 ME3116 + LP2992 + TL431 + TPS61040（非之前推测的 AMS1117）
   - ✅ 连接器间距 SH1.0mm 待确认
   - ⬜ 全部封装名与实物对应关系待核对

4. **PCB 下单**：嘉立创，10 片，4 层 FR-4，1.6mm，绿色阻焊，有铅喷锡，72-96h 加急
5. **文档修正**：`doc/电路板解析.md` 修正晶振频率（8→12MHz）、CAN 收发器型号、电源方案，新增 §3.6 时钟配置来源说明

**下一步**：核实 BOM 表中连接器和其他器件参数，确认后下单元器件采购

**相关文件**：`my/HardWarePCB/MotorDriver/Motor-42.kicad_pro`、`my/HardWarePCB/MotorDriver/Motor-42.csv`（BOM）、`doc/电路板解析.md`

---

### 2026/6/8  xuzhihao-248

**类型**：硬件

**内容**：通过 SolidWorks 测量了电机和减速器的关键尺寸

**步进电机**：

| 型号 | 数量 | 出轴直径 |
|------|------|----------|
| 42 步进电机 | 3 | Φ5mm |
| 20 步进电机 | 3 | Φ5mm |

**减速器**：

| 编号 | 外形尺寸 | 输入轴 | 输出轴 |
|------|----------|--------|--------|
| 1 | 42mm × 42mm × 32mm | — | Φ10mm |
| 2 | 30.7mm × 30.7mm × 26.9mm | Φ5mm | Φ9mm |
| 3 | 20mm × 20mm × 15.2mm | Φ3mm | Φ5mm |

**原因**：为后续结构设计和装配提供准确的尺寸参考

**相关文件**：—

---

### 2026/06/09  xuzhihao-248

**类型**：硬件

**内容**：42 驱动板 + 20 驱动板元器件采购核对，完成封装验证、数量验证、价格核对

> 涉及器件类型：电容（MLCC 去耦/滤波）、电阻（信号/采样/分压）、电感（DC-DC 储能）、二极管（肖特基/TVS/整流）、连接器（SH1.0 系列）、主控芯片（STM32F103CBT6）、电机驱动（TB67H450FNG 双H桥）、磁编码器（MT6816 14-bit SPI）、CAN 收发器（SN65HVD232D）、电源管理（ME3116 DC-DC + LP2992 LDO + TPS61040 升压 + TL431 基准）、晶振（12MHz 陶瓷谐振器）、按键、测试点

**原因**：下单前最终确认，避免买错器件或数量不够

**相关文件**：`my/selfHardWarePCB/MotorDriver/采购元件/Motor-42.csv`、`my/selfHardWarePCB/MotorDriver/42motorDriver/Motor-42.txt`、`my/selfHardWarePCB/MotorDriver/20motorDriver/Motor-20.txt`

#### 一、采购目标

3 块 42 驱动板 + 3 块 20 驱动板，共 6 块板子。

#### 二、每块板器件数量（原理图统计）

**42 驱动板（56 个器件/板）：**

| 器件 | 说明 | 位号 | 数量/板 |
|------|------|------|--------|
| 0.1uF 0402 | 电容，MLCC 去耦滤波 | C1,C2,C4,C5,C6,C7,C8,C9,C15,C17,C18,C21,C25,C26,C32 | 15 |
| 10uF 0402 | 电容，MLCC 电源滤波 | C3,C10,C19,C22,C24,C28,C30 | 7 |
| 100uF 1206 | 电容，MLCC 电机电源大容量滤波 | C11,C12,C13,C14 | 4 |
| 100pF 0402 | 电容，NPO 高频滤波 | C16 | 1 |
| 10uF/50V 0402 | 电容，MLCC 高压滤波 | C20 | 1 |
| 10nF 0402 | 电容，MLCC 去耦 | C23 | 1 |
| 220pF 0402 | 电容，NPO 滤波 | C33 | 1 |
| SS54 DO-214AC | 二极管，肖特基 防反接/续流 | D1 | 1 |
| SMBJ28CA | 二极管，TVS 28V 过压保护 | D2 | 1 |
| 1N5819W SOD-323 | 二极管，肖特基 整流 | D3,D4 | 2 |
| 1N5819WS SOD-323 | 二极管，肖特基 整流 | D6 | 1 |
| 跳线 0R 0402 | 电阻，0Ω 跳线/可选连接 | J1 | 1 |
| 按键 3×4×2 | 开关，SMD 贴片按键 | K1,K2 | 2 |
| 6.8uH 201612 | 电感，DC-DC 升压储能 | L1 | 1 |
| 2.2uH 2520 | 电感，DC-DC 储能 | L2 | 1 |
| SH1.0-5P | 连接器，1.0mm间距 5Pin 卧贴 | P1 | 1 |
| SH1.0-6P | 连接器，1.0mm间距 6Pin 卧贴 | P2,P3,P4 | 3 |
| SH1.0-3P | 连接器，1.0mm间距 3Pin 卧贴 | P5 | 1 |
| 1K 0402 | 电阻，信号限流/上拉 | R1,R2,R6,R7,R17 | 5 |
| 0.1R 1206 | 电阻，电流采样（LSS到GND） | R3,R8 | 2 |
| NTC 10K 0402 | 热敏电阻，温度检测（电机过热保护） | R4 | 1 |
| 3.3K 0402 | 电阻，分压/偏置 | R5 | 1 |
| 10K 0402 | 电阻，上拉/分压 | R9,R10,R13,R16 | 4 |
| 120R 0402 | 电阻，CAN 总线终端匹配 | R11,R12 | 2 |
| 1.8K 0402 | 电阻，上拉/限流 | R14 | 1 |
| 1M 0402 | 电阻，高阻分压 | R15 | 1 |
| 560K 0402 | 电阻，TPS61040 反馈分压 | R23 | 1 |
| 93.1K 0402 | 电阻，TPS61040 反馈分压（上臂） | R26 | 1 |
| 13.3K 0402 | 电阻，TPS61040 反馈分压（下臂） | R27 | 1 |
| 测试点 | 测试辅助，镀金测试环 | T1,T2 | 2 |
| TB67H450FNG SO-8-PAD | IC，东芝双H桥步进电机驱动 | U1,U2 | 2 |
| STM32F103CBT6 LQFP-48 | IC，ARM Cortex-M3 主控 MCU | U3 | 1 |
| TL431IDBZT SOT-23-3 | IC，精密可调电压基准 | U4 | 1 |
| SN65HVD232D SO-8 | IC，CAN 2.0 总线收发器 | U5 | 1 |
| ME3116AM6G SOT-23-6 | IC，DC-DC 降压（40V→5V） | U6 | 1 |
| LP2992 3.3V SOT-23-5 | IC，LDO 低压差稳压（5V→3.3V） | U7 | 1 |
| MT6816 SO-8 | IC，14-bit 磁编码器（SPI，角度反馈） | U8 | 1 |
| TPS61040DBV SOT-23-5 | IC，DC-DC 升压转换器 | U11 | 1 |
| 12MHz 晶振-3P | 晶振，陶瓷谐振器（内置电容，HSE时钟源） | Y1 | 1 |

**20 驱动板（与 42 板的差异）：**

| 差异项 | 说明 | 42 板 | 20 板 |
|--------|------|-------|-------|
| 0.1uF 数量 | 电容，MLCC 去耦 | 15 | 14 |
| 10uF 数量 | 电容，MLCC 电源滤波 | 7 | 6 |
| 100uF 数量 | 电容，MLCC 电机电源滤波 | 4 | 2 |
| 连接器 | 连接器，接口类型不同 | SH1.0-5P + 6P + 3P | SH1.0-**2P** + 3P |
| STM32 数量 | IC，主控 MCU | 1 | **2**（双 MCU 架构） |
| NTC/3.3K/TL431 | 热敏电阻/电阻/电压基准 | 有 | 无 |

#### 三、采购 vs 需求对比表（3×42 + 3×20）

| 器件 | 说明 | 42板×3 | 20板×3 | 合计 | 采购 | 余量 | 判定 |
|------|------|--------|--------|------|------|------|------|
| 0.1uF 0402 16V | 电容，MLCC 去耦 | 45 | 42 | 87 | 100 | +13 | ✅ |
| 10uF 0402 16V | 电容，MLCC 电源滤波 | 21 | 18 | 39 | 100 | +61 | ✅ |
| 100uF 1206 25V | 电容，MLCC 电机电源滤波 | 12 | 6 | 18 | 40 | +22 | ✅ |
| 100pF 0402 50V | 电容，NPO 高频滤波 | 3 | 3 | 6 | 100 | +94 | ✅ |
| 10uF/50V 0402 | 电容，MLCC 高压滤波 | 3 | 0 | 3 | 100 | +97 | ✅ |
| 10nF 0402 50V | 电容，MLCC 去耦 | 3 | 3 | 6 | 100 | +94 | ✅ |
| 220pF 0402 50V | 电容，NPO 滤波 | 3 | 3 | 6 | 100 | +94 | ✅ |
| SS54 DO-214AC | 二极管，肖特基 防反接/续流 | 3 | 3 | 6 | 20 | +14 | ✅ |
| SMBJ28CA | 二极管，TVS 过压保护 | 3 | 3 | 6 | 20 | +14 | ✅ |
| 1N5819W SOD-323 | 二极管，肖特基 整流 | 6 | 6 | 12 | 50 | +38 | ✅ |
| 1N5819WS SOD-323 | 二极管，肖特基 整流 | 3 | 3 | 6 | 100 | +94 | ✅ |
| 跳线 0R 0402 | 电阻，0Ω 跳线 | 3 | 3 | 6 | 16 | +10 | ✅ |
| 按键 3×4×2 | 开关，SMD 贴片按键 | 6 | 6 | 12 | 100 | +88 | ✅ |
| 6.8uH 201612 | 电感，DC-DC 升压储能 | 3 | 3 | 6 | 100 | +94 | ✅ |
| 2.2uH 2520 | 电感，DC-DC 储能 | 3 | 3 | 6 | 20 | +14 | ✅ |
| SH1.0-5P | 连接器，5Pin 卧贴 | 3 | 0 | 3 | 30 | +27 | ✅ |
| SH1.0-6P | 连接器，6Pin 卧贴 | 9 | 0 | 9 | 50 | +41 | ✅ |
| SH1.0-2P | 连接器，2Pin 卧贴 | 0 | 6 | 6 | 20 | +14 | ✅ |
| SH1.0-3P | 连接器，3Pin 卧贴 | 3 | 3 | 6 | 30 | +24 | ✅ |
| 1K 0402 | 电阻，信号限流/上拉 | 15 | 15 | 30 | 40 | +10 | ✅ |
| 0.1R 1206 | 电阻，电流采样 | 6 | 6 | 12 | 50 | +38 | ✅ |
| NTC 10K 0402 | 热敏电阻，温度检测 | 3 | 0 | 3 | 50 | +47 | ✅ |
| 3.3K 0402 | 电阻，分压/偏置 | 3 | 0 | 3 | 100 | +97 | ✅ |
| 10K 0402 | 电阻，上拉/分压 | 12 | 12 | 24 | 100 | +76 | ✅ |
| 120R 0402 | 电阻，CAN 终端匹配 | 6 | 3 | 9 | 100 | +91 | ✅ |
| 1.8K 0402 | 电阻，上拉/限流 | 3 | 3 | 6 | 100 | +94 | ✅ |
| 1M 0402 | 电阻，高阻分压 | 3 | 3 | 6 | 20 | +14 | ✅ |
| 560K 0402 | 电阻，TPS61040 反馈分压 | 3 | 3 | 6 | 16 | +10 | ✅ |
| 93.1K 0402 | 电阻，TPS61040 分压上臂 | 3 | 3 | 6 | 100 | +94 | ✅ |
| 13.3K 0402 | 电阻，TPS61040 分压下臂 | 3 | 3 | 6 | 100 | +94 | ✅ |
| 测试点 | 测试辅助，镀金测试环 | 6 | 6 | 12 | 50 | +38 | ✅ |
| TB67H450FNG | IC，双H桥步进电机驱动 | 6 | 6 | 12 | 20 | +8 | ✅ |
| **STM32F103CBT6** | **IC，ARM Cortex-M3 主控 MCU** | **3** | **6** | **9** | **8** | **-1** | ❌ |
| TL431IDBZT | IC，精密电压基准 | 3 | 0 | 3 | 20 | +17 | ✅ |
| SN65HVD232D | IC，CAN 总线收发器 | 3 | 3 | 6 | 10 | +4 | ✅ |
| ME3116AM6G | IC，DC-DC 降压 40V→5V | 3 | 3 | 6 | 20 | +14 | ✅ |
| LP2992 3.3V | IC，LDO 5V→3.3V | 3 | 3 | 6 | 8 | +2 | ✅ |
| MT6816 | IC，14-bit 磁编码器 SPI | 3 | 3 | 6 | 8 | +2 | ✅ |
| TPS61040DBV | IC，DC-DC 升压转换器 | 3 | 3 | 6 | 8 | +2 | ✅ |
| 12MHz 晶振-3P | 晶振，陶瓷谐振器 HSE 时钟 | 3 | 3 | 6 | 10 | +4 | ✅ |

#### 四、封装验证

所有 39 种器件的采购封装与原理图要求一一核对，**全部正确**。关键确认项：

| 器件 | 说明 | 原理图封装 | 采购封装 | 验证要点 |
|------|------|-----------|---------|---------|
| TB67H450FNG | IC，双H桥电机驱动 | SO-8-PAD | SO-8-PAD | 带散热焊盘，非普通 SO-8 |
| STM32F103CBT6 | IC，主控 MCU | LQFP-48 | LQFP-48 | 型号 B=48脚，T=LQFP |
| SN65HVD232D | IC，CAN 收发器 | SO-8 | SOP-8 | SO-8=SOP-8，仅命名差异 |
| 2.2uH 电感 | 电感，DC-DC 储能 | 2520-L | LQM2HPN2R2MG0L 2520 | 村田型号，1.3A 额定 |
| 12MHz 晶振 | 晶振，HSE 时钟源 | 晶振-3P | CSTNE12M0G520000R0 3Pin | 村田陶瓷谐振器，内置电容 |
| 1N5819W | 二极管，肖特基整流 | SOD-323 | SOD-323 | 卖家标题写"0805"但实际是 SOD-323 |

#### 五、价格差异分析

CSV 总价 336.45 元 vs 淘宝截图合计约 385 元，差额约 48 元。原因：

| 差异项 | 截图价 | CSV价 | 原因 |
|--------|--------|-------|------|
| C3 10uF | 5元(10V) | 9元(16V) | CSV 更新为 16V 版本 |
| C11-C14 100uF | 11元(20个) | 5.5元(10个) | 截图多买了 |
| L2 2.2uH | 6.2元(0.31×20) | 8元(0.40×20) | CSV 换了村田型号 |
| U5 SN65HVD232D | 15元(10×1.5) | 20元(10×2) | 截图单价更低 |
| 其他数量差异 | — | — | 部分器件截图多买 |

#### 六、关键器件型号解读

**STM32F103CBT6（主控 MCU）：**

| 字段 | 含义 |
|------|------|
| STM32 | ARM Cortex-M 系列 |
| F103 | 基础型，Cortex-M3，72MHz |
| C | 256KB Flash |
| **B** | **48 引脚** |
| **T** | **LQFP 封装** |
| 6 | -40~85°C 工业级 |

→ B+T = LQFP-48，与原理图封装完全匹配。

**YSMC201612H-6R8MB（6.8uH 电感）：**

| 字段 | 含义 |
|------|------|
| YSMC | 系列名（一体成型功率电感） |
| **201612** | 封装尺寸 2.0×1.6×1.2mm |
| H | 扁线 T-CORE 结构 |
| **6R8** | **6.8μH**（R = 小数点） |
| M | 精度 ±20% |
| B | 变体后缀 |

→ 封装 201612 = 原理图 IND-201612，电感值 6.8μH，额定电流 1.9A，完全匹配。

---

#### 七、待办

- [ ] 再购买 **1 个 STM32F103CBT6**（差 1 个）
- [ ] 确认 R23 560K 下单时选对阻值（卖家标题含多个值混装）
- [ ] 收货后实测 C20（10uF/50V 0402），该规格罕见，0.032 元/个价格偏低
- [ ] 收货后测试 STM32F103CBT6（4.45 元偏低，可能非原装）

---

### 2026/06/09  xuzhihao-248

**类型**：硬件

**内容**：42 步进电机和 20 步进电机采购选型指南

**原因**：驱动板元器件已下单，下一步采购电机，需提前确认选型参数

**相关文件**：`doc/电路板解析.md`、`doc/固件解析.md`

#### 一、必须匹配的参数

##### 1. 相电流 ≤ TB67H450 驱动能力

TB67H450FNG 是东芝双 H 桥电机驱动芯片，最大输出电流 2A（峰值）。

| 参数 | TB67H450 限制 | 固件默认值 | 采购要求 |
|------|-------------|-----------|---------|
| 额定电流 | 最大 2A（峰值） | 1000mA | **每相电流 ≤ 1.5A 留余量** |
| 校准电流 | — | 2000mA | 短时 2A，电机要能承受 |

电流太小（<0.5A）→ 扭矩不够；电流太大（>2A）→ 芯片过热保护。

##### 2. 步距角 1.8°

固件默认 `steps_per_revolution = 200`（1.8° 步距角）。如果买 0.9° 的电机，需改固件参数为 400，否则实际转速和位置会错。

##### 3. 两相四线制

TB67H450 是双 H 桥，驱动两相步进电机。必须买 4 线（A+/A-/B+/B-）的电机。

| 线制 | 能不能用 | 说明 |
|------|---------|------|
| 4 线（两相） | ✅ | 直接接 |
| 6 线（带中心抽头） | ✅ | 只接 A+/A-/B+/B-，中心抽头悬空 |
| 8 线（可串/并联） | ✅ | 串联接法 |
| 5 线（带公共端） | ❌ | 不能用 |

#### 二、重要但可调的参数

##### 4. 额定电压

步进电机的"额定电压"是绕组电阻 × 额定电流得出的参考值。实际驱动是 PWM 恒流控制，电压只是上限。

| 电机额定电压 | 能不能用 | 说明 |
|------------|---------|------|
| 2.5V~5V | ✅ 常见 | 配 VM=4.8V 供电刚好 |
| 12V | ✅ | 需要 VM ≥12V，否则电流打不到额定值 |
| 24V | ✅ | 需要 VM ≥24V |

**VM（电机电源）电压决定了电机选型。**

##### 5. 保持扭矩（Holding Torque）

| 电机规格 | 典型扭矩 | 建议 |
|---------|---------|------|
| 42 步进（NEMA17） | 0.2~0.6 N·m | 关节 1~3（负载大）选 ≥0.4 N·m |
| 20 步进（NEMA8） | 0.01~0.05 N·m | 关节 4~6（负载小）选 ≥0.02 N·m |

##### 6. 出轴直径

已测量的减速器尺寸：

| 电机 | 出轴直径 | 减速器输入轴 |
|------|---------|------------|
| 42 步进 | Φ5mm | Φ5mm |
| 20 步进 | Φ5mm（部分 Φ4mm） | Φ3mm |

买之前确认出轴直径和减速器匹配。

#### 三、选购 Checklist

```
□ 相电流：0.5A ~ 1.5A（推荐 1A 左右）
□ 步距角：1.8°（200 步/圈）
□ 线制：4 线 或 6 线
□ 额定电压：匹配 VM 供电电压
□ 出轴直径：匹配减速器
□ 扭矩：按关节负载选
□ 引线：带引线方便焊接
```

#### 四、固件可调参数

如果电机参数和默认值不同，固件中可修改（`Ctrl/Motor/motor.h`）：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `steps_per_revolution` | 200 | 1.8°→200，0.9°→400 |
| `micro_steps` | 256 | 细分数 |
| `rated_current` | 1000 | 额定电流 mA |
| `calibration_current` | 2000 | 校准电流 mA |