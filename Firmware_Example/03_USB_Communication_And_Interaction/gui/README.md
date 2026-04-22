# 直线模组控制系统 GUI

两个直线模组 (XY 平台) 的图形界面控制程序，支持实时位置显示、轨迹绘制、速度控制和回零功能。

## 功能特性

### 基础控制
- **連接管理**：自动检测 USB CDC 设备，一键连接/断开
- **全軸控制**：一键回零、一键停止所有轴
- **軌跡繪製**：实时显示 XY 平台移动轨迹，支持清除

### 每軸控制 (X轴、Y轴)
- **絕對位移**：设置目标位置，输入速度并执行移动
- **速度控制**：实时设置电机速度
- **回零函數**：单軸回零
- **緊急停止**：立即停止当前軸

### 状态监控
- **實時顯示**：
  - 連接状态 (在线/离线)
  - X轴/Y轴当前位置 (mm)
  - X轴/Y轴当前速度 (pulse/s)

### 日志系统
- **實時日志**：所有操作记录在查看窗口
- **时间戳**：每条消息附带时间戳
- **錯誤報告**：通信或解析错误实时提示

## 安装

### 1. 安装依赖
```bash
pip install -r requirements.txt
```

### 2. 運行程序
#### 方式 1：使用批处理文件 (Windows)
```bash
run.bat
```

#### 方式 2：直接运行 Python
```bash
python main.py
```

#### 方式 3：使用 Python 模块
```bash
python -m gui.gui
```

## 使用指南

### 连接设备

1. **选择端口**
   - 程序启动后，点击「刷新」按钮自动扫描 USB 端口
   - 从下拉列表选择对应的 COM 端口
   
2. **连接**
   - 点击「连接」按钮建立 USB 连接
   - 连接状态指示灯会变成绿色 (●)

### 控制电机

#### 绝对位移
1. 在「X轴控制」或「Y轴控制」中设置：
   - **目标位置**：目标移动到的位置 (单位: mm)
   - **速度**：电机转动速度 (单位: pulse/s)
2. 点击「移动到目标」按钮

#### 速度控制
1. 在对应轴的控制面板中修改「速度」值
2. 点击「设置速度」按钮立即改变电机速度

#### 回零
- 点击单轴的「回零」按钮回零单个轴
- 点击基础控制中的「全部回零」快速重置所有轴

#### 紧急停止
- 点击单轴的「停止」按钮停止该轴
- 点击基础控制中的「全部停止」快速停止所有运动

### 监控运动

- **位置图**：右侧的 XY 坐标图实时显示：
  - 红色圆点：当前位置
  - 蓝色轨迹线：历史移动轨迹
  
- **清除轨迹**：点击「清除轨迹」按钮重置位置图

- **日志**：左下方日志窗口记录所有操作，包括：
  - 命令发送记录
  - 状态更新信息
  - 错误和异常

## 协议说明

### 通信帧格式
```
[帧头] [命令] [长度] [数据] [校验和] [帧尾]
[0xAA] [CMD]  [LEN]  [DATA] [XOR]    [0xFF]
```

### 命令类型

| 命令 | 代码 | 说明 | 数据格式 |
|------|------|------|----------|
| HOME | 0x01 | 回零 | [轴ID] |
| MOVE_ABS | 0x02 | 绝对位移 | [轴ID][位置uint32][速度uint16] |
| SET_VELOCITY | 0x03 | 设置速度 | [轴ID][速度uint16] |
| STOP | 0x06 | 停止 | [轴ID] |
| QUERY_STATUS | 0x07 | 查询状态 | [轴ID] |
| STATUS_RESPONSE | 0xF0 | 状态响应 | [X位置][Y位置][X状态][Y状态][X速度][Y速度][错误码] |

### 轴 ID
- 0x00: X 轴
- 0x01: Y 轴
- 0xFF: 所有轴

### 校验算法
```
checksum = cmd ^ len
for each byte in data:
    checksum ^= byte
```

## 文件结构

```
gui/
├── main.py              # 启动脚本
├── gui.py               # GUI 主程序
├── protocol.py          # 通信协议定义
├── usb_comm.py          # USB 通信模块
├── requirements.txt     # Python 依赖
├── run.bat              # Windows 启动脚本
└── README.md            # 本文档
```

## 故障排除

### "未发现USB设备"
- 检查硬件连接
- 确认硬件已通电并正确配置
- 尝试重启硬件

### "连接失败"
- 检查串口号是否正确
- 确认波特率为 115200
- 尝试使用其他 USB 端口或数据线

### "发送失败"
- 检查设备是否仍在线
- 查看日志输出中的详细错误信息

### "解析响应出错"
- 可能是固件和通信协议不匹配
- 检查设备的固件版本

## 开发参考

### 添加新的轴控制

修改 `gui.py` 中 `create_control_panel()` 方法：

```python
# 在 motion_layout 中添加新轴
motion_layout.addWidget(self.create_axis_control_group("Z轴", self.controller_z, "z"))
```

然后初始化对应的控制器：

```python
self.controller_z = ModuleController(ModuleID.Z_AXIS, "Z轴", self.comm, self.signal_emitter)
```

### 修改轨迹图表范围

编辑 `gui.py` 中 `XYPlotCanvas.__init__()` 的坐标轴范围：

```python
self.ax.set_xlim(-5, 150)  # X 轴范围 -5 到 150 mm
self.ax.set_ylim(-5, 150)  # Y 轴范围 -5 到 150 mm
```

## 许可证

MIT License

## 联系方式

For issues or questions, please refer to the main BeihangMechatronicsLecture repository.
