@echo off

echo ==============================
echo Checking installed languages...
echo ==============================

echo.
echo [Java]
where java >nul 2>nul && java -version || echo Java is NOT installed.

echo.
echo [C - GCC]
where gcc >nul 2>nul && gcc --version || echo GCC (C compiler) is NOT installed.

echo.
echo [C++ - G++]
where g++ >nul 2>nul && g++ --version || echo G++ (C++ compiler) is NOT installed.

echo.
echo [Rust]
where rustc >nul 2>nul && rustc --version || echo Rust is NOT installed.

echo.
echo [Cargo]
where cargo >nul 2>nul && cargo --version || echo Cargo is NOT installed.

echo.
echo [Node.js]
where node >nul 2>nul && node --version || echo Node.js is NOT installed.

echo.
echo [Python]
where python >nul 2>nul && python --version || echo Python is NOT installed.

echo.
echo ==============================
echo Done checking!
pause
