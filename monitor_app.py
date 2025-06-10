#!/usr/bin/env python3
import serial
import sys
import time

def monitor_application(port="COM3", baudrate=115200, timeout=30):
    """监听应用程序的输出"""
    try:
        with serial.Serial(port, baudrate, timeout=1) as ser:
            print(f"连接到 {port}，波特率 {baudrate}")
            print(f"等待应用程序输出 (最多等待 {timeout} 秒)...")
            print("-" * 50)
            
            start_time = time.time()
            last_message_time = time.time()
            message_count = 0
            
            while time.time() - start_time < timeout:
                try:
                    data = ser.readline()
                    if data:
                        try:
                            line = data.decode('utf-8').strip()
                            if line:
                                current_time = time.strftime('%H:%M:%S')
                                print(f"{current_time} | {line}")
                                last_message_time = time.time()
                                message_count += 1
                        except UnicodeDecodeError:
                            current_time = time.strftime('%H:%M:%S')
                            print(f"{current_time} | [RAW] {data.hex()}")
                            message_count += 1
                            last_message_time = time.time()
                    
                    # 如果超过10秒没有消息，提示状态
                    if time.time() - last_message_time > 10 and message_count == 0:
                        print(f"已等待 {int(time.time() - start_time)} 秒，暂无应用程序输出...")
                        last_message_time = time.time()  # 重置时间避免重复打印
                        
                except KeyboardInterrupt:
                    print("\n监听已停止")
                    break
                except Exception as e:
                    print(f"读取错误: {e}")
                    time.sleep(0.1)
            
            if message_count == 0:
                print(f"\n在 {timeout} 秒内未检测到应用程序输出")
                print("可能的原因:")
                print("1. 应用程序使用不同的串口配置")
                print("2. 应用程序启动时间较长")
                print("3. 应用程序代码有问题")
                return False
            else:
                print(f"\n总共接收到 {message_count} 条消息")
                return True
                    
    except serial.SerialException as e:
        print(f"串口连接失败: {e}")
        return False
    except Exception as e:
        print(f"未知错误: {e}")
        return False

if __name__ == "__main__":
    port = "COM3"
    if len(sys.argv) > 1:
        port = sys.argv[1]
    
    success = monitor_application(port)
    if not success:
        print("\n尝试其他波特率...")
        for baudrate in [9600, 38400, 57600, 115200, 230400]:
            print(f"\n尝试波特率 {baudrate}...")
            if monitor_application(port, baudrate, 10):
                print(f"在波特率 {baudrate} 下检测到输出！")
                break 