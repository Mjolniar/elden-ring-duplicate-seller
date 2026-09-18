param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$LogPath
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $LogPath)) {
    Write-Error "Probe log does not exist: $LogPath"
    exit 1
}

$content = Get-Content -LiteralPath $LogPath -Raw
$requiredFragments = @(
    "ERDuplicateSeller loaded:",
    "Runtime guard:",
    "Mutation routine candidates:",
    "Read-only probe:",
    "Runtime diagnostics:",
    "inventory_read_ready=0",
    "No verified read-only inventory signatures are configured.",
    "mutation_routines_ready=0",
    "remove_item_ready=0",
    "add_runes_ready=0",
    "remove_item routine is missing",
    "add_runes routine is missing"
)

$missing = @()
foreach ($fragment in $requiredFragments) {
    if ($content.Contains($fragment)) {
        Write-Host "PASS $fragment"
    } else {
        Write-Host "FAIL $fragment"
        $missing += $fragment
    }
}

if ($missing.Count -gt 0) {
    Write-Error "Probe log is missing $($missing.Count) required fragment(s)."
    exit 1
}

Write-Host "Probe log check passed."
