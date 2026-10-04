@echo off
setlocal
rem Packaged standalone Win64 player produced by Build-Unreal-Street.cmd.
if not exist "%~dp0Build\Windows\SeattleUnreal.exe" (
    echo Packaged player missing. Run Build-Unreal-Street.cmd first.
    exit /b 1
)
start "" "%~dp0Build\Windows\SeattleUnreal.exe" -windowed -ResX=1600 -ResY=900
