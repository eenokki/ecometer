# ecometer

A Windows command-line tool that measures the energy consumption of programs using Intel PCM (Performance Counter Monitor), fetches real-time electricity grid emission data from Finland's national transmission system operator Fingrid, and calculates the carbon footprint of running any executable. It also identifies the three lowest-emission time windows within the forecast horizon for greener scheduling.

---

## How It Works

The program runs four stages in sequence:

1. **Fetch**  Downloads 13 datasets from the Fingrid Open Data API
2. **Forecast**  Calculates a gCO2/kWh emission factor for each 15-minute slot until the next 18:00
3. **Measure**  Runs the target program as a subprocess and measures its energy consumption using Intel RAPL via PCM
4. **Report**  Calculates the carbon emissions of the run and shows three green windows where emissions would be lower

All raw data and forecast results are stored in `data.json`. Each measurement is appended to `emissions.csv`.

---

## Requirements

### Software

- Windows 10 or 11 (64-bit)
- Visual Studio 2022 or later with the following components:
  - Desktop development with C++
  - C++ CMake tools for Windows
  - MSVC v143 build tools (x64/x86) with Spectre Mitigations
- CMake 3.20 or later
- Git
- vcpkg
- Windows Driver Kit (WDK) is required to build the Intel PCM MSR driver

### Accounts and Keys

- Fingrid Open Data API key. Register at https://data.fingrid.fi

---

## Installation

### 1. Install vcpkg

```powershell
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

### 2. Install required packages via vcpkg

```powershell
C:\vcpkg\vcpkg install curl:x64-windows
C:\vcpkg\vcpkg install nlohmann-json:x64-windows
```

### 3. Clone Intel PCM

```powershell
git clone https://github.com/intel/pcm.git
```

Note the path where you cloned it, you will need it in the `.env` file.

### 4. Build and sign the Intel PCM MSR driver

The MSR driver is required for PCM to access hardware energy counters.

Open **Developer Command Prompt for Visual Studio** and navigate to:

```bash
cd path\to\pcm\src\WinMSRDriver
```

Build the driver:

```bash
MSBuild.exe MSR.vcxproj -property:Configuration=Release -property:Platform=x64
```

The driver file will appear at:

```
src\WinMSRDriver\x64\Release\MSR.sys
```

Sign the driver by opening **PowerShell as Administrator** in the same folder:

```powershell
$cert = New-SelfSignedCertificate `
    -Type CodeSigning `
    -Subject "CN=TestCert" `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -KeyExportPolicy Exportable

$pwd = Read-Host -Prompt "Enter password" -AsSecureString

Export-PfxCertificate -Cert $cert -FilePath TestCert.pfx -Password $pwd

signtool sign /fd SHA256 /f TestCert.pfx `
    /p ([Runtime.InteropServices.Marshal]::PtrToStringAuto(
        [Runtime.InteropServices.Marshal]::SecureStringToBSTR($pwd))) `
    /t http://timestamp.digicert.com MSR.sys
```

Install the certificate by double-clicking `TestCert.pfx`:

- Install to: **Current User**
- Store: **Trusted Root Certification Authorities**

Enable test signing mode (required for self-signed drivers):

```powershell
bcdedit /set testsigning on
```

Restart the computer.

Copy the signed `MSR.sys` to the ecometer project folder:

```powershell
Copy-Item "path\to\pcm\src\WinMSRDriver\x64\Release\MSR.sys" `
          "path\to\ecometer\msr.sys"
```

---

## Project Setup

### 1. Clone ecometer

```powershell
git clone https://github.com/yourname/ecometer.git
cd ecometer
```

### 2. Create .env file

Create a file named `.env` in the project root with the following content:

```
APIKEY=your_fingrid_api_key_here
PCM_DIR=C:/path/to/pcm
```

- `APIKEY`  your Fingrid Open Data API key
- `PCM_DIR`  path to the Intel PCM source directory (use forward slashes)

### 3. Build

Run the build script as Administrator:

```powershell
.\build.ps1 -Clean
```

The build script handles:

- CMake configuration
- Patching the Spectre mitigation requirement in PCM if needed
- Compiling the Release binary

The executable is produced at:

```
build\Release\ecometer.exe
```

The following files are automatically copied next to the executable after build:

```
libcurl.dll
z.dll
```

---

## Usage

Run as Administrator from the project root:

```powershell
.\build\Release\ecometer.exe <program.exe>
.\build\Release\ecometer.exe <program.exe> <parameter>
```

Examples:

```powershell
.\build\Release\ecometer.exe notepad.exe
.\build\Release\ecometer.exe fannkuch.exe 12
```

The `.env` file and `msr.sys` must be in the directory from which the program is run.

---

## Output

### Console

```
Green Benchmark Emission Analyzer
Time:     25.05.2026 01:45 (Helsinki)
Program:  fannkuch.exe
Parameter: 12

STAGE 1: Fetching Fingrid data
...13/13 OK

STAGE 2: Calculating emission forecast
Slots: 65

