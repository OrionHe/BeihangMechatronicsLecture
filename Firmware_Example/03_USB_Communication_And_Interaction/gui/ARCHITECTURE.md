# GUI 架构设计说明

## 设计目标

- ✓ 支持两个独立直线模组 (XY 轴) 的控制
- ✓ 实时显示位置和速度信息
- ✓ 绘制运动轨迹，便于验证运动规划
- ✓ 易于扩展到三轴或更多轴

## 架构概览

```
┌─────────────────────────────────────────────────────┐
│                   GUI 主窗口 (MainWindow)            │
├─────────────────┬───────────────────────────────────┤
│  左侧控制面板   │     右侧图表和日志面板             │
│  ┌──────────┐   │  ┌──────────────────────────┐    │
│  │连接控制  │   │  │  XY 轨迹图 (pyplot)     │    │
│  ├──────────┤   │  │  • 当前位置 (红点)      │    │
│  │状态显示  │   │  │  • 历史轨迹 (蓝线)      │    │
│  ├──────────┤   │  └──────────────────────────┘    │
│  │基础控制  │   │  ┌──────────────────────────┐    │
│  │（全軸）  │   │  │     操作日志 QTextEdit   │    │
│  ├──────────┤   │  │  • [HH:MM:SS] 操作记录   │    │
│  │X轴控制   │   │  │  • [HH:MM:SS] 状态更新   │    │
│  │Y轴控制   │   │  └──────────────────────────┘    │
│  └──────────┘   │                                   │
└─────────────────┴───────────────────────────────────┘
```

## 核心模块

### 1. protocol.py - 通信协议

**责任：** 定义和处理 USB 通信帧格式

**核心类：**
- `CommandType`: 命令类型枚举
- `ModuleID`: 轴 ID (X轴=0, Y轴=1)
- `ProtocolFrame`: 帧打包/解包
- `CommandBuilder`: 命令构建工厂
- `ResponseParser`: 响应数据解析

**关键方法：**
```python
CommandBuilder.home(ModuleID.X_AXIS)           # 生成回零命令
CommandBuilder.move_abs(ModuleID.X_AXIS, 50, 1000)  # 生成移动命令
ProtocolFrame.pack(cmd, data)                  # 打包为字节序列
ProtocolFrame.unpack(frame_data)               # 解包字节序列
ResponseParser.parse_status(data)              # 解析设备状态
```

**帧格式：**
```
Byte 0     |  Byte 1    |  Byte 2  |  Byte 3..N-2  |  Byte N-1  |  Byte N
───────────┼────────────┼──────────┼───────────────┼────────────┼──────────
0xAA       |  CMD       |  LEN     |  DATA[LEN]    |  CHECKSUM  |  0xFF
(header)   |  (命令)    | (长度)   |  (数据)       |  (XOR)     | (tail)
```

### 2. usb_comm.py - USB 连接管理

**责任：** 管理 USB CDC 虚拟串口的连接和数据传输

**核心类：**
- `USBCommunicator`: 串口通信封装

**关键方法：**
```python
comm = USBCommunicator(baudrate=115200)
comm.list_ports()                              # 列举 USB 设备
comm.connect(port)                             # 连接
comm.send_data(cmd_bytes)                      # 发送
comm.set_data_callback(callback)               # 设置接收回调
```

**特点：**
- 后台接收线程，不阻塞 UI
- 自动帧同步 (找 0xAA 头开始)
- 线程安全的发送和接收

### 3. gui.py - 主程序

**责任：** 构建 UI、协调控制和通信

**核心类：**

#### SignalEmitter
- PyQt5 信号发射器
- 线程间安全通信
- 信号类型：
  - `status_updated`: 设备状态更新
  - `log_message`: 日志消息
  - `error_occurred`: 错误报告
  - `connected`: 连接状态变化

#### XYPlotCanvas
- Matplotlib 图表嵌入 PyQt5
- 功能：
  - 绘制 XY 工作区域背景
  - 实时绘制当前位置 (红点)
  - 绘制历史轨迹 (蓝线)
  - 支持清除轨迹

#### ModuleController
- 单个轴的控制器
- 封装轴特定命令
- 管理轴的状态

```python
class ModuleController:
    home()                          # 回零
    move_abs(pos, speed)            # 绝对位移
    set_velocity(velocity)          # 设置速度
    stop()                          # 停止
```

#### MainWindow
- 主 UI 窗口
- 创建和管理所有控制组件

**关键信号-槽连接：**
```python
# USB 数据接收
comm.set_data_callback(self.on_usb_data_received)
  ↓
ProtocolFrame.unpack()
  ↓
signal_emitter.status_updated
  ↓
on_status_updated()
  ↓
更新标签和图表
```

