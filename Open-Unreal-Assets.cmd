@echo off
setlocal
rem Opens the native Unreal project (compiled StreetGameMode) in the editor.
"E:\Epic Games\Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0SeattleUnreal.uproject" /Game/Seattle/Maps/FifthUniversity
exit /b %ERRORLEVEL%
