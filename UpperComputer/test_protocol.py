#!/usr/bin/env python3
"""
简单的测试脚本 - 用户不需要 UI，直接发送命令
"""

import sys
import time
import argparse
from protocol import CommandBuilder
from usb_comm import USBCommunicator

def test_communication(port, baudrate=115200):
    """测试 USB 通信"""
    
    comm = USBCommunicator(baudrate=baudrate)
    
    print("[*] 扫描端口...")
    ports = comm.list_ports()
    if not ports:
        print("[-] 无可用端口")
        return
    
    for p in ports:
        print(f"  [{p['port']}] {p['description']}")
    
    print(f"[*] 连接到 {port}...")
    if not comm.connect(port):
        print("[-] 连接失败")
        return
    
    print("[+] 已连接")
    
    # 接收回调
    responses = []
    def on_data(data):
        responses.append(data)
        print(f"[RX] {data.hex().upper()}")
    
    comm.set_data_callback(on_data)
    
    try:
        # 测试 1: 查询状态
        print("\n[*] 查询状态...")
        comm.send_data(CommandBuilder.query_status())
        time.sleep(0.5)
        
        # 测试 2: 回零
        print("\n[*] 发送回零命令...")
        comm.send_data(CommandBuilder.home())
        time.sleep(0.5)
        
        # 测试 3: 绝对位移
        print("\n[*] 绝对位移到 (10, 20)...")
        comm.send_data(CommandBuilder.move_abs(10.0, 20.0, 5000))
        time.sleep(0.5)
        
        # 测试 4: 相对位移
        print("\n[*] 相对位移 (5, -3)...")
        comm.send_data(CommandBuilder.move_rel(5.0, -3.0, 3000))
        time.sleep(0.5)
        
        # 测试 5: 直线插补
        print("\n[*] 直线插补 (0,0) -> (50,50)...")
        comm.send_data(CommandBuilder.line_interp(0, 0, 50, 50, 5000))
        time.sleep(0.5)
        
        # 测试 6: 圆弧插补
        print("\n[*] 圆弧插补 (圆心 25,25, 半径 10, 角度 90°)...")
        comm.send_data(CommandBuilder.arc_interp(25, 25, 10, 90, 5000))
        time.sleep(0.5)
        
        # 测试 7: 停止
        print("\n[*] 停止...")
        comm.send_data(CommandBuilder.stop())
        time.sleep(0.5)
        
        print(f"\n[+] 总共收到 {len(responses)} 个响应")
        
    finally:
        comm.disconnect()
        print("[*] 已断开连接")


def main():
    parser = argparse.ArgumentParser(description="XY 平台通信测试工具")
    parser.add_argument('port', nargs='?', default='COM3', help='串口名 (默认 COM3)')
    parser.add_argument('--baud', type=int, default=115200, help='波特率 (默认 115200)')
    parser.add_argument('--list', action='store_true', help='列出可用端口')
    
    args = parser.parse_args()
    
    if args.list:
        comm = USBCommunicator()
        ports = comm.list_ports()
        if not ports:
            print("无可用端口")
        else:
            print("可用端口:")
            for p in ports:
                print(f"  {p['port']}: {p['description']}")
        return
    
    test_communication(args.port, args.baud)


if __name__ == '__main__':
    main()
