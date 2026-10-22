<# : cmd runs the batch commands below, which PowerShell reads as a comment
@echo off
rem Installs MilkDrop 2 Plus into foobar2000: double-click this file, drop the foobar2000 folder on it,
rem or run  install-milkdrop2-plus.cmd "C:\foobar2000" [-Layout or -NoLayout]  -  the PowerShell part
rem below tells the rest. No labels or blocks here, which cmd misreads in a file with LF line endings.
setlocal
rem cmd's command line names this file when Explorer or Windows+R started it (an interactive command
rem prompt's does not), and then the PowerShell part may keep the window open at the end: see
rem Wait-IfOwnWindow. An "if" ignores a substitution made in the dynamic variable CMDCMDLINE itself,
rem so it is copied first.
setlocal EnableDelayedExpansion
set "CMDLINE=!CMDCMDLINE!"
set "NAMED="
if not "!CMDLINE!"=="!CMDLINE:%~nx0=!" set "NAMED=1"
endlocal & set "MD2P_PAUSE_IF_ALONE=%NAMED%"
rem PowerShell runs only files named .ps1, so it runs a copy of this one. "type" makes it, as "copy"
rem would carry over the mark of the web, which a RemoteSigned execution policy refuses.
set "COPY=%TEMP%\install-milkdrop2-plus-%RANDOM%%RANDOM%.ps1"
set "COPIED="
type "%~f0" > "%COPY%" && set "COPIED=1"
if not defined COPIED echo Could not copy this script to "%COPY%".
if not defined COPIED if defined MD2P_PAUSE_IF_ALONE pause
if not defined COPIED exit /b 1
rem The 64-bit PowerShell even when a 32-bit program started this, so that it sees 64-bit foobar2000.
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "PS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%COPY%" %*
set "RESULT=%ERRORLEVEL%"
del "%COPY%" >nul 2>&1
exit /b %RESULT%
#>
<#
.SYNOPSIS
    Installs MilkDrop 2 Plus into a foobar2000 installation, with the original MilkDrop presets,
    the MilkDrop texture pack and a Default UI layout that has a MilkDrop view.

.DESCRIPTION
    This file is both a batch file and a PowerShell script, so that it is one file to pass around and
    runs with a double-click: cmd runs the batch commands at the top, which run the rest in Windows
    PowerShell, bypassing its execution policy (unless Group Policy enforces one).

    It downloads everything first, then installs it into the installation's profile folder
    (<foobar2000>\profile in a portable installation, %APPDATA%\foobar2000-v2 otherwise):

      user-components-x64\<component>\      the latest release of MilkDrop 2 Plus
                                            (github.com/inhahe/milkdrop2-plus)
      milkdrop2\presets\Milkdrop-Original\  the presets that shipped with MilkDrop 2
                                            (github.com/projectM-visualizer/presets-milkdrop-original)
      milkdrop2\textures\                   the MilkDrop texture pack, which those presets use
                                            (github.com/projectM-visualizer/presets-milkdrop-texture-pack)
      theme.fth                             the Default UI layout, with its colours and fonts: the one
                                            embedded at the end of this script, or -LayoutFrom's
      browser_config.ini                    the plugin's preset folder, PresetLibraryPath, set to
                                            milkdrop2\presets\: relative to the profile, so that the
                                            installation can be moved

    It asks whether to install the layout, which replaces the current one: by default yes, unless the
    current layout already has a MilkDrop view (a customised one, perhaps, or this one from an earlier
    run). -Layout and -NoLayout answer without asking.

    Run again, it updates the plugin and adds the presets and textures that are missing, keeping those
    there already, which may have been edited. It also keeps a preset folder that exists, so as not to
    undo a change made since; -Force replaces it.

    Each run keeps a backup in <profile>\milkdrop2-plus-install-backup\<date and time>\: what it
    replaced (the plugin's previous build, a build under one of the plugin's older names, which would
    claim the same UI element, and theme.fth) and a list of every change it made (run.txt), including
    the preset folder's old value and what it put where there was nothing.

    -Restore undoes the newest install from its backup: it puts back what that install replaced,
    removes what it put where there was nothing and sets the preset folder back (unless it has been
    changed since), leaving the presets and textures it added. Another -Restore undoes the install
    before that. What a restore displaces
    is kept in a backup of its own. Run without options on a profile with an install to undo, the
    script asks whether to install or to undo it.

    Given the folder and -Layout or -NoLayout, it asks nothing, for a run nobody watches.

    foobar2000 must not be running with the target profile: it rewrites theme.fth when it exits.

.PARAMETER FoobarDir
    The folder that holds foobar2000.exe (64-bit foobar2000 v2), or foobar2000.exe itself. When it is
    not given, the script looks for foobar2000 and asks which to use, or lets you pick foobar2000.exe.

.PARAMETER Layout
    Install the layout without asking, even over one that has a MilkDrop view.

.PARAMETER NoLayout
    Leave the layout alone, without asking.

.PARAMETER LayoutFrom
    Install the layout from here, without asking, instead of the one embedded in this script: a .fth
    file (such as one saved by Preferences > Display > Default User Interface > Export theme), a
    foobar2000 profile folder, or a foobar2000 folder, whose profile it finds as it does FoobarDir's.

.PARAMETER Force
    Point the plugin at the installed presets even when its preset folder is set to one that exists,
    and install the layout without asking.

.PARAMETER Restore
    Undo the newest install instead of installing, as described above.

.EXAMPLE
    Double-click install-milkdrop2-plus.cmd, or drop the foobar2000 folder on it.

.EXAMPLE
    install-milkdrop2-plus.cmd "C:\foobar2000" -NoLayout

.EXAMPLE
    install-milkdrop2-plus.cmd "C:\foobar2000" -LayoutFrom "D:\foobar2000"

