#!/bin/bash
# STM32H750 调试工具

while true; do
    echo
    echo "STM32H750 调试工具"
    echo "=================="
    echo "1. 编译并烧录Bootloader"
    echo "2. 编译并烧录Application"  
    echo "3. 完整烧录 (Bootloader + Application)"
    echo "4. 仅编译Application"
    echo "5. 仅编译Bootloader"
    echo "0. 退出"
    echo
    
    read -p "请输入选择 (0-5): " choice
    
    case $choice in
        1)
            python3 debug_scripts/debug_setup.py flash-bootloader
            read -p "按回车键继续..."
            ;;
        2)
            python3 debug_scripts/debug_setup.py flash-application
            read -p "按回车键继续..."
            ;;
        3)
            python3 debug_scripts/debug_setup.py full-flash
            read -p "按回车键继续..."
            ;;
        4)
            cd application && make && cd ..
            read -p "按回车键继续..."
            ;;
        5)
            cd bootloader && make && cd ..
            read -p "按回车键继续..."
            ;;
        0)
            echo "退出"
            break
            ;;
        *)
            echo "无效选择，请重新输入"
            ;;
    esac
done
