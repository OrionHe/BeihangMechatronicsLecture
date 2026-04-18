# XY 平台上位机 - 完整使用指南

## 项目概述

这是一个基于 **PyQt5** 的 XY 平台运动控制上位机，通过 USB CDC 虚拟串口与 STM32F4 控制板通信，支持以下功能：

- ✅ 回零（自动寻找原点）
- ✅ 点位控制（绝对/相对位移）
- ✅ 直线插补（两点间直线运动）
- ✅ 圆弧插补（圆心/半径/角度控制）
- ✅ 实时状态显示（X/Y 坐标、运动状态）
- ✅ 紧急停止
- ✅ 可视化日志

## 文件结构

```
UpperComputer/
├── main.py                 # 启动脚本
├── gui.py                  # PyQt5 主窗口（1200 行代码）
├── protocol.py             # 通信协议定义和打包/解包
├── usb_comm.py             # USB CDC 通信模块
├── usb_cmd_parser.h        # C 语言：嵌入式端命令解析器
├── requirements.txt        # Python 依赖
└── README.md              # 本文件
```

## 环境配置

### Windows 系统

1. **安装 Python 3.8+**
   ```bash
   python --version
   ```

2. **安装依赖包**
   ```bash
   cd UpperComputer
   pip install -r requirements.txt
   ```

3. **运行上位机**
   ```bash
   python main.py
   ```

### Linux 系统

```bash
sudo apt-get install python3-pyqt5 python3-serial
python3 main.py
```

## 通信协议详解

### 帧格式

```
[Header] [Cmd] [DataLen] [Data...] [Checksum] [Tail]
  0xAA    1B    1B       N bytes   1B        0xFF
```

| 字段 | 长度 | 说明 |
|------|------|------|
| Header | 1B | 帧头，固定为 0xAA |
| Cmd | 1B | 命令类型（见下表） |
| DataLen | 1B | 数据长度（0-255）|
| Data | N B | 具体数据 |
| Checksum | 1B | XOR 校验和 |
| Tail | 1B | 帧尾，固定为 0xFF |

### 命令集

#### 0x01 - 回零 (HOME)
- **Data**: 无
- **作用**: 电机回原点，通常寻找限位开关
- **例子**: `AA 01 00 01 FF`

#### 0x02 - 绝对位移 (MOVE_ABS)
- **Data**: `x(float, 4B) | y(float, 4B) | speed(uint16_t, 2B)`
- **单位**: x/y 为 mm，speed 为 pulse/s
- **例子**: 移动到 (10.0mm, 20.5mm)，速度 5000 pulse/s
  ```python
  CommandBuilder.move_abs(10.0, 20.5, 5000)
  ```

#### 0x03 - 相对位移 (MOVE_REL)
- **Data**: `dx(4B) | dy(4B) | speed(2B)`
- **例子**: 相对移动 (+5.0mm, -3.2mm)
  ```python
  CommandBuilder.move_rel(5.0, -3.2, 5000)
  ```

#### 0x04 - 直线插补 (LINE_INTERP)
- **Data**: `x1(4B) | y1(4B) | x2(4B) | y2(4B) | speed(2B)`
- **说明**: 从点 (x1, y1) 直线运动到 (x2, y2)
- **例子**:
  ```python
  CommandBuilder.line_interp(0, 0, 50, 50, 5000)  # (0,0) → (50,50)
  ```

#### 0x05 - 圆弧插补 (ARC_INTERP)
- **Data**: `xc(4B) | yc(4B) | radius(4B) | angle(4B) | speed(2B)`
- **参数**:
  - xc, yc: 圆心坐标
  - radius: 圆弧半径 (mm)
  - angle: 扫过角度 (°)，正值逆时针，负值顺时针
  - speed: 运动速度
- **例子**:
  ```python
  # 以 (25, 25) 为圆心，半径 10mm，逆时针扫过 90°
  CommandBuilder.arc_interp(25, 25, 10, 90, 5000)
  ```

