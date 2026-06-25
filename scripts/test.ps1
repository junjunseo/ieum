$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source

Push-Location $root
try {
    New-Item -ItemType Directory -Force -Path "build" | Out-Null

    $commonArgs = @(
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-Isrc"
    )

    $targets = @(
        @{ Source = "test/testParser.cpp";   Output = "build/testParser.exe" },
        @{ Source = "test/testPipeline.cpp"; Output = "build/testPipeline.exe" },
        @{ Source = "test/testChecker.cpp";  Output = "build/testChecker.exe" }
    )

    foreach ($target in $targets) {
        Write-Host "Build: $($target.Source)"
        & $compiler @commonArgs $target.Source -o $target.Output
        if ($LASTEXITCODE -ne 0) {
            throw "Compilation failed: $($target.Source)"
        }

        Write-Host "Run: $($target.Output)"
        & ".\$($target.Output)"
        if ($LASTEXITCODE -ne 0) {
            throw "Test failed: $($target.Output)"
        }
        Write-Host ""
    }

    Write-Host "All tests passed."
} finally {
    Pop-Location
}
