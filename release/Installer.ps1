param(
    [ValidateSet('Install','Uninstall')][string]$Action = 'Install',
    [string]$TargetDirectory = '',
    [ValidateSet('Keep','Preview','Sell')][string]$SaleMode = 'Keep',
    [switch]$NonInteractive
)
$ErrorActionPreference = 'Stop'

# Parse only the string-array syntax used by Mod Engine 2. Refuse ambiguous
# input before changing anything; preserve every unrelated section and entry.
function Edit-DllList([string]$Text, [bool]$Remove) {
    if ($Text.Contains('"""') -or $Text.Contains("'''")) { throw 'Multiline TOML strings require manual installation.' }
    $headers = [regex]::Matches($Text, '(?m)^[ \t]*\[[^\r\n]+\][ \t]*(?:#[^\r\n]*)?\r?$')
    $sections = @($headers | Where-Object { $_.Value -match '^\s*\[modengine\]\s*(?:#.*)?$' })
    if ($sections.Count -ne 1) { throw 'Expected exactly one [modengine] section. No files changed.' }
    $section = $sections[0]
    $end = $Text.Length
    foreach ($h in $headers) { if ($h.Index -gt $section.Index) { $end = $h.Index; break } }
    $start = $section.Index + $section.Length
    $body = $Text.Substring($start, $end - $start)
    if ($body -match '(?m)^[ \t]*["'']external_dlls["''][ \t]*=') { throw 'Quoted external_dlls keys require manual installation.' }
    $keys = [regex]::Matches($body, '(?m)^[ \t]*external_dlls[ \t]*=')
    if ($keys.Count -eq 0) {
        if ($Remove) { return $Text }
        return $Text.Insert($start, "`r`nexternal_dlls = ['ERDuplicateSeller/ERDuplicateSeller.dll']")
    }
    if ($keys.Count -ne 1) { throw 'Duplicate external_dlls settings; fix the configuration first.' }
    $i = $start + $keys[0].Index + $keys[0].Length
    while ($i -lt $end -and [char]::IsWhiteSpace($Text[$i])) { $i++ }
    if ($i -ge $end -or $Text[$i] -ne '[') { throw 'external_dlls must be an array of quoted paths.' }
    $arrayStart = $i; $i++
    $entries = New-Object 'Collections.Generic.List[string]'
    $comments = New-Object 'Collections.Generic.List[string]'
    $needComma = $false
    $closed = $false
    while ($i -lt $end) {
        $c = $Text[$i]
        if ([char]::IsWhiteSpace($c)) { $i++; continue }
        if ($c -eq '#') {
            $commentStart = $i
            while ($i -lt $end -and $Text[$i] -ne "`n") { $i++ }
            $comments.Add($Text.Substring($commentStart, $i-$commentStart).TrimEnd())
            continue
        }
        if ($c -eq ']') { $i++; $closed=$true; break }
        if ($needComma) {
            if ($c -ne ',') { throw 'Missing comma in external_dlls.' }
            $needComma=$false; $i++; continue
        }
        if ($c -ne '"' -and $c -ne "'") { throw 'Unsupported external_dlls syntax; use manual installation.' }
        $quote = $c; $tokenStart = $i; $i++; $terminated=$false
        while ($i -lt $end) {
            if ($Text[$i] -eq "`n" -or $Text[$i] -eq "`r") { throw 'A DLL path contains a newline.' }
            if ($quote -eq '"' -and $Text[$i] -eq '\') { $i+=2; continue }
            if ($Text[$i] -eq $quote) { $i++; $terminated=$true; break }
            $i++
        }
        if (-not $terminated) { throw 'Unterminated DLL path.' }
        $raw = $Text.Substring($tokenStart, $i-$tokenStart)
        $value = if ($quote -eq "'") { $raw.Substring(1,$raw.Length-2) } else { ConvertFrom-Json -InputObject $raw }
        # Only entries for this mod are replaced; other DLL text stays intact.
        if (($value -replace '\\','/') -notmatch '(?i)(^|/)ERDuplicateSeller\.dll$') { $entries.Add($raw) }
        $needComma=$true
    }
    if (-not $closed) { throw 'Unclosed external_dlls array.' }
    $lineEnd = $Text.IndexOf("`n",$i)
    if ($lineEnd -lt 0) { $lineEnd=$Text.Length }
    if ($Text.Substring($i,$lineEnd-$i).Trim() -notmatch '^(#.*)?$') { throw 'Unexpected text after external_dlls.' }
    if (-not $Remove) { $entries.Add("'ERDuplicateSeller/ERDuplicateSeller.dll'") }
    $lines = @('[') + @($comments | ForEach-Object { '    '+$_ }) + @($entries | ForEach-Object { '    '+$_+',' }) + @(']')
    return $Text.Substring(0,$arrayStart) + ($lines -join "`r`n") + $Text.Substring($i)
}

function Invoke-Setup {
    if (Get-Process -Name eldenring -ErrorAction SilentlyContinue) { throw 'Close Elden Ring before installing or uninstalling.' }
    if (-not $TargetDirectory) {
        if ($NonInteractive) { throw 'TargetDirectory is required for non-interactive setup.' }
        Add-Type -AssemblyName System.Windows.Forms
        $picker = New-Object Windows.Forms.OpenFileDialog
        $picker.Title = 'Select your Mod Engine 2 config_eldenring.toml'
        $picker.Filter = 'Elden Ring Mod Engine configuration (config_eldenring.toml)|config_eldenring.toml'
        $parent = Split-Path -Parent $PSScriptRoot
        if (Test-Path -LiteralPath (Join-Path $parent 'modengine2_launcher.exe')) { $picker.InitialDirectory=$parent }
        if ($picker.ShowDialog() -ne 'OK') { Write-Output 'Cancelled. No files changed.'; return }
        $TargetDirectory = Split-Path -Parent $picker.FileName
        $picker.Dispose()
    }
    $target = (Resolve-Path -LiteralPath $TargetDirectory).Path.TrimEnd('\')
    $configPath = Join-Path $target 'config_eldenring.toml'
    if (-not (Test-Path -LiteralPath (Join-Path $target 'modengine2_launcher.exe') -PathType Leaf)) { throw 'Select the folder containing modengine2_launcher.exe.' }
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) { throw 'config_eldenring.toml is missing.' }
    $destination = Join-Path $target 'ERDuplicateSeller'
    if (Test-Path -LiteralPath $destination) {
        $nodes = @(Get-Item -LiteralPath $destination) + @(Get-ChildItem -LiteralPath $destination -Recurse -Force)
        if (@($nodes | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw 'The mod folder contains a link/junction. Use manual installation.' }
    }
    $originalBytes = [IO.File]::ReadAllBytes($configPath)
    $original = [IO.File]::ReadAllText($configPath)
    $updated = Edit-DllList $original ($Action -eq 'Uninstall')
    $newInstall = -not (Test-Path -LiteralPath (Join-Path $destination 'duplicate_seller.ini'))
    if (-not $NonInteractive) {
        Add-Type -AssemblyName System.Windows.Forms
        if ($Action -eq 'Uninstall') {
            if ([Windows.Forms.MessageBox]::Show('Remove the mod from this launcher? Its files will be moved into a backup folder.','Uninstall Duplicate Seller','OKCancel') -ne 'OK') { return }
        } elseif ($newInstall -and $SaleMode -eq 'Keep') {
            $answer = [Windows.Forms.MessageBox]::Show('Enable actual selling with F8? Yes = sell duplicates. No = preview only. Back up your save and play offline.','Duplicate Seller setup','YesNoCancel')
            if ($answer -eq 'Cancel') { return }
            $SaleMode = if ($answer -eq 'Yes') { 'Sell' } else { 'Preview' }
        }
    }
    $files = @()
    if ($Action -eq 'Install') {
        foreach ($line in Get-Content -LiteralPath (Join-Path $PSScriptRoot 'SHA256SUMS.txt')) {
            if ($line -notmatch '^([A-Fa-f0-9]{64})  (.+)$') { throw 'Invalid package checksum manifest.' }
            $digest=$Matches[1]; $relative=$Matches[2]
            if ([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[/\\])\.\.([/\\]|$)' -or $relative.Contains(':')) { throw 'Invalid path in package manifest.' }
            $source=Join-Path $PSScriptRoot $relative
            $hasher=[Security.Cryptography.SHA256]::Create()
            $stream=[IO.File]::OpenRead($source)
            try { $actual=[BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-','') }
            finally { $stream.Dispose(); $hasher.Dispose() }
            if ($actual -ne $digest) { throw ('Package checksum failed: '+$relative) }
            $files += $relative
        }
        foreach ($required in @('ERDuplicateSeller.dll','duplicate_seller.ini','sell_values.txt')) {
            if ($files -notcontains $required) { throw ('Package is missing '+$required) }
        }
    }
    $backupRoot=Join-Path $target '.ERDuplicateSeller-backups'
    if ((Test-Path -LiteralPath $backupRoot) -and ((Get-Item -LiteralPath $backupRoot).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'The backup folder must not be a link/junction.' }
    $backup = Join-Path $backupRoot ([guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $backup 'config_eldenring.toml'),$originalBytes)
    $changed = New-Object 'Collections.Generic.List[object]'
    $moved = $false
    $tempConfig = Join-Path $target ('.erdup-'+[guid]::NewGuid().ToString('N')+'.tmp')
    try {
        if ($Action -eq 'Install') {
            foreach ($relative in $files) {
                $source=Join-Path $PSScriptRoot $relative
                $dest=Join-Path $destination $relative
                $exists=Test-Path -LiteralPath $dest -PathType Leaf
                if ($exists -and $relative -in @('duplicate_seller.ini','sell_values.txt')) { continue }
                if ([IO.Path]::GetFullPath($source) -eq [IO.Path]::GetFullPath($dest)) { continue }
                $saved=Join-Path $backup ('files\'+$relative)
                if ($exists) { New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null; Copy-Item -LiteralPath $dest -Destination $saved }
                $changed.Add(@{Destination=$dest; Saved=$saved; Existed=$exists})
                New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
                Copy-Item -LiteralPath $source -Destination $dest -Force
            }
            if ($SaleMode -ne 'Keep') {
                $ini=Join-Path $destination 'duplicate_seller.ini'
                if (@($changed | Where-Object { $_.Destination -eq $ini }).Count -eq 0) {
                    $saved=Join-Path $backup 'mode-original.ini'; Copy-Item -LiteralPath $ini -Destination $saved
                    $changed.Add(@{Destination=$ini; Saved=$saved; Existed=$true})
                }
                $text=[IO.File]::ReadAllText($ini)
                $matches=[regex]::Matches($text,'(?m)^dry_run=.*$')
                if ($matches.Count -ne 1) { throw 'Expected one dry_run setting.' }
                $value=if ($SaleMode -eq 'Sell') { 'false' } else { 'true' }
                $text=[regex]::Replace($text,'(?m)^dry_run=[^\r\n]*',('dry_run='+$value))
                [IO.File]::WriteAllText($ini,$text,(New-Object Text.UTF8Encoding($false)))
            }
        } elseif (Test-Path -LiteralPath $destination) {
            $resolvedSource=[IO.Path]::GetFullPath($destination)
            $resolvedBackup=[IO.Path]::GetFullPath((Join-Path $backup 'ERDuplicateSeller'))
            if ((Split-Path -Parent $resolvedSource) -ne $target -or -not $resolvedBackup.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Backup path is outside the chosen launcher folder.' }
            Move-Item -LiteralPath $resolvedSource -Destination $resolvedBackup
            $moved=$true
        }
        [IO.File]::WriteAllText($tempConfig,$updated,(New-Object Text.UTF8Encoding($false)))
        [IO.File]::Replace($tempConfig,$configPath,(Join-Path $backup 'config-replaced.toml'))
    } catch {
        for ($n=$changed.Count-1; $n -ge 0; $n--) {
            $item=$changed[$n]
            if ($item.Existed) { Copy-Item -LiteralPath $item.Saved -Destination $item.Destination -Force }
            elseif (Test-Path -LiteralPath $item.Destination -PathType Leaf) { Remove-Item -LiteralPath $item.Destination }
        }
        if ($moved) { Move-Item -LiteralPath $resolvedBackup -Destination $resolvedSource }
        [IO.File]::WriteAllBytes($configPath,$originalBytes)
        throw
    } finally {
        if (Test-Path -LiteralPath $tempConfig -PathType Leaf) { Remove-Item -LiteralPath $tempConfig }
    }
    Write-Output ($Action+' complete. Backup: '+$backup)
    if ($Action -eq 'Install') { Write-Output 'Launch through your usual offline Mod Engine launcher. Load a character and press F8.' }
}

try { Invoke-Setup } catch { Write-Error $_; exit 1 }
