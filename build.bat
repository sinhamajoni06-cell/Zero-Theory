@echo off
echo =========================================
powershell -Command "'      Building', 'C++', 'Game', 'Engine ' | ForEach-Object { Write-Host -NoNewline (\"$_ \"); Start-Sleep -Milliseconds 250 }; '|','/','-','\','|','/','-','\','|','/','-','\' | ForEach-Object { Write-Host -NoNewline \"`b$_\" ; Start-Sleep -Milliseconds 160 }; Write-Host ''"
echo =========================================

:: ======================================================
:: 1. SETTINGS & PATHS
:: ======================================================

set COMPILER=g++
set CXX_STD=-std=c++17
set FLAGS=-Wall -Wextra
set OUTPUT=ZeroTheory\ZeroTheory.exe

:: A) Main Entry Point File
set MAIN_FILE=core/src/engine/NoTEngine.cpp

:: B) Header Folders (-I)
set INCLUDES=-I core/lib/header
set INCLUDES=%INCLUDES% -I core/src
set INCLUDES=%INCLUDES% -I core/src/engine
set INCLUDES=%INCLUDES% -I C:/msys64/ucrt64/include

:: C) Source Files (.cpp)
set SOURCES=core/lib/cpp/*.cpp

:: Library Path (-L)
set LIBS=-L C:/msys64/ucrt64/lib -lsfml-graphics -lsfml-window -lsfml-system -lopengl32 -lwinmm -lgdi32 -lfreetype -lstdc++

:: ======================================================
:: 2. BUILD PROCESS
:: ======================================================

if exist ZeroTheory goto CHECK_REBUILD
mkdir ZeroTheory
goto START_BUILD

:CHECK_REBUILD
echo.
powershell -Command "'Do you want to Update / Rebuild your project? '.ToCharArray() | ForEach-Object { Write-Host -NoNewline -ForegroundColor Yellow $_; Start-Sleep -Milliseconds 40 }; '[Y/N]: '.ToCharArray() | ForEach-Object { Write-Host -NoNewline -ForegroundColor White $_; Start-Sleep -Milliseconds 40 }"
set "CHOICE="
set /p "CHOICE="
if /i "%CHOICE%"=="Y" goto START_BUILD
if /i "%CHOICE%"=="N" exit /b
powershell -Command "Write-Host '[Error] ' -ForegroundColor Red -NoNewline; Write-Host 'Try again!'"
goto CHECK_REBUILD

:START_BUILD

echo [BUILDING] Compiling files...
echo Main File : %MAIN_FILE%
echo ----------------------------------------------------

%COMPILER% %CXX_STD% %FLAGS% %INCLUDES% -D_NOTDEBUG %MAIN_FILE% %SOURCES% %LIBS% -o %OUTPUT%

:: ======================================================
:: 3. RESULT & AUTO-RUN
:: ======================================================
if %ERRORLEVEL% NEQ 0 goto BUILD_FAILED

echo.
echo =========================================
echo [SUCCESS] Game built successfully!
echo Output: %OUTPUT%
echo =========================================

echo Copying assets and required DLLs into ZeroTheory\ ...
if not exist "ZeroTheory\main\assets" xcopy /y /s /i /d "main\assets" "ZeroTheory\main\assets" >nul
xcopy /y /d "C:\msys64\ucrt64\bin\*sfml*.dll" ZeroTheory\ >nul
xcopy /y /d "C:\msys64\ucrt64\bin\libfreetype-6.dll" ZeroTheory\ >nul
xcopy /y /d "C:\msys64\ucrt64\bin\libgcc_s_seh-1.dll" ZeroTheory\ >nul
xcopy /y /d "C:\msys64\ucrt64\bin\libstdc++-6.dll" ZeroTheory\ >nul
xcopy /y /d "C:\msys64\ucrt64\bin\libwinpthread-1.dll" ZeroTheory\ >nul

:CHECK_LAUNCH
echo.
powershell -Command "'Do you want to Launch the game now? '.ToCharArray() | ForEach-Object { Write-Host -NoNewline -ForegroundColor Yellow $_; Start-Sleep -Milliseconds 40 }; '[Y/N]: '.ToCharArray() | ForEach-Object { Write-Host -NoNewline -ForegroundColor White $_; Start-Sleep -Milliseconds 40 }"
set "LAUNCH_CHOICE="
set /p "LAUNCH_CHOICE="
if /i "%LAUNCH_CHOICE%"=="Y" goto RUN_GAME
if /i "%LAUNCH_CHOICE%"=="N" exit /b
powershell -Command "Write-Host '[Error] ' -ForegroundColor Red -NoNewline; Write-Host 'Try again!'"
goto CHECK_LAUNCH

:RUN_GAME
echo.
echo Running Game...
echo -----------------------------------------
cd /d ZeroTheory
ZeroTheory.exe
cd /d ..
echo.
echo Game exited with code %ERRORLEVEL%
pause
exit /b

:BUILD_FAILED
echo.
echo =========================================
echo [ERROR] Build Failed! Fix errors above.
echo =========================================
pause
