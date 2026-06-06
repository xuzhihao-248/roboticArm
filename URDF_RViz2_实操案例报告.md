# SolidWorks URDF → ROS2 RViz2 仿真显示 实操案例报告

> **日期：** 2026-06-06
> **项目：** 5轴机械臂 URDF 可视化
> **环境：** Windows 10 + VMware + Ubuntu 22.04 + ROS2 Humble
> **标准包名：** `newrobot`

---

## 一、环境总览

| 组件 | 版本/路径 |
|------|----------|
| 宿主机 | Windows 10 Home China 10.0.19045 |
| 虚拟化 | VMware Workstation Pro |
| 虚拟机 | Ubuntu 22.04 LTS |
| ROS2 | Humble Hawksbill（LTS 至 2027-05） |
| Python | 3.10 |
| 工作空间 | `~/ros2_ws/` |
| SolidWorks | Windows 侧安装，导出 URDF/STL |
| 项目目录 | `D:\code\project\roboticArm\` |
| 共享文件夹 | `D:\code\project\roboticArm\` → VM `/mnt/hgfs/` |
| 连接方式 | VSCode + Remote-SSH 扩展连接虚拟机 |

---

## 二、完整流程（SolidWorks → RViz2）

### 2.1 SolidWorks 导出 URDF

使用 **SW URDF Exporter** 插件，每个零件导出为 STL 网格文件，配合关系转换为 URDF joint。

**导出目录结构（Windows 侧）：**

```
D:\code\project\roboticArm\my\urdf\newrobot\
├── CMakeLists.txt          # 构建配置（ROS1 格式，需转换）
├── package.xml             # 包描述（ROS1 格式，需转换）
├── urdf/
│   └── newrobot.urdf       # 机器人描述文件
├── meshes/
│   ├── base_link.STL       # 底座网格
│   ├── link1.STL           # 连杆1网格
│   ├── link2.STL           # ...
│   ├── link3.STL
│   ├── link4.STL
│   └── link5.STL
├── launch/
│   └── display.launch      # ROS1 launch 文件（需转换为 .launch.py）
└── export.log              # 导出日志（可删除）
```

**URDF 关节定义：**

| Joint | 类型 | 限位 | 说明 |
|-------|------|------|------|
| joint1 | continuous | 无限制 | 底座旋转 |
| joint2 | revolute | ±1.047 rad (±60°) | 大臂俯仰 |
| joint3 | revolute | ±1.047 rad (±60°) | 小臂俯仰 |
| joint4 | continuous | 无限制 | 腕部旋转 |
| joint5 | revolute | ±2.094 rad (±120°) | 末端旋转 |

**坐标系说明：** SolidWorks 默认 Z-up Y-forward，ROS 是 X-forward Z-up。插件导出时自动转换，前提是每个关节处手动建了参考坐标系让 Z 轴对齐旋转轴。

---

### 2.2 通过 VMware 共享文件夹传输到 VM

**原理：** VMware 在宿主机内存里开一块区域，宿主机写入，VM 直接读取——不走网络，磁盘级速度（几百 MB/s）。比 SCP/SSH 快得多。

**Windows 侧配置（只需一次）：**

VMware → 右键虚拟机 → 设置 → 选项 → 共享文件夹 → 勾选"总是启用" → 添加 → 选择 `D:\code\project\roboticArm` → 名称填 `roboticArm`

**Ubuntu 侧挂载（每次开机或设置自动挂载）：**

```bash
# 手动挂载
sudo mkdir -p /mnt/hgfs
sudo vmhgfs-fuse .host:/roboticArm /mnt/hgfs -o allow_other

# 开机自动挂载（写入 fstab）
echo '.host:/roboticArm /mnt/hgfs fuse.vmhgfs-fuse allow_other 0 0' | sudo tee -a /etc/fstab
```

**路径映射：**

| Windows 路径 | VM 路径 |
|-------------|---------|
| `D:\code\project\roboticArm\` | `/mnt/hgfs/` |
| `D:\code\project\roboticArm\my\urdf\newrobot\` | `/mnt/hgfs/my/urdf/newrobot/` |

---

### 2.3 ROS1 → ROS2 文件转换

SolidWorks SW URDF Exporter 导出的是 ROS1 格式，需要手动转换为 ROS2。

#### 2.3.1 package.xml

**ROS1 导出的原始文件有问题**，需要替换为以下内容：

```xml
<package format="3">
  <name>newrobot</name>
  <version>1.0.0</version>
  <description>URDF Description package for newrobot</description>
  <author>xzh</author>
  <maintainer email="TODO@email.com">TODO</maintainer>
  <license>BSD</license>
  <buildtool_depend>ament_cmake</buildtool_depend>
  <depend>robot_state_publisher</depend>
  <depend>rviz2</depend>
  <depend>joint_state_publisher_gui</depend>
  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

