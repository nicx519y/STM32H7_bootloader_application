@echo off
REM H750项目自动构建和打包脚本 (Windows版本)

setlocal enabledelayedexpansion

REM 脚本配置
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set BUILD_OUTPUT=%PROJECT_ROOT%\build
set PACKAGE_OUTPUT=%PROJECT_ROOT%\packages
set TOOLS_DIR=%PROJECT_ROOT%\tools
set PACKAGE_TOOL=%TOOLS_DIR%\package_upgrade.py

REM 默认参数
set VERSION=1.0.0
set BUILD_TYPE=debug
set COMMAND=%1

REM 检查参数
if "%COMMAND%"=="" (
    call :show_help
    exit /b 1
)

REM 解析参数
:parse_args
if "%1"=="-v" (
    set VERSION=%2
    shift
    shift
    goto parse_args
)
if "%1"=="--version" (
    set VERSION=%2
    shift
    shift
    goto parse_args
)
if "%1"=="-r" (
    set BUILD_TYPE=release
    shift
    goto parse_args
)
if "%1"=="--release" (
    set BUILD_TYPE=release
    shift
    goto parse_args
)
if "%1"=="-d" (
    set BUILD_TYPE=debug
    shift
    goto parse_args
)
if "%1"=="--debug" (
    set BUILD_TYPE=debug
    shift
    goto parse_args
)
if "%1"=="-h" (
    call :show_help
    exit /b 0
)
if "%1"=="--help" (
    call :show_help
    exit /b 0
)

REM 执行命令
echo [INFO] 开始执行: %COMMAND% (版本: %VERSION%, 构建类型: %BUILD_TYPE%)

if "%COMMAND%"=="build-all" (
    call :build_bootloader
    call :build_application
    call :collect_artifacts
) else if "%COMMAND%"=="build-bootloader" (
    call :build_bootloader
    call :collect_artifacts
) else if "%COMMAND%"=="build-application" (
    call :build_application
    call :collect_artifacts
) else if "%COMMAND%"=="package-firmware" (
    call :collect_artifacts
    call :package_firmware
) else if "%COMMAND%"=="package-resources" (
    call :collect_artifacts
    call :package_resources
) else if "%COMMAND%"=="package-all" (
    call :collect_artifacts
    call :package_firmware
    call :package_resources
    call :package_single_components
) else if "%COMMAND%"=="clean" (
    call :cleanup
) else if "%COMMAND%"=="full-release" (
    call :build_bootloader
    call :build_application
    call :collect_artifacts
    call :package_firmware
    call :package_resources
    call :package_single_components
    call :generate_release_info
) else (
    echo [ERROR] 未知命令: %COMMAND%
    exit /b 1
)

echo [SUCCESS] 任务完成: %COMMAND%
exit /b 0

REM 显示帮助信息
:show_help
echo H750项目构建和打包工具
echo.
echo 用法: %~nx0 [选项] ^<命令^>
echo.
echo 命令:
echo   build-all         构建所有组件（bootloader + application）
echo   build-bootloader  仅构建bootloader
echo   build-application 仅构建application
echo   package-firmware  打包固件包（application + web resources）
echo   package-resources 打包资源包（config + adc mapping）
echo   package-all       打包所有类型的升级包
echo   clean            清理构建文件
echo   full-release     完整发布流程（构建 + 打包）
echo.
echo 选项:
echo   -v, --version     固件版本号（默认: 1.0.0）
echo   -r, --release     发布模式（优化编译）
echo   -d, --debug       调试模式
echo   -h, --help        显示帮助信息
echo.
echo 示例:
echo   %~nx0 -v 2.1.0 full-release
echo   %~nx0 --debug build-all
echo   %~nx0 package-firmware
goto :eof

REM 构建bootloader
:build_bootloader
echo [INFO] 构建bootloader...
pushd "%PROJECT_ROOT%\bootloader"
if "%BUILD_TYPE%"=="release" (
    make clean && make release
) else (
    make clean && make debug
)
if not exist "build\bootloader.bin" (
    echo [ERROR] bootloader构建失败
    popd
    exit /b 1
)
echo [SUCCESS] bootloader构建完成
popd
goto :eof

