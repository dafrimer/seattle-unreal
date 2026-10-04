@echo off
setlocal
set "ENGINE=E:\Epic Games\Games\UE_5.8"
set "PROJECT=%~dp0SeattleUnreal.uproject"
call "%ENGINE%\Engine\Build\BatchFiles\Build.bat" SeattleUnrealEditor Win64 Development -Project="%PROJECT%" -WaitMutex -NoHotReloadFromIDE
if errorlevel 1 exit /b %ERRORLEVEL%
"%ENGINE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -nullrhi -unattended -nop4 -nosplash -ExecutePythonScript="%~dp0Content\Python\migrate_scene.py" -abslog="%~dp0Evidence\native-build-import.log"
if errorlevel 1 exit /b %ERRORLEVEL%
powershell.exe -NoProfile -Command "$r=Get-Content -LiteralPath '%~dp0Evidence\import-result.json' -Raw | ConvertFrom-Json; if (-not $r.success -or -not $r.runtimeModeReady) { Write-Error 'Native Unreal import is incomplete'; exit 1 }"
if errorlevel 1 exit /b %ERRORLEVEL%
call "%ENGINE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJECT%" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="%~dp0Build" -unattended
exit /b %ERRORLEVEL%