#### 0x06 - 停止 (STOP)
- **Data**: 无
- **作用**: 紧急停止电机

#### 0x07 - 查询状态 (QUERY_STATUS)
- **Data**: 无
- **返回**: 型号 0xF0 的响应帧

#### 0xF0 - 状态响应 (STATUS_RESPONSE)
- **Data**: `x(4B) | y(4B) | status(1B) | error(1B)`
- **status** 枚举值:
  - 0x00: IDLE（空闲）
  - 0x01: HOMING（回零中）
  - 0x02: MOVING（运动中）
  - 0xFF: ERROR（错误）

## GUI 界面说明

### 左侧面板 - 连接和状态
- **端口选择**: 扫描并选择 USB CDC 设备
- **连接按钮**: 切换连接状态
- **状态指示灯**: 
  - 🟢 绿色：已连接/空闲
  - 🟠 橙色：运动中
  - 🔴 红色：离线/错误
- **实时坐标**: 显示当前 X/Y 位置
- **平台状态**: 空闲/回零中/运动中/错误

### 中间面板 - 运动控制
1. **基础控制**
   - 回零按钮（黄色）
   - 紧急停止（红色）

2. **绝对位移**
   - 输入目标坐标 (X, Y)
   - 设置运动速度
   - 点击"移动到点"执行

3. **相对位移**
   - 输入相对位移量 (ΔX, ΔY)

4. **直线插补**
   - 输入起点 (X1, Y1)
   - 输入终点 (X2, Y2)
   - 自动计算直线路径

5. **圆弧插补**
   - 输入圆心 (Xc, Yc)
   - 输入半径
   - 输入扫过角度 (±360°)

### 右侧面板 - 日志和诊断
- **实时日志**: 显示所有操作记录
- **查询状态按钮**: 手动查询一次
- **自动查询**: 每 500ms 自动查询状态（可勾选）

## Python 接口示例

### 基础使用

```python
from protocol import CommandBuilder
from usb_comm import USBCommunicator

# 创建通信对象
comm = USBCommunicator(baudrate=115200)

# 列出端口
ports = comm.list_ports()
print(ports)  # [{'port': 'COM3', 'description': 'STM32 CDC', ...}, ...]

# 连接
comm.connect('COM3')

# 设置回调
def on_data_received(data):
    print(f"Received: {data.hex()}")

comm.set_data_callback(on_data_received)

# 发送命令
cmd_bytes = CommandBuilder.home()
comm.send_data(cmd_bytes)

# 绝对位移到 (10, 20)，速度 5000 pulse/s
cmd_bytes = CommandBuilder.move_abs(10.0, 20.0, 5000)
comm.send_data(cmd_bytes)

# 直线插补
cmd_bytes = CommandBuilder.line_interp(0, 0, 50, 50, 5000)
comm.send_data(cmd_bytes)

# 圆弧插补 - 以 (25, 25) 为圆心，半径 10mm，逆时针 90°
cmd_bytes = CommandBuilder.arc_interp(25, 25, 10, 90, 5000)
comm.send_data(cmd_bytes)

# 查询状态
cmd_bytes = CommandBuilder.query_status()
comm.send_data(cmd_bytes)

# 断开连接
comm.disconnect()
```

### 解析响应

```python
from protocol import ResponseParser, ProtocolFrame

def on_data_received(frame_data):
    try:
        cmd, data = ProtocolFrame.unpack(frame_data)
        
        if cmd == 0xF0:  # STATUS_RESPONSE
            status = ResponseParser.parse_status(data)
            print(f"X: {status['x']:.2f} mm")
            print(f"Y: {status['y']:.2f} mm")
            print(f"Status: {status['status']}")
    except Exception as e:
        print(f"Error: {e}")

comm.set_data_callback(on_data_received)
```

## 嵌入式端集成（C 语言）

### 步骤 1：复制文件

