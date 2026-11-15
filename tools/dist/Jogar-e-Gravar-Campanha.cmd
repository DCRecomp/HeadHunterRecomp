@echo off
rem HH16.1: grava a campanha completa do Head Hunter (docs/CAMPANHA.md).
rem Nao usar antes do portao de prontidao da HH16.P (docs/planos/HH16-PLANO.md): falta a sessao piloto na janela.
rem A primeira vez cria a campanha; as seguintes continuam a mesma. Feche a janela do jogo para terminar a sessao.
rem Copia de seguranca automatica ao fim de cada sessao: em campanhas\_copias, ou na pasta de HH_CAMPANHA_COPIA
rem (de preferencia outro disco), definida antes da primeira sessao.
setlocal
cd /d "%~dp0..\.."
python -I tools\dev\campanha.py status >nul 2>&1
if errorlevel 1 (
  if defined HH_CAMPANHA_COPIA (
    python -I tools\dev\campanha.py nova --copia "%HH_CAMPANHA_COPIA%"
  ) else (
    python -I tools\dev\campanha.py nova
  )
)
python -I tools\dev\campanha.py jogar
echo.
python -I tools\dev\campanha.py status
pause
