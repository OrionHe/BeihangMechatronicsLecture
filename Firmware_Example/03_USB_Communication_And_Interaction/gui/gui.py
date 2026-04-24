"""
两个直线模组GUI控制程序 - 支持速度、位置和回零控制，带实时位置图
"""

import sys
import logging
from datetime import datetime
from typing import Optional
from collections import deque

from PyQt5.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QPushButton, QSpinBox, QDoubleSpinBox, QComboBox,
    QStatusBar, QGroupBox, QTextEdit, QMessageBox,
    QScrollArea, QSplitter, QApplication, QSlider
)
from PyQt5.QtCore import Qt, QTimer, pyqtSignal, QObject, QSize
from PyQt5.QtGui import QFont, QColor

import matplotlib
matplotlib.use('Qt5Agg')
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import matplotlib.patches as patches

from protocol import (
    CommandBuilder, ResponseParser, CommandType, ModuleID, 
    PlatformStatus, ProtocolFrame
)
from usb_comm import USBCommunicator

logger = logging.getLogger(__name__)

STATUS_QUERY_INTERVAL_MS = 100
SPEED_PLOT_Y_MIN = -15
SPEED_PLOT_Y_MAX = 15
SPEED_INPUT_MIN = -10.0
SPEED_INPUT_MAX = 10.0


class SignalEmitter(QObject):
    """信号发射器（用于线程安全的 UI 更新）"""
    status_updated = pyqtSignal(dict)  # 状态更新信号
    log_message = pyqtSignal(str)      # 日志消息信号
    error_occurred = pyqtSignal(str)   # 错误信号
    connected = pyqtSignal(bool)       # 连接状态信号


class XYPlotCanvas(FigureCanvas):
    """XY 平台实时坐标图"""
    
    def __init__(self, parent=None, width=6, height=5, dpi=100):
        self.fig = Figure(figsize=(width, height), dpi=dpi)
        self.ax = self.fig.add_subplot(111)
        super().__init__(self.fig)
        self.setParent(parent)

        # 使用简洁默认样式
        self.ax.grid(True, linestyle='--', alpha=0.4)
        self.ax.set_xlabel('X (mm)')
        self.ax.set_ylabel('Y (mm)')
        self.ax.set_title('XY Platform Real-time Position')
        
        # 初始化
        self.ax.set_xlim(-10, 300)
        self.ax.set_ylim(-10, 300)
        self.ax.set_aspect('equal', adjustable='box')
        self._draw_workspace()
        
        # 轨迹记录
        self.trajectory_x = deque(maxlen=500)
        self.trajectory_y = deque(maxlen=500)
        self.trajectory_line, = self.ax.plot([], [], 'o-', color='tab:blue', markersize=2, linewidth=1, alpha=0.7)
        
        # 当前位置点
        self.current_point, = self.ax.plot([], [], 'o', color='tab:red', markersize=10, label='Current')
        
        # 图例
        self.ax.legend(loc='upper right')
        
        self.fig.tight_layout()
        self.draw_idle()

    def _draw_workspace(self):
        """绘制 300 mm x 300 mm 工作框"""
        # rect = patches.Rectangle(
        #     (0, 0), 300, 300,
        #     fill=False,
        #     edgecolor='black',
        #     linewidth=1.5,
        #     linestyle='--',
        #     alpha=0.7
        # )
        # self.ax.add_patch(rect)
        for spine in self.ax.spines.values():
            spine.set_visible(True)
            spine.set_color('black')
            spine.set_linewidth(1.2)
    
    def update_current_position(self, x: float, y: float):
        """更新当前位置"""
        self.current_point.set_data([x], [y])
        
        # 添加到轨迹
        if not self.trajectory_x or abs(x - self.trajectory_x[-1]) > 0.5 or abs(y - self.trajectory_y[-1]) > 0.5:
            self.trajectory_x.append(x)
            self.trajectory_y.append(y)
            self.trajectory_line.set_data(list(self.trajectory_x), list(self.trajectory_y))
        
        self.draw_idle()
    
    def clear_trajectory(self):
        """清除轨迹"""
        self.trajectory_x.clear()
        self.trajectory_y.clear()
        self.trajectory_line.set_data([], [])
        self.draw_idle()


