[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.6'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot 'mechanic_testing.uproject'
$BuildScript = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'

if (-not (Test-Path -LiteralPath $BuildScript)) {
    throw "Unreal Build.bat was not found at $BuildScript. Supply -EngineRoot for your UE 5.6 installation."
}
if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Save and close Unreal Editor before rebuilding so the project DLL can be replaced. This script does not close it for you.'
}

Push-Location $ProjectRoot
try {
    & $BuildScript mechanic_testingEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoEngineChanges -NoHotReloadFromIDE -NoUBA -NoUBALocal
    if ($LASTEXITCODE -ne 0) {
        throw "Unreal build failed (exit $LASTEXITCODE). Keep the output above; reopen the project only after Result: Succeeded."
    }
}
finally {
    Pop-Location
}
