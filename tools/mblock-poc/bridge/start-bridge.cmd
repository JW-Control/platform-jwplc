@echo off
rem Arranca el JWPLC mBlock Bridge (127.0.0.1:47820). Dejar esta ventana abierta mientras se usa mBlock.
title JWPLC mBlock Bridge
node "%~dp0jwplc-mblock-bridge.cjs"
pause
