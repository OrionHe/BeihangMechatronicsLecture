# 直线模组控制系统 GUI 统一说明

本文档是 `Firmware_Example/03_USB_Communication_And_Interaction/gui` 目录的统一说明文档，内容覆盖目录结构、功能模块、运行方式、通信协议、界面组成、数据流、调试与扩展点。

## 1. 简介与快速上手

### 1.1 项目简介
这是一个基于 `PyQt5` 的双直线模组上位机，通过 USB CDC 虚拟串口与下位机通信，提供 X/Y 两轴的基础控制、状态监控和可视化调试界面。

当前已实现的主要能力：
- USB 串口扫描、连接与断开
- X/Y 单轴回零、停止、绝对位移
- X/Y 单轴速度设置
- 全部回零、全部停止
- 平台状态周期查询
- 实时坐标显示、XY 轨迹显示
- X/Y 轴速度曲线显示
- 日志输出与通信/解析错误提示

### 1.2 运行环境与依赖
- Python 3.8+
- Windows 或 Linux
- 下位机已枚举为 USB CDC 虚拟串口

**安装依赖：**
```bash
pip install -r requirements.txt
```

当前依赖见 `requirements.txt`：`PyQt5`, `matplotlib`, `pyserial`。

### 1.3 启动方式
在当前目录下运行：
```bash
python main.py
```

也可以直接运行：
```bash
python gui.py
```

### 1.4 基本使用流程
1. 点击“刷新”，扫描可用串口。
2. 选择下位机对应端口。
3. 点击“连接”建立 USB 通信。
4. 在 X/Y 轴控制面板中输入目标位置和速度。
5. 点击“移动到目标”“设置速度”“回零”或“停止”执行控制动作。
6. 通过右侧轨迹图、速度曲线和日志观察运行结果。
7. 需要中断运动时点击“全部停止”。

---

## 2. 软件架构与模块

### 2.1 目录概览
```text
gui/
├── main.py             # 启动入口
├── gui.py              # 主窗口、控制面板、图表显示和交互逻辑
├── protocol.py         # 协议定义、命令打包、响应解析
├── usb_comm.py         # USB CDC 串口通信与后台接收线程
├── requirements.txt    # Python 依赖
└── README.md           # 本文档
```

### 2.2 总体架构
整体分为三层架构：
1. **启动入口层**：`main.py`
2. **应用层**：`gui.py`（包含主界面、单轴控制器、轨迹画板、速度画板和信号发射器）
3. **协议与通信层**：`protocol.py` + `usb_comm.py`

整体调用关系：
```text
main.py
  -> gui.main()
      -> MainWindow
          -> USBCommunicator (负责底层串口通信)
          -> ModuleController(X/Y) (负责单轴动作控制)
              -> CommandBuilder (打包控制命令)
              -> ResponseParser(状态响应解析)
          -> XYPlotCanvas / SpeedPlotCanvas (轨迹和速度显示)
          -> SignalEmitter (Qt 信号机制)
```

### 2.3 核心模块说明
- **`main.py`**：轻量级入口，只负责将当前目录加入模块搜索路径并启动 GUI。
- **`gui.py`**：核心应用层。包含主窗口 (`MainWindow`)、单轴控制器 (`ModuleController`)、轨迹画布 (`XYPlotCanvas`)、速度画布 (`SpeedPlotCanvas`) 以及用于跨线程通信的 `SignalEmitter`。
- **`protocol.py`**：协议层，包含命令定义 (`CommandType`)、轴 ID 定义 (`ModuleID`)、状态定义 (`PlatformStatus`)、协议帧打包/解包 (`ProtocolFrame`)，以及命令构建 (`CommandBuilder`) 和状态响应解析 (`ResponseParser`)。
- **`usb_comm.py`**：通信层，封装串口扫描、连接、断开、发送和后台接收线程 (`_receive_loop`)。接收线程从字节流中提取完整协议帧后，通过回调传递给上层。

### 2.4 数据流与线程模型
- **主线程（UI 线程）**：负责 Qt 事件循环、界面绘制、用户输入响应、日志显示和图表刷新。
- **后台线程（通信线程）**：负责串口数据接收和字节流切帧。收到完整帧后回调到 `MainWindow.on_usb_data_received()`。
- **发送路径**：UI 交互 -> `ModuleController` -> `CommandBuilder` 生成协议帧 -> `USBCommunicator.send_data()` 串口发送。
- **接收路径**：下位机状态帧 -> 串口接收线程 -> `ProtocolFrame.unpack()` 解包 -> `ResponseParser.parse_status()` 解析 -> `SignalEmitter` 发射信号 -> 主线程更新 UI。
- **状态查询**：连接成功后启动 `QTimer`，每 100 ms 发送一次 `QUERY_STATUS`。

---

## 3. GUI 功能与界面交互

主界面由左右两个区域组成。左侧负责连接、状态和控制输入，右侧负责轨迹图、速度图和日志输出。界面事件通过控制层发送命令，不直接拼接协议帧。

