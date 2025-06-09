#!/bin/bash
# H750项目自动构建和打包脚本

set -e  # 出错时退出

# 脚本配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_OUTPUT="$PROJECT_ROOT/build"
PACKAGE_OUTPUT="$PROJECT_ROOT/packages"
TOOLS_DIR="$PROJECT_ROOT/tools"
PACKAGE_TOOL="$TOOLS_DIR/package_upgrade.py"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# 显示帮助信息
show_help() {
    echo "H750项目构建和打包工具"
    echo ""
    echo "用法: $0 [选项] <命令>"
    echo ""
    echo "命令:"
    echo "  build-all         构建所有组件（bootloader + application）"
    echo "  build-bootloader  仅构建bootloader"
    echo "  build-application 仅构建application"
    echo "  package-firmware  打包固件包（application + web resources）"
    echo "  package-resources 打包资源包（config + adc mapping）"
    echo "  package-all       打包所有类型的升级包"
    echo "  clean            清理构建文件"
    echo "  full-release     完整发布流程（构建 + 打包）"
    echo ""
    echo "选项:"
    echo "  -v, --version     固件版本号（默认: 1.0.0）"
    echo "  -r, --release     发布模式（优化编译）"
    echo "  -d, --debug       调试模式"
    echo "  -h, --help        显示帮助信息"
    echo ""
    echo "示例:"
    echo "  $0 -v 2.1.0 full-release"
    echo "  $0 --debug build-all"
    echo "  $0 package-firmware"
}

# 默认参数
VERSION="1.0.0"
BUILD_TYPE="debug"
COMMAND=""

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--version)
            VERSION="$2"
            shift 2
            ;;
        -r|--release)
            BUILD_TYPE="release"
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="debug"
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        build-all|build-bootloader|build-application|package-firmware|package-resources|package-all|clean|full-release)
            COMMAND="$1"
            shift
            ;;
        *)
            log_error "未知参数: $1"
            show_help
            exit 1
            ;;
    esac
done

if [[ -z "$COMMAND" ]]; then
    log_error "请指定命令"
    show_help
    exit 1
fi

# 检查必要工具
check_tools() {
    local missing_tools=()
    
    # 检查make
    if ! command -v make &> /dev/null; then
        missing_tools+=("make")
    fi
    
    # 检查Python3
    if ! command -v python3 &> /dev/null; then
        missing_tools+=("python3")
    fi
    
    # 检查打包工具
    if [[ ! -f "$PACKAGE_TOOL" ]]; then
        missing_tools+=("package_upgrade.py")
    fi
    
    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        log_error "缺少必要工具: ${missing_tools[*]}"
        exit 1
    fi
}

# 构建bootloader
build_bootloader() {
    log_info "构建bootloader..."
    
    cd "$PROJECT_ROOT/bootloader"
    
    if [[ "$BUILD_TYPE" == "release" ]]; then
        make clean && make release
    else
        make clean && make debug
    fi
    
    if [[ ! -f "build/bootloader.bin" ]]; then
        log_error "bootloader构建失败"
        return 1
    fi
    
    log_success "bootloader构建完成"
    return 0
}

# 构建application
build_application() {
    log_info "构建application..."
    
    cd "$PROJECT_ROOT/application"
    
    if [[ "$BUILD_TYPE" == "release" ]]; then
        make clean && make release
    else
        make clean && make debug
    fi
    
    if [[ ! -f "build/application.bin" ]]; then
        log_error "application构建失败"
        return 1
    fi
    
    log_success "application构建完成"
    return 0
}

