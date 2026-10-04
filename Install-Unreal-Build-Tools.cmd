@echo off
setlocal
echo Unreal C++ build prerequisite: Microsoft Visual Studio Build Tools + Windows SDK.
echo Approve the Windows prompt. This does not restart Windows or modify the Unity project.
powershell.exe -NoProfile -File "%~dp0Scripts\install-build-tools.ps1"
set "RESULT=%ERRORLEVEL%"
echo Installer finished with code %RESULT%.
pause
exit /b %RESULT%
