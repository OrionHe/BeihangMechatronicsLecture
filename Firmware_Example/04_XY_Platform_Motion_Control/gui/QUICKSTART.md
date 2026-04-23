# 快速入门指南

## 5 分钟快速开始

### 第 1 步：安装依赖 (1 分钟)

```bash
cd UpperComputer
pip install -r requirements.txt
```

### 第 2 步：连接硬件 (1 分钟)

- 用 USB 线连接 STM32 开发板到电脑
- 确保 STM32 已编程并运行

### 第 3 步：启动 GUI (1 分钟)

**Windows:**
```bash
python main.py
```
或双击 `run.bat`

**Linux:**
```bash
python3 main.py
```

### 第 4 步：选择并连接端口 (1 分钟)

1. 点击"刷新"按钮
2. 从下拉列表选择 USB CDC 设备
3. 点击"连接"按钮
4. 状态指示灯变绿表示连接成功

### 第 5 步：控制电机 (1 分钟)

#### 回零
```
点击 "回零" 按钮
```

#### 移动到点
```
输入: X = 10 mm, Y = 20 mm, 速度 = 5000 pulse/s
点击 "移动到点" 按钮
```

#### 直线插补
```
输入: X1 = 0, Y1 = 0, X2 = 50, Y2 = 50
点击 "执行直线插补" 按钮
```

## 上位机快速命令参考

### Python 脚本 - 不用 GUI

```python
from protocol import CommandBuilder
from usb_comm import USBCommunicator
import time

# 创建通信对象
comm = USBCommunicator()

# 连接
comm.connect('COM3')  # Windows: COM3, Linux: /dev/ttyACM0

# 回零
comm.send_data(CommandBuilder.home())
time.sleep(2)

# 绝对位移
comm.send_data(CommandBuilder.move_abs(10, 20, 5000))
time.sleep(2)

# 直线插补
comm.send_data(CommandBuilder.line_interp(0, 0, 50, 50, 5000))
time.sleep(2)

# 断开
comm.disconnect()
```

### 命令行测试

```bash
# 列出端口
python test_protocol.py --list

# 运行自动化测试
python test_protocol.py COM3
```

## 嵌入式端快速集成

### 1. 复制文件 (30 秒)
```
usb_cmd_parser.h  →  MDK-ARM/Drivers/
```

### 2. 修改 freertos.c (2 分钟)

在 `USER CODE BEGIN Includes`:
```c
#include "usb_cmd_parser.h"
#include "queue.h"
```

在 `MX_FREERTOS_Init()` 的 `RTOS_QUEUES` 部分:
```c
osMessageQueueId_t g_UsbRxQueueHandle = NULL;
const osMessageQueueAttr_t queue_attr = { .name = "UsbRxQueue" };
g_UsbRxQueueHandle = osMessageQueueNew(10, sizeof(UsbRxMsg_t), &queue_attr);
```

### 3. 创建解析任务 (1 分钟)

```c
void StartUsbParseTask(void *argument)
{
    UsbRxMsg_t rxMsg;
    uint8_t cmd, *data, data_len;
    for(;;) {
        if (osMessageQueueGet(g_UsbRxQueueHandle, &rxMsg, NULL, 100) == osOK)
            if (usb_parse_command(rxMsg.data, rxMsg.length, &cmd, &data, &data_len))
                usb_handle_command(cmd, data, data_len);
    }
}
```

### 4. 修改 usbd_cdc_if.c (30 秒)

```c
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
    UsbRxMsg_t msg;
    msg.length = *Len > 64 ? 64 : *Len;
    memcpy(msg.data, Buf, msg.length);
    osMessageQueuePut(g_UsbRxQueueHandle, &msg, 0, 0);
    
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, Buf);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}
```

### 5. 实现平台接口 (3 分钟)

在 `usb_cmd_parser.h` 中取消注释并实现：

```c
void xy_platform_home(void) {
    g_linearModule[0].stepper.SetTargetAngle(0);
    g_linearModule[1].stepper.SetTargetAngle(0);
}

void xy_platform_move_abs(float x, float y, uint16_t speed) {
    g_linearModule[0].stepper.SetTargetAngleWithVel(x / 4.0 * 360, speed);
    g_linearModule[1].stepper.SetTargetAngleWithVel(y / 4.0 * 360, speed);
}
```

## 常见问题

### Q1: 无法找到 USB 设备
**A**: 
```bash
python test_protocol.py --list  # 查看可用端口
# 如果为空，检查:
# 1. USB 线连接
# 2. STM32 是否运行
# 3. CDC 驱动 (Windows 用户)
```

### Q2: 坐标不准确
**A**: 检查以下参数是否正确（在 my_config.cpp）:
```c
1.8f        // 步距角
32          // 细分
4.0f        // 丝杆导程 (mm)
```

### Q3: GUI 崩溃或闪退
**A**:
```bash
# 检查 Python 环境
python --version

# 重新安装依赖
pip install --upgrade PyQt5 pyserial

# 查看错误信息
python -u main.py
```

### Q4: 命令发送但电机不动
**A**: 
1. 检查 FreeRTOS 任务是否启动: `osThreadNew(StartUsbParseTask, ...)`
2. 检查 `usb_handle_command()` 中函数是否真正被调用
3. 检查 Stepper/XYPlatform 类的初始化

### Q5: 状态显示延迟
**A**: 
- 增加自动查询频率吗? (目前 500ms)
- 手动点击"查询状态"按钮立即获取

## 完整的测试流程

1. **验证底层通信**
   ```bash
   python test_protocol.py COM3
   # 应该看到 [RX] 响应帧
   ```

2. **验证 GUI 连接**
   - 运行 `python main.py`
   - 点击"刷新"看到端口
   - 点击"连接"看到绿色指示灯

3. **验证命令执行**
   - 点击"回零"观察日志出现 "✓ 发送命令: 回零"
   - 点击"移动到点"观察坐标变化

4. **验证实时反馈**
   - 勾选"自动查询"
   - 执行运动，观察左侧坐标实时更新

## 性能测试

### 测试 1: 通信延迟

```python
import time
start = time.time()
comm.send_data(CommandBuilder.query_status())
# 等待响应...
elapsed = time.time() - start
print(f"往返延迟: {elapsed*1000:.1f} ms")
```

**预期**: < 50ms

### 测试 2: 最大频率

```python
# 持续发送查询命令
for i in range(100):
    comm.send_data(CommandBuilder.query_status())
    time.sleep(0.01)  # 100 Hz
```

**预期**: 无丢帧，日志显示 100 条响应

### 测试 3: 长距离运动

```python
# 运动 200mm
comm.send_data(CommandBuilder.move_abs(200, 200, 5000))
time.sleep(10)  # 等待完成
comm.send_data(CommandBuilder.query_status())  # 验证位置
```

**预期**: 最终位置接近 (200, 200)

## 获取帮助

1. **查看源码文档**
   - `protocol.py`: 通信协议
   - `usb_comm.py`: USB 模块
   - `gui.py`: 界面代码
   - `ARCHITECTURE.md`: 系统架构

2. **运行调试模式**
   ```python
   import logging
   logging.basicConfig(level=logging.DEBUG)
   # 启动 GUI，观察日志
   ```

3. **查看日志**
   - GUI 右侧面板显示实时日志
   - 时间戳、命令、响应都有记录

## 下一步

- [ ] 集成数据记录/回放功能
- [ ] 添加轨迹可视化
- [ ] 实现多段插补序列
- [ ] 性能优化（增加查询频率）

祝使用愉快！
