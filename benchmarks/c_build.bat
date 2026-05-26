@echo off
setlocal EnableDelayedExpansion

echo ==========================
echo Building C benchmarks...
echo ==========================

set SUCCESS_LIST=
set FAIL_LIST=

for %%F in (*.c) do (

    set NAME=%%~nF
    set OUTFILE=%%~nF.exe

    if exist "!OUTFILE!" (
        echo Skipping !NAME!, already exists
        set SUCCESS_LIST=!SUCCESS_LIST! !NAME!
    ) else (
        echo.
        echo Compiling !NAME!...

        gcc "%%F" -O3 -lm -o "!OUTFILE!"

        if exist "!OUTFILE!" (
            echo SUCCESS: !NAME!
            set SUCCESS_LIST=!SUCCESS_LIST! !NAME!
        ) else (
            echo FAILED: !NAME!
            set FAIL_LIST=!FAIL_LIST! !NAME!
        )
    )
)

echo.
echo ==========================
echo Summary
echo ==========================
echo Successful: !SUCCESS_LIST!
echo Failed: !FAIL_LIST!
echo ==========================
pause