**关键变化：**
- `format="3"` — ROS2 使用 format 3
- `<buildtool_depend>ament_cmake</buildtool_depend>` — ROS2 用 ament 而非 catkin
- `<build_type>ament_cmake</build_type>` — 声明构建类型

#### 2.3.2 CMakeLists.txt

替换为以下 **5行内容**（不能有前导空格、多余空行）：

```cmake
cmake_minimum_required(VERSION 3.8)
project(newrobot)
find_package(ament_cmake REQUIRED)
install(DIRECTORY launch meshes urdf DESTINATION share/${PROJECT_NAME})
ament_package()
```

**逐行解释：**

| 行 | 作用 |
|----|------|
| `cmake_minimum_required(VERSION 3.8)` | 指定 CMake 最低版本，ROS2 Humble 要求 ≥3.8 |
| `project(newrobot)` | 定义项目名称，`${PROJECT_NAME}` 变量来源 |
| `find_package(ament_cmake REQUIRED)` | 查找 ROS2 的 ament 构建系统 |
| `install(DIRECTORY launch meshes urdf ...)` | 将 launch、meshes、urdf 目录安装到 share 目录，供运行时访问 |
| `ament_package()` | 必须放在最后一行，标记这是一个 ament 包 |

**⚠️ 重要注意事项：**
- 不要从 Windows 直接复制 CMakeLists.txt 到 VM，Windows 的换行符和隐藏字符会导致 CMake 解析失败
- 安全做法：在 VM 中用 `printf` 或 `cat` 直接写入

#### 2.3.3 launch/display.launch.py

创建 `launch/display.launch.py`（ROS2 使用 Python 格式的 launch 文件）：

```python
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    pkg_path = get_package_share_directory('newrobot')
    with open(os.path.join(pkg_path, 'urdf', 'newrobot.urdf'), 'r') as f:
        robot_desc = f.read()

    return LaunchDescription([
        Node(package='joint_state_publisher_gui', executable='joint_state_publisher_gui'),
        Node(package='robot_state_publisher', executable='robot_state_publisher',
             parameters=[{'robot_description': robot_desc}]),
        Node(package='rviz2', executable='rviz2'),
    ])
```

**三个节点的作用：**

| 节点 | 包 | 作用 |
|------|----|------|
| `joint_state_publisher_gui` | joint_state_publisher_gui | 提供 GUI 滑块，手动控制每个关节的角度 |
| `robot_state_publisher` | robot_state_publisher | 读取 URDF，发布 TF 坐标变换和 `/robot_description` 话题 |
| `rviz2` | rviz2 | 3D 可视化工具，显示机器人模型和坐标系 |

#### 2.3.4 删除 ROS1 遗留文件

```bash
# 在包目录下删除这些文件
rm -f launch/display.launch    # ROS1 XML 格式 launch
rm -f launch/gazebo.launch     # ROS1 Gazebo launch
rm -f export.log               # 导出日志，无用
```

---

### 2.4 编译与运行

```bash
# 1. 清空旧的编译产物（重要！避免缓存冲突）
cd ~/ros2_ws
rm -rf build install log

# 2. 删除旧包（如果存在）
rm -rf ~/ros2_ws/src/newrobot

# 3. 从共享文件夹拷贝到工作空间
cp -r /mnt/hgfs/my/urdf/newrobot ~/ros2_ws/src/

# 4. 检查 src 目录下没有多余的 CMakeLists.txt（关键！）
ls ~/ros2_ws/src/CMakeLists.txt 2>/dev/null && rm -f ~/ros2_ws/src/CMakeLists.txt

# 5. 编译
cd ~/ros2_ws
colcon build --symlink-install

# 6. 加载环境
source ~/ros2_ws/install/setup.bash

# 7. 启动
ros2 launch newrobot display.launch.py
```

**命令解释：**

