# 直线模组 GUI 架构说明

本文档描述 `Firmware_Example/03_USB_Communication_And_Interaction/gui` 当前源码对应的实际上位机架构。

## 目录结构

```text
gui/
├── main.py             # 启动入口，补充 sys.path 后调用 gui.main()
├── gui.py              # PyQt5 主窗口、图表和交互逻辑
├── protocol.py         # 协议枚举、帧封装和响应解析
├── usb_comm.py         # USB CDC 串口通信与后台接收线程
├── requirements.txt    # Python 依赖
├── README.md           # 使用说明
├── QUICKSTART.md       # 快速上手
└── ARCHITECTURE.md     # 本文档
```

运行时还可能出现 `__pycache__/`，属于 Python 生成物，不属于架构的一部分。

## 总体分层

```text
main.py
  -> gui.main()
      -> MainWindow
          -> ModuleController(X/Y)
              -> CommandBuilder
              -> USBCommunicator
          -> XYPlotCanvas / SpeedPlotCanvas
          -> SignalEmitter

protocol.py
  -> CommandType / ModuleID / PlatformStatus
  -> ProtocolFrame
  -> CommandBuilder
  -> ResponseParser

usb_comm.py
  -> USBCommunicator
      -> pyserial
      -> 后台接收线程
```

职责上可以拆成四层：

1. 启动层：`main.py` 只负责导入并启动 GUI。
2. 界面层：`gui.py` 负责窗口布局、用户输入、图表和日志显示。
3. 业务控制层：`ModuleController` 将单轴动作翻译成协议命令。
4. 通信层：`protocol.py` 定义协议，`usb_comm.py` 负责串口收发和帧边界识别。

## 核心模块

### 1. `protocol.py`

这个文件定义了上位机与固件之间的通信协议。

- `CommandType`
  - `HOME = 0x01`
  - `MOVE_ABS = 0x02`
  - `SET_VELOCITY = 0x03`
  - `STOP = 0x06`
  - `QUERY_STATUS = 0x07`
  - `STATUS_RESPONSE = 0xF0`
- `ModuleID`
  - `X_AXIS = 0x00`
  - `Y_AXIS = 0x01`
- `PlatformStatus`
  - `IDLE`
  - `HOMING`
  - `MOVING`
  - `ERROR`

`ProtocolFrame` 负责帧打包和解包，固定格式为：

```text
[0xAA][CMD][LEN][DATA...][CHECKSUM][0xFF]
```

其中校验算法是对 `cmd`、`len` 和全部 `data` 字节做 XOR。

`CommandBuilder` 提供 GUI 使用的高级接口：

- `home(axis)`
- `move_abs(axis, position, speed)`
- `set_velocity(axis, velocity)`
- `stop(axis=None)`，`None` 表示全部停止
- `query_status(axis=None)`，`None` 表示查询全部轴

`ResponseParser.parse_status()` 解析 19 字节状态载荷：

```text
x_pos(4B) | y_pos(4B) | x_status(1B) | y_status(1B) | x_vel(4B) | y_vel(4B) | error(1B)
```

## 2. `usb_comm.py`

`USBCommunicator` 是通信唯一入口。

主要职责：

- 枚举串口：`list_ports()`
- 建立连接：`connect(port)`
- 断开连接：`disconnect()`
- 发送数据：`send_data(data)`
- 注册回调：`set_data_callback(callback)`
- 后台接收：`_receive_loop()`

实现特点：

- 基于 `pyserial`
- 发送和接收都通过 `Lock` 保护
- 接收线程持续从串口读取字节流
- 在缓冲区中查找 `0xAA ... 0xFF` 完整帧
- 完整帧到达后，回调到 GUI 层做协议解析

这里的通信层只负责“字节流到完整帧”，不负责业务语义。

## 3. `gui.py`

### 3.1 关键类

- `SignalEmitter`
  - `status_updated`
  - `log_message`
  - `error_occurred`
  - `connected`
- `XYPlotCanvas`
  - 绘制 XY 位置和历史轨迹
  - 轨迹缓存使用 `deque(maxlen=500)`
- `SpeedPlotCanvas`
  - 绘制 X/Y 两轴速度曲线
  - 曲线缓存使用 `deque(maxlen=300)`
- `ModuleController`
  - 每个轴一个实例
  - 封装 `home / move_abs / set_velocity / stop`
  - 统一处理“未连接”“发送成功/失败”的日志与错误提示
- `MainWindow`
  - 创建整个 UI
  - 持有 `USBCommunicator`
  - 持有两个 `ModuleController`
  - 定时查询状态并驱动界面刷新

### 3.2 UI 结构

主窗口使用横向 `QSplitter` 分成左右两栏。

左侧控制区：

- 连接组
  - 串口下拉框
  - 刷新按钮
  - 连接/断开按钮
- 状态组
  - 连接指示灯
  - X/Y 位置
  - X/Y 速度
  - X/Y 状态
- 基础控制组
  - 全部回零
  - 全部停止
  - 清除轨迹
- 单轴控制组
  - X 轴控制
  - Y 轴控制
  - 每组包含回零、停止、目标位置、速度、移动、设置速度

右侧信息区：

- `XYPlotCanvas`
- `SpeedPlotCanvas`
- 日志面板

右侧内部再通过纵向 `QSplitter` 划分图表和日志。

### 3.3 定时机制

`MainWindow` 中定义：

```python
STATUS_QUERY_INTERVAL_MS = 100
```

连接成功后启动 `QTimer`，每 100 ms 发送一次 `query_status()`。

## 数据流

### 命令发送链路

```text
用户点击按钮
  -> MainWindow / ModuleController
  -> CommandBuilder 生成命令帧
  -> USBCommunicator.send_data()
  -> 串口发送到下位机
```

### 状态接收链路

```text
下位机返回状态帧
  -> USBCommunicator._receive_loop()
  -> on_usb_data_received(frame)
  -> ProtocolFrame.unpack()
  -> ResponseParser.parse_status()
  -> SignalEmitter.status_updated.emit()
  -> MainWindow.on_status_updated()
  -> 刷新标签、XY 轨迹图、速度曲线
```

## 线程模型

界面线程：

- Qt 事件循环
- 所有控件更新
- 定时状态查询

后台线程：

- `USBCommunicator._receive_loop()`
- 只做串口读取和帧切分

线程间协作方式：

- 接收线程通过数据回调把完整帧交回 GUI
- GUI 再通过 `SignalEmitter` 把状态更新切回 Qt 主线程

## 当前实现的架构特点

- 双轴控制是通过两个 `ModuleController` 实例实现的，而不是更通用的设备列表。
- 协议层支持“按轴发送命令”和“查询全部轴状态”。
- 界面同时提供位置轨迹图和速度趋势图，旧文档里只描述轨迹图是不完整的。
- 轨迹图显示范围当前固定为 `-10 ~ 310 mm`，并绘制 `300 mm x 300 mm` 工作框。
- 单轴目标位置输入范围是 `-100 ~ 300 mm`，速度输入范围是 `0.0 ~ 10.0 mm/s`。

## 可扩展点

如果要增加新轴或新命令，主要落点如下：

1. 在 `protocol.py` 扩展 `ModuleID` 或 `CommandType`。
2. 在 `CommandBuilder` 和 `ResponseParser` 补充打包/解析逻辑。
3. 在 `gui.py` 中新增控制器实例和对应面板。
4. 如涉及新的状态字段，还需要同步扩展状态显示和图表逻辑。