# 收集构建产物
collect_artifacts() {
    log_info "收集构建产物..."
    
    mkdir -p "$BUILD_OUTPUT"
    
    # 复制bootloader
    if [[ -f "$PROJECT_ROOT/bootloader/build/bootloader.bin" ]]; then
        cp "$PROJECT_ROOT/bootloader/build/bootloader.bin" "$BUILD_OUTPUT/"
        cp "$PROJECT_ROOT/bootloader/build/bootloader.elf" "$BUILD_OUTPUT/" 2>/dev/null || true
        log_info "已复制bootloader到 $BUILD_OUTPUT"
    fi
    
    # 复制application
    if [[ -f "$PROJECT_ROOT/application/build/application.bin" ]]; then
        cp "$PROJECT_ROOT/application/build/application.bin" "$BUILD_OUTPUT/"
        cp "$PROJECT_ROOT/application/build/application.elf" "$BUILD_OUTPUT/" 2>/dev/null || true
        log_info "已复制application到 $BUILD_OUTPUT"
    fi
    
    # 复制web resources（如果存在）
    if [[ -d "$PROJECT_ROOT/web_resources" ]]; then
        cd "$PROJECT_ROOT/web_resources"
        if [[ -f "pack.sh" ]]; then
            ./pack.sh
        fi
        if [[ -f "web_resources.bin" ]]; then
            cp "web_resources.bin" "$BUILD_OUTPUT/"
            log_info "已复制web resources到 $BUILD_OUTPUT"
        fi
    fi
    
    # 复制配置文件
    local config_files=("$PROJECT_ROOT/config/device_config.bin" "$PROJECT_ROOT/config/adc_mapping.bin")
    for config_file in "${config_files[@]}"; do
        if [[ -f "$config_file" ]]; then
            cp "$config_file" "$BUILD_OUTPUT/"
            log_info "已复制 $(basename "$config_file") 到 $BUILD_OUTPUT"
        fi
    done
}

# 创建固件包
package_firmware() {
    log_info "创建固件包..."
    
    mkdir -p "$PACKAGE_OUTPUT"
    
    local app_file="$BUILD_OUTPUT/application.bin"
    local web_file="$BUILD_OUTPUT/web_resources.bin"
    
    if [[ ! -f "$app_file" ]]; then
        log_error "找不到application文件: $app_file"
        return 1
    fi
    
    local args="--version $VERSION --output $PACKAGE_OUTPUT"
    args="$args --application $app_file"
    
    if [[ -f "$web_file" ]]; then
        args="$args --web-resources $web_file"
    else
        log_warn "未找到web resources文件，将创建仅application的固件包"
    fi
    
    python3 "$PACKAGE_TOOL" firmware $args
    log_success "固件包创建完成"
}

# 创建资源包
package_resources() {
    log_info "创建资源包..."
    
    mkdir -p "$PACKAGE_OUTPUT"
    
    local config_file="$BUILD_OUTPUT/device_config.bin"
    local adc_file="$BUILD_OUTPUT/adc_mapping.bin"
    
    local args="--version $VERSION --output $PACKAGE_OUTPUT"
    local has_resources=false
    
    if [[ -f "$config_file" ]]; then
        args="$args --config $config_file"
        has_resources=true
    fi
    
    if [[ -f "$adc_file" ]]; then
        args="$args --adc-mapping $adc_file"
        has_resources=true
    fi
    
    if [[ "$has_resources" == "false" ]]; then
        log_warn "未找到任何资源文件，跳过资源包创建"
        return 0
    fi
    
    python3 "$PACKAGE_TOOL" resources $args
    log_success "资源包创建完成"
}

# 创建单组件包
package_single_components() {
    log_info "创建单组件包..."
    
    mkdir -p "$PACKAGE_OUTPUT"
    
    # Application包
    if [[ -f "$BUILD_OUTPUT/application.bin" ]]; then
        python3 "$PACKAGE_TOOL" application "$BUILD_OUTPUT/application.bin" \
            --version "$VERSION" --output "$PACKAGE_OUTPUT"
        log_info "Application包已创建"
    fi
    
    # Web Resources包
    if [[ -f "$BUILD_OUTPUT/web_resources.bin" ]]; then
        python3 "$PACKAGE_TOOL" web "$BUILD_OUTPUT/web_resources.bin" \
            --version "$VERSION" --output "$PACKAGE_OUTPUT"
        log_info "Web Resources包已创建"
    fi
    
    # Config包
    if [[ -f "$BUILD_OUTPUT/device_config.bin" ]]; then
        python3 "$PACKAGE_TOOL" config "$BUILD_OUTPUT/device_config.bin" \
            --version "$VERSION" --output "$PACKAGE_OUTPUT"
        log_info "Config包已创建"
    fi
    
    # ADC Mapping包
    if [[ -f "$BUILD_OUTPUT/adc_mapping.bin" ]]; then
        python3 "$PACKAGE_TOOL" adc "$BUILD_OUTPUT/adc_mapping.bin" \
            --version "$VERSION" --output "$PACKAGE_OUTPUT"
        log_info "ADC Mapping包已创建"
    fi
}