| 命令 | 作用 |
|------|------|
| `rm -rf build install log` | 清空编译缓存，确保从头编译，避免旧缓存导致的诡异错误 |
| `cp -r /mnt/hgfs/... ~/ros2_ws/src/` | 从共享文件夹拷贝包到 ROS2 工作空间的 src 目录 |
| `ls ... 2>/dev/null && rm -f ...` | 检查并删除 src 下多余的 CMakeLists.txt，`2>/dev/null` 抑制"文件不存在"的报错 |
| `colcon build --symlink-install` | 编译所有包；`--symlink-install` 用软链接安装，Python 脚本修改后无需重新编译 |
| `source install/setup.bash` | 加载编译后的环境变量，让 ROS2 能找到新编译的包 |
| `ros2 launch newrobot display.launch.py` | 启动 launch 文件，同时打开三个节点 |

---

### 2.5 RViz2 显示设置

启动 RViz2 后需要手动配置：

1. **Fixed Frame** → 改为 `base_link`（左上角下拉框）
2. **Add** → 选择 `RobotModel`（左下角 Add 按钮）
3. **展开 RobotModel** → 确认 **Description Topic** 为 `/robot_description`
4. **查看 Status** → 应显示绿色 `OK`

**操作说明：**
- 旋转视角：按住 **Shift + 鼠标左键** 拖拽
- 平移视角：按住 **Ctrl + 鼠标左键** 拖拽
- 缩放：**鼠标滚轮**

---

## 三、已知问题与解决方案

### 问题1：CMakeLists.txt 重复导致编译失败

**问题：**

编译时报错 `CMake Error at CMakeLists.txt:7: Parse error. Expected "(", got newline`，但包内的 CMakeLists.txt 只有5行，内容完全正确。

**原因：**

`~/ros2_ws/src/` 根目录下存在一个**多余的 CMakeLists.txt**。这是之前用 heredoc 命令（`cat > file << 'EOF'`）创建文件时，命令没有正确执行，导致内容被写到了 `src/` 目录而不是 `src/newrobot/` 子目录下。CMake 在处理包时读取到了这个错误文件，导致解析失败。

可通过以下命令确认：

```bash
cat ~/ros2_ws/src/CMakeLists.txt
```

如果输出包含 `EOF` 标记和 package.xml 内容，就是这个错误文件。

**解决方案：**

```bash
# 删除错误的文件
rm ~/ros2_ws/src/CMakeLists.txt

# 清空编译缓存，重新编译
rm -rf ~/ros2_ws/build ~/ros2_ws/install ~/ros2_ws/log
colcon build --symlink-install
```

> **经验：** 不要用 heredoc 从 Windows 向 VM 写文件，容易出错。推荐用 `printf` 逐行写入，或直接在 VSCode 中编辑。每次编译前检查 `src/` 下只有子文件夹，没有散落的 CMakeLists.txt。

---

### 问题2：RViz2 中只有 TF 坐标轴，不显示 3D 模型

**问题：**

`ros2 launch` 启动成功，RViz2 能打开，能看到各关节的 TF 坐标轴，但**看不到机械臂的 3D 网格模型**。

**原因：**

RViz2 中 RobotModel 的 **Description Topic** 没有正确设置为 `/robot_description`。`robot_state_publisher` 节点将 URDF 内容发布到 `/robot_description` 话题，但 RViz2 的 RobotModel 插件默认可能没有订阅这个话题，或话题名为空。

排查确认了以下环节均正常：STL 文件格式、mesh 安装路径、URDF 中 `package://` 路径、joint limits、节点运行状态、TF 发布。问题最终定位到 RViz2 的话题配置上。

**解决方案：**

在 RViz2 左侧面板中：
1. 展开 `RobotModel`
2. 找到 `Description Topic`
3. 手动输入 `/robot_description`
4. 按回车确认

模型立即出现。

> **经验：** 如果只看到 TF 坐标轴但没有 3D 模型，首先检查 RobotModel 的 Description Topic 是否为 `/robot_description`。

---

### 问题3：RViz2 中鼠标无法旋转 3D 视角

**问题：**

在 RViz2 中用鼠标左键拖拽，无法旋转 3D 视角。

**原因：**

RViz2 的鼠标操作与普通 3D 软件不同，不支持直接左键拖拽旋转：
- **旋转视角**：Shift + 鼠标左键拖拽
- **平移视角**：Ctrl + 鼠标左键拖拽
- **缩放**：鼠标滚轮

直接用左键拖拽会操作 RViz2 的其他交互功能（如选择物体），而不是旋转视角。

**解决方案：**

