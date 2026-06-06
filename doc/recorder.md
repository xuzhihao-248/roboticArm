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