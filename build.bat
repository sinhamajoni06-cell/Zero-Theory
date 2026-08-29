@echo off
powershell -Command "[console]::CursorVisible=$false"
echo =========================================
powershell -Command "'      Building', 'C++', 'Game', 'Engine ' | ForEach-Object { Write-Host -NoNewline (\"$_ \"); Start-Sleep -Milliseconds 250 }; '|','/','-','\','|','/','-','\','|','/','-','\' | ForEach-Object { Write-Host -NoNewline \"`b$_\" ; Start-Sleep -Milliseconds 160 }; Write-Host ''"
echo =========================================

:: ======================================================
:: 1. SETTINGS & PATHS
:: ======================================================

set COMPILER=g++.exe
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
powershell -Command "$t1='Do you want to Update / Rebuild your project? '; $t2='[Y/N]: '; $full=$t1+$t2; for ($i=0; $i -lt $full.Length; $i++) { if ([console]::KeyAvailable) { if ($i -lt $t1.Length) { Write-Host -NoNewline -ForegroundColor Yellow $full.Substring($i,$t1.Length-$i); Write-Host -NoNewline -ForegroundColor White $t2 } else { Write-Host -NoNewline -ForegroundColor White $full.Substring($i) }; break }; $c=$full[$i]; if ($i -lt $t1.Length) { Write-Host -NoNewline -ForegroundColor Yellow $c } else { Write-Host -NoNewline -ForegroundColor White $c }; Start-Sleep -Milliseconds 20 }"
powershell -Command "[console]::CursorVisible=$true"
set "CHOICE="
set /p "CHOICE="
powershell -Command "[console]::CursorVisible=$false"
if /i "%CHOICE%"=="Y" goto START_BUILD
if /i "%CHOICE%"=="N" powershell -Command "[console]::CursorVisible=$true" & exit /b
powershell -Command "Write-Host '[Error] ' -ForegroundColor Red -NoNewline; Write-Host 'Try again!'"
goto CHECK_REBUILD

:START_BUILD

echo [BUILDING] Compiling files...
echo Main File : %MAIN_FILE%
echo ----------------------------------------------------

powershell -Command "$src = (Resolve-Path '%SOURCES%' | ForEach-Object { $_.Path }); $allArgs = @('%CXX_STD%'.Split(' ')) + @('%FLAGS%'.Split(' ')) + @('%INCLUDES%'.Split(' ')) + @('-D_NOTDEBUG','%MAIN_FILE%') + $src + @('%LIBS%'.Split(' ')) + @('-o','%OUTPUT%'); $wd = (Get-Location).Path; $sw = [System.Diagnostics.Stopwatch]::StartNew(); $job = Start-Job -ScriptBlock { param($exe,$jargs,$dir) Set-Location $dir; & $exe @jargs 2>&1 } -ArgumentList '%COMPILER%', $allArgs, $wd; $pct=0; while ($job.State -eq 'Running') { if ($pct -lt 90) { $pct += 3 } elseif ($pct -lt 99) { $pct++ }; if ($pct -gt 99) { $pct = 99 }; $filled = [Math]::Floor($pct/5); $bar = ('#' * $filled) + ('-' * (20-$filled)); Write-Host -NoNewline \"`r[\"; Write-Host -NoNewline -ForegroundColor Yellow \"BUILDING\"; Write-Host -NoNewline \"] [ $bar ] $pct%%   \"; Start-Sleep -Milliseconds ([Math]::Max(30, 150 - $pct)) }; $result = Receive-Job -Job $job -Wait; Remove-Job -Job $job; $sw.Stop(); $elapsed = ($sw.Elapsed.TotalSeconds).ToString('0.00'); if ($job.State -eq 'Completed' -and ($result -join \"`n\") -notmatch 'error:') { Write-Host -NoNewline \"`r[\"; Write-Host -NoNewline -ForegroundColor Green \"FINISHED\"; Write-Host \"] [ #################### ] 100%% - Done in ${elapsed}s   \" } else { Write-Host -NoNewline \"`r[\"; Write-Host -NoNewline -ForegroundColor Yellow \"BUILDING\"; Write-Host \"] [FAILED] Compilation failed!                  \"; $result | ForEach-Object { Write-Host $_ }; exit 1 }"

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
powershell -Command "$t1='Do you want to Launch the game now? '; $t2='[Y/N]: '; $full=$t1+$t2; for ($i=0; $i -lt $full.Length; $i++) { if ([console]::KeyAvailable) { if ($i -lt $t1.Length) { Write-Host -NoNewline -ForegroundColor Yellow $full.Substring($i,$t1.Length-$i); Write-Host -NoNewline -ForegroundColor White $t2 } else { Write-Host -NoNewline -ForegroundColor White $full.Substring($i) }; break }; $c=$full[$i]; if ($i -lt $t1.Length) { Write-Host -NoNewline -ForegroundColor Yellow $c } else { Write-Host -NoNewline -ForegroundColor White $c }; Start-Sleep -Milliseconds 20 }"
powershell -Command "[console]::CursorVisible=$true"
set "LAUNCH_CHOICE="
set /p "LAUNCH_CHOICE="
powershell -Command "[console]::CursorVisible=$false"
if /i "%LAUNCH_CHOICE%"=="Y" goto RUN_GAME
if /i "%LAUNCH_CHOICE%"=="N" powershell -Command "[console]::CursorVisible=$true" & exit /b
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
powershell -Command "[console]::CursorVisible=$true"
pause
exit /b

:BUILD_FAILED
powershell -Command "[console]::CursorVisible=$true"
echo.
echo =========================================
echo [ERROR] Build Failed! Fix errors above.
echo =========================================
powershell -Command "[console]::CursorVisible=$true"
pause