REM 构建application
:build_application
echo [INFO] 构建application...
pushd "%PROJECT_ROOT%\application"
if "%BUILD_TYPE%"=="release" (
    make clean && make release
) else (
    make clean && make debug
)
if not exist "build\application.bin" (
    echo [ERROR] application构建失败
    popd
    exit /b 1
)
echo [SUCCESS] application构建完成
popd
goto :eof

REM 收集构建产物
:collect_artifacts
echo [INFO] 收集构建产物...
if not exist "%BUILD_OUTPUT%" mkdir "%BUILD_OUTPUT%"

REM 复制bootloader
if exist "%PROJECT_ROOT%\bootloader\build\bootloader.bin" (
    copy "%PROJECT_ROOT%\bootloader\build\bootloader.bin" "%BUILD_OUTPUT%\"
    if exist "%PROJECT_ROOT%\bootloader\build\bootloader.elf" (
        copy "%PROJECT_ROOT%\bootloader\build\bootloader.elf" "%BUILD_OUTPUT%\"
    )
    echo [INFO] 已复制bootloader到 %BUILD_OUTPUT%
)

REM 复制application
if exist "%PROJECT_ROOT%\application\build\application.bin" (
    copy "%PROJECT_ROOT%\application\build\application.bin" "%BUILD_OUTPUT%\"
    if exist "%PROJECT_ROOT%\application\build\application.elf" (
        copy "%PROJECT_ROOT%\application\build\application.elf" "%BUILD_OUTPUT%\"
    )
    echo [INFO] 已复制application到 %BUILD_OUTPUT%
)

REM 复制web resources（如果存在）
if exist "%PROJECT_ROOT%\web_resources" (
    pushd "%PROJECT_ROOT%\web_resources"
    if exist "pack.bat" (
        call pack.bat
    ) else if exist "pack.sh" (
        bash pack.sh
    )
    if exist "web_resources.bin" (
        copy "web_resources.bin" "%BUILD_OUTPUT%\"
        echo [INFO] 已复制web resources到 %BUILD_OUTPUT%
    )
    popd
)

REM 复制配置文件
if exist "%PROJECT_ROOT%\config\device_config.bin" (
    copy "%PROJECT_ROOT%\config\device_config.bin" "%BUILD_OUTPUT%\"
    echo [INFO] 已复制device_config.bin到 %BUILD_OUTPUT%
)
if exist "%PROJECT_ROOT%\config\adc_mapping.bin" (
    copy "%PROJECT_ROOT%\config\adc_mapping.bin" "%BUILD_OUTPUT%\"
    echo [INFO] 已复制adc_mapping.bin到 %BUILD_OUTPUT%
)
goto :eof

REM 创建固件包
:package_firmware
echo [INFO] 创建固件包...
if not exist "%PACKAGE_OUTPUT%" mkdir "%PACKAGE_OUTPUT%"

set app_file=%BUILD_OUTPUT%\application.bin
set web_file=%BUILD_OUTPUT%\web_resources.bin

if not exist "%app_file%" (
    echo [ERROR] 找不到application文件: %app_file%
    exit /b 1
)

set args=--version %VERSION% --output "%PACKAGE_OUTPUT%" --application "%app_file%"

if exist "%web_file%" (
    set args=%args% --web-resources "%web_file%"
) else (
    echo [WARN] 未找到web resources文件，将创建仅application的固件包
)

python "%PACKAGE_TOOL%" firmware %args%
echo [SUCCESS] 固件包创建完成
goto :eof

REM 创建资源包
:package_resources
echo [INFO] 创建资源包...
if not exist "%PACKAGE_OUTPUT%" mkdir "%PACKAGE_OUTPUT%"

set config_file=%BUILD_OUTPUT%\device_config.bin
set adc_file=%BUILD_OUTPUT%\adc_mapping.bin

set args=--version %VERSION% --output "%PACKAGE_OUTPUT%"
set has_resources=false

if exist "%config_file%" (
    set args=%args% --config "%config_file%"
    set has_resources=true
)

