@echo off
setlocal enabledelayedexpansion

:: 设置依赖包路径。
set REPO_ROOT=%~dp0..
set FFMPEG_DIR=%REPO_ROOT%\deps\ffmpeg-master-latest-win64-gpl-shared
set PROJECT_DIR=%REPO_ROOT%\Xinle_Player

if not exist "%FFMPEG_DIR%" (
    echo 错误：未找到 ffmpeg 依赖包 %FFMPEG_DIR%。
    echo 请先从 https://github.com/BtbN/FFmpeg-Builds/releases 下载 shared 版本，
    echo 解压到 deps\ffmpeg-master-latest-win64-gpl-shared 后再运行此脚本。
    exit /b 1
)

:: 拷贝头文件与导入库到工程目录。
echo 正在拷贝 FFmpeg 头文件到 %PROJECT_DIR%\include ...
xcopy /E /I /Y "%FFMPEG_DIR%\include\*" "%PROJECT_DIR%\include\" > nul

echo 正在拷贝 FFmpeg 导入库到 %PROJECT_DIR%\lib ...
xcopy /E /I /Y "%FFMPEG_DIR%\lib\*" "%PROJECT_DIR%\lib\" > nul

echo 依赖设置完成。现在可以打开 Xinle_Player.sln 编译。
endlocal
