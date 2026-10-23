@echo off
REM Build and run the real-source host unit tests with MSVC.
REM Run from the repository root:  tests\run_host_tests.cmd
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /W3 /I tests\hostshim /I core ^
   tests\test_real.c core\math\fixed_point.c core\math\statistics.c storage\compression.c ^
   /Fe:tests\test_real.exe /Fo:tests\ >nul
if errorlevel 1 exit /b 1
tests\test_real.exe
