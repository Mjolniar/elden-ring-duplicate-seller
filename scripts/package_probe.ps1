$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$Configuration = "Release"
$buildDll = Join-Path $repoRoot "build\$Configuration\ERDuplicateSeller.dll"
$configPath = Join-Path $repoRoot "config\duplicate_seller.ini"
$routineTemplatePath = Join-Path $repoRoot "config\mutation_routines.example.ini"
$distRoot = Join-Path $repoRoot "dist"
$probeDir = Join-Path $distRoot "probe"

function Resolve-RequiredPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description is missing: $Path. Run .\scripts\build.ps1 first."
    }

    return (Resolve-Path -LiteralPath $Path).Path
}

function Assert-SafeProbePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$ProbeDir
    )

    $repoFull = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd('\')
    $probeFull = [System.IO.Path]::GetFullPath($ProbeDir).TrimEnd('\')
    $expectedProbe = [System.IO.Path]::GetFullPath((Join-Path $repoFull "dist\probe")).TrimEnd('\')

    if (-not $probeFull.Equals($expectedProbe, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean unexpected probe path: $probeFull"
    }
}

function Test-DryRunEnabled {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ConfigPath
    )

    $activeDryRunLines = Get-Content -LiteralPath $ConfigPath |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith("#") -and -not $_.StartsWith(";") -and $_ -match "^dry_run\s*=" }

    $effectiveDryRunLine = $activeDryRunLines | Select-Object -Last 1
    return $effectiveDryRunLine -match "^dry_run\s*=\s*true\s*$"
}

$resolvedDll = Resolve-RequiredPath -Path $buildDll -Description "Release DLL"
$resolvedConfig = Resolve-RequiredPath -Path $configPath -Description "Config file"
$resolvedRoutineTemplate = Resolve-RequiredPath -Path $routineTemplatePath -Description "Mutation routine example"

if (-not (Test-DryRunEnabled -ConfigPath $resolvedConfig)) {
    throw "Config must contain an active dry_run=true line before creating a probe package."
}

Assert-SafeProbePath -RepoRoot $repoRoot -ProbeDir $probeDir

# This directory may now be the installed, live-validated native seller.
# Do not silently erase its active profile, prices or validation evidence.
if (Test-Path -LiteralPath (Join-Path $probeDir 'duplicate_seller.ini')) {
    $existing = Get-Content -LiteralPath (Join-Path $probeDir 'duplicate_seller.ini') -Raw
    if ($existing -match '(?m)^native_sale_enabled\s*=\s*true\s*$') {
        throw 'dist/probe contains an active native seller. Use the provisional package workflow; preserve this installed directory.'
    }
}

if (Test-Path -LiteralPath $probeDir) {
    Remove-Item -LiteralPath $probeDir -Recurse -Force
}

New-Item -ItemType Directory -Path $probeDir | Out-Null

Copy-Item -LiteralPath $resolvedDll -Destination (Join-Path $probeDir "ERDuplicateSeller.dll")
Copy-Item -LiteralPath $resolvedConfig -Destination (Join-Path $probeDir "duplicate_seller.ini")
Copy-Item -LiteralPath $resolvedRoutineTemplate -Destination (Join-Path $probeDir "mutation_routines.example.ini")

$readme = @"
ERDuplicateSeller offline probe package

Purpose:
- Validate DLL load, runtime guards, module metadata, and read-only probe blockers.
- This package is not live inventory integration and cannot sell items.

Safety:
- Offline modded play only.
- Do not use with Easy Anti-Cheat or official online play.
- Use a throwaway save/profile.
- Keep dry_run=true.
- Do not enable mutation or selling from this package.

Manual install:
1. Copy ERDuplicateSeller.dll and duplicate_seller.ini together.
2. Manually add ERDuplicateSeller.dll to your Mod Engine 2 external_dlls list.
3. Do not let this script edit Mod Engine 2 or randomizer files.
4. Launch through the offline Mod Engine 2 flow.
5. After launch, check ERDuplicateSeller.log beside the DLL.
6. OutputDebugString logs are also available through DebugView or Visual Studio Output.

Expected log lines:
- ERDuplicateSeller loaded:
- Runtime guard:
- Mutation routine candidates:
- Read-only probe:
- Runtime diagnostics:

Expected current blocker:
- No verified read-only inventory signatures are configured.
- mutation_routines.ini is absent by default; mutation_routines.example.ini is documentation only.
- mutation_routines_ready=0
- remove_item_ready=0
- add_runes_ready=0
- remove_item routine is missing
- add_runes routine is missing

Stop after collecting diagnostics. Inventory decoding, item removal, and rune awarding are intentionally not implemented in this package.
"@

Set-Content -LiteralPath (Join-Path $probeDir "README_PROBE.txt") -Value $readme -Encoding ASCII

Write-Host "Created probe package: $probeDir"
Get-ChildItem -LiteralPath $probeDir | Select-Object Name, Length
