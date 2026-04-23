"""
两个直线模组控制通信协议定义
"""

import struct
from enum import IntEnum
from typing import Tuple


class CommandType(IntEnum):
    """命令类型枚举"""
    HOME = 0x01                # 回零
    MOVE_ABS = 0x02            # 绝对位移
    SET_VELOCITY = 0x03        # 设置速度
    STOP = 0x06                # 停止
    QUERY_STATUS = 0x07        # 查询状态
    STATUS_RESPONSE = 0xF0     # 状态响应


class ModuleID(IntEnum):
    """模组ID"""
    X_AXIS = 0x00              # X轴（第一个直线模组）
    Y_AXIS = 0x01              # Y轴（第二个直线模组）


class PlatformStatus(IntEnum):
    """平台状态"""
    IDLE = 0x00                # 空闲
    HOMING = 0x01              # 回零中
    MOVING = 0x02              # 运动中
    ERROR = 0xFF               # 错误


FRAME_HEADER = 0xAA
FRAME_TAIL = 0xFF


class ProtocolFrame:
    """USB 通信协议帧"""
    
    def __init__(self):
        self.header = FRAME_HEADER
        self.cmd = 0
        self.data = b''
        self.checksum = 0
        self.tail = FRAME_TAIL
    
    @staticmethod
    def calculate_checksum(cmd: int, data_len: int, data: bytes) -> int:
        """计算校验和 (XOR)"""
        checksum = cmd ^ data_len
        for byte in data:
            checksum ^= byte
        return checksum
    
    def pack(self, cmd: CommandType, data: bytes = b'') -> bytes:
        """打包帧为字节序列"""
        self.cmd = cmd
        self.data = data
        data_len = len(data)
        self.checksum = self.calculate_checksum(cmd, data_len, data)
        
        frame = bytes([
            self.header,
            self.cmd,
            data_len,
            *data,
            self.checksum,
            self.tail
        ])
        return frame
    
    @staticmethod
    def unpack(frame_data: bytes) -> Tuple[int, bytes]:
        """解包字节序列为 (cmd, data)"""
        if len(frame_data) < 5:
            raise ValueError("Frame too short")
        
        if frame_data[0] != FRAME_HEADER or frame_data[-1] != FRAME_TAIL:
            raise ValueError("Invalid frame header or tail")
        
        cmd = frame_data[1]
        data_len = frame_data[2]
        data = frame_data[3:3+data_len]
        checksum = frame_data[3+data_len]
        
        # 校验校验和
        calculated_checksum = ProtocolFrame.calculate_checksum(cmd, data_len, data)
        if checksum != calculated_checksum:
            raise ValueError(f"Checksum mismatch: {checksum} != {calculated_checksum}")
        
        return cmd, data


class CommandBuilder:
    """命令构建器"""
    
    @staticmethod
    def home(axis: ModuleID) -> bytes:
        """构建回零命令
        
        Args:
            axis: 轴ID (X_AXIS or Y_AXIS)
        """
        data = bytes([axis])
        return ProtocolFrame().pack(CommandType.HOME, data)
    
    @staticmethod
    def move_abs(axis: ModuleID, position: float, speed: int) -> bytes:
        """构建绝对位移命令
        
        Args:
            axis: 轴ID
            position: 目标位置 (mm)
            speed: 速度 (mm/s)
        """
        data = bytes([axis]) + struct.pack('<fH', position, int(round(speed)))
        return ProtocolFrame().pack(CommandType.MOVE_ABS, data)
    
    @staticmethod
    def set_velocity(axis: ModuleID, velocity: int) -> bytes:
        """构建设置速度命令
        
        Args:
            axis: 轴ID
            velocity: 速度 (mm/s)
        """
        data = bytes([axis]) + struct.pack('<H', int(round(velocity)))
        return ProtocolFrame().pack(CommandType.SET_VELOCITY, data)
    
    @staticmethod
    def stop(axis: ModuleID = None) -> bytes:
        """构建停止命令
        
        Args:
            axis: 轴ID，None表示停止所有轴
        """
        if axis is None:
            data = bytes([0xFF])  # 特殊值表示停止所有
        else:
            data = bytes([axis])
        return ProtocolFrame().pack(CommandType.STOP, data)
    
    @staticmethod
    def query_status(axis: ModuleID = None) -> bytes:
        """构建状态查询命令
        
        Args:
            axis: 轴ID，None表示查询所有
        """
        if axis is None:
            data = bytes([0xFF])
        else:
            data = bytes([axis])
        return ProtocolFrame().pack(CommandType.QUERY_STATUS, data)


class ResponseParser:
    """响应解析器"""
    
    @staticmethod
    def parse_status(data: bytes) -> dict:
        """解析状态响应
        
        Returns:
            {
                'x_pos': X轴当前位置,
                'y_pos': Y轴当前位置,
                'x_status': X轴状态,
                'y_status': Y轴状态,
                'x_vel': X轴速度,
                'y_vel': Y轴速度,
                'error': 错误码
            }
        """
        if len(data) < 19:
            raise ValueError(f"Invalid status data length: {len(data)}")
        
        # 格式: x_pos(4B) y_pos(4B) x_status(1B) y_status(1B) x_vel(float,4B) y_vel(float,4B) error(1B)
        x_pos, y_pos, x_status, y_status, x_vel, y_vel, error = struct.unpack(
            '<ffBBffB', data[:19]
        )
        
        return {
            'x_pos': x_pos,
            'y_pos': y_pos,
            'x_status': PlatformStatus(x_status),
            'y_status': PlatformStatus(y_status),
            'x_vel': x_vel,
            'y_vel': y_vel,
            'error': error
        }
