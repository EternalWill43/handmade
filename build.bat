@echo off
call clean.bat
cl /DDEBUG:FULL /Zi ./handmade/win32_handmade.cpp /link user32.lib Gdi32.lib Ole32.lib /machine:x64