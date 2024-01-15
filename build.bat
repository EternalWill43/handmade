@echo off
call clean.bat
cl -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 /DDEBUG:FULL /Zi ./handmade/win32_handmade.cpp /link user32.lib Gdi32.lib Ole32.lib /machine:x64