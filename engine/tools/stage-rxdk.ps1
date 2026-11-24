# Stages the RXDK toolchain into an RXDK root (default C:\ProgramData\RXDK), the
# same layout the RXDK VS Code / Visual Studio setup installs:
#   llvm\   RXDK clang          (Team-Resurgent/llvm-project, latest)
#   sdk\    headers + libs      (Team-Resurgent/RXDK-SDK, main)
#   tools\  Rxdk.Cli, imagebld, bundler, ... (Team-Resurgent/RXDK-Tools, latest)
#           xdvdfs              (Team-Resurgent/XDVDFS-TR, latest)
# Used by CI; also handy for a fresh machine. Point Rxdk.Cli at a non-default
# root with the RXDK environment variable.
param(
    [string]$Root = $(if ($env:RXDK) { $env:RXDK } else { 'C:\ProgramData\RXDK' })
)
$ErrorActionPreference = 'Stop'
$tmp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [IO.Path]::GetTempPath() }
New-Item -ItemType Directory -Force -Path "$Root\llvm", "$Root\tools" | Out-Null

function Fetch([string]$url, [string]$name) {
    $zip = Join-Path $tmp $name
    Write-Host "downloading $url"
    curl.exe -fsSL -o $zip $url
    if ($LASTEXITCODE -ne 0) { throw "download failed: $url" }
    return $zip
}

# RXDK clang
if (-not (Test-Path "$Root\llvm\xbox-windows-x64\bin\clang.exe")) {
    $zip = Fetch 'https://github.com/Team-Resurgent/llvm-project/releases/download/latest/xbox-windows-x64.zip' 'xbox-windows-x64.zip'
    tar.exe -xf $zip -C "$Root\llvm"
    if (-not (Test-Path "$Root\llvm\xbox-windows-x64\bin\clang.exe")) { throw 'clang.exe missing after extract' }
}

# SDK
if (-not (Test-Path "$Root\sdk\include")) {
    # git reports progress on stderr; Windows PowerShell 5.1 would treat that as an error.
    $ErrorActionPreference = 'Continue'
    git clone --quiet --depth 1 https://github.com/Team-Resurgent/RXDK-SDK.git "$Root\sdk" 2>&1 | Out-Host
    $ErrorActionPreference = 'Stop'
    if ($LASTEXITCODE -ne 0) { throw 'cloning RXDK-SDK failed' }
}
if (-not (Test-Path "$Root\sdk\lib\libxapi.lib")) { throw 'RXDK-SDK libxapi.lib missing' }

# Host tools (Rxdk.Cli, imagebld, bundler, ...)
$zip = Fetch 'https://github.com/Team-Resurgent/RXDK-Tools/releases/download/latest/rxdk-managed-win-x64.zip' 'rxdk-tools.zip'
$extract = Join-Path $tmp 'rxdk-tools'
Remove-Item -Recurse -Force $extract -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $extract | Out-Null
tar.exe -xf $zip -C $extract
$toolsDir = Get-ChildItem $extract -Recurse -Directory -Filter tools | Select-Object -First 1
if (-not $toolsDir) { throw 'tools\ missing from the RXDK-Tools release' }
Copy-Item "$($toolsDir.FullName)\*" "$Root\tools" -Recurse -Force
foreach ($t in 'Rxdk.Cli.exe', 'imagebld.exe', 'bundler.exe') {
    if (-not (Test-Path "$Root\tools\$t")) { throw "$t missing from the RXDK-Tools release" }
}

# xdvdfs (asset names carry a commit suffix)
$meta = Invoke-RestMethod https://api.github.com/repos/Team-Resurgent/XDVDFS-TR/releases/latest
$asset = $meta.assets | Where-Object { $_.name -like 'xdvdfs-windows-x64-*.zip' } | Select-Object -First 1
if (-not $asset) { throw 'xdvdfs-windows-x64 zip missing from the XDVDFS-TR release' }
$zip = Fetch $asset.browser_download_url 'xdvdfs.zip'
$extract = Join-Path $tmp 'xdvdfs'
Remove-Item -Recurse -Force $extract -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $extract | Out-Null
tar.exe -xf $zip -C $extract
$xdvdfs = Get-ChildItem $extract -Recurse -Filter xdvdfs.exe | Select-Object -First 1
if (-not $xdvdfs) { throw 'xdvdfs.exe missing from the XDVDFS-TR zip' }
Copy-Item $xdvdfs.FullName "$Root\tools\xdvdfs.exe" -Force

Write-Host "RXDK staged at $Root"
