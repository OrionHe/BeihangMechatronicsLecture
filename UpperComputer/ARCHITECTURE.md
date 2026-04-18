# XY 平台控制系统架构

## 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                     上位机（Windows/Linux）                    │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              PyQt5 图形用户界面 (GUI)                  │   │
│  │  ┌────────────┬───────────┬──────┬────────┬───────┐  │   │
│  │  │ 连接管理    │ 位置显示  │ 日志 │  控制 │ 诊断 │  │   │
│  │  └────────────┴───────────┴──────┴────────┴───────┘  │   │
│  └───────────────────┬─────────────────────────────────┘   │
│                      │                                       │
│  ┌─────────────────────────────────────────────────────┐   │
│  │           命令构建和响应解析                           │   │
│  │  ┌─────────────────────────────────────────────┐   │   │
│  │  │  protocol.py (CommandBuilder/ResponseParser) │   │   │
│  │  └─────────────────────────────────────────────┘   │   │
│  └───────────────────┬─────────────────────────────────┘   │
│                      │                                       │
│  ┌─────────────────────────────────────────────────────┐   │
│  │           USB CDC 通信模块                           │   │
│  │  ┌─────────────────────────────────────────────┐   │   │
│  │  │  usb_comm.py (USBCommunicator)               │   │   │
│  │  │  - 端口扫描                                   │   │   │
│  │  │  - 连接管理                                   │   │   │
│  │  │  - 数据收发                                   │   │   │
│  │  │  - 线程接收循环                               │   │   │
│  │  └─────────────────────────────────────────────┘   │   │
│  └───────────────────┬─────────────────────────────────┘   │
└──────────────────────┼──────────────────────────────────────┘
                       │ USB CDC Virtual COM Port
                       │ (虚拟串口, 115200 baud)
                       │
┌──────────────────────┼──────────────────────────────────────┐
│                      │     嵌入式设备（STM32F4）              │
│                      ▼                                       │
│  ┌─────────────────────────────────────────────────────┐   │
│  │           USB CDC 驱动 (STM32 HAL)                   │   │
│  └───────────────────┬─────────────────────────────────┘   │
│                      │                                       │
│  ┌─────────────────────────────────────────────────────┐   │
│  │     FreeRTOS USB 解析任务                           │   │
│  │  ┌─────────────────────────────────────────────┐   │   │
│  │  │  usb_cmd_parser.h                            │   │   │
│  │  │  - 帧解析                                     │   │   │
│  │  │  - 命令分发                                   │   │   │
│  │  │  - 状态响应                                   │   │   │
│  │  └──────────────┬──────────────────────────────┘   │   │
│  └─────────────────┼─────────────────────────────────┘   │
│                    │                                        │
│  ┌─────────────────────────────────────────────────────┐   │
│  │     XY 平台控制层（现有代码）                        │   │
│  │  ┌─────────────┬──────────────┬─────────────────┐  │   │
│  │  │ LinearModule │ Stepper      │ XYPlatform      │  │   │
│  │  │ (轴驱动)    │ (电机脉冲)   │ (平台协调)  │  │   │
│  │  └─────────────┴──────┬───────┴─────────────────┘  │   │
│  └────────────────────────┼────────────────────────────┘   │
│                          │                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              硬件驱动 (Timer/GPIO)                    │  │
│  │  ┌──────┬─────────┬──────────┬──────────┐           │  │
│  │  │ PWM  │ DIR/EN  │ 限位开关 │ 其他 GPIO │           │  │
│  │  └──────┴─────────┴──────────┴──────────┘           │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
│  ┌──────────────┬──────────────┐                           │
│  │  X 轴电机    │  Y 轴电机    │                           │
│  └──────────────┴──────────────┘                           │
└─────────────────────────────────────────────────────────────┘
```

## 模块说明

### 1. 通信协议 (protocol.py)

**责任**: 定义和处理 USB 帧格式

- **ProtocolFrame**: 帧打包/解包
- **CommandBuilder**: 高级命令接口
- **ResponseParser**: 响应解析
- **命令类型**: HOME, MOVE_ABS, MOVE_REL, LINE_INTERP, ARC_INTERP, STOP, QUERY_STATUS

**流程**:
```
用户输入 → CommandBuilder.move_abs(10, 20, 5000)
         → ProtocolFrame.pack(cmd, data)
         → 返回字节序列 [AA 02 0A ...] 
         → 通过 USB 发送
```

### 2. USB 通信 (usb_comm.py)

**责任**: 管理 USB CDC 虚拟串口通信

- **USBCommunicator**: 主通信类
  - `list_ports()`: 扫描 USB CDC 设备
  - `connect(port)`: 建立连接
  - `send_data(bytes)`: 发送数据
  - `set_data_callback()`: 设置接收回调
  - `_receive_loop()`: 后台接收线程

**特点**:
- 异步接收（后台线程）
- 非阻塞发送
- 自动帧同步（寻找 AA...FF 边界）
- 校验和验证

### 3. 图形界面 (gui.py)

**责任**: 提供友好的用户交互界面

- **MainWindow**: 主窗口
- **XYPlatformController**: 业务逻辑
- **SignalEmitter**: 线程安全信号

**UI 布局**:
```
┌─────────────────────────────────────────────┐
│      ┌─────────────┬──────────┬──────────┐  │
│      │  连接/状态  │   控制   │ 日志诊断 │  │
│      │(1 列)      │(2 列)   │(1 列)   │  │
│      │ - 端口选择  │          │ - 实时   │  │
│      │ - 连接状态  │ 基础控制 │   日志   │  │
│      │ - 坐标显示  │          │ - 查询   │  │
│      │ - 平台状态  │ 直线/圆弧│   状态   │  │
│      │            │ 插补     │          │  │
│      └─────────────┴──────────┴──────────┘  │
└─────────────────────────────────────────────┘
```

### 4. 嵌入式命令解析 (usb_cmd_parser.h)

**责任**: 在STM32 上解析和处理上位机命令

- 帧验证（校验和）
- 命令分发
- 状态查询回复
- 与现有 Stepper/XYPlatform 类的适配

## 数据流

### 发送流程

```
GUI 用户输入
  ↓
