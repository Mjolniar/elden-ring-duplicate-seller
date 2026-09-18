param([ValidateSet('Preview','Sell')][string]$Mode = '')
$ErrorActionPreference = 'Stop'
try {
    $configPath = Join-Path $PSScriptRoot 'duplicate_seller.ini'
    $text = [IO.File]::ReadAllText($configPath)
    if ([regex]::Matches($text,'(?m)^dry_run=[^\r\n]*').Count -ne 1) { throw 'Expected one dry_run setting.' }
    if (-not $Mode) {
        Add-Type -AssemblyName System.Windows.Forms
        $answer = [Windows.Forms.MessageBox]::Show('Enable actual selling with F8? Yes = sell duplicates. No = preview only.','Duplicate Seller settings','YesNoCancel')
        if ($answer -eq 'Cancel') { exit 0 }
        $Mode = if ($answer -eq 'Yes') { 'Sell' } else { 'Preview' }
    }
    $value = if ($Mode -eq 'Sell') { 'false' } else { 'true' }
    $text = [regex]::Replace($text,'(?m)^dry_run=[^\r\n]*',('dry_run='+$value))
    Copy-Item -LiteralPath $configPath -Destination ($configPath+'.'+[guid]::NewGuid().ToString('N')+'.bak')
    [IO.File]::WriteAllText($configPath,$text,(New-Object Text.UTF8Encoding($false)))
    Write-Output ('Mode: '+$Mode+'. The setting applies on the next F8 press.')
} catch { Write-Error $_; exit 1 }