STAGE 3: PCM Energy Measurement
PKG energy:  80.5566 J
DRAM energy: N/A
Wall time:   6.19 s
Avg freq:    0.250 GHz
Nominal:     4.200 GHz
CPU load:    6.0 %

STAGE 4: Emission Report
Energy used:  80.5566 J (PKG)  (0.00002238 kWh)
Wall time:    6.19 s

Current emission
Time:        25.05.2026 01:18 (Helsinki)
Grid factor: 13.142 gCO2/kWh
Emission:    0.0002941 gCO2

3 green windows (3 lowest emission slots until 18:00)
  #1  25.05.2026 07:15   12.59 gCO2/kWh  →  0.0002817 gCO2  (saves 4.22%)
  #2  25.05.2026 07:30   12.62 gCO2/kWh  →  0.0002823 gCO2  (saves 3.99%)
  #3  25.05.2026 07:45   12.62 gCO2/kWh  →  0.0002825 gCO2  (saves 3.94%)
```

### Files

`data.json`  raw Fingrid data and calculated forecast, overwritten on each run

`emissions.csv`  one row per measurement, appended on each run:

```
Timestamp;Program;Param;PKG_J;DRAM_J;WallTime_s;Now_gCO2_kWh;Now_emission;
Best1_time;Best1_gCO2_kWh;Best1_emission;Best1_saving_pct;...
```

---

## Energy Measurement

PKG energy is read from Intel RAPL (Running Average Power Limit) hardware counters via Intel PCM. It covers the processor package including cores, cache, and memory controller.

DRAM energy is read from the RAPL DRAM domain if supported by the hardware. This is available on Intel Xeon server processors but typically not on consumer Core i-series processors. If DRAM measurement returns zero despite being reported as available, it is treated as unavailable and shown as N/A.

If DRAM is available, total energy is calculated as:

```
totalJoules = PKG_joules + DRAM_joules
```

Otherwise only PKG is used.

---

## Emission Calculation

The emission factor A (gCO2/kWh) for each 15-minute slot is calculated as:

```
A = B / C
```

Where:

```
B = sum of production sources × emission factors
  + sum of transmission flows × emission factors

C = electricity production forecast (dataset 242, MW)
```

Transmission flows use signed values  positive for export, negative for import.

The program emission is then:

```
kWh     = totalJoules / 3,600,000
gCO2    = kWh × A
```

The forecast horizon is always limited to the next 18:00 Helsinki time, when dataset 242 is updated with the following day's forecast.

---

## Emission Factors

Emission factors (gCO2/kWh) are defined in `main.cpp` in the `EmissionFactors` struct and must be filled in manually:

```cpp
struct EmissionFactors {
    double p188 = 0;    // Nuclear power
    double p191 = 0;    // Hydro power
    double p201 = 221;  // District heating CHP
    double p202 = 123;  // Industrial CHP
    double p205 = 30;   // Reserve / small-scale
    double p245 = 0;    // Wind forecast
    double p248 = 0;    // Solar forecast
    double p31  = 0;    // Transmission FI-SE1
    double p32  = 0;    // Transmission FI-SE3
    double p140 = 0;    // Transmission FI-EE
    double p370 = 0;    // Transmission FI-NO4
};
```

---

## Fingrid Datasets Used

| ID  | Description | Period | Unit |
|-----|-------------|--------|------|
| 31  | Transmission FI-SE1 | 1h | MW |
| 32  | Transmission FI-SE3 | 1h | MW |
| 140 | Transmission FI-EE | 15min | MW |
| 370 | Transmission FI-NO4 | 1h | MW |
| 188 | Nuclear power real-time | 3min | MW |
| 191 | Hydro power real-time | 3min | MW |
| 201 | District heating CHP real-time | 3min | MW |
| 202 | Industrial CHP real-time | 3min | MW |
| 205 | Reserve power plants real-time | 3min | MW |
| 245 | Wind power forecast | 15min | MW |
| 248 | Solar power forecast | 15min | MW |
| 242 | Electricity production forecast | 15min | MW |
| 265 | Emission factor real-time | 3min | gCO2/kWh |

---

## Common Issues

**Spectre mitigation error during build**

Install the Spectre-mitigated libraries from Visual Studio Installer under Individual Components. Search for Spectre and install all entries matching your toolset version.

**PCM init failed**

The program must be run as Administrator. Right-click PowerShell and select Run as Administrator.

**DRAM energy shows N/A**

DRAM RAPL measurement is not supported on consumer Intel Core processors. This is a hardware limitation and does not affect the validity of PKG energy measurements.

**API key missing**

Ensure the `.env` file exists in the directory from which the program is run and contains a valid `APIKEY=` line.

**data.json not found when running forecast**

Run the fetcher stage first by running the full program. The forecast stage reads from `data.json` produced by the fetch stage.

---

## Notes

- The program must be run as Administrator for PCM hardware counter access
- Test signing mode must be enabled if using a self-signed MSR driver
- The `.env` file contains sensitive information and is excluded from version control
- Fingrid API allows one request per 2 seconds per dataset. The program waits 3 seconds between requests