使用 **Shift + 鼠标左键** 拖拽即可旋转视角。

另外，在 VMware 虚拟机中使用 RViz2 时，建议启用 3D 加速（虚拟机设置 → 显示器 → 加速3D图形），以获得更流畅的操作体验。

---

### 问题4：机械臂在 RViz2 中水平放置（应该直立）

**问题：**

机械臂模型在 RViz2 中显示为水平躺倒状态，而不是预期的直立姿态。base_link 的 Z 轴没有指向竖直方向，整个机械臂"躺"在了水平面上。

**可能原因分析：**

这个问题的根本原因是 **base_link 的参考坐标系方向不正确**，但具体是哪个环节导致的，存在以下几种可能：

**可能性 ①：SW URDF Exporter 自动生成的坐标系方向错误（最可能）**

SW URDF Exporter 在导出时，会为每个 link 生成一个参考坐标系。如果用户没有手动指定，插件会**自动推断**坐标系方向。对于 base_link，插件可能基于以下逻辑生成坐标系：
- 使用装配体中第一个零件的局部坐标系
- 使用装配体全局原点的方向
- 使用零件自身的惯性主轴

这些自动生成的坐标系**不一定符合 ROS 的 Z-up 约定**。如果插件生成的坐标系 Z 轴指向了水平方向（例如指向了机械臂的前方或侧方），那么整个机械臂在 ROS 中就会表现为躺倒状态。

**可能性 ②：SolidWorks 装配体坐标系与 ROS 约定不一致**

SolidWorks 装配体的全局坐标系方向取决于建模时的设置。虽然 SolidWorks 默认是 Z-up，但：
- 如果建模时旋转过装配体的坐标系方向
- 如果第一个零件放置时的方向与全局坐标系不一致
- 如果装配体使用了不同的"前视基准面"方向

都会导致插件自动推断的坐标系与 ROS 的 X-forward Z-up 约定不匹配。

**可能性 ③：base_link 的 `<origin>` 中 rpy 值不正确**

URDF 中 base_link 的 `<origin>` 标签定义了 link 相对于父坐标系的位置和姿态。如果导出时 rpy（roll-pitch-yaw）值不为零，就说明插件做了某种坐标变换，这个变换可能将 Z 轴旋转到了非竖直方向。

```xml
<!-- 可能导出成了这样（Z 轴被旋转到了水平方向） -->
<joint name="fixed_joint" type="fixed">
  <origin xyz="..." rpy="1.5708 0 0" />  <!-- 90度旋转 -->
</joint>
```

**可能性 ④：STL 网格本身的原点偏移**

STL 文件的几何中心不一定在坐标原点。如果 base_link 的 STL 网格在 SolidWorks 导出时，原点被设置在了零件的某个角落或重心位置，而不是预期的底座中心，虽然这不会直接导致"躺倒"，但可能与其他因素叠加，造成姿态异常。

**最终解决方案（已验证有效）：**

在 SolidWorks 中**手动为 base_link 创建参考坐标系**，确保方向正确：

1. 打开 SolidWorks 装配体
2. 选中 base_link 零件
3. 菜单：**插入 → 参考几何体 → 坐标系**
4. 手动设置：
   - **原点**：base_link 底座中心
   - **Z 轴**：竖直向上（与 ROS Z-up 约定一致）
   - **X 轴**：机械臂前方（与 ROS X-forward 约定一致）
   - **Y 轴**：由右手定则自动确定
5. 在 SW URDF Exporter 中，将 base_link 的坐标系**绑定到这个手动创建的参考坐标系**（而非使用自动生成的）
6. 重新导出 URDF

> **经验：** SW URDF Exporter 自动生成的坐标系**不可靠**，尤其对于 base_link。正确做法是：
> - **base_link**：手动建参考坐标系，Z 轴竖直向上，X 轴朝前
> - **每个关节**：手动建参考坐标系，Z 轴对齐旋转轴
> - **不要依赖插件的自动推断**，这是 SolidWorks → ROS 坐标系映射中最容易出错的环节
>
> 如果发现模型方向不对，优先检查 base_link 的坐标系方向——它是整个运动链的根节点，它的方向错了，所有子连杆都会跟着错。

---

## 四、标准文件模板（newrobot）

以下是从 SolidWorks 导出后，需要在 VM 中创建/替换的全部文件。以 `newrobot` 包名为例，新项目替换包名即可。

