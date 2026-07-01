@echo off
title OLO Show — Building Executable
echo.
echo  ==========================================
echo   OLO Show Firmware Builder — EXE Packager
echo  ==========================================
echo.

REM Check Python
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [Error] Python not found. Install Python 3 from python.org
    pause
    exit /b 1
)

REM Install PyInstaller if needed
echo [Step 1] Installing PyInstaller...
python -m pip install pyinstaller --quiet

echo.
echo [Step 2] Building OLO_Show_Builder.exe ...
pyinstaller --noconfirm --onefile --windowed ^
    --name "OLO_Show_Builder" ^
    --icon NONE ^
    build_firmware.py

echo.
if exist dist\OLO_Show_Builder.exe (
    echo [Success] OLO_Show_Builder.exe created in dist\
    echo.
    echo Place OLO_Show_Builder.exe inside your OLO_Show project folder
    echo and double-click to run — no Python required!
) else (
    echo [Error] Build failed. Check output above.
)

echo.
pause
