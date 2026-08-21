@echo off
echo =========================================
echo       Building C++ Game Engine...
echo =========================================

:: ======================================================
:: 1. SETTINGS & PATHS
:: ======================================================

set COMPILER=g++
set CXX_STD=-std=c++17
set FLAGS=-Wall -Wextra
set OUTPUT=bin\game.exe

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

if not exist bin mkdir bin

echo [BUILDING] Compiling files...
echo Main File : %MAIN_FILE%
echo ----------------------------------------------------

%COMPILER% %CXX_STD% %FLAGS% %INCLUDES% -D_NOTDEBUG %MAIN_FILE% %SOURCES% %LIBS% -o %OUTPUT%

:: ======================================================
:: 3. RESULT & AUTO-RUN
:: ======================================================
if %ERRORLEVEL% EQU 0 (
    echo.
    echo =========================================
    echo [SUCCESS] Game built successfully!
    echo Output: %OUTPUT%
    echo =========================================

    echo Copying assets and required DLLs into bin\ ...
    if not exist "bin\main\assets" xcopy /y /s /i /d "main\assets" "bin\main\assets" >nul
    xcopy /y /d "C:\msys64\ucrt64\bin\*sfml*.dll" bin\ >nul
    xcopy /y /d "C:\msys64\ucrt64\bin\libfreetype-6.dll" bin\ >nul
    xcopy /y /d "C:\msys64\ucrt64\bin\libgcc_s_seh-1.dll" bin\ >nul
    xcopy /y /d "C:\msys64\ucrt64\bin\libstdc++-6.dll" bin\ >nul
    xcopy /y /d "C:\msys64\ucrt64\bin\libwinpthread-1.dll" bin\ >nul

    echo.
    echo Running Game...
    echo -----------------------------------------
    cd /d bin
    game.exe
    cd /d ..
    echo.
    echo Game exited with code %ERRORLEVEL%
    pause
) else (
    echo.
    echo =========================================
    echo [ERROR] Build Failed! Fix errors above.
    echo =========================================
    pause
) 