class SpeedPlotCanvas(FigureCanvas):
    """双轴速度实时曲线"""

    def __init__(self, parent=None, width=6, height=3, dpi=100):
        self.fig = Figure(figsize=(width, height), dpi=dpi)
        self.ax = self.fig.add_subplot(111)
        super().__init__(self.fig)
        self.setParent(parent)

        self.ax.grid(True, linestyle='--', alpha=0.4)
        self.ax.set_xlabel('Sample')
        self.ax.set_ylabel('Speed (mm/s)')
        self.ax.set_title('XY Axis Speed')

        self.sample_index = 0
        self.sample_x = deque(maxlen=300)
        self.speed_x = deque(maxlen=300)
        self.sample_y = deque(maxlen=300)
        self.speed_y = deque(maxlen=300)

        self.x_line, = self.ax.plot([], [], color='tab:blue', linewidth=1.5, label='X Speed')
        self.y_line, = self.ax.plot([], [], color='tab:orange', linewidth=1.5, label='Y Speed')
        self.ax.legend(loc='upper right')
        self.ax.set_ylim(SPEED_PLOT_Y_MIN, SPEED_PLOT_Y_MAX)

        self.fig.tight_layout()
        self.draw_idle()

    def update_speed(self, x_speed: float, y_speed: float):
        """更新速度曲线"""
        self.sample_index += 1
        self.sample_x.append(self.sample_index)
        self.speed_x.append(x_speed)
        self.sample_y.append(self.sample_index)
        self.speed_y.append(y_speed)

        self.x_line.set_data(list(self.sample_x), list(self.speed_x))
        self.y_line.set_data(list(self.sample_y), list(self.speed_y))

        if self.sample_index < 5:
            self.ax.set_xlim(0, 5)
        else:
            self.ax.set_xlim(max(0, self.sample_index - 100), self.sample_index + 5)

        self.ax.set_ylim(SPEED_PLOT_Y_MIN, SPEED_PLOT_Y_MAX)

        self.draw_idle()

    def clear_speed(self):
        """清除速度曲线"""
        self.sample_index = 0
        self.sample_x.clear()
        self.speed_x.clear()
        self.sample_y.clear()
        self.speed_y.clear()
        self.x_line.set_data([], [])
        self.y_line.set_data([], [])
        self.ax.set_xlim(0, 5)
        self.ax.set_ylim(SPEED_PLOT_Y_MIN, SPEED_PLOT_Y_MAX)
        self.draw_idle()


class ModuleController:
    """单个直线模组控制器"""
    
    def __init__(self, axis_id: ModuleID, axis_name: str, communicator: USBCommunicator, signal_emitter: SignalEmitter):
        self.axis_id = axis_id
        self.axis_name = axis_name
        self.comm = communicator
        self.signal = signal_emitter
        self.current_pos = 0.0
        self.current_vel = 0.0
        self.status = PlatformStatus.IDLE
    
    def send_command(self, cmd_bytes: bytes, cmd_name: str = "") -> bool:
        """发送命令"""
        if not self.comm.is_connected():
            self.signal.error_occurred.emit(f"{self.axis_name}: 未连接到设备")
            return False
        
        success = self.comm.send_data(cmd_bytes)
        if success:
            self.signal.log_message.emit(f"✓ [{self.axis_name}] {cmd_name}")
        else:
            self.signal.error_occurred.emit(f"✗ [{self.axis_name}] 发送失败: {cmd_name}")
        
        return success
    
    def home(self) -> bool:
        """回零"""
        return self.send_command(CommandBuilder.home(self.axis_id), "回零")
    
    def move_abs(self, position: float, speed: float) -> bool:
        """绝对位移"""
        return self.send_command(
            CommandBuilder.move_abs(self.axis_id, position, speed),
            f"移动到 {position:.1f} mm (速度 {speed})"
        )
    
    def set_velocity(self, velocity: float) -> bool:
        """设置速度"""
        return self.send_command(
            CommandBuilder.set_velocity(self.axis_id, velocity),
            f"设置速度 {velocity:.1f} mm/s"
        )
    
    def stop(self) -> bool:
        """停止"""
        return self.send_command(CommandBuilder.stop(self.axis_id), "停止")


