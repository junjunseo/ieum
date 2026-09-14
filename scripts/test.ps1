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

    $commonArgs = @(
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-Isrc",
        $versionDefinition
    )

    $targets = @(
        @{ Source = "test/testParser.cpp";   Output = "build/testParser.exe" },
        @{ Source = "test/testPipeline.cpp"; Output = "build/testPipeline.exe" },
        @{ Source = "test/testChecker.cpp";  Output = "build/testChecker.exe" },
        @{ Source = "test/testGraph.cpp";    Output = "build/testGraph.exe" },
        @{ Source = "test/testSemantic.cpp"; Output = "build/testSemantic.exe" },
        @{ Source = "test/testInterpreter.cpp"; Output = "build/testInterpreter.exe" }
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

    Write-Host "Build: benchmark/benchmarkChecker.cpp"
    & $compiler @commonArgs `
        "-O2" `
        "-DNDEBUG" `
        "benchmark/benchmarkChecker.cpp" `
        -o "build/benchmarkChecker.exe"
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed: benchmark/benchmarkChecker.cpp"
    }

    Write-Host "Run: build/benchmarkChecker.exe 2 1"
    $benchmarkOutput = & ".\build\benchmarkChecker.exe" 2 1 2>&1
    $benchmarkExitCode = $LASTEXITCODE
    $benchmarkText = $benchmarkOutput -join "`n"
    Write-Host $benchmarkText
    if ($benchmarkExitCode -ne 0) {
        throw "Benchmark smoke test failed"
    }
    if (-not $benchmarkText.Contains("scenario=layered-valid-chain")) {
        throw "Benchmark smoke test did not report the expected scenario"
    }
    Write-Host ""

    Write-Host "Build: src/main.cpp"
    & $compiler @commonArgs "src/main.cpp" -o "build/ieum.exe"
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed: src/main.cpp"
    }

    Write-Host "Run: build/ieum.exe --version"
    $versionOutput = & ".\build\ieum.exe" "--version" 2>&1
    $versionExitCode = $LASTEXITCODE
    $versionText = ($versionOutput -join "`n").Trim()
    Write-Host $versionText
    if ($versionExitCode -ne 0) {
        throw "Expected --version to exit with 0, got $versionExitCode"
    }
    if ($versionText -ne "ieum $version") {
        throw "Expected version output 'ieum $version', got '$versionText'"
    }
    Write-Host ""

    $validExamples = @(
        @{ Name = "valid"; Expected = "modules=3"; Arguments = @() },
        @{ Name = "module_body"; Expected = "body_modules=2"; Arguments = @() },
        @{ Name = "execution"; Expected = "calls_executed=2"; Arguments = @("--run", "service.main") }
    )

    foreach ($example in $validExamples) {
        $path = ".\examples\$($example.Name).ieum"
        Write-Host "Run: build/ieum.exe $path"
        $arguments = @($path) + $example.Arguments
        $validOutput = & ".\build\ieum.exe" @arguments 2>&1
        $validExitCode = $LASTEXITCODE
        $validText = $validOutput -join "`n"
        Write-Host $validText
        if ($validExitCode -ne 0) {
            throw "Expected valid example to pass: $path"
        }
        if (-not $validText.Contains($example.Expected)) {
            throw "Expected output '$($example.Expected)' for $path"
        }
        Write-Host ""
    }

    $runBoundaryCases = @(
        @{ Entry = "service.missing"; ExpectedExit = 1; Expected = "service.missing" },
        @{ Entry = "service.prepare"; ExpectedExit = 1; Expected = "service.prepare" },
        @{ Entry = "service"; ExpectedExit = 2; Expected = "entry_format=module.function" }
    )

    foreach ($case in $runBoundaryCases) {
        $path = ".\examples\execution.ieum"
        Write-Host "Run: build/ieum.exe $path --run $($case.Entry)"
        $savedErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $runOutput = & ".\build\ieum.exe" $path "--run" $case.Entry 2>&1
        $runExitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedErrorActionPreference
        $runText = $runOutput -join "`n"
        Write-Host $runText
        if ($runExitCode -ne $case.ExpectedExit) {
            throw "Expected --run $($case.Entry) to exit with $($case.ExpectedExit), got $runExitCode"
        }
        if (-not $runText.Contains($case.Expected)) {
            throw "Expected output '$($case.Expected)' for --run $($case.Entry)"
        }
        Write-Host ""
    }

    $invalidExamples = @(
        @{ Name = "implicit_dependency"; Expected = "notification" },
        @{ Name = "cyclic_dependency"; Expected = "order -> payment -> order" },
        @{ Name = "layer_violation"; Expected = "'data'" },
        @{ Name = "transitive_layer_violation"; Expected = "'data'" },
        @{ Name = "invalid_declarations"; Expected = "'missing'" },
        @{ Name = "semantic_undefined_function"; Expected = "missing" },
        @{ Name = "semantic_arity_mismatch"; Expected = "helper" },
        @{ Name = "semantic_missing_dependency"; Expected = "depends" },
        @{ Name = "semantic_undefined_variable"; Expected = "missing" }
    )

    foreach ($example in $invalidExamples) {
        $path = ".\examples\$($example.Name).ieum"
        Write-Host "Run: build/ieum.exe $path"
        $exampleOutput = & ".\build\ieum.exe" $path 2>&1
        $exampleExitCode = $LASTEXITCODE
        $exampleText = $exampleOutput -join "`n"
        Write-Host $exampleText
        if ($exampleExitCode -eq 0) {
            throw "Expected validation violation for $path"
        }
        if ($exampleExitCode -ne 1) {
            throw "Expected exit code 1 for $path, got $exampleExitCode"
        }
        if (-not $exampleText.Contains($example.Expected)) {
            throw "Expected output '$($example.Expected)' for $path"
        }
        Write-Host ""
    }

    $graphCases = @(
        @{ Name = "valid"; ExpectedExit = 0 },
        @{ Name = "implicit_dependency"; ExpectedExit = 1 },
        @{ Name = "cyclic_dependency"; ExpectedExit = 1 },
        @{ Name = "layer_violation"; ExpectedExit = 1 },
        @{ Name = "transitive_layer_violation"; ExpectedExit = 1 },
        @{ Name = "invalid_declarations"; ExpectedExit = 1 }
    )
    $graphOutputDirectory = "build/graph-tests"
    New-Item -ItemType Directory -Force -Path $graphOutputDirectory | Out-Null

    foreach ($case in $graphCases) {
        $sourcePath = ".\examples\$($case.Name).ieum"
        $actualPath = Join-Path $graphOutputDirectory "$($case.Name).dot"
        $expectedPath = ".\test\snapshots\$($case.Name).dot"
        Write-Host "Run: build/ieum.exe $sourcePath --emit-dot $actualPath"

        $savedErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $graphOutput = & ".\build\ieum.exe" $sourcePath "--emit-dot" $actualPath 2>&1
        $graphExitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedErrorActionPreference
        Write-Host ($graphOutput -join "`n")

        if ($graphExitCode -ne $case.ExpectedExit) {
            throw "Expected graph command for $sourcePath to exit with $($case.ExpectedExit), got $graphExitCode"
        }

        $actualGraph = (Get-Content -LiteralPath $actualPath -Raw) -replace "`r`n", "`n"
        $expectedGraph = (Get-Content -LiteralPath $expectedPath -Raw) -replace "`r`n", "`n"
        if ($actualGraph -cne $expectedGraph) {
            throw "Graph snapshot mismatch for $sourcePath"
        }
        Write-Host "Graph snapshot matched: $expectedPath"
        Write-Host ""
    }

    $graphFailureCases = @(
        @{ Name = "valid"; ExpectedExit = 0 },
        @{ Name = "cyclic_dependency"; ExpectedExit = 1 }
    )
    foreach ($case in $graphFailureCases) {
        $sourcePath = ".\examples\$($case.Name).ieum"
        Write-Host "Run graph failure isolation: $sourcePath"
        $savedErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $failureOutput = & ".\build\ieum.exe" $sourcePath "--emit-dot" ".\build" 2>&1
        $failureExitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedErrorActionPreference
        $failureText = $failureOutput -join "`n"
        Write-Host $failureText

        if ($failureExitCode -ne $case.ExpectedExit) {
            throw "Graph export failure changed validation exit for $sourcePath"
        }
        if (-not $failureText.Contains("graph_export=failed")) {
            throw "Graph export failure warning was not printed for $sourcePath"
        }
        Write-Host ""
    }

    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($pythonCommand) {
        foreach ($script in @("test/test_workflows.py", "scripts/evaluate.py", "scripts/demo.py")) {
            & $pythonCommand.Source -B $script --ieum "build/ieum.exe"
            if ($LASTEXITCODE -ne 0) {
                throw "Workflow validation failed: $script"
            }
        }
    } else {
        Write-Host "Python unavailable: demo/evaluation checks skipped (requires Python 3.9+)."
    }

    Write-Host "All tests passed."
} finally {
    Pop-Location
}
