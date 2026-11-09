$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$buildDir = $env:LARRY_BUILD_DIR
if (-not $buildDir) {
  $buildDir = if (Test-Path (Join-Path $repo "build-release-14.38")) { "build-release-14.38" } else { "build-release" }
}
$build = Join-Path $repo $buildDir
$version = (Get-Content (Join-Path $PSScriptRoot "VERSION.json") -Raw | ConvertFrom-Json).version
$artifactDir = Join-Path $repo "artifacts"
$stage = Join-Path $artifactDir "lldb-dap-$version-win-x64"
$zipPath = "$stage.zip"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
New-Item (Join-Path $stage "bin") -ItemType Directory -Force | Out-Null

$required = @("lldb-dap.exe", "lldb-server.exe", "liblldb.dll")
foreach ($name in $required) {
  $source = Join-Path $build "bin\$name"
  if (-not (Test-Path $source)) {
    throw "Required build output not found: $source"
  }
  Copy-Item $source (Join-Path $stage "bin\$name")
}

# App-local VC++ runtime. The binaries import these three; the Universal CRT
# (api-ms-win-crt-*) stays with Windows 10+. Newest redist under the same VS
# install is a superset of the toolset that linked the build.
$vsRoot = $env:LARRY_VS_ROOT
if (-not $vsRoot) {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
}
if (-not $vsRoot) { throw "Visual Studio with the C++ tools was not found." }
$crtDir = $null
$redistDirs = Get-ChildItem (Join-Path $vsRoot "VC\Redist\MSVC") -Directory |
  Where-Object { $_.Name -match '^\d+\.\d+\.\d+' } |
  Sort-Object { [version]$_.Name } -Descending
foreach ($dir in $redistDirs) {
  $candidate = Get-ChildItem (Join-Path $dir.FullName "x64") -Directory -Filter "Microsoft.VC*.CRT" |
    Select-Object -First 1
  if ($candidate -and (Test-Path (Join-Path $candidate.FullName "vcruntime140.dll"))) {
    $crtDir = $candidate.FullName
    break
  }
}
if (-not $crtDir) { throw "VC++ redistributable CRT was not found under $vsRoot" }
foreach ($name in @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")) {
  $source = Join-Path $crtDir $name
  if (-not (Test-Path $source)) { throw "Required VC++ runtime not found: $source" }
  Copy-Item $source (Join-Path $stage "bin\$name")
}

$clangResource = Join-Path $build "lib\clang\23\include"
if (Test-Path $clangResource) {
  $resourceTarget = Join-Path $stage "lib\clang\23\include"
  New-Item $resourceTarget -ItemType Directory -Force | Out-Null
  Copy-Item (Join-Path $clangResource "*") $resourceTarget -Recurse
}

Copy-Item (Join-Path $PSScriptRoot "VERSION.json") $stage
Copy-Item (Join-Path $repo "LICENSE.TXT") (Join-Path $stage "LICENSE.txt")

Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zipPath -CompressionLevel Optimal
$sha = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -Path "$zipPath.sha256" -Value "$sha  $(Split-Path $zipPath -Leaf)" -NoNewline

Write-Output $zipPath
Write-Output $sha
