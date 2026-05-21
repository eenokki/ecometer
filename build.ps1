# Load .env manually
if (Test-Path ".env") {
    Get-Content ".env" | ForEach-Object {
        if ($_ -match "^\s*([^#=]+)\s*=\s*(.+)\s*$") {
            $name = $matches[1]
            $value = $matches[2]
            Set-Item -Path "env:$name" -Value $value
        }
    }
}

# Debug (optional but recommended)
echo "PCM_INCLUDE = $env:PCM_INCLUDE"

# Build
cl /EHsc /O2 /std:c++17 `
/I "$env:PCM_INCLUDE" `
pcm_measurer.cpp `
"$env:PCM_INCLUDE\cpucounters.h"