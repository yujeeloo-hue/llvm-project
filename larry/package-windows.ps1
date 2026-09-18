$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$build = Join-Path $repo "build-release-14.38"
$version = "23.1.1-larry.1"
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
