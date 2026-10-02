$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$version = (Get-Content -LiteralPath (Join-Path $root "VERSION") -Raw).Trim()

if ($version -notmatch '^\d+\.\d+\.\d+$') {
    throw "VERSION must contain a semantic version such as 0.1.0"
}

$versionDefinition = '-DIEUM_VERSION=\"' + $version + '\"'

Push-Location $root
try {
    New-Item -ItemType Directory -Force -Path "build" | Out-Null

    & $compiler `
        -std=c++17 `
        -Wall `
        -Wextra `
        -pedantic `
        -Isrc `
        $versionDefinition `
        src/main.cpp `
        src/cli_args.cpp `
        -o build/ieum.exe `
        -lshell32

    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed: src/main.cpp"
    }

    Write-Host "Build complete: build/ieum.exe"
} finally {
    Pop-Location
}
