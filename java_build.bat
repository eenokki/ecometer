@echo off
setlocal enabledelayedexpansion

echo ==========================
echo Building Java benchmarks...
echo ==========================

rem Initialize lists
set SUCCESS_LIST=
set FAIL_LIST=

rem Loop through all subfolders inside Java\
for /d %%D in (Java\*) do (

    rem Loop through .java files inside each benchmark subfolder
    for %%F in (%%D\*.java) do (

        set NAME=%%~nF

        echo.
        echo Compiling !NAME! from %%F...

        rem Compile using javac
        javac "%%F"
        
        rem Check result
        if !ERRORLEVEL! EQU 0 (
            echo SUCCESS: !NAME!
            set SUCCESS_LIST=!SUCCESS_LIST! !NAME!
        ) else (
            echo FAILED: !NAME!
            set FAIL_LIST=!FAIL_LIST! !NAME!
        )

    )
)

rem Print summary lists
echo.
echo ==========================
echo Java Compilation Summary
echo ==========================
echo Successful benchmarks: !SUCCESS_LIST!
echo Failed benchmarks: !FAIL_LIST!
echo ==========================
pause
