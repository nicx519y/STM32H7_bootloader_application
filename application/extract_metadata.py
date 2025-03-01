#!/usr/bin/env python3
import struct
import subprocess
import re
import sys
import os

# 设置段名的固定长度
SECTION_NAME_LENGTH = 32

def extract_metadata(elf_file, output_file):
    try:
        if not os.path.exists(elf_file):
            print(f'Error: ELF file {elf_file} does not exist')
            return 1
            
        try:
            output = subprocess.check_output(['arm-none-eabi-objdump', '-h', elf_file], 
                                          stderr=subprocess.STDOUT).decode('utf-8')
        except subprocess.CalledProcessError as e:
            print(f'Error executing objdump: {e.output.decode("utf-8")}')
            return 1
            
        sections = []
        pattern = re.compile(r'^\s*\d+\s+(\.[a-zA-Z0-9_]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)')
        
        for line in output.split('\n'):
            match = pattern.search(line)
            if match:
                name = match.group(1)
                size = int(match.group(2), 16)
                vma = int(match.group(3), 16)
                lma = int(match.group(4), 16)
                sections.append((name, size, vma, lma))
                
        # 生成二进制元数据文件
        with open(output_file, 'wb') as f:
            # 写入段数量
            f.write(struct.pack('<I', len(sections)))
            
            # 写入每个段的信息，使用固定长度的段名
            for name, size, vma, lma in sections:
                # 准备固定长度段名，截断过长的段名，填充过短的段名
                fixed_name = name.ljust(SECTION_NAME_LENGTH, '\0')[:SECTION_NAME_LENGTH]
                name_bytes = fixed_name.encode('utf-8')
                
                # 直接写入固定长度段名和其他数据
                f.write(name_bytes)
                f.write(struct.pack('<III', size, vma, lma))
                
        # 生成文本元数据文件
        txt_file = os.path.splitext(output_file)[0] + '.txt'
        with open(txt_file, 'w') as f:
            f.write(f"Metadata for {elf_file}\n")
            f.write(f"Total sections: {len(sections)}\n\n")
            
            # 表头
            f.write(f"{'Section Name':<20} {'Size':<12} {'VMA':<12} {'LMA':<12}\n")
            f.write(f"{'-'*20} {'-'*12} {'-'*12} {'-'*12}\n")
            
            # 段信息
            for name, size, vma, lma in sections:
                f.write(f"{name:<20} 0x{size:08X} 0x{vma:08X} 0x{lma:08X}\n")
            
            # 追加总计和统计信息
            total_size = sum(size for _, size, _, _ in sections)
            f.write(f"\nTotal Size: 0x{total_size:08X} ({total_size:,} bytes)\n")
            
            # 添加特殊段的信息摘要
            text_section = next((s for s in sections if s[0] == '.text'), None)
            data_section = next((s for s in sections if s[0] == '.data'), None)
            bss_section = next((s for s in sections if s[0] == '.bss'), None)
            
            f.write("\nMemory Usage Summary:\n")
            if text_section:
                f.write(f"  Code (.text):    0x{text_section[1]:08X} ({text_section[1]:,} bytes)\n")
            if data_section:
                f.write(f"  Data (.data):    0x{data_section[1]:08X} ({data_section[1]:,} bytes)\n")
            if bss_section:
                f.write(f"  BSS (.bss):      0x{bss_section[1]:08X} ({bss_section[1]:,} bytes)\n")
                
        print(f'Metadata written to {output_file}')
        print(f'Human-readable metadata written to {txt_file}')
        print('Sections found:')
        for name, size, vma, lma in sections:
            print(f'  {name}: size=0x{size:08x}, VMA=0x{vma:08x}, LMA=0x{lma:08x}')
            
        return 0
        
    except Exception as e:
        print(f'Error: {str(e)}')
        return 1

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: python extract_metadata.py <elf_file> <output_file>')
        sys.exit(1)
        
    sys.exit(extract_metadata(sys.argv[1], sys.argv[2])) 