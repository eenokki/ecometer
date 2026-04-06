@echo off
setlocal enabledelayedexpansion

echo ==========================
echo Building C# benchmarks...
echo ==========================

set SUCCESS_LIST=
set FAIL_LIST=

for /d %%D in (CSharp\*) do (
    set "FOLDER=%%~nxD"
    set "CSFILE=%%D\Program.cs"
    set "CSPROJ=%%D\%%~nxD.csproj"
    set "PUBLISH_DIR=%%D\bin\Release\net10.0\win-x64\publish"
    set "PUBLISH_EXE=!PUBLISH_DIR!\%%~nxD.exe"
    set "FINAL_EXE=%%D\%%~nxD.exe"

    echo.
    echo Processing !FOLDER!...

    rem Check Program.cs
    if not exist "!CSFILE!" (
        echo WARNING: No Program.cs
        set "FAIL_LIST=!FAIL_LIST! !FOLDER!"
    )

    if exist "!CSFILE!" (

        rem Create csproj if missing
        if not exist "!CSPROJ!" (
            >"!CSPROJ!" echo ^<Project Sdk="Microsoft.NET.Sdk"^>
            >>"!CSPROJ!" echo.
            >>"!CSPROJ!" echo   ^<PropertyGroup^>
            >>"!CSPROJ!" echo     ^<OutputType^>Exe^</OutputType^>
            >>"!CSPROJ!" echo     ^<TargetFramework^>net10.0^</TargetFramework^>
            >>"!CSPROJ!" echo     ^<ImplicitUsings^>enable^</ImplicitUsings^>
            >>"!CSPROJ!" echo     ^<Nullable^>enable^</Nullable^>
            >>"!CSPROJ!" echo   ^</PropertyGroup^>
            >>"!CSPROJ!" echo.
            >>"!CSPROJ!" echo   ^<ItemGroup^>
            >>"!CSPROJ!" echo     ^<Compile Include="Program.cs" /^>
            >>"!CSPROJ!" echo   ^</ItemGroup^>
            >>"!CSPROJ!" echo.
            >>"!CSPROJ!" echo ^</Project^>
        )

        rem Build exe
        rem dotnet publish "!CSPROJ!" -c Release -r win-x64 --self-contained false 
        dotnet publish "!CSPROJ!" -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true >nul 2>&1


        rem Copy exe to root folder
        if exist "!PUBLISH_EXE!" (
            copy /Y "!PUBLISH_EXE!" "!FINAL_EXE!" >nul
        )

        rem Cleanup bin/obj
        if exist "%%D\bin" rmdir /s /q "%%D\bin"
        if exist "%%D\obj" rmdir /s /q "%%D\obj"

        rem Result check
        if exist "!FINAL_EXE!" (
            echo SUCCESS: !FOLDER!
            set "SUCCESS_LIST=!SUCCESS_LIST! !FOLDER!"
        )
        if not exist "!FINAL_EXE!" (
            echo FAILED: !FOLDER!
            set "FAIL_LIST=!FAIL_LIST! !FOLDER!"
        )
    )
)

echo.
echo ==========================
echo Compilation Summary
echo ==========================
echo Successful: !SUCCESS_LIST!
echo Failed: !FAIL_LIST!
echo ==========================
pause