### 4.1 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.8)
project(newrobot)
find_package(ament_cmake REQUIRED)
install(DIRECTORY launch meshes urdf DESTINATION share/${PROJECT_NAME})
ament_package()
```

### 4.2 package.xml

```xml
<package format="3">
  <name>newrobot</name>
  <version>1.0.0</version>
  <description>URDF Description package for newrobot</description>
  <author>xzh</author>
  <maintainer email="TODO@email.com">TODO</maintainer>
  <license>BSD</license>
  <buildtool_depend>ament_cmake</buildtool_depend>
  <depend>robot_state_publisher</depend>
  <depend>rviz2</depend>
  <depend>joint_state_publisher_gui</depend>
  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

### 4.3 launch/display.launch.py

```python
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    pkg_path = get_package_share_directory('newrobot')
    with open(os.path.join(pkg_path, 'urdf', 'newrobot.urdf'), 'r') as f:
        robot_desc = f.read()

    return LaunchDescription([
        Node(package='joint_state_publisher_gui', executable='joint_state_publisher_gui'),
        Node(package='robot_state_publisher', executable='robot_state_publisher',
             parameters=[{'robot_description': robot_desc}]),
        Node(package='rviz2', executable='rviz2'),
    ])
```

---

## 五、快速重建指南（标准流程）

从 SolidWorks 重新导出一个新的 URDF 包时，按以下步骤操作。以包名 `newrobot` 为例。

### 第1步：清理旧包

```bash
cd ~/ros2_ws
rm -rf build install log
rm -rf ~/ros2_ws/src/newrobot
```

> **为什么要清 build/install/log：** CMake 会缓存上一次的编译配置。如果旧缓存引用了已删除的文件或不同的目录结构，会导致诡异的编译错误。清空后从头编译最安全。

### 第2步：从共享文件夹拷贝

```bash
cp -r /mnt/hgfs/my/urdf/newrobot ~/ros2_ws/src/
```

### 第3步：检查并清理 src 目录

```bash
# 查看 src 下的内容，应该只有子文件夹
ls ~/ros2_ws/src/

# 如果有散落的 CMakeLists.txt，删除它
rm -f ~/ros2_ws/src/CMakeLists.txt
```

> **为什么要检查：** 之前的 heredoc 操作可能在 src/ 下留下了错误的 CMakeLists.txt，它会干扰编译。

### 第4步：替换 CMakeLists.txt

```bash
cd ~/ros2_ws/src/newrobot

# 用 printf 安全写入（不要用 heredoc，不要从 Windows 复制）
printf 'cmake_minimum_required(VERSION 3.8)\nproject(newrobot)\nfind_package(ament_cmake REQUIRED)\ninstall(DIRECTORY launch meshes urdf DESTINATION share/${PROJECT_NAME})\nament_package()\n' > CMakeLists.txt
```

> **为什么用 printf 而不是 heredoc：** heredoc 在跨平台操作时容易引入隐藏字符（Windows 换行符 `^M`、BOM 头等），导致 CMake 解析失败。`printf` 逐行写入最干净。

### 第5步：替换 package.xml

```bash
cat > package.xml << 'PKGEOF'
<package format="3">
  <name>newrobot</name>
  <version>1.0.0</version>
  <description>URDF Description package for newrobot</description>
  <author>xzh</author>
  <maintainer email="TODO@email.com">TODO</maintainer>
  <license>BSD</license>
  <buildtool_depend>ament_cmake</buildtool_depend>
  <depend>robot_state_publisher</depend>
  <depend>rviz2</depend>
  <depend>joint_state_publisher_gui</depend>
  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
PKGEOF
```

### 第6步：删除 ROS1 遗留文件

```bash
rm -f launch/display.launch launch/gazebo.launch export.log
```

> **为什么要删：** 这些是 SW URDF Exporter 自动生成的 ROS1 格式文件。ROS2 不识别 XML 格式的 launch 文件，保留它们可能导致混淆。

### 第7步：创建 ROS2 launch 文件

```bash
mkdir -p launch
cat > launch/display.launch.py << 'LAUNCHEOF'
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    pkg_path = get_package_share_directory('newrobot')
    with open(os.path.join(pkg_path, 'urdf', 'newrobot.urdf'), 'r') as f:
        robot_desc = f.read()

    return LaunchDescription([
        Node(package='joint_state_publisher_gui', executable='joint_state_publisher_gui'),
        Node(package='robot_state_publisher', executable='robot_state_publisher',
             parameters=[{'robot_description': robot_desc}]),
        Node(package='rviz2', executable='rviz2'),
    ])
LAUNCHEOF
```

