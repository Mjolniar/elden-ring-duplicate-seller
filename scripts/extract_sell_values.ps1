param(
    [Parameter(Mandatory=$true)][string]$RegulationBin,
    [Parameter(Mandatory=$true)][string]$ParamDefsDir,
    [string]$OutputPath = "",
    [string]$PythonDepsDir = "",
    [string]$SoulstructVersion = "2.3.2",
    [switch]$SkipConfigUpdate
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "dist\probe\sell_values.txt"
}
if ([string]::IsNullOrWhiteSpace($PythonDepsDir)) {
    $PythonDepsDir = Join-Path $repoRoot "build\pydeps\soulstruct"
}

function Resolve-RequiredPath {
    param(
        [string]$Path,
        [string]$Description
    )

    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
    if (-not $resolved) {
        throw "$Description not found: $Path"
    }
    return $resolved.Path
}

function Ensure-SoulstructDependency {
    param(
        [string]$TargetDir,
        [string]$Version
    )

    $packageMarker = Join-Path $TargetDir "soulstruct"
    if (-not (Test-Path -LiteralPath $packageMarker)) {
        New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
        python -m pip install --disable-pip-version-check --target $TargetDir "soulstruct==$Version"
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to install soulstruct==$Version into $TargetDir"
        }
    }

    $runtimeConfig = Join-Path $TargetDir "soulstruct\base\params\ParamCrypt\ParamCrypt.runtimeconfig.json"
    $runtimeConfigDir = Split-Path -Parent $runtimeConfig
    if (-not (Test-Path -LiteralPath $runtimeConfigDir)) {
        throw "ParamCrypt directory not found after dependency install: $runtimeConfigDir"
    }

    @'
{
  "runtimeOptions": {
    "tfm": "net8.0",
    "framework": {
      "name": "Microsoft.NETCore.App",
      "version": "8.0.0"
    }
  }
}
'@ | Set-Content -LiteralPath $runtimeConfig -Encoding UTF8
}

function Set-SellValuePath {
    param(
        [string]$ConfigPath,
        [string]$SellValuePath
    )

    if (-not (Test-Path -LiteralPath $ConfigPath)) {
        Write-Output "Probe config not found; skipped config update: $ConfigPath"
        return
    }

    $resolvedSellValuePath = (Resolve-Path -LiteralPath $SellValuePath).Path
    $lines = Get-Content -LiteralPath $ConfigPath
    $updated = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^\s*sell_value_path\s*=') {
            $lines[$i] = "sell_value_path=$resolvedSellValuePath"
            $updated = $true
        }
    }
    if (-not $updated) {
        $lines += "sell_value_path=$resolvedSellValuePath"
    }
    Set-Content -LiteralPath $ConfigPath -Value $lines -Encoding ASCII
    Write-Output "Updated probe config sell_value_path: $ConfigPath"
}

$resolvedRegulation = Resolve-RequiredPath -Path $RegulationBin -Description "regulation.bin"
$resolvedDefs = Resolve-RequiredPath -Path $ParamDefsDir -Description "Param defs directory"
$extractor = Resolve-RequiredPath -Path (Join-Path $PSScriptRoot "extract_sell_values.py") -Description "Sell-value extractor"

Ensure-SoulstructDependency -TargetDir $PythonDepsDir -Version $SoulstructVersion

$previousPythonPath = $env:PYTHONPATH
try {
    if ([string]::IsNullOrWhiteSpace($previousPythonPath)) {
        $env:PYTHONPATH = $PythonDepsDir
    } else {
        $env:PYTHONPATH = "$PythonDepsDir;$previousPythonPath"
    }
    $env:PYTHONIOENCODING = "utf-8"

    python $extractor `
        --regulation-bin $resolvedRegulation `
        --param-defs-dir $resolvedDefs `
        --output $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "Sell-value extraction failed with exit code $LASTEXITCODE"
    }
} finally {
    $env:PYTHONPATH = $previousPythonPath
}

if (-not $SkipConfigUpdate) {
    Set-SellValuePath `
        -ConfigPath (Join-Path $repoRoot "dist\probe\duplicate_seller.ini") `
        -SellValuePath $OutputPath
}

Write-Output "Sell-value extraction complete: $OutputPath"
