@echo off
setlocal
cd /d "%~dp0"

title Dreamcast AI - Encerrar
opencode service stop >nul 2>&1
"C:\Users\rodri\AppData\Local\Programs\Ollama\ollama.exe" stop devstral-small-2:24b >nul 2>&1
powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process ^| Where-Object { $_.Name -eq 'ollama.exe' -and $_.CommandLine -match '\bserve\b' } ^| ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"

echo OpenCode e Ollama foram encerrados.
pause

