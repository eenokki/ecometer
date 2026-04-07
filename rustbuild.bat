@echo off
setlocal enabledelayedexpansion

echo ==========================
echo Building Rust benchmarks...
echo ==========================

set SUCCESS_LIST=
set FAIL_LIST=

for /d %%D in (Rust\*) do (
    set "FOLDER=%%~nxD"
    set "RSFILE=%%D\%%~nxD.rs"
    set "OUTEXE=%%D\%%~nxD.exe"

    echo.
    echo Compiling !FOLDER!...

    if exist "!RSFILE!" (
        rustc "!RSFILE!" -O -o "!OUTEXE!"

        if exist "!OUTEXE!" (
            echo SUCCESS: !FOLDER!
            set "SUCCESS_LIST=!SUCCESS_LIST! !FOLDER!"
        ) else (
            echo FAILED: !FOLDER!
            set "FAIL_LIST=!FAIL_LIST! !FOLDER!"
        )
    ) else (
        echo WARNING: !FOLDER! missing !FOLDER!.rs
        set "FAIL_LIST=!FAIL_LIST! !FOLDER!"
    )
)

echo.
echo ==========================
echo Compilation Summary
echo ==========================
echo Successful benchmarks: !SUCCESS_LIST!
echo Failed benchmarks: !FAIL_LIST!
echo ==========================
pause
