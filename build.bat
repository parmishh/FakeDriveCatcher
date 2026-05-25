@echo off
echo Compiling FakeDriveCatcher...
g++ FakeDriveCatcher.cpp -o FakeDriveCatcher.exe -static
if %errorlevel% neq 0 (
    echo Compilation failed. Ensure you have g++ installed (MinGW).
) else (
    echo Compilation successful! You can now run FakeDriveCatcher.exe as Administrator.
)
pause
