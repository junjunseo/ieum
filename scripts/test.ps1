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

    Write-Host "Build: src/main.cpp"
    & $compiler @commonArgs "src/main.cpp" -o "build/ieum.exe"
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed: src/main.cpp"
    }

    Write-Host "Run: build/ieum.exe examples/valid.ieum"
    & ".\build\ieum.exe" ".\examples\valid.ieum"
    if ($LASTEXITCODE -ne 0) {
        throw "Expected valid example to pass"
    }
    Write-Host ""

    $invalidExamples = @(
        "implicit_dependency",
        "cyclic_dependency",
        "layer_violation",
        "transitive_layer_violation",
        "invalid_declarations"
    )

    foreach ($example in $invalidExamples) {
        $path = ".\examples\$example.ieum"
        Write-Host "Run: build/ieum.exe $path"
        & ".\build\ieum.exe" $path
        if ($LASTEXITCODE -eq 0) {
            throw "Expected structural violation for $path"
        }
        if ($LASTEXITCODE -ne 1) {
            throw "Expected exit code 1 for $path, got $LASTEXITCODE"
        }
        Write-Host ""
    }

    Write-Host "All tests passed."
} finally {
    Pop-Location
}
