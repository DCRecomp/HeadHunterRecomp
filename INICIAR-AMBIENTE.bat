@echo off
setlocal
cd /d "%~dp0"

title Dreamcast AI - Ollama + OpenCode
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start-environment.ps1"

if errorlevel 1 (
  echo.
  echo Falha ao iniciar o ambiente.
  pause
  exit /b 1
)

echo.
echo A janela pode ser fechada depois que o iPhone conectar.
pause

