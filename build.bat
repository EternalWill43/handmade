@echo off
call clean.bat
cl -MT -Gm- -nologo -Oi -GR- -EHa -WX -W4 -wd4201 -wd4100 -wd4996 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 /DDEBUG:FULL /Zi ./handmade/win32_handmade.cpp /link user32.lib Gdi32.lib Ole32.lib /machine:x64