# 生成发布信息
generate_release_info() {
    log_info "生成发布信息..."
    
    local release_info="$PACKAGE_OUTPUT/release_info_v${VERSION}.txt"
    
    cat > "$release_info" << EOF
H750项目发布信息
================

版本: $VERSION
构建时间: $(date)
构建类型: $BUILD_TYPE

组件信息:
EOF

    if [[ -f "$BUILD_OUTPUT/bootloader.bin" ]]; then
        local size=$(stat -c%s "$BUILD_OUTPUT/bootloader.bin")
        local crc=$(python3 -c "import binascii; print(hex(binascii.crc32(open('$BUILD_OUTPUT/bootloader.bin', 'rb').read()) & 0xffffffff))")
        echo "- Bootloader: $size 字节, CRC32: $crc" >> "$release_info"
    fi
    
    if [[ -f "$BUILD_OUTPUT/application.bin" ]]; then
        local size=$(stat -c%s "$BUILD_OUTPUT/application.bin")
        local crc=$(python3 -c "import binascii; print(hex(binascii.crc32(open('$BUILD_OUTPUT/application.bin', 'rb').read()) & 0xffffffff))")
        echo "- Application: $size 字节, CRC32: $crc" >> "$release_info"
    fi
    
    if [[ -f "$BUILD_OUTPUT/web_resources.bin" ]]; then
        local size=$(stat -c%s "$BUILD_OUTPUT/web_resources.bin")
        local crc=$(python3 -c "import binascii; print(hex(binascii.crc32(open('$BUILD_OUTPUT/web_resources.bin', 'rb').read()) & 0xffffffff))")
        echo "- Web Resources: $size 字节, CRC32: $crc" >> "$release_info"
    fi
    
    echo "" >> "$release_info"
    echo "升级包列表:" >> "$release_info"
    find "$PACKAGE_OUTPUT" -name "*.upg" -printf "- %f (%s 字节)\n" >> "$release_info"
    
    log_success "发布信息已生成: $release_info"
}

# 清理函数
cleanup() {
    log_info "清理构建文件..."
    
    cd "$PROJECT_ROOT"
    
    # 清理bootloader
    if [[ -d "bootloader" ]]; then
        cd bootloader && make clean && cd ..
    fi
    
    # 清理application
    if [[ -d "application" ]]; then
        cd application && make clean && cd ..
    fi
    
    # 清理输出目录
    rm -rf "$BUILD_OUTPUT"
    rm -rf "$PACKAGE_OUTPUT"
    
    log_success "清理完成"
}

# 主逻辑
main() {
    log_info "开始执行: $COMMAND (版本: $VERSION, 构建类型: $BUILD_TYPE)"
    
    check_tools
    
    case "$COMMAND" in
        "clean")
            cleanup
            ;;
        "build-bootloader")
            build_bootloader
            collect_artifacts
            ;;
        "build-application")
            build_application
            collect_artifacts
            ;;
        "build-all")
            build_bootloader
            build_application
            collect_artifacts
            ;;
        "package-firmware")
            collect_artifacts
            package_firmware
            ;;
        "package-resources")
            collect_artifacts
            package_resources
            ;;
        "package-all")
            collect_artifacts
            package_firmware
            package_resources
            package_single_components
            ;;
        "full-release")
            build_bootloader
            build_application
            collect_artifacts
            package_firmware
            package_resources
            package_single_components
            generate_release_info
            ;;
        *)
            log_error "未知命令: $COMMAND"
            exit 1
            ;;
    esac
    
    log_success "任务完成: $COMMAND"
}

# 运行主函数
main 