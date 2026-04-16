@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\prepare-download-page.ps1" %*
exit /b %ERRORLEVEL%