### 第8步：检查 URDF 中的包名

```bash
grep -i "package://" urdf/*.urdf
```

确认输出中的路径格式为 `package://newrobot/meshes/xxx.STL`，包名必须与 package.xml 中的 `<name>` 一致。

### 第9步：编译

```bash
cd ~/ros2_ws
colcon build --symlink-install
```

> **`--symlink-install` 的作用：** 用软链接代替文件拷贝安装 Python 脚本。修改 Python 文件后无需重新编译，直接生效。

### 第10步：运行

```bash
source install/setup.bash
ros2 launch newrobot display.launch.py
```

### 第11步：RViz2 设置

1. **Fixed Frame** → 改为 `base_link`
2. **Add** → 选择 `RobotModel`
3. **展开 RobotModel** → **Description Topic** → 输入 `/robot_description`
4. 确认 Status 显示绿色 `OK`

---

## 六、下一步：仿真规划

当前已完成 **RViz2 可视化**（手动拖动关节滑块查看模型），后续可以向真正的仿真推进：

### 阶段1：Gazebo 物理仿真

```bash
sudo apt install -y ros-humble-gazebo-ros-pkgs
```

Gazebo 可以模拟：重力、摩擦力、碰撞检测、电机力矩控制、传感器（摄像头/激光雷达/IMU）、物理环境交互。

需要额外做的工作：
- 在 URDF 中添加 `<gazebo>` 标签（摩擦系数、惯性参数等）
- 添加传动装置 `<transmission>` 定义
- 编写 Gazebo launch 文件
- 配置控制器（`ros2_control`）

### 阶段2：MoveIt2 运动规划

```bash
sudo apt install -y ros-humble-moveit
```

MoveIt2 可以：正/逆运动学求解、路径规划（避障）、碰撞检测、抓取规划。

需要额外做的工作：
- 创建 MoveIt2 配置包（`moveit_setup_assistant`）
- 定义运动学链（planning group）
- 配置运动学求解器
- 添加场景物体和障碍物

### 阶段3：联合仿真

Gazebo + MoveIt2 联合使用：在 Gazebo 中模拟物理环境，用 MoveIt2 做运动规划，实现完整的机器人仿真。

---

## 七、常用 ROS2 命令速查

| 命令 | 用途 |
|------|------|
| `colcon build --symlink-install` | 编译工作空间所有包 |
| `colcon build --packages-select <pkg>` | 只编译指定包 |
| `source install/setup.bash` | 加载编译后的环境变量 |
| `ros2 launch <pkg> <launch.py>` | 启动 launch 文件 |
| `ros2 node list` | 列出当前运行的所有节点 |
| `ros2 topic list` | 列出所有话题 |
| `ros2 topic echo /<topic>` | 监听指定话题的数据 |
| `ros2 topic info /<topic>` | 查看话题的发布者/订阅者信息 |
| `ros2 run <pkg> <node>` | 直接运行包中的某个节点 |
| `rosdep install -i --from-path src --rosdistro humble -y` | 自动安装依赖 |

---

## 八、排错清单

遇到问题时，按以下顺序排查：

| # | 检查项 | 命令/操作 |
|---|--------|----------|
| 1 | src/ 下有没有多余的 CMakeLists.txt | `ls ~/ros2_ws/src/CMakeLists.txt 2>/dev/null` |
| 2 | CMakeLists.txt 有没有 Windows 隐藏字符 | `cat -An CMakeLists.txt` |
| 3 | 编译缓存是否干净 | `ls ~/ros2_ws/build/` |
| 4 | URDF 中包名是否与 package.xml 一致 | `grep "package://" urdf/*.urdf` |
| 5 | STL 文件是否存在于 meshes/ | `ls meshes/` |
| 6 | 节点是否都在运行 | `ros2 node list` |
| 7 | TF 是否正常发布 | `ros2 topic echo /tf` |
| 8 | robot_description 话题是否有内容 | `ros2 topic echo /robot_description --once` |
| 9 | RViz2 Description Topic 设置 | 手动检查 RobotModel 面板，确认为 `/robot_description` |
| 10 | Fixed Frame 设置 | 确认为 `base_link` |
| 11 | 机械臂躺倒 / 方向不对 | 检查 SolidWorks 中 base_link 参考坐标系 Z 轴是否竖直向上 |