.EXAMPLE
    install-milkdrop2-plus.cmd "C:\foobar2000" -Restore
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)] [string] $FoobarDir,
    [switch] $Layout,
    [switch] $NoLayout,
    [string] $LayoutFrom,
    [switch] $Force,
    [switch] $Restore
)

$ErrorActionPreference = 'Stop'
# Windows PowerShell redraws Invoke-WebRequest's progress bar so often that it slows downloads many times over.
$ProgressPreference = 'SilentlyContinue'
# Windows PowerShell on an older .NET does not offer TLS 1.2, which GitHub requires, unless asked.
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
# Unpacking and hashing go through .NET rather than Expand-Archive and Get-FileHash, which are defined in
# modules: Windows PowerShell started from PowerShell 7 inherits 7's module path and cannot load them.
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

$PluginRepo = 'inhahe/milkdrop2-plus'
# Archives of the default branches.
$PresetsUrl = 'https://github.com/projectM-visualizer/presets-milkdrop-original/archive/HEAD.zip'
$TexturesUrl = 'https://github.com/projectM-visualizer/presets-milkdrop-texture-pack/archive/HEAD.zip'
# The plugin's preset folder as browser_config.ini stores it: relative to the profile.
$PresetLibraryStored = 'milkdrop2\presets\'
# Every name the plugin's component has had: foo_vis_milk2 until v0.2.1 (the name of jecassis's
# original, too), and milkdrop2_plus in builds that an earlier install.bat installed. They all
# register the same Default UI element, so only one may be installed.
$ComponentNames = @('foo_vis_milk2', 'milkdrop2_plus', 'foo_milkdrop2_plus')
# The folder in the profile that holds a backup folder for each run that changed something, and the
# manifest in each of those (see Backups below).
$BackupFolderName = 'milkdrop2-plus-install-backup'
$ManifestName = 'run.txt'
# The first 16 bytes of every .fth file.
$ThemeMagic = [byte[]](0x98, 0xBA, 0xA0, 0xAD, 0xD4, 0x36, 0x16, 0x4A, 0x8E, 0x14, 0x41, 0x5F, 0x73, 0xB4, 0xCB, 0x21)
# The MilkDrop Default UI element, as a layout stores it.
$MilkDropElement = ([guid]'204B0345-4DF5-4B47-ADD3-989F811BD9A5').ToByteArray()

$script:ProfileDir = $null
$script:BackupDir = $null
# What this run does, for its manifest: install, or restore.
$script:RunKind = 'install'
# The backup folders already warned about, so that each warning is given once.
$script:Warned = New-Object 'Collections.Generic.HashSet[string]'

function Step([string] $Text) { Write-Host "`n== $Text" -ForegroundColor Cyan }

function Get-Download([string] $Url, [string] $OutFile) {
    Write-Host "  downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile $OutFile -UseBasicParsing
}

# The SHA-256 of what $Stream holds, in hexadecimal. Closes the stream.
function Get-Sha256([IO.Stream] $Stream) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($sha.ComputeHash($Stream)).Replace('-', '') }
    finally { $Stream.Dispose(); $sha.Dispose() }
}

# The path $Path, as typed or pasted, without surrounding blanks and quotes. $What names it in errors.
function Get-CleanPath([string] $Path, [string] $What) {
    $clean = $Path.Trim().Trim('"')
    # Windows reads \" as an escaped quote, so "C:\foobar2000\" -NoLayout arrives as one argument.
    if ($clean.Contains('"')) {
        throw "The $What ($Path) contains a quote. Leave out the backslash before a closing quote: write `"C:\foobar2000`", not `"C:\foobar2000\`"."
    }
    $clean
}

# The profile folder of the foobar2000 in $Dir: profile\ in a portable installation, %APPDATA%\foobar2000-v2 otherwise.
function Get-ProfileDir([string] $Dir) {
    if (Test-Path -LiteralPath (Join-Path $Dir 'portable_mode_enabled')) { Join-Path $Dir 'profile' }
    else { Join-Path $env:APPDATA 'foobar2000-v2' }
}

# Stops if foobar2000 is running with the profile $ProfileDir. A running foobar2000 keeps the
# profile's config.sqlite open, which shows whoever runs it and by whatever path: another user, an
# administrator, a subst drive or a short name. Its process is named too, when this can see it.
function Assert-NotRunning([string] $ProfileDir) {
    $db = Join-Path $ProfileDir 'config.sqlite'
    if (-not (Test-Path -LiteralPath $db -PathType Leaf)) { return }
    try { [IO.File]::Open($db, 'Open', 'Read', 'None').Dispose(); return }
    catch [IO.IOException] { }
    $process = Get-Process foobar2000 -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and (Get-ProfileDir (Split-Path -Parent $_.Path)) -eq $ProfileDir } | Select-Object -First 1
    $which = if ($process) { " ($($process.Path), process $($process.Id))" } else { '' }
    throw "foobar2000 is running with this profile$which. Close it first: it rewrites the layout when it exits."
}

# The machine type in the PE header of $Path: 0x8664 for x64, 0x14C for x86, 0xAA64 for ARM64.
function Get-PeMachine([string] $Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = New-Object IO.BinaryReader($stream)
        $stream.Position = 0x3C
        $stream.Position = $reader.ReadInt32() + 4
        $reader.ReadUInt16()
    }
    finally { $stream.Dispose() }
}

# Whether the byte array $Haystack contains $Needle at $Offset, or anywhere when $Offset is negative.
function Test-Bytes([byte[]] $Haystack, [byte[]] $Needle, [int] $Offset = -1) {
    $hex = [BitConverter]::ToString($Haystack)
    $find = [BitConverter]::ToString($Needle)
    # Each byte is 3 characters ("XX-"), and a match can start only at a byte, as $find starts with two digits.
    if ($Offset -ge 0) { return $hex.Length -ge $Offset * 3 + $find.Length -and $hex.Substring($Offset * 3, $find.Length) -eq $find }
    $hex.Contains($find)
}