XYPlatformController.move_abs(10, 20, 5000)
  ↓
CommandBuilder.move_abs(...) → 字节数组 [AA 02 0A 00 00 20 41 00 00 A0 40 88 13 ...]
  ↓
USBCommunicator.send_data(bytes) → 通过 serial.write()
  ↓
STM32 USB CDC 接收
  ↓（在嵌入式端）
CDC_Receive_FS() → 放入 osMessageQueue
  ↓
StartUsbParseTask() → usb_parse_command()
  ↓
usb_handle_command() → 调用 xy_platform_move_abs()
  ↓
g_linearModule[0/1].stepper.SetMode/SetTargetAngle/MotionConfig()
  ↓
定时器中断触发 ControlLoop()
  ↓
PWM 输出脉冲到电机驱动
  ↓
电机转动
```

### 接收流程

```
电机运动（实时更新位置）
  ↓（在嵌入式端）
ControlLoop() 中 step_current_angle++
  ↓
用户/GUI 发送 QUERY_STATUS 命令
  ↓
usb_handle_command(CMD_QUERY_STATUS)
  ↓
xy_platform_get_status() → 获取当前 x/y/status
  ↓
打包 StatusResponse 帧 [AA F0 0A 00 00 20 41 ...] 
  ↓
CDC_Transmit_FS(response_buf, len)
  ↓
STM32 USB 发送
  ↓
上位机 USBCommunicator._receive_loop() 收到字节
  ↓
识别帧边界，调用 on_data_received() 回调
  ↓
GUI 的 on_usb_data_received()
  ↓
ProtocolFrame.unpack() + ResponseParser.parse_status()
  ↓
MainWindow.on_status_updated() 更新 UI
  ↓
刷新 X/Y 坐标显示、状态指示灯
```

## 关键参数

### 电机和运动参数

| 参数 | 值 | 单位 |
|------|-----|------|
| 步距角 | 1.8 | ° |
| 细分数 | 32 | - |
| 每转脉冲 | 6400 | pulse/rev |
| 丝杆导程 | 4.0 | mm |
| 脉冲分辨率 | 0.025 | mm/pulse |
| 定时器频率 | 1000000 | Hz |
| 最大脉冲频率 | 50000 | Hz |
| 对应最大速度 | 46.875 | mm/s |

### 通信参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 波特率 | 115200 | bps |
| 超时 | 0.1 | s (读) |
| 帧头 | 0xAA | - |
| 帧尾 | 0xFF | - |
| 校验算法 | XOR | - |
| 最大数据长度 | 255 | B |

## 扩展点

### 1. 添加新命令

**步骤**:
1. 在 `protocol.py` 的 `CommandType` 枚举中添加新命令码
2. 在 `CommandBuilder` 中实现构建函数
3. 在嵌入式端 `usb_cmd_parser.h` 的 `usb_handle_command()` 中添加处理
4. 在 GUI 中添加相应的按钮/面板

### 2. 添加实时数据图表显示

可以集成 matplotlib 或 pyqtgraph：
```python
from pyqtgraph import PlotWidget
plot = PlotWidget(title="位置轨迹")
plot.plot(self.trajectory_x, self.trajectory_y)
```

### 3. 支持离线命令队列

```python
class OfflineCommander:
    def __init__(self):
        self.commands = []
    
    def add_command(self, cmd_bytes):
        self.commands.append(cmd_bytes)
    
    def execute_all(self, comm):
        for cmd in self.commands:
            comm.send_data(cmd)
            time.sleep(0.1)
```

### 4. 实现软件限位保护

```python
def safe_move_abs(self, x, y, speed):
    if not (0 <= x <= 100 and 0 <= y <= 100):
        raise ValueError("位置超出范围")
    return self.move_abs(x, y, speed)
```

## 性能优化

### 1. 批量命令

```python
# 不推荐（20 个命令，20 次通信）
for i in range(20):
    comm.send_data(CommandBuilder.move_abs(...))

# 推荐（可以在嵌入式端缓存）
```

### 2. 查询策略

- 自动查询（勾选）: 每 500ms 查询一次
- 手动查询: 按需查询
- 事件驱动: 运动完成时自动回复

### 3. 帧缓冲

接收线程自动缓冲多个帧，避免丢失数据。

## 测试方案

### 单元测试

```bash
python -m pytest protocol.py -v  # 测试打包/解包
python -m pytest usb_comm.py -v  # 测试通信
```

### 集成测试

```bash
python test_protocol.py COM3        # 自动化测试所有命令
python test_protocol.py --list      # 列出可用端口
```

### 功能测试

1. **连接测试**: 连接/断开稳定性
2. **命令测试**: 每种命令是否正确执行
3. **状态显示**: 坐标实时性和准确性
4. **容错测试**: 异常断开、噪声干扰恢复

## 故障排查

| 症状 | 可能原因 | 解决方案 |
|------|-------|---------|
| 无法连接 | USB 驱动缺失 | 安装 CDC 驱动 |
| 坐标不动 | 嵌入式端未启动任务 | 检查 FreeRTOS 配置 |
| 坐标跳变 | 通信干扰或丢帧 | 更换 USB 线缆 |
| 响应延迟 | 查询间隔太短 | 增加 osMessageQueueGet 的超时 |
| 电机不转 | PWM 未输出 | 检查 GPIO/定时器初始化 |
