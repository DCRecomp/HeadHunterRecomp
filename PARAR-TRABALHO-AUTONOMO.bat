@echo off
setlocal
cd /d "%~dp0"

type nul > "%~dp0.opencode-autonomo.stop"
echo Sinal de parada criado. O executor encerrara depois do turno atual.
pause