# --- Backups ----------------------------------------------------------------------------------------
# Each run that changes the profile keeps a backup folder, <profile>\milkdrop2-plus-install-backup\
# <date and time>\, holding what it moved away at the same place relative to the profile, and a
# manifest, run.txt: tab-separated lines, "sequence <n>" and "kind install" (or "kind restore", or
# "kind undone" for a backup kept after its restore because it holds files this script did not put
# there) first, then one line for each change, written before the change is made, so that a run which
# stops part-way can still be undone:
#   saved <path>                      the item at <path>, relative to the profile, is moved here
#   absent <path>                     there was nothing at <path>; the run puts something there
#   setting <file> <key> <new> <old>  the run sets <key> in the ini file <file> to <new>; it was <old>
#   nosetting <file> <key> <new>      the same, where <key> was not set
# Values are kept as Latin-1 (one character per byte), so that they come back byte for byte. Runs are
# ordered by their sequence numbers, not by the dates, which a change of clock can reorder.

# A change a backup records (see above): Kind saved, absent or setting; Path, relative to the profile;
# Source, where the backup holds what goes back; Key; New, the value the run set; Old, the one before.
function New-Entry([string] $Kind, [string] $Path, [string] $Source, [string] $Key, $New, $Old) {
    [pscustomobject]@{ Kind = $Kind; Path = $Path; Source = $Source; Key = $Key; New = $New; Old = $Old }
}

# The backups in the profile, as objects: Path, Name, Sequence, Kind, Entries and Legacy. A backup that an
# earlier version of this script made has no manifest, and counts as older than any other (see
# Get-LegacyEntries). One whose manifest this script cannot read is passed over, with a warning.
function Get-Backups {
    $fields = @{ sequence = 2; kind = 2; saved = 2; absent = 2; setting = 5; nosetting = 4 }
    $root = Join-Path $script:ProfileDir $BackupFolderName
    foreach ($dir in @(Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue)) {
        $backup = [pscustomobject]@{ Path = $dir.FullName; Name = $dir.Name; Sequence = 0; Kind = 'install'; Entries = @(); Legacy = $false }
        $manifest = Join-Path $dir.FullName $ManifestName
        if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
            $backup.Legacy = $true
            $backup.Entries = @(Get-LegacyEntries $dir.FullName)
            $backup
            continue
        }
        try {
            $entries = @()
            foreach ($line in [IO.File]::ReadAllLines($manifest, [Text.Encoding]::UTF8)) {
                if (-not $line) { continue }
                $f = $line.Split([char[]] @([char] 9), 5)
                if ($fields[$f[0]] -ne $f.Count) { throw "a line this script does not know: $line" }
                switch ($f[0]) {
                    'sequence' { $backup.Sequence = [int] $f[1] }
                    'kind' { $backup.Kind = $f[1] }
                    'saved' { $entries += New-Entry 'saved' $f[1] -Source $f[1] }
                    'absent' { $entries += New-Entry 'absent' $f[1] }
                    'setting' { $entries += New-Entry 'setting' $f[1] -Key $f[2] -New $f[3] -Old $f[4] }
                    'nosetting' { $entries += New-Entry 'setting' $f[1] -Key $f[2] -New $f[3] -Old $null }
                }
            }
            $backup.Entries = $entries
        }
        catch {
            $backup.Kind = 'unreadable'
            if ($script:Warned.Add($dir.FullName)) { Write-Warning "Passing over $($dir.FullName): its $ManifestName has $($_.Exception.Message)" }
        }
        $backup
    }
}

# The changes that a backup made by an earlier version of this script, in the folder $Dir, stands for.
# Those versions moved components and theme.fth there under their own names, and copied
# browser_config.ini there before changing its PresetLibraryPath to $PresetLibraryStored; they did not
# note what they created. A run that moved a component away installed another, so the other
# components are taken out again: only one may be installed.
function Get-LegacyEntries([string] $Dir) {
    $components = @()
    foreach ($item in @(Get-ChildItem -LiteralPath $Dir -Force)) {
        if ($ComponentNames -contains $item.Name) {
            $components += $item.Name
            New-Entry 'saved' "user-components-x64\$($item.Name)" -Source $item.Name
        }
        elseif ($item.Name -eq 'theme.fth') { New-Entry 'saved' 'theme.fth' -Source 'theme.fth' }
        elseif ($item.Name -eq 'browser_config.ini') {
            $old = @(Get-IniValues $item.FullName 'PresetLibraryPath')
            New-Entry 'setting' 'browser_config.ini' -Source 'browser_config.ini' -Key 'PresetLibraryPath' -New $PresetLibraryStored -Old $(if ($old.Count -gt 0) { $old[-1] } else { $null })
        }
    }
    if ($components.Count -gt 0) {
        foreach ($name in $ComponentNames | Where-Object { $components -notcontains $_ }) { New-Entry 'absent' "user-components-x64\$name" }
    }
}

# The backup of the newest install, which -Restore undoes, or $null.
function Get-NewestInstallBackup {
    Get-Backups | Where-Object { $_.Kind -eq 'install' } | Sort-Object Sequence, Name | Select-Object -Last 1
}

# When the run that made $Backup happened, for people: from its folder's name.
function Get-BackupDate($Backup) {
    try { [datetime]::ParseExact($Backup.Name.Substring(0, 19), 'yyyy-MM-dd_HH-mm-ss', [Globalization.CultureInfo]::InvariantCulture).ToString('f') }
    catch { $Backup.Name }
}

# This run's backup folder, made with its manifest the first time it is needed.
function Get-BackupDir {
    if (-not $script:BackupDir) {
        $sequence = 1 + (@(Get-Backups | ForEach-Object { $_.Sequence }) + 0 | Measure-Object -Maximum).Maximum
        $stamp = (Get-Date).ToString('yyyy-MM-dd_HH-mm-ss', [Globalization.CultureInfo]::InvariantCulture)
        $base = Join-Path (Join-Path $script:ProfileDir $BackupFolderName) $stamp
        $dir = $base
        for ($n = 2; Test-Path -LiteralPath $dir; $n++) { $dir = "$base-$n" }
        New-Item -ItemType Directory -Path $dir | Out-Null
        $script:BackupDir = $dir
        Add-ManifestLine 'sequence' $sequence
        Add-ManifestLine 'kind' $script:RunKind
    }
    $script:BackupDir
}

