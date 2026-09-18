param(
    [string]$DllPath = '',
    [string]$OutputDirectory = '',
    [string]$Version = '0.1.0-beta.2'
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $DllPath) { $DllPath = Join-Path $repoRoot 'build\Release\ERDuplicateSeller.dll' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot 'dist\nexus' }
if ($Version -notmatch '^[0-9A-Za-z.-]+$') { throw 'Invalid version label.' }
$DllPath = (Resolve-Path -LiteralPath $DllPath).Path
$stage = Join-Path $OutputDirectory ('Elden-Ring-Duplicate-Seller-' + $Version)
$zipPath = $stage + '.zip'
if ((Test-Path -LiteralPath $stage) -or (Test-Path -LiteralPath $zipPath)) {
    throw 'Output already exists. Select a fresh output directory; existing packages are never overwritten.'
}
$payload = Join-Path $stage 'ERDuplicateSeller'
New-Item -ItemType Directory -Path (Join-Path $payload 'docs') -Force | Out-Null
Copy-Item -LiteralPath $DllPath -Destination (Join-Path $payload 'ERDuplicateSeller.dll')
Copy-Item -LiteralPath (Join-Path $repoRoot 'config\provisional_2710.ini') -Destination (Join-Path $payload 'duplicate_seller.ini')
foreach ($name in @('sell_values.txt','Configure.ps1','Settings.cmd','Install.cmd','Uninstall.cmd','Installer.ps1')) {
    Copy-Item -LiteralPath (Join-Path $repoRoot ('release\' + $name)) -Destination $payload
}
foreach ($name in @('README.md','INSTALL.md','CHANGELOG.md','CREDITS.md')) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $name) -Destination $payload
}
foreach ($name in @('NATIVE_2710.md','MENU_INDICATOR.md','CATALOG_STRESS_2026-09-18.md')) {
    Copy-Item -LiteralPath (Join-Path $repoRoot ('docs\' + $name)) -Destination (Join-Path $payload 'docs')
}
$payload = (Resolve-Path -LiteralPath $payload).Path
$hashes = Get-ChildItem -LiteralPath $payload -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($payload.Length + 1).Replace('\','/')
    '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash, $relative
}
Set-Content -LiteralPath (Join-Path $payload 'SHA256SUMS.txt') -Value $hashes -Encoding ASCII
Compress-Archive -LiteralPath $payload -DestinationPath $zipPath
$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
Set-Content -LiteralPath ($zipPath + '.sha256') -Value ($zipHash + '  ' + (Split-Path -Leaf $zipPath)) -Encoding ASCII
Write-Output $zipPath
Write-Output ('SHA256: ' + $zipHash)
