# Builds the release package, milkdrop2-plus.fb2k-component: the x64 Release DLL at its top and, in
# data\, the shader sources (external\winamp\data\*.fx), which the engine reads when it starts:
# without one of them it does not start (CPlugin::AllocateMilkDropNonDX11), so the script stops if
# any is missing. (The help text is built into the DLL.) Build Release x64 first.
#
# Usage: pwsh tools\package.ps1 [-OutDir <folder>]    (default: the repository's root; *.fb2k-component
#        files are ignored by git)
param([string]$OutDir)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

$root = Split-Path -Parent $PSScriptRoot
if (-not $OutDir) { $OutDir = $root }
# .NET opens files relative to the process's directory, not PowerShell's location: resolve it here.
$OutDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutDir)
if (-not (Test-Path -LiteralPath $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }
$dll = Join-Path $root 'Bin\x64\Release\foo_milkdrop2_plus.dll'
if (-not (Test-Path -LiteralPath $dll)) { throw "No Release x64 build at $dll." }

$shaders = 'include.fx', 'warp_vs.fx', 'warp_ps.fx', 'comp_vs.fx', 'comp_ps.fx', 'blur_vs.fx', 'blur1_ps.fx', 'blur2_ps.fx'
$data = @(foreach ($name in $shaders) { Join-Path $root "external\winamp\data\$name" })
foreach ($file in $data) { if (-not (Test-Path -LiteralPath $file)) { throw "Missing $file." } }

$out = Join-Path $OutDir 'milkdrop2-plus.fb2k-component'
if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out }
$zip = [IO.Compression.ZipFile]::Open($out, [IO.Compression.ZipArchiveMode]::Create)
try {
    [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $dll, 'foo_milkdrop2_plus.dll', [IO.Compression.CompressionLevel]::Optimal)
    foreach ($file in $data) {
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file, 'data/' + (Split-Path -Leaf $file), [IO.Compression.CompressionLevel]::Optimal)
    }
}
finally { $zip.Dispose() }

$version = (Get-Item -LiteralPath $dll).VersionInfo.FileVersion
Write-Output "${out}: foo_milkdrop2_plus.dll $version and $($data.Count) files in data\"