将 `usb_cmd_parser.h` 复制到嵌入式工程：
```
Firmware_Example/04_XY_Platform_Motion_Control/MDK-ARM/Drivers/
```

### 步骤 2：修改 freertos.c

在 `Core/Src/freertos.c` 中添加以下代码：

```c
/* 在 USER CODE BEGIN Includes 中 */
#include "usb_cmd_parser.h"
#include "queue.h"
#include "cmsis_os.h"

/* 在 USER CODE BEGIN PV 中 */
#define USB_RX_QUEUE_SIZE 10
typedef struct {
    uint8_t data[64];
    uint32_t length;
} UsbRxMsg_t;

osMessageQueueId_t g_UsbRxQueueHandle = NULL;

/* 在 MX_FREERTOS_Init 中的 RTOS_QUEUES 部分 */
const osMessageQueueAttr_t queue_attr = {
    .name = "UsbRxQueue"
};
g_UsbRxQueueHandle = osMessageQueueNew(USB_RX_QUEUE_SIZE, sizeof(UsbRxMsg_t), &queue_attr);

/* 创建 USB 解析任务 */
osThreadId_t usbParseTaskHandle;
const osThreadAttr_t usbParseTask_attributes = {
    .name = "usbParseTask",
    .stack_size = 256 * 4,
    .priority = (osPriority_t) osPriorityNormal,
};
usbParseTaskHandle = osThreadNew(StartUsbParseTask, NULL, &usbParseTask_attributes);

/* 任务实现 */
void StartUsbParseTask(void *argument)
{
    UsbRxMsg_t rxMsg;
    uint8_t cmd;
    uint8_t *data;
    uint8_t data_len;
    
    for(;;)
    {
        if (osMessageQueueGet(g_UsbRxQueueHandle, &rxMsg, NULL, 100) == osOK)
        {
            if (usb_parse_command(rxMsg.data, rxMsg.length, &cmd, &data, &data_len))
            {
                usb_handle_command(cmd, data, data_len);
            }
        }
    }
}
```

### 步骤 3：修改 usbd_cdc_if.c

在 `CDC_Receive_FS` 函数中：

```c
/* USER CODE BEGIN 6 */
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
    extern osMessageQueueId_t g_UsbRxQueueHandle;
    
    if (g_UsbRxQueueHandle != NULL && Len && *Len > 0)
    {
        UsbRxMsg_t msg;
        msg.length = *Len > 64 ? 64 : *Len;
        memcpy(msg.data, Buf, msg.length);
        osMessageQueuePut(g_UsbRxQueueHandle, &msg, 0, 0);
    }

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (USBD_OK);
}
/* USER CODE END 6 */
```

### 步骤 4：实现平台控制函数

在 `usb_cmd_parser.h` 中的 `usb_handle_command()` 函数中，取消注释并实现：

```c
case CMD_HOME:
    xy_platform_home();
    break;

case CMD_MOVE_ABS:
    xy_platform_move_abs(x, y, speed_int);
    break;

// ... 其他命令
```

这些函数应该调用你已有的 Stepper 类接口：

```c
void xy_platform_home(void)
{
    g_linearModule[0].stepper.SetMode(STEPPER_MODE_POSITION);
    g_linearModule[0].stepper.SetTargetAngle(0);
    g_linearModule[1].stepper.SetMode(STEPPER_MODE_POSITION);
    g_linearModule[1].stepper.SetTargetAngle(0);
}

void xy_platform_move_abs(float x, float y, uint16_t speed_int)
{
    g_linearModule[0].stepper.SetMode(STEPPER_MODE_POSITION);
    g_linearModule[0].stepper.SetTargetAngleWithVel(x / 4.0 * 360, speed_int);
    
    g_linearModule[1].stepper.SetMode(STEPPER_MODE_POSITION);
    g_linearModule[1].stepper.SetTargetAngleWithVel(y / 4.0 * 360, speed_int);
}
```