### 3.1 左侧控制区
- **连接组**：端口选择、刷新、连接/断开。
- **状态组**：连接状态、X/Y 当前位置、X/Y 当前速度、X/Y 当前运行状态。
- **基础控制组**：全部回零、全部停止、清除轨迹。
- **X/Y 轴控制组**：单轴回零、单轴停止、绝对位移、速度设置。

### 3.2 右侧显示区
- **XY 轨迹图**：显示 300 mm x 300 mm 工作区、当前位置点和历史轨迹。
- **速度曲线图**：显示 X/Y 两轴的实时速度变化。
- **日志面板**：实时输出连接状态、命令发送记录和错误信息。

---

## 4. 通信协议

### 4.1 帧格式
所有数据按以下格式发送：
```text
[0xAA] [CMD] [LEN] [DATA...] [CHECKSUM] [0xFF]
```

- **Header (1B)**：固定 `0xAA`
- **Cmd (1B)**：命令字
- **Len (1B)**：数据负载长度
- **Data (N B)**：负载数据
- **Checksum (1B)**：`cmd ^ len ^ data[0] ^ data[1] ^ ...`
- **Tail (1B)**：固定 `0xFF`

### 4.2 命令集
| 命令 | 代码 | 数据格式 | 说明 |
|------|------|----------|------|
| HOME | `0x01` | `axis(uint8)` | 指定轴回零 |
| MOVE_ABS | `0x02` | `axis(uint8) position(float32) speed(uint16)` | 指定轴绝对位移 |
| SET_VELOCITY | `0x03` | `axis(uint8) velocity(uint16)` | 设置指定轴速度 |
| STOP | `0x06` | `axis(uint8)` | 停止指定轴 |
| QUERY_STATUS | `0x07` | `axis(uint8)` | 查询指定轴或全部轴状态 |
| STATUS_RESPONSE | `0xF0` | 见 4.4 | 下位机状态响应 |

### 4.3 轴 ID
- `0x00`：X 轴
- `0x01`：Y 轴
- `0xFF`：全部轴（用于停止或状态查询）

### 4.4 状态响应 (`0xF0`)
状态响应负载长度为 19 字节，格式如下：
```text
x_pos(float32)
y_pos(float32)
x_status(uint8)
y_status(uint8)
x_vel(float32)
y_vel(float32)
error(uint8)
```

对应 Python 解析格式为：
```python
struct.unpack('<ffBBffB', data[:19])
```

**Status (1B)**：
- `IDLE = 0x00`
- `HOMING = 0x01`
- `MOVING = 0x02`
- `ERROR = 0xFF`

---

## 5. 常见问题与调试

### 5.1 调试方法
- **GUI 日志**：通过界面右侧日志面板查看连接、命令发送和解析错误。
- **终端日志**：`gui.py` 中已配置 `logging.basicConfig(level=logging.DEBUG)`，可在终端查看更详细的通信日志。
- **协议排查**：优先确认下位机帧头、帧尾、长度、校验和和 `STATUS_RESPONSE` 数据长度是否与本文档一致。

### 5.2 常见问题排查
- **扫描不到设备**：检查 USB 线缆、下位机供电和 USB CDC 枚举状态，点击“刷新”重新扫描。
- **连接失败**：检查串口是否被其他程序占用，并确认通信波特率为 115200。
- **点击控制按钮无响应**：确认界面已经连接设备；未连接时控制器会在日志中提示“未连接到设备”。
- **解析响应出错**：通常是下位机返回帧格式、校验和或状态响应长度与上位机协议不一致。
- **坐标或速度不更新**：检查下位机是否周期或按查询返回 `STATUS_RESPONSE (0xF0)`。

---

## 6. 维护与扩展建议

### 6.1 扩展点
如果后续需要增加功能，请参考以下位置：
1. **新指令/响应**：在 `protocol.py` 增加 `CommandType`、打包方法和解析逻辑。
2. **新轴控制**：在 `ModuleID` 中添加枚举值，并在 `gui.py` 中初始化新的 `ModuleController`。
3. **新 UI 控件**：在 `MainWindow.create_control_panel()` 或对应的 `create_*_group()` 方法中扩展界面。
4. **图表调整**：修改 `XYPlotCanvas` 或 `SpeedPlotCanvas` 的坐标范围、缓存长度和绘制样式。

### 6.2 添加新的轴控制示例
```python
self.controller_z = ModuleController(ModuleID.Z_AXIS, "Z轴", self.comm, self.signal_emitter)
motion_layout.addWidget(self.create_axis_control_group("Z轴", self.controller_z, "z"))
```

同时需要同步修改：
- `protocol.py` 中的 `ModuleID`
- 下位机协议支持
- `ResponseParser.parse_status()` 的状态数据结构
- UI 状态显示字段

### 6.3 维护须知
本 `README.md` 是当前 GUI 目录的统一说明入口。如遇架构、协议、文件结构或界面功能变更，请同步更新此文档，避免说明与代码实现不一致。
