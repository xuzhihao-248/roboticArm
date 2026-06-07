# 机械臂开发计划

> 最后更新：2026/06/07

---

- [x] 1. **环境搭建**（06/05）

  **实施方案：** Ubuntu 22.04 虚拟机安装 ROS2 Humble（桌面版 + colcon + rosdep），安装 VMware Tools 实现剪贴板/拖拽共享，配置 SSH Server 供 VSCode Remote-SSH 连接，设置共享文件夹 `D:\code\project\roboticArm` → `/mnt/hgfs/` 实现 Windows↔VM 高速传输，安装 Python uv 包管理器及 Gazebo/MoveIt2 仿真工具

  **预期效果：** VSCode 可远程编辑 VM 代码，`ros2 run demo_nodes_cpp talker` 与 listener 通信成功，共享文件夹可正常读写

  **实际效果：** 全部正常，talker/listener 通信验证通过

  **验收：** `ros2 topic echo /chatter` 能收到消息，`ls /mnt/hgfs/` 能看到 Windows 侧文件

---

- [x] 2. **URDF 可视化**（06/05~06）

  **实施方案：** SolidWorks 中每个关节处手动建参考坐标系（Z 轴对齐旋转轴），使用 SW URDF Exporter 导出 5 轴机械臂 URDF + STL 网格。通过共享文件夹传到 VM，将 ROS1 格式文件转换为 ROS2（package.xml format=3 + ament_cmake、CMakeLists.txt 5 行模板、launch.py 启动 robot_state_publisher + joint_state_publisher_gui + rviz2）。用 `printf` 写入文件避免 Windows 换行符问题。修复 Joint3 上下限相同导致的关节锁死。编译后在 RViz2 中配置 Fixed Frame=base_link、RobotModel Description Topic=/robot_description

  **预期效果：** RViz2 显示完整机械臂 3D 模型，Joint State Publisher 滑块可控制 5 个关节旋转

  **实际效果：** 模型正常显示，滑块可控。过程中遇到 4 个问题均已解决（src/ 多余 CMakeLists.txt、Description Topic 未设置、base_link 躺倒、Windows 隐藏字符）

  **验收：** `ros2 launch newrobot display.launch.py` 启动无报错，RViz2 中机械臂直立，拖动滑块各关节跟随转动

---

- [x] 3. **MoveIt2 运动规划**（06/06）

  **实施方案：** 先编译 URDF 功能包并 source，启动 MoveIt2 Setup Assistant 加载 URDF。配置 planning group（名称 arm，KDL 运动学求解器，添加 joint1~5），添加 home 姿态（全零），配置 ROS2 Controller（JointTrajectoryController）和 MoveIt Controller（FollowJointTrajectory），生成配置包到 `newrobot_moveit_config`。安装 ros-humble-ros2-control 等缺失依赖后启动 demo

  **预期效果：** RViz 中显示机器人模型，可拖动末端球体规划路径，Plan & Execute 使模型沿规划路径运动

  **实际效果：** 全部正常。过程中遇到 4 个问题：Setup Assistant 闪退（需先编译再 source）、controller_manager 缺失（需装 ros2_control 包）、重影（关闭 Scene Robot 显示）、move_group 崩溃（VMware 内存不足）

  **验收：** `ros2 launch newrobot_moveit_config demo.launch.py` 启动无报错，RViz 中 MotionPlanning 面板可拖动末端、点击 Plan & Execute 机器人执行运动

---

- [ ] 4. **硬件选型与采购**

  **实施方案：** 确定电机类型（伺服/步进/BLDC）、扭矩需求、是否带 CAN 接口或需外挂驱动板。确定驱动板型号，确认 CAN 协议和报文格式。选配 USB-CAN 适配器（如 CANable、PCAN USB）。收到硬件后逐件验收：电机通电转动、驱动板 CAN 收发、适配器在 Ubuntu 下 socketcan 识别

  **预期效果：** 电机 + 驱动板 + USB-CAN 三件套齐全，Ubuntu 下 `ip link show can0` 能识别 CAN 接口

  **实际效果：**

  **验收：** `candump can0` 能收到驱动板反馈报文，电机通电后手动发 CAN 指令可转动

---

- [ ] 5. **单电机 CAN 通信调试**

  **实施方案：** Ubuntu 安装 can-utils 和 python3-can，配置 socketcan（`ip link set can0 type can bitrate 1000000`）。用 Python 脚本对单个电机发位置指令，确认电机 ID（1~6）、CAN 波特率、报文格式。逐个电机测试：发目标位置 → 等待到位 → 读编码器反馈，确认闭环控制正常

  **预期效果：** 每个电机可独立通过 CAN 指令控制到指定位置，编码器反馈位置与目标一致

  **实际效果：**

  **验收：** 6 个电机逐个测试通过，位置误差在可接受范围内（如 ±1°），无丢帧/超时

---

- [ ] 6. **ros2_control 硬件接口开发**

  **实施方案：** 编写 ros2_control 硬件接口节点，实现 `hardware_interface::SystemInterface`，将关节位置指令转为 CAN 报文发送，将 CAN 编码器反馈转为关节状态发布。修改 MoveIt 配置包的 controller 设置，将 joint_trajectory_controller 输出对接到硬件接口。先接 3 个电机做端到端联调

  **预期效果：** MoveIt 中拖动末端点击 Plan & Execute，真实电机按规划轨迹运动

  **实际效果：**

  **验收：** 3 个电机端到端联动，MoveIt 规划路径与实际电机运动一致，无明显延迟或丢步

---

- [ ] 7. **6 轴完整联调**

  **实施方案：** 在 SolidWorks 中为第 6 轴（末端执行器）建参考坐标系，导出新的 6 轴 URDF。MoveIt Setup Assistant 更新规划组加入第 6 个 joint。6 个电机全部接入，逐轴验证后做全轴联动测试。最后加装减速器，重新标定关节限位和控制参数

  **预期效果：** 6 个电机在 MoveIt 控制下协调运动，加减速器后扭矩满足负载需求

  **实际效果：**

  **验收：** MoveIt 中 6 轴联动规划执行无报错，各关节运动范围符合 URDF 限位，减速器安装后电机不堵转、不过热