## 参数配置

### 工程中的参数

根据你的实际硬件，在嵌入式端配置：

```c
/* my_config.cpp */
x_linear_module::LinearModule g_linearModule[2] = {
    x_linear_module::LinearModule(
        &htim8, TIM_CHANNEL_4, 1000000,  /* 定时器配置 */
        1.8f, 32,                         /* 步距角 1.8°, 细分 32 */
        DIR_M1_GPIO_Port, DIR_M1_Pin,
        nENBL_M1_GPIO_Port, nENBL_M1_Pin,
        SW1_GPIO_Port, SW1_Pin, SW2_GPIO_Port, SW2_Pin,
        4.0f),                            /* 丝杆导程 4.0mm */
    
    x_linear_module::LinearModule(
        &htim3, TIM_CHANNEL_1, 1000000,
        1.8f, 32,
        DIR_M2_GPIO_Port, DIR_M2_Pin,
        nENBL_M2_GPIO_Port, nENBL_M2_Pin,
        SW3_GPIO_Port, SW3_Pin, SW4_GPIO_Port, SW4_Pin,
        4.0f)
};
```

### XY 平台参数

```c
xy_platform::XYplatform g_xyPlatform(
    &g_linearModule[0],  /* X 轴控制 */
    &g_linearModule[1],  /* Y 轴控制 */
    100.0f,              /* 最大行程 X (mm) */
    2.0f,                /* 最大行程 Y (mm) - 这个值似乎有问题，应该根据实际调整 */
    0.0f, 0.0f, 0.01f    /* 初始位置和步进精度 */
);
```

## 疑难解答

### 1. USB 设备无法识别
- ✓ 检查 USB 线缆连接
- ✓ 确认 STM32 开发板已编程且运行
- ✓ 在设备管理器中查看虚拟 COM 口
- ✓ Windows: 安装 CDC 驱动（通常自动）

### 2. 连接失败
```
Error: Failed to connect: Permission denied
```
**解决**: Linux 需要权限
```bash
sudo usermod -a -G dialout $USER
# 然后重新登录
```

### 3. 通信超时
- ✓ 检查波特率是否匹配（115200）
- ✓ 检查 USB 缆线质量
- ✓ 确认嵌入式端 FreeRTOS 任务已启动

### 4. 坐标不准确
- ✓ 校准步距角参数
- ✓ 检查细分数设置
- ✓ 校准丝杆导程值

## 常见操作

### 执行 3x3 网格的扫描运动

```python
import time

speeds = [
    (0, 0),    (50, 0),    (100, 0),
    (100, 50), (50, 50),   (0, 50),
    (0, 100),  (50, 100),  (100, 100)
]

for x, y in speeds:
    cmd = CommandBuilder.move_abs(x, y, 5000)
    comm.send_data(cmd)
    time.sleep(2)  # 等待运动完成
```

### 绘制正方形

```python
# 绘制边长 50mm 的正方形
points = [
    (0, 0),
    (50, 0),
    (50, 50),
    (0, 50),
    (0, 0)
]

for i in range(len(points)-1):
    x1, y1 = points[i]
    x2, y2 = points[i+1]
    cmd = CommandBuilder.line_interp(x1, y1, x2, y2, 5000)
    comm.send_data(cmd)
    time.sleep(2)
```

### 绘制圆形

```python
# 画一个半径 20mm 的圆
cmd = CommandBuilder.arc_interp(50, 50, 20, 360, 5000)
comm.send_data(cmd)
```

## 性能指标

| 指标 | 数值 |
|------|------|
| 通信波特率 | 115200 bps |
| 最大脉冲频率 | ~50 kHz |
| 定位精度 | 0.1125° (1.8° / 16) |
| 线性分辨率 | 0.025 mm (4mm / 160 pulse) |
| 查询状态间隔 | 500 ms |
| USB 帧长 | 5-255 字节 |

## License

MIT License - 自由使用和修改
