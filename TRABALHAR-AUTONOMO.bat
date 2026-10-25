@echo off
setlocal
cd /d "%~dp0"

title Head Hunter - OpenCode autonomo
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\opencode-autonomo.ps1" %*

if errorlevel 1 (
  echo.
  echo O executor autonomo terminou com erro.
) else (
  echo.
  echo O executor parou por decisao, pausa solicitada ou conclusao total.
)
pause