# Adds a line of its arguments, separated by tabs, to this run's manifest.
function Add-ManifestLine {
    $line = (@($args) | ForEach-Object { "$_" }) -join "`t"
    [IO.File]::AppendAllText((Join-Path (Get-BackupDir) $ManifestName), "$line`r`n", (New-Object Text.UTF8Encoding($false)))
}

# $Path, which must be in the profile, relative to it.
function Get-ProfileRelative([string] $Path) {
    $root = $script:ProfileDir.TrimEnd('\') + '\'
    if (-not $Path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw "Internal error: $Path is not in the profile $root." }
    $Path.Substring($root.Length)
}

# Moves $From to $To, which must not exist yet (Move-Item would move into a folder there), and, with a
# list $Log, records the move there, so that it can be reversed.
function Move-Logged([string] $From, [string] $To, $Log) {
    if (Test-Path -LiteralPath $To) { throw "$To is in the way." }
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $To))
    Move-Item -LiteralPath $From -Destination $To
    if ($null -ne $Log) { $Log.Add([pscustomobject]@{ From = $From; To = $To }) }
}

# Moves the item at $Path (a file or folder in the profile) into this run's backup folder, at the same
# place relative to the profile; or, when there is nothing there, notes that, so that -Restore takes
# out what this run puts there. With a list $Log, the move is recorded there too (see Move-Logged).
function Save-Backup([string] $Path, $Log) {
    $relative = Get-ProfileRelative $Path
    if (-not (Test-Path -LiteralPath $Path)) {
        Add-ManifestLine 'absent' $relative
        return
    }
    Add-ManifestLine 'saved' $relative
    Move-Logged $Path (Join-Path (Get-BackupDir) $relative) $Log
    Write-Host "  saved the old $relative in $(Get-BackupDir)"
}

# Sets $Key in the ini file at $Path (in the profile) to $Value (Latin-1, or $null to remove it), noting
# the new and the old value in the manifest first, and returns the old value, or $null if it was not set.
function Save-Setting([string] $Path, [string] $Key, $Value) {
    $current = @(Get-IniValues $Path $Key)
    $old = if ($current.Count -gt 0) { $current[-1] } else { $null }
    $relative = Get-ProfileRelative $Path
    if ($null -ne $old) { Add-ManifestLine 'setting' $relative $Key $Value $old } else { Add-ManifestLine 'nosetting' $relative $Key $Value }
    Set-IniValue $Path $Key $Value
    $old
}

# Undoes the install that $Backup (from Get-Backups) records, newest change first: puts back what it
# moved away, takes out what it put where there was nothing, and sets back the settings it changed,
# unless they have been changed since. What this displaces goes into this run's own backup folder, of
# kind restore, which -Restore passes over, so the next -Restore undoes the install before. If a step
# fails, the steps made so far are reversed, so that the backup stays whole for another try; once all
# succeed, the backup is gone, as it is undone.
function Restore-Backup($Backup) {
    if ($Backup.Legacy) {
        Write-Host '  (An earlier version of this script made this backup, without noting what it created, which stays, but for another build of the plugin.)'
    }
    $entries = @($Backup.Entries)
    [array]::Reverse($entries)
    # The moves (From, To) and setting changes (Setting, Key, and Value before) made so far, to reverse.
    $done = New-Object Collections.Generic.List[object]
    try {
        foreach ($entry in $entries) {
            $target = Join-Path $script:ProfileDir $entry.Path
            switch ($entry.Kind) {
                'setting' {
                    $current = @(Get-IniValues $target $entry.Key)
                    $now = if ($current.Count -gt 0) { $current[-1] } else { $null }
                    if ($now -cne $entry.New) {
                        Write-Host "  left $($entry.Key) in $($entry.Path), which has been changed since"
                        break
                    }
                    $before = Save-Setting $target $entry.Key $entry.Old
                    $done.Add([pscustomobject]@{ Setting = $target; Key = $entry.Key; Value = $before })
                    Write-Host "  set $($entry.Key) in $($entry.Path) back"
                }
                'saved' {
                    $source = Join-Path $Backup.Path $entry.Source
                    if (-not (Test-Path -LiteralPath $source)) {
                        Write-Warning "Cannot put back $($entry.Path): it is not in $($Backup.Path)."
                        break
                    }
                    Save-Backup $target $done
                    Move-Logged $source $target $done
                    Write-Host "  put back $($entry.Path)"
                }
                'absent' {
                    if (Test-Path -LiteralPath $target) {
                        Save-Backup $target $done
                        Write-Host "  took out $($entry.Path), which that run put there"
                    }
                }
            }
        }
    }
    catch {
        $problem = $_.Exception.Message
        $stuck = @()
        for ($i = $done.Count - 1; $i -ge 0; $i--) {
            $step = $done[$i]
            try {
                if ($step.Setting) { Set-IniValue $step.Setting $step.Key $step.Value }
                else { Move-Item -LiteralPath $step.To -Destination $step.From }
            }
            catch { $stuck += $(if ($step.Setting) { "$($step.Key) in $($step.Setting)" } else { "$($step.To) to $($step.From)" }) }
        }
        if ($stuck.Count -gt 0) {
            throw "Could not restore ($problem), nor undo all of what it restored so far. Still to move back by hand: $($stuck -join '; '). The backup is in $($Backup.Path), and what this displaced in $script:BackupDir."
        }
        if ($script:BackupDir) {
            Remove-Item -LiteralPath $script:BackupDir -Recurse -Force
            $script:BackupDir = $null
        }
        throw "Could not restore ($problem). What it restored so far is undone again, and the backup in $($Backup.Path) is whole for another try."
    }
    # Left in the backup should be its manifest, an earlier version's copy of browser_config.ini, and
    # empty folders; anything else, this script did not put there.
    $known = @($ManifestName) + @($entries | Where-Object { $_.Kind -eq 'setting' -and $_.Source } | ForEach-Object { $_.Source })
    $others = @(Get-ChildItem -LiteralPath $Backup.Path -Recurse -File -Force | Where-Object { $known -notcontains $_.FullName.Substring($Backup.Path.Length + 1) })
    if ($others.Count -eq 0) { Remove-Item -LiteralPath $Backup.Path -Recurse -Force }
    else {
        [IO.File]::WriteAllText((Join-Path $Backup.Path $ManifestName), "sequence`t$($Backup.Sequence)`r`nkind`tundone`r`n")
        Write-Host "  left $($Backup.Path), which holds files this script did not put there"
    }
}