class MainWindow(QMainWindow):
    """主窗口"""
    
    def __init__(self):
        super().__init__()
        self.setWindowTitle("直线模组控制系统 v1.0 - 两轴 XY 平台")
        self.setGeometry(50, 50, 1600, 900)
        
        # 应用样式
        self.apply_theme()
        
        # 信号发射器
        self.signal_emitter = SignalEmitter()
        
        # USB 通信
        self.comm = USBCommunicator(baudrate=115200)
        self.comm.set_data_callback(self.on_usb_data_received)
        
        # 模组控制器
        self.controller_x = ModuleController(ModuleID.X_AXIS, "X轴", self.comm, self.signal_emitter)
        self.controller_y = ModuleController(ModuleID.Y_AXIS, "Y轴", self.comm, self.signal_emitter)
        
        # 连接信号
        self.signal_emitter.status_updated.connect(self.on_status_updated)
        self.signal_emitter.log_message.connect(self.on_log_message)
        self.signal_emitter.error_occurred.connect(self.on_error)
        self.signal_emitter.connected.connect(self.on_connected)
        
        # 定时器：定期查询状态
        self.status_timer = QTimer()
        self.status_timer.timeout.connect(self.on_query_status)
        
        # 初始化 UI
        self.init_ui()
        
        # 日志
        self.setup_logging()
    
    def apply_theme(self):
        """应用样式"""
        stylesheet = """
        QGroupBox {
            border: 1px solid #b0b0b0;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: bold;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 3px 0 3px;
        }
        
        QPushButton {
            padding: 6px 10px;
            border-radius: 2px;
        }
        
        QPushButton:hover {
            border: 1px solid #808080;
        }
        
        QPushButton:pressed {
            border: 1px solid #606060;
        }
        
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            border: 1px solid #a0a0a0;
            border-radius: 4px;
            padding: 4px;
        }
        
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border: 1px solid #606060;
        }
        
        QTextEdit {
            border: 1px solid #a0a0a0;
            border-radius: 4px;
            font-family: 'Courier New';
            font-size: 10px;
        }
        """
        self.setStyleSheet(stylesheet)
    
    def setup_logging(self):
        """配置日志"""
        logging.basicConfig(
            level=logging.DEBUG,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )
    
    def init_ui(self):
        """初始化 UI"""
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # 创建主布局
        main_layout = QHBoxLayout()
        
        # 左侧：控制面板
        left_widget = self.create_control_panel()
        
        # 右侧：图表和日志
        right_widget = self.create_plot_and_log_panel()
        
        # 使用分割器实现可调整的布局
        splitter = QSplitter(Qt.Horizontal)
        splitter.addWidget(left_widget)
        splitter.addWidget(right_widget)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 2)
        
        main_layout.addWidget(splitter)
        central_widget.setLayout(main_layout)
        
        # 状态栏
        self.statusBar().showMessage("✓ 就绪")
    
    def create_control_panel(self) -> QWidget:
        """创建控制面板"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        # 连接控制
        conn_group = self.create_connection_group()
        layout.addWidget(conn_group)
        
        # 状态显示
        status_group = self.create_status_group()
        layout.addWidget(status_group)
        
        # 基础控制
        basic_group = self.create_basic_control_group()
        layout.addWidget(basic_group)
        
        # 运动控制（可滚动）
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        motion_widget = QWidget()
        motion_layout = QVBoxLayout()
        
        # X轴控制
        motion_layout.addWidget(self.create_axis_control_group("X轴", self.controller_x, "x"))
        motion_layout.addWidget(self.create_axis_control_group("Y轴", self.controller_y, "y"))
        motion_layout.addStretch()
        
        motion_widget.setLayout(motion_layout)
        scroll.setWidget(motion_widget)
        layout.addWidget(scroll)
        
        widget.setLayout(layout)
        return widget
    
    def create_connection_group(self) -> QGroupBox:
        """连接控制组"""
        group = QGroupBox("连接")
        layout = QGridLayout()
        
        layout.addWidget(QLabel("选择端口:"), 0, 0)
        self.port_combo = QComboBox()
        layout.addWidget(self.port_combo, 0, 1)
        
        refresh_btn = QPushButton("刷新")
        refresh_btn.clicked.connect(self.on_refresh_ports)
        refresh_btn.setMaximumWidth(80)
        layout.addWidget(refresh_btn, 0, 2)
        
        self.connect_btn = QPushButton("连接")
        self.connect_btn.clicked.connect(self.on_connect_toggle)
        layout.addWidget(self.connect_btn, 1, 0, 1, 3)
        
        group.setLayout(layout)
        return group
    
    def create_status_group(self) -> QGroupBox:
        """状态显示组"""
        group = QGroupBox("状态")
        layout = QGridLayout()
        
        # 连接状态
        layout.addWidget(QLabel("连接:"), 0, 0)
        self.status_indicator = QLabel("●")
        self.status_indicator.setFont(QFont("Arial", 20))
        self.status_indicator.setStyleSheet("color: red;")
        layout.addWidget(self.status_indicator, 0, 1)
        
        self.connection_status_label = QLabel("离线")
        layout.addWidget(self.connection_status_label, 0, 2)
        
        # X 轴位置
        layout.addWidget(QLabel("X 位置:"), 1, 0)
        self.x_pos_label = QLabel("0.0 mm")
        layout.addWidget(self.x_pos_label, 1, 1, 1, 2)
        
        # Y 轴位置
        layout.addWidget(QLabel("Y 位置:"), 2, 0)
        self.y_pos_label = QLabel("0.0 mm")
        layout.addWidget(self.y_pos_label, 2, 1, 1, 2)
        
        # X 轴速度
        layout.addWidget(QLabel("X 速度:"), 3, 0)
        self.x_vel_label = QLabel("0.0 mm/s")
        layout.addWidget(self.x_vel_label, 3, 1, 1, 2)
        
        # Y 轴速度
        layout.addWidget(QLabel("Y 速度:"), 4, 0)
        self.y_vel_label = QLabel("0.0 mm/s")
        layout.addWidget(self.y_vel_label, 4, 1, 1, 2)

        # X 轴状态
        layout.addWidget(QLabel("X 状态:"), 5, 0)
        self.x_status_label = QLabel("IDLE")
        layout.addWidget(self.x_status_label, 5, 1, 1, 2)

        # Y 轴状态
        layout.addWidget(QLabel("Y 状态:"), 6, 0)
        self.y_status_label = QLabel("IDLE")
        layout.addWidget(self.y_status_label, 6, 1, 1, 2)
        
        group.setLayout(layout)
        return group

    def format_platform_status(self, status: PlatformStatus) -> tuple[str, str]:
        """返回状态文本和对应颜色"""
        if status == PlatformStatus.HOMING:
            return "HOMING", "#FB8C00"
        if status == PlatformStatus.MOVING:
            return "MOVING", "#1E88E5"
        if status == PlatformStatus.ERROR:
            return "ERROR", "#E53935"
        return "IDLE", "#43A047"
    
    def create_basic_control_group(self) -> QGroupBox:
        """基础电机控制组"""
        group = QGroupBox("基础控制")
        layout = QVBoxLayout()
        
        # 全部回零
        home_all_btn = QPushButton("全部回零")
        home_all_btn.setStyleSheet("background-color: #FFE0B2; font-weight: bold;")
        home_all_btn.clicked.connect(self.on_home_all)
        layout.addWidget(home_all_btn)
        
        # 全部停止
        stop_all_btn = QPushButton("全部停止")
        stop_all_btn.setStyleSheet("background-color: #FFCDD2; font-weight: bold;")
        stop_all_btn.clicked.connect(self.on_stop_all)
        layout.addWidget(stop_all_btn)
        
        # 清除轨迹
        clear_btn = QPushButton("清除轨迹")
        clear_btn.clicked.connect(self.on_clear_trajectory)
        layout.addWidget(clear_btn)
        
        group.setLayout(layout)
        return group
    
    def create_axis_control_group(self, axis_name: str, controller: ModuleController, axis_tag: str) -> QGroupBox:
        """创建单轴控制组"""
        group = QGroupBox(f"{axis_name}控制")
        layout = QGridLayout()
        
        # 回零
        home_btn = QPushButton("回零")
        home_btn.setStyleSheet("background-color: #FFE0B2;")
        home_btn.clicked.connect(lambda: controller.home())
        layout.addWidget(home_btn, 0, 0)
        
        # 停止
        stop_btn = QPushButton("停止")
        stop_btn.setStyleSheet("background-color: #FFCDD2;")
        stop_btn.clicked.connect(lambda: controller.stop())
        layout.addWidget(stop_btn, 0, 1)
        
        # 目标位置
        layout.addWidget(QLabel("目标位置 (mm):"), 1, 0)
        pos_spinbox = QDoubleSpinBox()
        pos_spinbox.setRange(-100, 300)
        pos_spinbox.setValue(0)
        pos_spinbox.setSingleStep(1)
        setattr(self, f"{axis_tag}_pos_input", pos_spinbox)
        layout.addWidget(pos_spinbox, 1, 1)
        
        # 速度
        layout.addWidget(QLabel("速度 (mm/s):"), 2, 0)
        speed_spinbox = QDoubleSpinBox()
        speed_spinbox.setRange(SPEED_INPUT_MIN, SPEED_INPUT_MAX)
        speed_spinbox.setValue(10)
        speed_spinbox.setSingleStep(0.1)
        speed_spinbox.setDecimals(1)
        setattr(self, f"{axis_tag}_speed_input", speed_spinbox)
        layout.addWidget(speed_spinbox, 2, 1)
        
        # 移动按钮
        move_btn = QPushButton("移动到目标")
        move_btn.setStyleSheet("background-color: #C8E6C9;")
        move_btn.clicked.connect(lambda: controller.move_abs(
            getattr(self, f"{axis_tag}_pos_input").value(),
            getattr(self, f"{axis_tag}_speed_input").value()
        ))
        layout.addWidget(move_btn, 3, 0, 1, 2)
        
        # 设置速度按钮
        set_vel_btn = QPushButton("设置速度")
        set_vel_btn.clicked.connect(lambda: controller.set_velocity(
            getattr(self, f"{axis_tag}_speed_input").value()
        ))
        layout.addWidget(set_vel_btn, 4, 0, 1, 2)
        
        group.setLayout(layout)
        return group
    
    def create_plot_and_log_panel(self) -> QWidget:
        """创建图表和日志面板"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        # 分割器
        splitter = QSplitter(Qt.Vertical)
        
        # 图表
        self.xy_plot = XYPlotCanvas()
        splitter.addWidget(self.xy_plot)

        self.speed_plot = SpeedPlotCanvas()
        splitter.addWidget(self.speed_plot)
        
        # 日志
        log_group = QGroupBox("日志")
        log_layout = QVBoxLayout()
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setMaximumHeight(200)
        log_layout.addWidget(self.log_text)
        log_group.setLayout(log_layout)
        splitter.addWidget(log_group)
        
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)
        splitter.setStretchFactor(2, 1)
        
        layout.addWidget(splitter)
        widget.setLayout(layout)
        return widget
    
    def on_refresh_ports(self):
        """刷新端口列表"""
        self.port_combo.clear()
        ports = self.comm.list_ports()
        if ports:
            for port_info in ports:
                port_text = f"{port_info['port']} - {port_info['description']}"
                self.port_combo.addItem(port_text, port_info['port'])
            self.on_log_message(f"✓ 发现 {len(ports)} 个端口")
        else:
            self.on_log_message("✗ 未发现 USB 设备")
    
    def on_connect_toggle(self):
        """连接/断开切换"""
        if self.comm.is_connected():
            self.comm.disconnect()
            self.status_timer.stop()
            self.signal_emitter.connected.emit(False)
        else:
            port = self.port_combo.currentData()
            if port:
                if self.comm.connect(port):
                    self.status_timer.start(STATUS_QUERY_INTERVAL_MS)
                    self.signal_emitter.connected.emit(True)
                else:
                    self.on_error("连接失败")
    
    def on_query_status(self):
        """定期查询状态"""
        self.comm.send_data(CommandBuilder.query_status())
    
    def on_home_all(self):
        """全部回零"""
        self.controller_x.home()
        self.controller_y.home()
    
    def on_stop_all(self):
        """全部停止"""
        self.controller_x.stop()
        self.controller_y.stop()
    
    def on_clear_trajectory(self):
        """清除轨迹"""
        self.xy_plot.clear_trajectory()
        self.speed_plot.clear_speed()
        self.on_log_message("轨迹已清除")
    
    def on_usb_data_received(self, frame_data: bytes):
        """处理 USB 接收的数据"""
        try:
            cmd, data = ProtocolFrame.unpack(frame_data)
            
            if cmd == CommandType.STATUS_RESPONSE:
                status_info = ResponseParser.parse_status(data)
                self.signal_emitter.status_updated.emit(status_info)
        
        except Exception as e:
            self.signal_emitter.error_occurred.emit(f"解析响应出错: {e}")
    
    def on_status_updated(self, status_info: dict):
        """更新状态显示"""
        self.controller_x.current_pos = status_info['x_pos']
        self.controller_y.current_pos = status_info['y_pos']
        self.controller_x.current_vel = status_info['x_vel']
        self.controller_y.current_vel = status_info['y_vel']
        self.controller_x.status = status_info['x_status']
        self.controller_y.status = status_info['y_status']
        
        # 更新标签
        self.x_pos_label.setText(f"{status_info['x_pos']:.2f} mm")
        self.y_pos_label.setText(f"{status_info['y_pos']:.2f} mm")
        x_status_text, x_status_color = self.format_platform_status(status_info['x_status'])
        y_status_text, y_status_color = self.format_platform_status(status_info['y_status'])
        self.x_status_label.setText(x_status_text)
        self.y_status_label.setText(y_status_text)
        self.x_status_label.setStyleSheet(f"color: {x_status_color}; font-weight: bold;")
        self.y_status_label.setStyleSheet(f"color: {y_status_color}; font-weight: bold;")
        self.x_vel_label.setText(f"{status_info['x_vel']:.1f} mm/s")
        self.y_vel_label.setText(f"{status_info['y_vel']:.1f} mm/s")
        
        # 更新图表
        self.xy_plot.update_current_position(status_info['x_pos'], status_info['y_pos'])
        self.speed_plot.update_speed(status_info['x_vel'], status_info['y_vel'])
    
    def on_log_message(self, message: str):
        """添加日志消息"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.append(f"[{timestamp}] {message}")
        # 滚动到底部
        self.log_text.verticalScrollBar().setValue(self.log_text.verticalScrollBar().maximum())
    
    def on_error(self, error_msg: str):
        """处理错误"""
        self.on_log_message(f"✗ 错误: {error_msg}")
    
    def on_connected(self, connected: bool):
        """处理连接状态变化"""
        if connected:
            self.status_indicator.setText("●")
            self.status_indicator.setStyleSheet("color: green;")
            self.connection_status_label.setText("在线")
            self.connect_btn.setText("断开")
            self.on_log_message("✓ 已连接")
            self.on_refresh_ports()
        else:
            self.status_indicator.setText("●")
            self.status_indicator.setStyleSheet("color: red;")
            self.connection_status_label.setText("离线")
            self.connect_btn.setText("连接")
            self.on_log_message("✓ 已断开")
    
    def closeEvent(self, event):
        """关闭窗口"""
        if self.comm.is_connected():
            self.comm.disconnect()
        self.status_timer.stop()
        event.accept()


def main():
    """主函数"""
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == '__main__':
    main()
