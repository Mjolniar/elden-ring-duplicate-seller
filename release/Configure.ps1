# Optional convenience helper. Changes only sell_value_path beside this script.
$ErrorActionPreference = 'Stop'
$configPath = Join-Path $PSScriptRoot 'duplicate_seller.ini'
$pricePath = Join-Path $PSScriptRoot 'sell_values.txt'
if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) { throw 'duplicate_seller.ini is missing.' }
if (-not (Test-Path -LiteralPath $pricePath -PathType Leaf)) { throw 'sell_values.txt is missing.' }
$pricePath = (Resolve-Path -LiteralPath $pricePath).Path
$lines = Get-Content -LiteralPath $configPath
if (@($lines | Where-Object { $_ -match '^sell_value_path=' }).Count -ne 1) {
    throw 'Expected exactly one sell_value_path setting; edit the configuration manually.'
}
$backupPath = $configPath + '.' + (Get-Date -Format 'yyyyMMdd-HHmmss-ffff') + '.bak'
Copy-Item -LiteralPath $configPath -Destination $backupPath
$lines = $lines | ForEach-Object {
    if ($_ -match '^sell_value_path=') { 'sell_value_path=' + $pricePath } else { $_ }
}
[IO.File]::WriteAllLines($configPath, [string[]]$lines, (New-Object Text.UTF8Encoding($false)))
Write-Output 'Price path configured. Other settings, including preview mode, were preserved.'
Write-Output 'Next: register ERDuplicateSeller.dll in your existing Mod Engine 2 external_dlls list.'
