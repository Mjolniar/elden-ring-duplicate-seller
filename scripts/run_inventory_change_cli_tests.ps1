param(
    [Parameter(Mandatory = $true)]
    [string]$CliPath,
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"

function New-TestFile {
    param(
        [string]$Prefix,
        [string]$Contents
    )

    $path = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "$Prefix$([System.Guid]::NewGuid()).txt")
    Set-Content -LiteralPath $path -Value $Contents -Encoding ASCII
    return $path
}

function Invoke-ChangeCli {
    param(
        [string]$BeforePath,
        [string]$AfterPath
    )

    $stdoutPath = [System.IO.Path]::GetTempFileName()
    $stderrPath = [System.IO.Path]::GetTempFileName()
    $arguments = "`"$BeforePath`" `"$AfterPath`""
    try {
        $process = Start-Process `
            -FilePath $CliPath `
            -ArgumentList $arguments `
            -NoNewWindow `
            -Wait `
            -PassThru `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath

        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            Stdout = Get-Content -LiteralPath $stdoutPath -Raw
            Stderr = Get-Content -LiteralPath $stderrPath -Raw
        }
    } finally {
        Remove-Item -LiteralPath $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
    }
}

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Expected,
        [string]$Context
    )

    if ($Text -notlike "*$Expected*") {
        throw "$Context missing expected text: $Expected`nActual text:`n$Text"
    }
}

$beforeSample = Join-Path $RepoRoot "examples\inventory_snapshot_before.txt"
$afterCompactSample = Join-Path $RepoRoot "examples\inventory_snapshot_after_compact.txt"

$compactRun = Invoke-ChangeCli -BeforePath $beforeSample -AfterPath $afterCompactSample
if ($compactRun.ExitCode -ne 0) {
    throw "Compact snapshot CLI run failed with exit code $($compactRun.ExitCode): $($compactRun.Stderr)"
}
Assert-Contains $compactRun.Stdout "kind=compacted_and_count_decremented" "compact snapshot"
Assert-Contains $compactRun.Stdout "recognized_removal_shape=1" "compact snapshot"
Assert-Contains $compactRun.Stdout "changed_index=1" "compact snapshot"
Assert-Contains $compactRun.Stdout "moved_slots=1" "compact snapshot"

$unknownAfter = New-TestFile "erdup_unknown_after_" @"
count=3
slot=index:0,handle:0x300,id:0x1000,category:armor,qty:2
slot=index:1,handle:0x301,id:0x2000,category:talisman,qty:1
slot=index:2,handle:0x302,id:0x3000,category:ash_of_war,qty:1
"@
try {
    $unknownRun = Invoke-ChangeCli -BeforePath $beforeSample -AfterPath $unknownAfter
    if ($unknownRun.ExitCode -ne 0) {
        throw "Unknown snapshot CLI run should parse successfully; got $($unknownRun.ExitCode)."
    }
    Assert-Contains $unknownRun.Stdout "kind=unknown" "unknown snapshot"
    Assert-Contains $unknownRun.Stdout "recognized_removal_shape=0" "unknown snapshot"
    Assert-Contains $unknownRun.Stderr "analysis warning:" "unknown snapshot"
} finally {
    Remove-Item -LiteralPath $unknownAfter -Force -ErrorAction SilentlyContinue
}

$badAfter = New-TestFile "erdup_bad_after_" @"
count=2
slot=index:0,handle:0x300,id:0x1000,category:armor,qty:1
"@
try {
    $badRun = Invoke-ChangeCli -BeforePath $beforeSample -AfterPath $badAfter
    if ($badRun.ExitCode -ne 1) {
        throw "Bad snapshot should exit 1; got $($badRun.ExitCode)."
    }
    Assert-Contains $badRun.Stderr "after active count exceeds provided slot count" "bad snapshot"
} finally {
    Remove-Item -LiteralPath $badAfter -Force -ErrorAction SilentlyContinue
}

Write-Host "InventoryChangeCli snapshot checks passed."
