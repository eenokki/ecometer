# Intel PCM Installation Guide (Windows)

## Overview

This guide explains how to install and build Intel PCM (Performance Counter Monitor) on Windows, including the additional steps required for building and signing the MSR driver.

Official Intel PCM repository:

https://github.com/intel/pcm

Windows installation requires several additional dependencies compared to Linux, including Visual Studio components, Windows SDK, Windows Driver Kit (WDK), and driver signing steps.

## Prerequisites

Install the following software before starting:

* Visual Studio and Windows DriverKit (WDK) during installation
* Git
* CMake
* Windows Software Development Kit (SDK)

## Visual Studio Components

Open **Visual Studio Installer** and install the following components.

### Workloads

Enable:

* Desktop development with C++

Inside it also select:

* C++ CMake tools for Windows

### Desktop & Mobile

Enable:

* .NET desktop development

### Individual Components

Search and install:

* MSVC v143 - VS 2022 C++ x64/x86 Spectre-mitigated libs (Latest)

If you get errors install any additional components containing words:

* Latest
* Build Tools
* Spectre

## Clone Intel PCM Repository

Open command prompt and run:

```bash
git clone https://github.com/intel/pcm.git
cd pcm
```

or manually download and extract the repository from GitHub.

## Common Spectre Error

A common build error is:

```text
error MSB8040: Spectre-mitigated libraries are required for this project.
Install them from the Visual Studio installer.
```

### Solution

The issue was resolved by:

1. Installing the latest build tools
2. Installing Spectre-related libraries
3. Enabling Spectre mitigations inside Visual Studio project settings

Open project settings:

```text
Solution Explorer
    → Right click Project
    → Properties
    → Configuration Properties
    → C/C++
    → Code Generation
    → Spectre Mitigations
    → Enabled
```

## Build MSR Driver

Open **Developer Command Prompt for Visual Studio**

Navigate to:

```bash
cd src\WinMSRDriver
```

Compile the driver:

```bash
MSBuild.exe MSR.vcxproj -property:Configuration=Release -property:Platform=x64
```

If successful, the driver file should appear at:

```text
src\WinMSRDriver\x64\Release\MSR.sys
```

## Sign the Driver

Open **PowerShell as Administrator**

Navigate in PowerShell to the folder containing:

* MSR.sys

Run:

```powershell
$cert = New-SelfSignedCertificate -Type CodeSigning -Subject "CN=TestCert" -CertStoreLocation "Cert:\CurrentUser\My" -KeyExportPolicy Exportable

$pwd = Read-Host -Prompt "Enter password for PFX file" -AsSecureString

Export-PfxCertificate -Cert $cert -FilePath TestCert.pfx -Password $pwd

signtool sign /fd SHA256 /f TestCert.pfx /p ([Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($pwd))) /t http://timestamp.digicert.com MSR.sys
```

Set a password when prompted.

## Install Certificate

Double-click TestCert.pfx

Install to current user

Select "Place all certificates in the following store"

Choose "Trusted Root Certification Authorities"

## Enable Test Signing Mode

Open command prompt as Administrator:

```bash
bcdedit /set testsigning on
```

Restart the computer.

This allows Windows to load test-signed drivers.


## Optional: Disable Secure Boot

If driver signing still causes issues:

1. Enter BIOS/UEFI during startup
2. Disable:

```text
Secure Boot
```
To disable Secure Boot, you may need to set a supervisor password in the BIOS/UEFI settings and change settings on the following:

* CSM support
* UEFI mode

Warning:

Disabling Secure Boot reduces system security. Re-enable it after installation and install default keys if necessary in BIOS/UEFI settings.

## Build PCM

Navigate to project root:

```bash
cd pcm
```

Run:

```bash
cmake -B build
cmake --build build --config Release --parallel
```

## Final Setup

Copy MSR.sys

to the same directory where PCM.exe is located.

Run PCM.exe as Administrator

Intel PCM should now be installed and able to access processor performance counters and RAPL energy measurements.

## Notes

Windows installation is considerably more complex than Linux because it requires:

* Driver compilation
* Driver signing
* Additional Visual Studio components
* Administrative permissions
* Test-signing configuration
