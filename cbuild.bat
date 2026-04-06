@echo off
setlocal enabledelayedexpansion

echo ==========================
echo Building C benchmarks...
echo ==========================

rem Initialize lists
set SUCCESS_LIST=
set FAIL_LIST=

rem Loop through all subfolders inside C\
for /d %%D in (C\*) do (

    rem Loop through .c files inside each benchmark subfolder
    for %%F in (%%D\*.c) do (

        set NAME=%%~nF
        set OUTFILE=%%D\!NAME!.exe

        rem Check if .exe already exists
        if exist "!OUTFILE!" (
            echo Skipping !NAME!, already exists
            set SUCCESS_LIST=!SUCCESS_LIST! !NAME!
        ) else (
            echo.
            echo Compiling !NAME! from %%F...
            gcc "%%F" -O3 -lm -o "!OUTFILE!"

            rem Check result
            if exist "!OUTFILE!" (
                echo SUCCESS: !NAME!
                set SUCCESS_LIST=!SUCCESS_LIST! !NAME!
            ) else (
                echo FAILED: !NAME!
                set FAIL_LIST=!FAIL_LIST! !NAME!
            )
        )

    )
)

rem Print summary lists
echo.
echo ==========================
echo C Compilation Summary
echo ==========================
echo Successful benchmarks: !SUCCESS_LIST!
echo Failed benchmarks: !FAIL_LIST!
echo ==========================
pause
