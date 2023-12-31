@echo off
call clean.bat
cl /DDEBUG:FULL /Zi handmade.cpp /link user32.lib Gdi32.lib Ole32.lib /machine:x64