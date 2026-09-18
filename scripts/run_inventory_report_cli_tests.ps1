param(
    [Parameter(Mandatory = $true)]
    [string]$CliPath,
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"

function New-TestConfig {
    param([string]$Contents)

    $path = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "erdup_cli_test_$([System.Guid]::NewGuid()).ini")
    Set-Content -LiteralPath $path -Value $Contents -Encoding ASCII
    return $path
}

function New-TestFile {
    param(
        [string]$Prefix,
        [string]$Contents
    )

    $path = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "$Prefix$([System.Guid]::NewGuid()).txt")
    Set-Content -LiteralPath $path -Value $Contents -Encoding ASCII
    return $path
}

function Invoke-ReportCli {
    param(
        [string]$ConfigPath,
        [string]$FixturePath
    )

    $stdoutPath = [System.IO.Path]::GetTempFileName()
    $stderrPath = [System.IO.Path]::GetTempFileName()
    $arguments = "`"$ConfigPath`" `"$FixturePath`""
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

$defaultConfig = Join-Path $RepoRoot "config\duplicate_seller.ini"
$defaultFixture = Join-Path $RepoRoot "examples\default_policy_fixture.txt"
$stockFixture = Join-Path $RepoRoot "examples\consumable_stock_fixture.txt"
$malformedFixture = Join-Path $RepoRoot "examples\malformed_fixture.txt"
$emptyFixture = Join-Path $RepoRoot "examples\empty_fixture.txt"

$defaultRun = Invoke-ReportCli -ConfigPath $defaultConfig -FixturePath $defaultFixture
if ($defaultRun.ExitCode -ne 0) {
    throw "Default fixture CLI run failed with exit code $($defaultRun.ExitCode): $($defaultRun.Stderr)"
}
Assert-Contains $defaultRun.Stdout "Inventory dry-run report: sell 5 item(s) for 275 rune(s)" "default fixture"
Assert-Contains $defaultRun.Stdout "SELL id=1000" "default fixture"
Assert-Contains $defaultRun.Stdout "SELL id=1100" "default fixture"
Assert-Contains $defaultRun.Stdout "SELL id=1200" "default fixture"
Assert-Contains $defaultRun.Stdout "SELL id=1300" "default fixture"
Assert-Contains $defaultRun.Stdout "SELL id=1400" "default fixture"
Assert-Contains $defaultRun.Stdout "SKIP id=1500 name=`"Upgraded Sword`" category=weapon qty=1 upgrade=3 modified=0 keep=2 sell=0 runes=0 sell_value=unknown reason=`"retained by keep count`"" "default fixture"
Assert-Contains $defaultRun.Stdout "SKIP id=1600 name=`"Modified Sword`" category=weapon qty=1 upgrade=0 modified=1 keep=2 sell=0 runes=0 sell_value=unknown reason=`"retained by keep count`"" "default fixture"
Assert-Contains $defaultRun.Stdout "reason=`"consumable selling disabled`"" "default fixture"
Assert-Contains $defaultRun.Stdout "reason=`"safe-excluded category skipped`"" "default fixture"
Assert-Contains $defaultRun.Stdout "reason=`"unknown category skipped`"" "default fixture"
Assert-Contains $defaultRun.Stdout "reason=`"category disabled`"" "default fixture"

$stockConfig = New-TestConfig @"
sell_consumables=true
item_stock_rules=9000:25,9001:5
"@
try {
    $stockRun = Invoke-ReportCli -ConfigPath $stockConfig -FixturePath $stockFixture
    if ($stockRun.ExitCode -ne 0) {
        throw "Stock fixture CLI run failed with exit code $($stockRun.ExitCode): $($stockRun.Stderr)"
    }
    Assert-Contains $stockRun.Stdout "Inventory dry-run report: sell 20 item(s) for 65 rune(s)" "stock fixture"
    Assert-Contains $stockRun.Stdout "SELL id=9000" "stock fixture"
    Assert-Contains $stockRun.Stdout "keep=25" "stock fixture"
    Assert-Contains $stockRun.Stdout "SELL id=9001" "stock fixture"
    Assert-Contains $stockRun.Stdout "keep=5" "stock fixture"
} finally {
    Remove-Item -LiteralPath $stockConfig -Force -ErrorAction SilentlyContinue
}

$valueTable = New-TestFile "erdup_sell_values_" @"
1000 11
2000000 22
"@
$valueFixture = New-TestFile "erdup_sell_value_fixture_" @"
item=instance:1,id:1000,category:weapon,qty:1,upgrade:0,modified:false,sell:999,name:Exact Value Sword
item=instance:2,id:1000,category:weapon,qty:1,upgrade:0,modified:false,sell:999,name:Exact Value Sword
item=instance:3,id:1000,category:weapon,qty:1,upgrade:0,modified:false,sell:999,name:Exact Value Sword
item=instance:4,id:2000000,category:weapon,qty:1,upgrade:0,modified:false,sell:999,name:Base Value Sword
item=instance:5,id:2000000,category:weapon,qty:1,upgrade:0,modified:false,sell:999,name:Base Value Sword
item=instance:6,id:2000100,category:weapon,qty:1,upgrade:0,modified:true,sell:999,name:Heavy Value Sword
item=instance:7,id:3000,category:armor,qty:1,upgrade:0,modified:false,sell:999,name:Unknown Value Armor
item=instance:8,id:3000,category:armor,qty:1,upgrade:0,modified:false,sell:999,name:Unknown Value Armor
"@
$valueConfig = New-TestConfig @"
sell_value_path=$valueTable
"@
try {
    $valueRun = Invoke-ReportCli -ConfigPath $valueConfig -FixturePath $valueFixture
    if ($valueRun.ExitCode -ne 0) {
        throw "Sell-value fixture CLI run failed with exit code $($valueRun.ExitCode): $($valueRun.Stderr)"
    }
    Assert-Contains $valueRun.Stdout "Inventory dry-run report: sell 3 item(s) for 33 rune(s)" "sell-value fixture"
    Assert-Contains $valueRun.Stdout "Sell value summary: resolved=5 approximate=1 unknown=2" "sell-value fixture"
    Assert-Contains $valueRun.Stdout "SELL id=1000" "sell-value fixture"
    Assert-Contains $valueRun.Stdout "runes=11 sell_value=resolved" "sell-value fixture"
    Assert-Contains $valueRun.Stdout "SELL id=2000100" "sell-value fixture"
    Assert-Contains $valueRun.Stdout "runes=22 sell_value=approximate" "sell-value fixture"
    Assert-Contains $valueRun.Stdout "SELL id=3000" "sell-value fixture"
    Assert-Contains $valueRun.Stdout "runes=0 sell_value=unknown" "sell-value fixture"
} finally {
    Remove-Item -LiteralPath $valueTable, $valueFixture, $valueConfig -Force -ErrorAction SilentlyContinue
}

$malformedRun = Invoke-ReportCli -ConfigPath $defaultConfig -FixturePath $malformedFixture
if ($malformedRun.ExitCode -ne 0) {
    throw "Malformed fixture CLI run should keep valid rows and exit 0; got $($malformedRun.ExitCode)."
}
Assert-Contains $malformedRun.Stderr "non-positive quantity 0" "malformed fixture"
Assert-Contains $malformedRun.Stderr "non-positive quantity -1" "malformed fixture"
Assert-Contains $malformedRun.Stderr "reuses instance id 2" "malformed fixture"
Assert-Contains $malformedRun.Stderr "missing or invalid category" "malformed fixture"
Assert-Contains $malformedRun.Stderr "missing or invalid instance" "malformed fixture"
Assert-Contains $malformedRun.Stderr "invalid qty" "malformed fixture"
Assert-Contains $malformedRun.Stderr "invalid upgrade" "malformed fixture"
Assert-Contains $malformedRun.Stderr "invalid modified" "malformed fixture"
Assert-Contains $malformedRun.Stderr "invalid sell" "malformed fixture"
Assert-Contains $malformedRun.Stderr "Skipped unrecognized fixture row" "malformed fixture"
Assert-Contains $malformedRun.Stdout "reason=`"non-positive quantity skipped`"" "malformed fixture"

$emptyRun = Invoke-ReportCli -ConfigPath $defaultConfig -FixturePath $emptyFixture
if ($emptyRun.ExitCode -ne 1) {
    throw "Empty fixture should exit 1; got $($emptyRun.ExitCode)."
}
Assert-Contains $emptyRun.Stderr "No fixture inventory items were loaded." "empty fixture"

Write-Host "InventoryReportCli fixture checks passed."
