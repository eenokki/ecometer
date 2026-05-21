@echo off
setlocal enabledelayedexpansion

echo ==========================
echo Building Go benchmarks
echo ==========================

set SUCCESS_LIST=
set FAIL_LIST=

REM Load build.env
for /f "usebackq tokens=1,2 delims==" %%A in ("build.env") do (
    set "%%A=%%B"
)

REM Global CGO ON
set CGO_ENABLED=1

for /d %%D in (Go\*) do (

    set "NAME=%%~nD"

    echo.
    echo ==========================
    echo Compiling !NAME!
    echo ==========================

    pushd "%%D"

    REM Reset CGO flags each run (important)
    set CC=%CC%
    set CGO_CFLAGS=
    set CGO_LDFLAGS=

    REM Special case: regex-redux (needs PCRE)
    if /I "!NAME!"=="regex-redux" (
        set CGO_CFLAGS=-I%MSYS2_ROOT%\mingw64\include
        set CGO_LDFLAGS=-L%MSYS2_ROOT%\mingw64\lib -lpcre
    )

    go build -o "!NAME!.exe" .

    if errorlevel 1 (
        echo FAILED: !NAME!
        set "FAIL_LIST=!FAIL_LIST! !NAME!"
    ) else (
        echo SUCCESS: !NAME!
        set "SUCCESS_LIST=!SUCCESS_LIST! !NAME!"
    )

    popd
)

echo.
echo ==========================
echo Go Compilation Summary
echo ==========================
echo Successful benchmarks: !SUCCESS_LIST!
echo Failed benchmarks: !FAIL_LIST!
echo ==========================

pause
