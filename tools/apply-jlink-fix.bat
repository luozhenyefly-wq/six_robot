@echo off
REM ===========================================================================
REM  apply-jlink-fix.bat  --  one-click wrapper for fix-jlink-env.ps1
REM
REM  ALL MESSAGES HERE ARE ASCII ON PURPOSE so cmd.exe can never garble them.
REM  The real work is done by fix-jlink-env.ps1 (which is UTF-8 with BOM).
REM
REM  REQUIREMENT: Keil (UV4.exe) MUST be closed first, or the script refuses
REM  to run. That is intentional - renaming a loaded DLL would fail.
REM ===========================================================================

setlocal
set "SCRIPT=%~dp0fix-jlink-env.ps1"

echo.
echo ===========================================================================
echo   Keil J-Link environment rollback  (fix-jlink-env.ps1 -Apply)
echo ===========================================================================
echo.

if not exist "%SCRIPT%" (
    echo [X] Cannot find: "%SCRIPT%"
    echo     Keep this .bat in the same folder as fix-jlink-env.ps1 .
    goto :end
)

echo Checking whether Keil is still running ...
tasklist /fi "IMAGENAME eq UV4.exe" 2>nul | find /i "UV4.exe" >nul
if not errorlevel 1 (
    echo.
    echo [X] UV4.exe IS STILL RUNNING.
    echo.
    echo     Please CLOSE KEIL COMPLETELY first:
    echo       - close the uVision window
    echo       - if it is hung / "not responding", kill it in Task Manager
    echo       - confirm UV4.exe is gone from Task Manager
    echo     Then run this .bat again.
    echo.
    goto :end
)
echo     OK - Keil is not running.
echo.

echo Running the rollback script ...
echo ---------------------------------------------------------------------------
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -Apply
set "RC=%ERRORLEVEL%"
echo ---------------------------------------------------------------------------
echo Script exit code: %RC%
echo.

:end
echo.
echo Press any key to close this window ...
pause >nul
endlocal
