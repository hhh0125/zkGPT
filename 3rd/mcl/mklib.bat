@echo off
call setvar.bat
if "%1"=="dll" (
  echo make dynamic library DLL
) else (
  echo make static library LIB
)

if not defined MCL_PYTHON set "MCL_PYTHON=python.exe"

"%MCL_PYTHON%" src\gen_bint_header.py proto > include\mcl\bint_proto.hpp
if errorlevel 1 exit /b 1
"%MCL_PYTHON%" src\gen_bint_header.py switch > src\bint_switch.hpp
if errorlevel 1 exit /b 1

if 1 == 1 (
  echo use masm
  "%MCL_PYTHON%" src\gen_bint_x64.py -win -m masm > src\asm\bint-x64-win.asm
  if errorlevel 1 exit /b 1
  ml64 -c src\asm\bint-x64-win.asm
  if errorlevel 1 exit /b 1
) else (
  echo use nasm
  "%MCL_PYTHON%" src\gen_bint_x64.py -win -m nasm > src\asm\bint-x64-win.asm
  if errorlevel 1 exit /b 1
  nasm -f win64 -o bint-x64-win.obj src\asm\bint-x64-win.asm
  if errorlevel 1 exit /b 1
)


if "%1"=="dll" (
  set CFLAGS=%CFLAGS% /DMCL_NO_AUTOLINK /DMCLBN_NO_AUTOLINK
)
echo CFLAGS=%CFLAGS%

set OBJ=obj\fp.obj bint-x64-win.obj

cl /c %CFLAGS% src\fp.cpp /Foobj\fp.obj
if errorlevel 1 exit /b 1
lib /nologo /OUT:lib\mcl.lib /nodefaultlib %OBJ%
if errorlevel 1 exit /b 1

if "%1"=="dll" (
     cl /c %CFLAGS% src\bn_c256.cpp /Foobj\bn_c256.obj
     if errorlevel 1 exit /b 1
     link /nologo /DLL /OUT:bin\mclbn256.dll obj\bn_c256.obj %OBJ% %LDFLAGS% /implib:lib\mclbn256.lib
     if errorlevel 1 exit /b 1

     cl /c %CFLAGS% src\bn_c384_256.cpp /Foobj\bn_c384_256.obj
     if errorlevel 1 exit /b 1
     link /nologo /DLL /OUT:bin\mclbn384_256.dll obj\bn_c384_256.obj %OBJ% %LDFLAGS% /implib:lib\mclbn384_256.lib
     if errorlevel 1 exit /b 1

     cl /c %CFLAGS% src\she_c384_256.cpp /Foobj\she_c384_256.obj /DMCL_NO_AUTOLINK
     if errorlevel 1 exit /b 1
     link /nologo /DLL /OUT:bin\mclshe384_256.dll obj\she_c384_256.obj %OBJ% %LDFLAGS% /implib:lib\mclshe_c384_256.lib
     if errorlevel 1 exit /b 1
) else (
     cl /c %CFLAGS% src\bn_c256.cpp /Foobj\bn_c256.obj
     if errorlevel 1 exit /b 1
     lib /nologo /OUT:lib\mclbn256.lib /nodefaultlib obj\bn_c256.obj lib\mcl.lib
     if errorlevel 1 exit /b 1

     cl /c %CFLAGS% src\bn_c384_256.cpp /Foobj\bn_c384_256.obj
     if errorlevel 1 exit /b 1
     lib /nologo /OUT:lib\mclbn384_256.lib /nodefaultlib obj\bn_c384_256.obj lib\mcl.lib
     if errorlevel 1 exit /b 1
)
