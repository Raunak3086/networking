# test.ps1 - Automated test script for reliable UDP file transfer
param(
    [string]$BuildDir = ".\build\Release",
    [int]$Port = 9000,
    [string]$TempDir = ".\test_temp"
)

$ErrorActionPreference = "Stop"
$global:TestsPassed = 0
$global:TestsFailed = 0

function Run-Test {
    param(
        [string]$TestName,
        [string]$InputFile,
        [string]$Mode = "saw",
        [int]$Window = 1,
        [int]$Loss = 0,
        [int]$Dup = 0,
        [int]$Corrupt = 0,
        [int]$Seed = 42,
        [int]$TimeoutMs = 500,
        [int]$MaxWaitSeconds = 120
    )
    
    $outputFile = Join-Path $TempDir "received_$(Split-Path $InputFile -Leaf)"
    $port = $Port
    $Port++  # Use different ports for parallel safety
    
    Write-Host "TEST: $TestName" -ForegroundColor Cyan
    
    # Start receiver
    $receiverArgs = "$port $TempDir --loss $Loss --dup $Dup --corrupt $Corrupt --seed $($Seed + 1)"
    $receiverProc = Start-Process -FilePath "$BuildDir\receiver.exe" -ArgumentList $receiverArgs -PassThru -NoNewWindow -RedirectStandardOutput "$TempDir\recv_out.txt" -RedirectStandardError "$TempDir\recv_err.txt"
    Start-Sleep -Milliseconds 500  # Let receiver start
    
    # Run sender
    $senderArgs = "127.0.0.1 $port `"$InputFile`" --mode $Mode --window $Window --timeout $TimeoutMs --loss $Loss --dup $Dup --corrupt $Corrupt --seed $Seed"
    $senderProc = Start-Process -FilePath "$BuildDir\sender.exe" -ArgumentList $senderArgs -PassThru -NoNewWindow -RedirectStandardOutput "$TempDir\send_out.txt" -RedirectStandardError "$TempDir\send_err.txt"
    
    # Wait for processes
    $senderProc | Wait-Process -Timeout $MaxWaitSeconds -ErrorAction SilentlyContinue
    if (!$senderProc.HasExited) { $senderProc.Kill(); Write-Host "  TIMEOUT (sender)" -ForegroundColor Red }
    
    Start-Sleep -Milliseconds 1000
    if (!$receiverProc.HasExited) { $receiverProc.Kill() }
    
    # Find the received file
    # Actually the receiver saves with the original filename
    $expectedOutput = Join-Path $TempDir (Split-Path $InputFile -Leaf)
    
    # Compare SHA-256
    if (Test-Path $expectedOutput) {
        $origSize = (Get-Item $InputFile).Length
        if ($origSize -eq 0) {
            # Empty file - pass if it exists and is empty
            $recvSize = (Get-Item $expectedOutput).Length
            if ($recvSize -eq 0) {
                Write-Host "  PASS (empty file)" -ForegroundColor Green
                $global:TestsPassed++
            } else {
                Write-Host "  FAIL (expected empty file, got $recvSize bytes)" -ForegroundColor Red
                $global:TestsFailed++
            }
        } else {
            $hashOrig = (Get-FileHash -Path $InputFile -Algorithm SHA256).Hash
            $hashRecv = (Get-FileHash -Path $expectedOutput -Algorithm SHA256).Hash
            
            if ($hashOrig -eq $hashRecv) {
                Write-Host "  PASS (SHA-256 match)" -ForegroundColor Green
                $global:TestsPassed++
            } else {
                Write-Host "  FAIL (SHA-256 mismatch)" -ForegroundColor Red
                Write-Host "    Original: $hashOrig"
                Write-Host "    Received: $hashRecv"
                $global:TestsFailed++
            }
        }
        Remove-Item $expectedOutput -Force -ErrorAction SilentlyContinue
    } else {
        # For empty files, check if file exists and is empty
        $origSize = (Get-Item $InputFile).Length
        if ($origSize -eq 0) {
            # Empty file - might not be created or should be 0 bytes
            Write-Host "  PASS (empty file not explicitly written)" -ForegroundColor Green
            $global:TestsPassed++
        } else {
            Write-Host "  FAIL (output file not found)" -ForegroundColor Red
            $global:TestsFailed++
        }
    }
}

# Create temp directory
if (Test-Path $TempDir) { Remove-Item $TempDir -Recurse -Force }
New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

# Generate test files
$testFiles = @{
    "test_0B.bin" = 0
    "test_1B.bin" = 1
    "test_1023B.bin" = 1023
    "test_1024B.bin" = 1024
    "test_1025B.bin" = 1025
    "test_1MB.bin" = 1048576
}

foreach ($file in $testFiles.GetEnumerator()) {
    $path = Join-Path $TempDir $file.Key
    if ($file.Value -eq 0) {
        New-Item -ItemType File -Path $path -Force | Out-Null
    } else {
        $bytes = New-Object byte[] $file.Value
        $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
        $rng.GetBytes($bytes)
        [System.IO.File]::WriteAllBytes($path, $bytes)
    }
}

Write-Host "`n=== Test Suite: Reliable UDP File Transfer ===`n" -ForegroundColor Yellow

# Test each file size with each mode
$modes = @("saw", "gbn", "sr")
$basePort = 9000

foreach ($mode in $modes) {
    $window = if ($mode -eq "saw") { 1 } else { 16 }
    Write-Host "`n--- Mode: $mode (window=$window) ---`n" -ForegroundColor Yellow
    
    foreach ($file in $testFiles.GetEnumerator()) {
        $filePath = Join-Path $TempDir $file.Key
        $basePort++
        
        # Test without faults
        $Port = $basePort; $basePort++
        Run-Test -TestName "$mode - $($file.Key) - no faults" -InputFile $filePath -Mode $mode -Window $window -Port $Port
        
        # Test with faults (skip for 0-byte files)
        if ($file.Value -gt 0) {
            $Port = $basePort; $basePort++
            Run-Test -TestName "$mode - $($file.Key) - 10% loss" -InputFile $filePath -Mode $mode -Window $window -Loss 10 -Port $Port
        }
    }
}

# Additional tests: GBN window sizes
Write-Host "`n--- GBN Window Size Tests ---`n" -ForegroundColor Yellow
$testFile = Join-Path $TempDir "test_1MB.bin"
foreach ($w in @(1, 4, 16, 64)) {
    $basePort++
    Run-Test -TestName "GBN window=$w - 1MB - 10% loss" -InputFile $testFile -Mode gbn -Window $w -Loss 10 -Port $basePort
}

Write-Host "`n=== Results: $($global:TestsPassed) passed, $($global:TestsFailed) failed ==="  -ForegroundColor $(if ($global:TestsFailed -eq 0) { "Green" } else { "Red" })

# Cleanup
# Remove-Item $TempDir -Recurse -Force

if ($global:TestsFailed -gt 0) {
    exit 1
} else {
    exit 0
}
