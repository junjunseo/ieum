$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source

Push-Location $root
try {
    New-Item -ItemType Directory -Force -Path "build" | Out-Null

    & $compiler `
        -std=c++17 `
        -Wall `
        -Wextra `
        -pedantic `
        -Isrc `
        src/main.cpp `
        -o build/ieum.exe

    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed: src/main.cpp"
    }

    Write-Host "Build complete: build/ieum.exe"
} finally {
    Pop-Location
}
