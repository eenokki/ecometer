@echo off
setlocal enabledelayedexpansion

echo ==========================
echo Building C++ benchmarks...
echo ==========================

set SUCCESS_LIST=
set FAIL_LIST=

rem Loop through all subfolders inside C++\
for /d %%D in (C++\*) do (

    rem Loop through .cpp and .c++ files
    for %%F in (%%D\*.cpp %%D\*.c++) do (

        set NAME=%%~nF
        set OUTFILE=%%D\!NAME!.exe

        if exist "!OUTFILE!" (
            echo Skipping !NAME!, already exists
            set SUCCESS_LIST=!SUCCESS_LIST! !NAME!
        ) else (
            echo.
            echo Compiling !NAME! from %%F...

            g++ "%%F" -O3 -o "!OUTFILE!"

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

echo.
echo ==========================
echo C++ Compilation Summary
echo ==========================
echo Successful: !SUCCESS_LIST!
echo Failed: !FAIL_LIST!
echo ==========================
pause
