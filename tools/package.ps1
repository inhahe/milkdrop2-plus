# Builds the release package, milkdrop2-plus.fb2k-component: the x64 Release DLL at its top and,
# in data\, the files the plugin reads when it starts (the shader sources, external\winamp\data\*.fx,
# and the help text, vis_milk2\help.bin). Without data\ the plugin cannot start, so the script stops
# if any of those is missing. Build Release x64 first.
#
# Usage: pwsh tools\package.ps1 [-OutDir <folder>]    (default: the repository's root; *.fb2k-component
#        files are ignored by git)
param([string]$OutDir)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

$root = Split-Path -Parent $PSScriptRoot
if (-not $OutDir) { $OutDir = $root }
$dll = Join-Path $root 'Bin\x64\Release\foo_milkdrop2_plus.dll'
if (-not (Test-Path -LiteralPath $dll)) { throw "No Release x64 build at $dll." }

# The shader files the engine reads at start (CPlugin::AllocateMilkDropNonDX11, which fails
# without them, so that the plugin does not start).
$shaders = 'include.fx', 'warp_vs.fx', 'warp_ps.fx', 'comp_vs.fx', 'comp_ps.fx', 'blur_vs.fx', 'blur1_ps.fx', 'blur2_ps.fx'
$data = @(foreach ($name in $shaders) { Join-Path $root "external\winamp\data\$name" }) + (Join-Path $root 'vis_milk2\help.bin')
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