# The files below the folder $Folder of the open zip archive $Archive ('' for all of it), as a
# hashtable from each one's path relative to that folder to its entry. With -SkipRoot, $Folder is
# below the archive's one top folder, which GitHub names after the repository and commit.
function Get-ZipFiles($Archive, [string] $Folder, [switch] $SkipRoot) {
    $prefix = ''
    if ($SkipRoot) {
        $roots = @($Archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/').Split('/')[0] } | Select-Object -Unique)
        if ($roots.Count -ne 1) { throw "Expected one top folder in the archive, found $($roots.Count)." }
        $prefix = $roots[0] + '/'
    }
    if ($Folder) { $prefix += $Folder + '/' }
    $files = @{}
    foreach ($entry in $Archive.Entries) {
        $name = $entry.FullName.Replace('\', '/')
        if ($name.EndsWith('/') -or -not $name.StartsWith($prefix, [StringComparison]::Ordinal)) { continue }
        $relative = $name.Substring($prefix.Length)
        if ($relative -match '(^|/)\.\.?(/|$)' -or $relative.StartsWith('/') -or $relative.Contains(':')) {
            throw "The archive holds a file whose name would put it elsewhere: $name"
        }
        $files[$relative.Replace('/', '\')] = $entry
    }
    $files
}

# Writes the files $Files (from Get-ZipFiles) below the folder $Dest and returns how many it wrote.
# It replaces files already there, or with -KeepExisting leaves them alone, as they may have been edited.
function Expand-ZipFiles([hashtable] $Files, [string] $Dest, [switch] $KeepExisting) {
    $written = 0
    foreach ($relative in $Files.Keys) {
        $target = Join-Path $Dest $relative
        if ($KeepExisting -and [IO.File]::Exists($target)) { continue }
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
        [IO.Compression.ZipFileExtensions]::ExtractToFile($Files[$relative], $target, $true)
        $written++
    }
    $written
}

# The layout to install, as the bytes of a .fth file: from $From (see -LayoutFrom), or the embedded one.
# (The leading commas keep PowerShell from unrolling the byte array into a stream of single bytes.)
function Get-Layout([string] $From) {
    if (-not $From) {
        Write-Host '  using the layout embedded in this script'
        return , [Convert]::FromBase64String($EmbeddedLayout)
    }
    $candidates = @($From, (Join-Path $From 'theme.fth'))
    if (Test-Path -LiteralPath (Join-Path $From 'foobar2000.exe') -PathType Leaf) {
        $candidates += Join-Path (Get-ProfileDir $From) 'theme.fth'
    }
    $file = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (-not $file) { throw "No layout found at $From : expected a .fth file, a foobar2000 profile folder or a foobar2000 folder." }
    # A full path, as .NET resolves a relative one against its own current folder, not PowerShell's.
    $file = (Get-Item -LiteralPath $file).FullName
    Write-Host "  using the layout in $file"
    , [IO.File]::ReadAllBytes($file)
}

# The ini files here are read and written as Latin-1, which maps every byte to one character and back,
# so that lines this does not touch keep their bytes, whatever encoding they are in (UTF-8 now, Latin-1
# before v0.3.0, a byte order mark), and a value read and written back comes back byte for byte.
$Latin1 = [Text.Encoding]::GetEncoding(28591)

# The values of the "<Key>=" lines in the ini file $Path, as Latin-1, in order (the plugin uses the last).
function Get-IniValues([string] $Path, [string] $Key) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return @() }
    $text = $Latin1.GetString([IO.File]::ReadAllBytes($Path))
    @([regex]::Matches($text, "(?m)^$([regex]::Escape($Key))=([^\r\n]*)") | ForEach-Object { $_.Groups[1].Value })
}

# Sets every "<Key>=" line of the ini file $Path to $Value (Latin-1, as Get-IniValues returns it),
# adding one, and the file with the [BrowserConfig] header the plugin writes, when there is none; or,
# with $Value $null, removes those lines. Keeps the file's other lines, encoding and line endings.
function Set-IniValue([string] $Path, [string] $Key, $Value) {
    $exists = Test-Path -LiteralPath $Path -PathType Leaf
    if (-not $exists -and $null -eq $Value) { return }
    $text = if ($exists) { $Latin1.GetString([IO.File]::ReadAllBytes($Path)) } else { "[BrowserConfig]`r`n" }
    $line = "(?m)^$([regex]::Escape($Key))=[^\r\n]*"
    if ($null -eq $Value) { $text = [regex]::Replace($text, "$line(\r?\n)?", '') }
    elseif ([regex]::IsMatch($text, $line)) { $text = [regex]::Replace($text, $line, "$Key=$Value".Replace('$', '$$')) }
    else {
        $newline = if ($text.Contains("`r`n") -or -not $text.Contains("`n")) { "`r`n" } else { "`n" }
        if ($text.Length -gt 0 -and -not $text.EndsWith("`n")) { $text += $newline }
        $text += "$Key=$Value$newline"
    }
    [IO.File]::WriteAllBytes($Path, $Latin1.GetBytes($text))
}

# Points the plugin at the preset folder $Value (as browser_config.ini stores it) in the
# browser_config.ini at $Path, unless that already names a folder that exists and -Force was not
# given, noting the old value for -Restore.
function Set-PresetFolder([string] $Path, [string] $Value) {
    $key = 'PresetLibraryPath'
    $values = @(Get-IniValues $Path $key)
    if ($values.Count -gt 0 -and @($values | Where-Object { $_ -ne $Value }).Count -eq 0) {
        Write-Host "  already $Value"
        return
    }
    # The plugin reads a line as UTF-8, or as Latin-1 if it is not valid UTF-8.
    $raw = if ($values.Count -gt 0) { $values[-1] } else { $null }
    $old = "$raw".TrimEnd(' ', "`t")
    try { $old = (New-Object Text.UTF8Encoding($false, $true)).GetString($Latin1.GetBytes($old)) } catch { }
    if ($old -and -not $Force) {
        $oldDir = if ([IO.Path]::IsPathRooted($old)) { $old } else { Join-Path $script:ProfileDir $old }
        if (Test-Path -LiteralPath $oldDir -PathType Container) {
            Write-Host "  kept $old, which exists (-Force replaces it); the presets installed here are in milkdrop2\presets\Milkdrop-Original\"
            return
        }
    }
    $null = Save-Setting $Path $key $Value
    if ($old) { Write-Host "  changed from $old, which does not exist, to $Value" } else { Write-Host "  set to $Value" }
}

# The folders of the foobar2000 installations this can find: running ones, ones in Windows' list of
# installed programs, and ones in the usual places.
function Find-Foobar {
    $dirs = @(Get-Process foobar2000 -ErrorAction SilentlyContinue | Where-Object { $_.Path } | ForEach-Object { Split-Path -Parent $_.Path })
    $uninstall = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall',
                 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall',
                 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall'
    $apps = Get-ChildItem $uninstall -ErrorAction SilentlyContinue | Get-ItemProperty -ErrorAction SilentlyContinue |
        Where-Object { "$($_.DisplayName)" -like 'foobar2000*' }
    foreach ($app in $apps) {
        # A folder, or a program in it, perhaps quoted and followed by arguments or an icon index.
        foreach ($value in @($app.InstallLocation, $app.DisplayIcon, $app.UninstallString) | Where-Object { $_ }) {
            if ($value -match '^\s*"?([^",]+?\.exe)') { $dirs += $Matches[1] -replace '\\[^\\]*$', '' }
            else { $dirs += $value.Trim().Trim('"') }
        }
    }
    $dirs += [IO.Path]::Combine($env:ProgramFiles, 'foobar2000'), [IO.Path]::Combine($env:SystemDrive + '\', 'foobar2000')
    $found = @()
    foreach ($dir in $dirs) {
        # .NET rather than Test-Path and Join-Path, which fail on a drive that does not exist, as a
        # stale entry in the list of installed programs may name; and the search is only a guess.
        try {
            if (-not [IO.Path]::IsPathRooted($dir) -or -not [IO.File]::Exists([IO.Path]::Combine($dir, 'foobar2000.exe'))) { continue }
            $full = [IO.Path]::GetFullPath($dir).TrimEnd('\')
            if ($found -notcontains $full) { $found += $full }
        }
        catch { }
    }
    $found
}

# The foobar2000 folder to install into when none was given: one found and confirmed, or one chosen
# by picking its foobar2000.exe. Empty when the choice is cancelled.
function Select-FoobarDir {
    $found = @(Find-Foobar)
    if ($found.Count -eq 1) {
        $answer = Read-Host "Use the foobar2000 in $($found[0])? Press Enter for yes, or type N and Enter to choose another"
        if ("$answer".Trim() -notmatch '^n') { return $found[0] }
    }
    elseif ($found.Count -gt 1) {
        Write-Host 'foobar2000 is in:'
        for ($i = 0; $i -lt $found.Count; $i++) { Write-Host "  $($i + 1)  $($found[$i])" }
        $answer = Read-Host 'Type the number of the one to install into and press Enter, or press Enter alone to choose another'
        $n = 0
        if ([int]::TryParse("$answer".Trim(), [ref] $n) -and $n -ge 1 -and $n -le $found.Count) { return $found[$n - 1] }
    }
    try {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog = New-Object Windows.Forms.OpenFileDialog
        $dialog.Title = 'Choose foobar2000.exe in the foobar2000 to use'
        $dialog.Filter = 'foobar2000|foobar2000.exe'
        $dialog.InitialDirectory = $env:SystemDrive + '\'
        # An invisible topmost owner keeps the dialog from opening behind the console window.
        $owner = New-Object Windows.Forms.Form -Property @{ TopMost = $true }
        try { if ($dialog.ShowDialog($owner) -eq 'OK') { Split-Path -Parent $dialog.FileName } else { '' } }
        finally { $owner.Dispose(); $dialog.Dispose() }
    }
    catch {
        # No dialog where there is no desktop to show it on.
        Read-Host 'Folder that holds foobar2000.exe'
    }
}

function Install-MilkDrop {
    $installOptions = $Layout -or $NoLayout -or $LayoutFrom -or $Force
    if ($Restore -and $installOptions) { throw '-Restore takes no option but the foobar2000 folder.' }

    # --- Check the target -------------------------------------------------------------------------
    $dir = if ($FoobarDir) { $FoobarDir } else { Select-FoobarDir }
    $dir = Get-CleanPath $dir 'foobar2000 folder'
    if (-not $dir) { throw 'No foobar2000 folder given.' }
    # foobar2000.exe itself, dropped on the script, say, stands for its folder.
    if ((Split-Path -Leaf $dir) -eq 'foobar2000.exe' -and (Test-Path -LiteralPath $dir -PathType Leaf)) { $dir = Split-Path -Parent $dir }
    if (-not (Test-Path -LiteralPath $dir -PathType Container)) { throw "There is no folder $dir." }
    $dir = (Get-Item -LiteralPath $dir).FullName.TrimEnd('\')
    $exe = Join-Path $dir 'foobar2000.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "There is no foobar2000.exe in $dir." }
    $machine = Get-PeMachine $exe
    if ($machine -ne 0x8664) {
        throw ('{0} is not the 64-bit (x64) foobar2000: its PE machine type is 0x{1:X}. MilkDrop 2 Plus needs the x64 build of foobar2000 v2, from https://www.foobar2000.org/download.' -f $exe, $machine)
    }
    $portable = Test-Path -LiteralPath (Join-Path $dir 'portable_mode_enabled')
    $script:ProfileDir = Get-ProfileDir $dir
    $profileDir = $script:ProfileDir
    Assert-NotRunning $profileDir

    Write-Host "foobar2000:  $dir ($(if ($portable) { 'portable' } else { 'standard installation' }))"
    Write-Host "Profile:     $profileDir"
    if (-not $portable) {
        Write-Host '  A standard installation keeps components, presets and settings in the profile above, not beside foobar2000.exe.'
    }

    # Install, or undo the newest install: as -Restore says, or as asked when there is one to undo and
    # no option says to install.
    $newest = Get-NewestInstallBackup
    $restoring = [bool] $Restore
    if (-not $Restore -and -not $installOptions -and $newest) {
        $answer = Read-Host "This script installed into this foobar2000 on $(Get-BackupDate $newest). Press Enter to install or update, or type R and press Enter to undo that install"
        $restoring = "$answer".Trim() -eq 'r'
    }
    if ($restoring) {
        if (-not $newest) { throw "There is nothing to restore: no backup of an install in $(Join-Path $profileDir $BackupFolderName)." }
        # foobar2000 may have been started while the question waited.
        Assert-NotRunning $profileDir
        Step "Undoing the install of $(Get-BackupDate $newest)"
        $script:RunKind = 'restore'
        Restore-Backup $newest
        Write-Host "`nDone. Start foobar2000." -ForegroundColor Green
        if ($script:BackupDir) { Write-Host "What this displaced is in $script:BackupDir." }
        $older = Get-NewestInstallBackup
        if ($older) { Write-Host "To undo the install of $(Get-BackupDate $older) too, run this script again without options and type R when it asks, or run it with -Restore." }
        return
    }

    # Settle the layout before downloading anything, so that a bad -LayoutFrom costs nothing and the
    # question comes before the wait. ($newLayout, as PowerShell's $layout would be the -Layout switch.)
    if ($NoLayout -and ($Layout -or $LayoutFrom)) { throw '-NoLayout contradicts -Layout and -LayoutFrom.' }
    $installLayout = $false
    if (-not $NoLayout) {
        Step 'Layout'
        $from = if ($LayoutFrom) { Get-CleanPath $LayoutFrom 'layout' } else { '' }
        $newLayout = Get-Layout $from
        if (-not (Test-Bytes $newLayout $ThemeMagic 0)) { throw 'That layout is not a foobar2000 theme (.fth) file.' }
        if (-not (Test-Bytes $newLayout $MilkDropElement)) { Write-Warning 'That layout has no MilkDrop view.' }
        $themeFile = Join-Path $profileDir 'theme.fth'
        $current = if (Test-Path -LiteralPath $themeFile) { , [IO.File]::ReadAllBytes($themeFile) } else { $null }
        if ($null -ne $current -and [BitConverter]::ToString($current) -eq [BitConverter]::ToString($newLayout)) {
            Write-Host '  already installed'
        }
        elseif ($Layout -or $LayoutFrom -or $Force) { $installLayout = $true }
        else {
            # An empty answer, as when input is not a console, takes the default.
            Write-Host "  It has $EmbeddedLayoutDescription. It replaces your current layout, which is saved first."
            if ($null -ne $current -and (Test-Bytes $current $MilkDropElement)) {
                $answer = Read-Host '  Your layout already has a MilkDrop view. Replace it anyway? Type Y and press Enter for yes, or press Enter alone for no'
                $installLayout = "$answer".Trim() -match '^y'
            }
            else {
                $answer = Read-Host '  Install it? Press Enter for yes, or type N and press Enter for no'
                $installLayout = "$answer".Trim() -notmatch '^n'
            }
            if (-not $installLayout) { Write-Host '  keeping your layout' }
        }
    }

    $work = Join-Path ([IO.Path]::GetTempPath()) ('milkdrop2-plus-install-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $work | Out-Null
    $archives = New-Object Collections.Generic.List[object]
    try {
        # --- Download everything, so that a failed download leaves the installation untouched -----
        Step 'Downloading'
        $api = "https://api.github.com/repos/$PluginRepo/releases/latest"
        try { $release = Invoke-RestMethod -Uri $api -Headers @{ Accept = 'application/vnd.github+json' } -UseBasicParsing }
        catch { throw "Could not get the latest release of $PluginRepo from GitHub ($api): $($_.Exception.Message)" }
        $assets = @($release.assets | Where-Object { $_.name -like '*.fb2k-component' })
        if ($assets.Count -ne 1) {
            throw "Release $($release.tag_name) of $PluginRepo has $($assets.Count) .fb2k-component files; expected one."
        }
        $asset = $assets[0]
        $componentZip = Join-Path $work 'component.zip'
        Get-Download $asset.browser_download_url $componentZip
        if ("$($asset.digest)" -match '^sha256:([0-9a-fA-F]{64})$') {
            $expected = $Matches[1]
            $actual = Get-Sha256 ([IO.File]::OpenRead($componentZip))
            if ($actual -ne $expected) { throw "$($asset.name) is corrupt: its SHA-256 is $actual, GitHub says $expected." }
            Write-Host '  checksum matches'
        }
        $presetsZip = Join-Path $work 'presets.zip'
        Get-Download $PresetsUrl $presetsZip
        $texturesZip = Join-Path $work 'textures.zip'
        Get-Download $TexturesUrl $texturesZip

        foreach ($zip in $componentZip, $presetsZip, $texturesZip) { $archives.Add([IO.Compression.ZipFile]::OpenRead($zip)) }
        $componentFiles = Get-ZipFiles $archives[0] ''
        $dlls = @($componentFiles.Keys | Where-Object { -not $_.Contains('\') -and $_ -like '*.dll' })
        if ($dlls.Count -ne 1) { throw "Expected one DLL at the top of $($asset.name), found $($dlls.Count)." }
        $componentName = [IO.Path]::GetFileNameWithoutExtension($dlls[0])
        $presetFiles = Get-ZipFiles $archives[1] 'Milkdrop-Original' -SkipRoot
        if (@($presetFiles.Keys | Where-Object { $_ -like '*.milk' }).Count -eq 0) {
            throw "The presets archive has no Milkdrop-Original folder of .milk files ($PresetsUrl)."
        }
        $textureFiles = Get-ZipFiles $archives[2] 'textures' -SkipRoot
        if ($textureFiles.Count -eq 0) { throw "The texture pack archive has no textures folder ($TexturesUrl)." }

        # foobar2000 may have been started during the download.
        Assert-NotRunning $profileDir

        # --- Install ------------------------------------------------------------------------------
        Step "MilkDrop 2 Plus $($release.tag_name)"
        $componentsDir = Join-Path $profileDir 'user-components-x64'
        $componentDest = Join-Path $componentsDir $componentName
        # This very build, installed already, is written again rather than saved again.
        $installedDll = Join-Path $componentDest $dlls[0]
        $sameBuild = (Test-Path -LiteralPath $installedDll -PathType Leaf) -and
            (Get-Sha256 ([IO.File]::OpenRead($installedDll))) -eq (Get-Sha256 $componentFiles[$dlls[0]].Open())
        foreach ($name in $ComponentNames + $componentName | Select-Object -Unique) {
            $installed = Join-Path $componentsDir $name
            if ($name -eq $componentName) {
                # Saved, or noted as absent, so that -Restore can take it out again.
                if (-not $sameBuild) { Save-Backup $installed }
            }
            elseif (Test-Path -LiteralPath $installed) {
                Write-Host "  moving $name out of the way: it would claim the same UI element"
                Save-Backup $installed
            }
        }
        $n = Expand-ZipFiles $componentFiles $componentDest
        Write-Host "  $n files in $componentDest$(if ($sameBuild) { ' (this build was installed already)' })"

        # Presets and textures already there are kept: they may have been edited since.
        Step 'Presets and textures'
        $presetsDest = Join-Path $profileDir 'milkdrop2\presets\Milkdrop-Original'
        $n = Expand-ZipFiles $presetFiles $presetsDest -KeepExisting
        Write-Host "  $($presetFiles.Count) presets in ${presetsDest}: $n added, $($presetFiles.Count - $n) there already, kept"
        $texturesDest = Join-Path $profileDir 'milkdrop2\textures'
        $n = Expand-ZipFiles $textureFiles $texturesDest -KeepExisting
        Write-Host "  $($textureFiles.Count) textures in ${texturesDest}: $n added, $($textureFiles.Count - $n) there already, kept"

        Step 'Preset folder'
        Set-PresetFolder (Join-Path $profileDir 'browser_config.ini') $PresetLibraryStored

        if ($installLayout) {
            Step 'Installing the layout'
            Save-Backup $themeFile
            [IO.File]::WriteAllBytes($themeFile, $newLayout)
            Write-Host "  wrote $themeFile"
            if (Test-Path -LiteralPath (Join-Path $componentsDir 'foo_ui_columns')) {
                Write-Warning 'Columns UI is installed. This layout is for the Default UI: if foobar2000 uses Columns UI, choose Default User Interface in Preferences > Display > User interface module.'
            }
        }
    }
    finally {
        foreach ($archive in $archives) { $archive.Dispose() }
        Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    }

    Write-Host "`nDone. Start foobar2000." -ForegroundColor Green
    if ($script:BackupDir) {
        Write-Host "To undo this install, run this script again without options and type R when it asks, or run it with -Restore. (Its backup: $script:BackupDir)"
    }
}

# The Default UI layout (profile\theme.fth) of the foobar2000 this script was made from, what it shows,
# and the layout itself, base64-encoded.
$EmbeddedLayoutDescription = 'MilkDrop on the right, and a spectrum, an oscilloscope, a spectrogram and the playlists on the left'
$EmbeddedLayout = @'
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
'@

# Keeps the window open at the end, so that its messages can be read, when it closes with this script:
# when the batch part found this file named in cmd's command line (Explorer, Windows+R or another
# program started it, rather than an interactive command prompt) and no other program shares the
# console, as a shell that started it from its own window would (PowerShell runs a .cmd with cmd /c).
function Wait-IfOwnWindow {
    if (-not $env:MD2P_PAUSE_IF_ALONE) { return }
    try {
        Add-Type -Namespace Md2p -Name NativeConsole -MemberDefinition '[DllImport("kernel32.dll")] public static extern uint GetConsoleProcessList(uint[] list, uint count);'
        # cmd and this PowerShell are two; GetConsoleProcessList returns how many there are, even past 8.
        if ([Md2p.NativeConsole]::GetConsoleProcessList((New-Object uint32[] 8), 8) -gt 2) { return }
    }
    catch { }
    Read-Host 'Press Enter to close' | Out-Null
}

$exitCode = 0
try { Install-MilkDrop }
catch {
    Write-Host "`nERROR: $($_.Exception.Message)" -ForegroundColor Red
    $exitCode = 1
}
Wait-IfOwnWindow
exit $exitCode
