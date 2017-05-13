@echo off

rem call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat"

del /q out\*
node node_modules\glsl-unit\bin\template_glsl_compiler.js --input=shader.glsl --variable_renaming=INTERNAL --output=out\shader.min.glsl
node build-shader-header.js
node preprocess-notes.js
cl /O1 /Oi /Oy /GR- /GS- /fp:fast /QIfist /arch:IA32 /FA /Faout\intro.asm /c /Foout\intro.obj out\intro.c && crinkler20\crinkler /ENTRY:entry /PRIORITY:NORMAL /COMPMODE:FAST /UNSAFEIMPORT /REPORT:out\stats.html /OUT:out\intro.exe out\intro.obj winmm.lib gdi32.lib opengl32.lib kernel32.lib user32.lib
