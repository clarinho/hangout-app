@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\update-release.ps1" %*
exit /b %ERRORLEVEL%
