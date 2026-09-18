param(
    [Parameter(Mandatory = $false)]
    [string]$ConfigPath,

    [Parameter(Mandatory = $false)]
    [string]$SnapshotDir,

    [Parameter(Mandatory = $false)]
    [string]$OutputPath,

    [Parameter(Mandatory = $false)]
    [ValidateSet("mark_empty", "mark_empty_and_decrement_count", "compact_and_decrement_count")]
    [string]$RemovalMode = "mark_empty_and_decrement_count",

    [Parameter(Mandatory = $false)]
    [ValidateRange(1, 100)]
    [int]$MinimumDecodedPercent = 75,

    [Parameter(Mandatory = $false)]
    [ValidateRange(0, 1000000)]
    [int]$MaximumDecodedShortfall = 4,

    [Parameter(Mandatory = $false)]
    [ValidateRange(0, 1000)]
    [int]$SampleSellRows = 15
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ConfigPath) {
    $ConfigPath = Join-Path $repoRoot "dist\probe\duplicate_seller.ini"
}
if (-not $SnapshotDir) {
    $SnapshotDir = Join-Path $repoRoot "dist\probe"
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot "dist\probe\snapshot_sale_plan_latest.txt"
}

$cliPath = Join-Path $repoRoot "build\Release\SnapshotSalePlanCli.exe"

function Resolve-RequiredPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description is missing: $Path"
    }

    return (Resolve-Path -LiteralPath $Path).Path
}

function Parse-InventorySourceLine {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Line
    )

    $rawMatch = [regex]::Match($Line, "raw_count=(\d+)")
    $decodedMatch = [regex]::Match($Line, "decoded=(\d+)")
    $skippedMatch = [regex]::Match($Line, "skipped=(\d+)")

    if (-not $rawMatch.Success -or -not $decodedMatch.Success -or -not $skippedMatch.Success) {
        return $null
    }

    return [pscustomobject]@{
        Raw = [int]$rawMatch.Groups[1].Value
        Decoded = [int]$decodedMatch.Groups[1].Value
        Skipped = [int]$skippedMatch.Groups[1].Value
        Line = $Line
    }
}

function Select-LatestStableSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Directory,

        [Parameter(Mandatory = $true)]
        [int]$MinimumDecodedPercent,

        [Parameter(Mandatory = $true)]
        [int]$MaximumDecodedShortfall
    )

    $snapshots = Get-ChildItem -LiteralPath $Directory -Filter "inventory_snapshot_*.txt" |
        Sort-Object LastWriteTime -Descending

    foreach ($snapshot in $snapshots) {
        $source = Select-String -LiteralPath $snapshot.FullName -Pattern "^# source=inventory_read" |
            Select-Object -First 1
        if (-not $source) {
            continue
        }

        $parsed = Parse-InventorySourceLine -Line $source.Line
        if (-not $parsed -or $parsed.Raw -le 0) {
            continue
        }

        $decodedShortfall = $parsed.Raw - $parsed.Decoded
        if (($parsed.Decoded * 100) -ge ($parsed.Raw * $MinimumDecodedPercent) -and
            $decodedShortfall -le $MaximumDecodedShortfall) {
            return [pscustomobject]@{
                File = $snapshot
                Raw = $parsed.Raw
                Decoded = $parsed.Decoded
                Skipped = $parsed.Skipped
                DecodedShortfall = $decodedShortfall
                SourceLine = $parsed.Line
            }
        }
    }

    return $null
}

$resolvedConfig = Resolve-RequiredPath -Path $ConfigPath -Description "Config"
$resolvedSnapshotDir = Resolve-RequiredPath -Path $SnapshotDir -Description "Snapshot directory"
$resolvedCli = Resolve-RequiredPath -Path $cliPath -Description "SnapshotSalePlanCli"

$chosen = Select-LatestStableSnapshot `
    -Directory $resolvedSnapshotDir `
    -MinimumDecodedPercent $MinimumDecodedPercent `
    -MaximumDecodedShortfall $MaximumDecodedShortfall

if (-not $chosen) {
    throw "No stable high-density inventory snapshot found in $resolvedSnapshotDir."
}

$outputFull = [System.IO.Path]::GetFullPath($OutputPath)
$outputParent = Split-Path -Parent $outputFull
if ($outputParent -and -not (Test-Path -LiteralPath $outputParent)) {
    New-Item -ItemType Directory -Path $outputParent | Out-Null
}

& $resolvedCli $resolvedConfig $chosen.File.FullName $RemovalMode |
    Set-Content -LiteralPath $outputFull -Encoding UTF8

$planHead = Get-Content -LiteralPath $outputFull -First 3
$sellLines = Select-String -LiteralPath $outputFull -CaseSensitive -Pattern "^SELL " |
    ForEach-Object { $_.Line }

Write-Host "Selected snapshot: $($chosen.File.Name)"
Write-Host "Source counts: raw=$($chosen.Raw) decoded=$($chosen.Decoded) skipped=$($chosen.Skipped) decoded_shortfall=$($chosen.DecodedShortfall)"
Write-Host "Removal mode: $RemovalMode"
Write-Host "Wrote plan: $outputFull"
foreach ($line in $planHead) {
    Write-Host $line
}

Write-Host "--- category counts ---"
$categoryRows = $sellLines |
    ForEach-Object {
        $match = [regex]::Match($_, "category=([^ ]+)")
        if ($match.Success) {
            $match.Groups[1].Value
        }
    } |
    Group-Object |
    Sort-Object Name |
    Select-Object Name,Count

if ($categoryRows) {
    $categoryRows | Format-Table -AutoSize
} else {
    Write-Host "(no SELL rows)"
}

Write-Host "--- first SELL rows ---"
if ($SampleSellRows -gt 0) {
    $sellLines | Select-Object -First $SampleSellRows | ForEach-Object { Write-Host $_ }
}