if exist "%adc_file%" (
    set args=%args% --adc-mapping "%adc_file%"
    set has_resources=true
)

if "%has_resources%"=="false" (
    echo [WARN] 未找到任何资源文件，跳过资源包创建
    goto :eof
)

python "%PACKAGE_TOOL%" resources %args%
echo [SUCCESS] 资源包创建完成
goto :eof

REM 创建单组件包
:package_single_components
echo [INFO] 创建单组件包...
if not exist "%PACKAGE_OUTPUT%" mkdir "%PACKAGE_OUTPUT%"

REM Application包
if exist "%BUILD_OUTPUT%\application.bin" (
    python "%PACKAGE_TOOL%" application "%BUILD_OUTPUT%\application.bin" --version %VERSION% --output "%PACKAGE_OUTPUT%"
    echo [INFO] Application包已创建
)

REM Web Resources包
if exist "%BUILD_OUTPUT%\web_resources.bin" (
    python "%PACKAGE_TOOL%" web "%BUILD_OUTPUT%\web_resources.bin" --version %VERSION% --output "%PACKAGE_OUTPUT%"
    echo [INFO] Web Resources包已创建
)

REM Config包
if exist "%BUILD_OUTPUT%\device_config.bin" (
    python "%PACKAGE_TOOL%" config "%BUILD_OUTPUT%\device_config.bin" --version %VERSION% --output "%PACKAGE_OUTPUT%"
    echo [INFO] Config包已创建
)

REM ADC Mapping包
if exist "%BUILD_OUTPUT%\adc_mapping.bin" (
    python "%PACKAGE_TOOL%" adc "%BUILD_OUTPUT%\adc_mapping.bin" --version %VERSION% --output "%PACKAGE_OUTPUT%"
    echo [INFO] ADC Mapping包已创建
)
goto :eof

REM 生成发布信息
:generate_release_info
echo [INFO] 生成发布信息...
set release_info=%PACKAGE_OUTPUT%\release_info_v%VERSION%.txt

echo H750项目发布信息 > "%release_info%"
echo ================ >> "%release_info%"
echo. >> "%release_info%"
echo 版本: %VERSION% >> "%release_info%"
echo 构建时间: %date% %time% >> "%release_info%"
echo 构建类型: %BUILD_TYPE% >> "%release_info%"
echo. >> "%release_info%"
echo 组件信息: >> "%release_info%"

if exist "%BUILD_OUTPUT%\bootloader.bin" (
    for %%F in ("%BUILD_OUTPUT%\bootloader.bin") do echo - Bootloader: %%~zF 字节 >> "%release_info%"
)

if exist "%BUILD_OUTPUT%\application.bin" (
    for %%F in ("%BUILD_OUTPUT%\application.bin") do echo - Application: %%~zF 字节 >> "%release_info%"
)

if exist "%BUILD_OUTPUT%\web_resources.bin" (
    for %%F in ("%BUILD_OUTPUT%\web_resources.bin") do echo - Web Resources: %%~zF 字节 >> "%release_info%"
)

echo. >> "%release_info%"
echo 升级包列表: >> "%release_info%"
for %%F in ("%PACKAGE_OUTPUT%\*.upg") do echo - %%~nxF (%%~zF 字节) >> "%release_info%"

echo [SUCCESS] 发布信息已生成: %release_info%
goto :eof

REM 清理函数
:cleanup
echo [INFO] 清理构建文件...

REM 清理bootloader
if exist "%PROJECT_ROOT%\bootloader" (
    pushd "%PROJECT_ROOT%\bootloader"
    make clean
    popd
)

REM 清理application
if exist "%PROJECT_ROOT%\application" (
    pushd "%PROJECT_ROOT%\application"
    make clean
    popd
)

REM 清理输出目录
if exist "%BUILD_OUTPUT%" rmdir /s /q "%BUILD_OUTPUT%"
if exist "%PACKAGE_OUTPUT%" rmdir /s /q "%PACKAGE_OUTPUT%"

echo [SUCCESS] 清理完成
goto :eof 