**定时器：**
- `status_timer`: 每 100ms 查询一次设备状态

## 数据流

### 命令发送流程
```
UI 按钮点击
  ↓
ModuleController.move_abs(pos, speed)
  ↓
CommandBuilder.move_abs()
  ↓
ProtocolFrame.pack()
  ↓
USBCommunicator.send_data()
  ↓
设备接收
```

### 状态接收流程
```
定时器 (100ms)
  ↓
MainWindow.on_query_status()
  ↓
CommandBuilder.query_status()
  ↓
USBCommunicator.send_data()
  ↓
设备返回状态响应
  ↓
后台接收线程
  ↓
ProtocolFrame.unpack()
  ↓
signal_emitter.status_updated.emit()
  ↓
MainWindow.on_status_updated()
  ↓
更新 UI 标签: x_pos_label, y_pos_label, ...
  ↓
XYPlotCanvas.update_current_position()
  ↓
绘制新位置点和轨迹
```

## UI 布局

### 左侧控制面板 (1/3 宽度)
1. **连接** (QGroupBox)
   - 端口选择下拉框
   - 刷新按钮
   - 连接/断开按钮

2. **状态** (QGroupBox)
   - 连接指示灯 (●)
   - X/Y 位置显示
   - X/Y 速度显示

3. **基础控制** (QGroupBox)
   - 全部回零 (橙色)
   - 全部停止 (红色)
   - 清除轨迹

4. **X轴/Y轴控制** (可滚动)
   - 回零按钮
   - 停止按钮
   - 目标位置输入 (mm)
   - 速度输入 (pulse/s)
   - 移动按钮
   - 设置速度按钮

### 右侧信息面板 (2/3 宽度)
1. **XY 轨迹图**
   - Matplotlib 图表
   - 显示范围: -5~105 mm (可配置)
   - 等比例显示 (1:1)

2. **操作日志**
   - 只读 QTextEdit
   - 带时间戳
   - 自动滚动到最新

## 扩展指南

### 添加第三个轴 (Z轴)

1. 在 `protocol.py` 更新 `ModuleID`：
```python
class ModuleID(IntEnum):
    X_AXIS = 0x00
    Y_AXIS = 0x01
    Z_AXIS = 0x02  # 新增
```

2. 在 `gui.py` 初始化控制器：
```python
self.controller_z = ModuleController(ModuleID.Z_AXIS, "Z轴", self.comm, self.signal_emitter)
```

3. 在 `create_control_panel()` 添加控制组：
```python
motion_layout.addWidget(self.create_axis_control_group("Z轴", self.controller_z, "z"))
```

### 修改 UI 配色
编辑 `apply_theme()` 中的 QSS 样式表

### 更改图表范围
修改 `XYPlotCanvas.__init__()` 中的 `set_xlim()` 和 `set_ylim()`

## 依赖关系

```
gui.py
  ├── PyQt5 (UI 框架)
  ├── matplotlib (图表绘制)
  ├── protocol.py (通信协议)
  │   └── struct, enum (标准库)
  └── usb_comm.py (USB 通信)
      └── pyserial (串口驱动)
```

## 性能考虑

1. **状态查询间隔**：100ms
   - 可在代码中修改 `STATUS_QUERY_INTERVAL_MS`
   - 过短会增加 USB 负荷，过长会降低响应性

2. **轨迹历史长度**：最大 500 点
   - deque 中配置 `maxlen=500`
   - 自动丢弃最旧的点

3. **日志输出**：无限制
   - 可能导致内存占用增加
   - 建议每个会话清空或定期重启

## 线程模型

```
主线程 (PyQt5 事件循环)
  ├── UI 事件处理 (同步)
  ├── 定时器 (status_timer)
  └── 信号-槽机制

后台线程 (USBCommunicator._receive_loop)
  └── 串口接收 (异步)
      └── 帧同步和回调
          └── signal_emitter.status_updated.emit()
              └── 回到主线程处理
```

## 一致性和错误处理

- **通信错误**：显示在日志中，不中断 UI
- **解析错误**：捕获异常，记录错误信息
- **连接丢失**：状态指示灯变红，按钮禁用
- **命令失败**：用户得到即时反馈

## 测试建议

1. **连接测试**：验证端口扫描和连接
2. **通信测试**：发送各类命令，检查响应
3. **轨迹测试**：绘制简单图形 (正方形、圆形等)，验证位置准确性
4. **压力测试**：快速发送多个命令，检查队列处理

## 已知限制

- 假设单一设备连接 (多设备需要重新设计)
- 硬编码 X/Y 两个轴 (第三轴需要代码修改)
- 暂无命令历史或脚本录制功能
- 不支持设备功能发现 (固定协议)
