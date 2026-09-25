$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$executable = Join-Path $root 'All_one.exe'
$demoDirectory = Join-Path $root 'demo'
$watchDirectory = Join-Path $demoDirectory 'watch_target'
$eventFile = Join-Path $watchDirectory 'all_run_socket_test.tmp'
$watcherRawLog = Join-Path $demoDirectory 'watcher_raw.log'
$clientRawLog = Join-Path $demoDirectory 'client_raw.log'
$watcherLog = Join-Path $demoDirectory 'watcher_capture.txt'
$clientLog = Join-Path $demoDirectory 'client_capture.txt'

function Remove-AnsiCodes([string]$value) {
    $escape = [char]27
    return $value -replace "$escape\[[0-9;]*m", ''
}

function Wait-ForLogText([string]$path, [string]$text, [int]$timeoutSeconds = 5) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $timeoutSeconds) {
        if (Test-Path $path) {
            $content = Get-Content $path -Raw -ErrorAction SilentlyContinue
            if ($content -and $content.Contains($text)) {
                return
            }
        }
        [Threading.Thread]::Yield() | Out-Null
    }
    throw "Timed out waiting for '$text' in $path"
}

if (-not (Test-Path $executable)) {
    throw 'All_one.exe is missing. Build it with: cl /nologo /std:c++14 /EHsc /W4 All_one.cpp /Fe:All_one.exe'
}

New-Item -Path $demoDirectory -ItemType Directory -Force | Out-Null
New-Item -Path $watchDirectory -ItemType Directory -Force | Out-Null
Remove-Item $watcherRawLog, $clientRawLog, $watcherLog, $clientLog, $eventFile -Force -ErrorAction SilentlyContinue

$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = ([Net.IPEndPoint]$listener.LocalEndpoint).Port
$listener.Stop()

Write-Host "`n=== LINKED LIST + THREAD DEMO ===" -ForegroundColor Cyan
& $executable
if ($LASTEXITCODE -ne 0) {
    throw "All_one.exe failed with exit code $LASTEXITCODE"
}

$watcher = $null
$client = $null
try {
    Write-Host "`n=== WATCHER SERVER ===" -ForegroundColor Yellow
    $watcher = Start-Process -FilePath $executable `
        -ArgumentList @('watcher', 'demo\watch_target', $port) `
        -WorkingDirectory $root `
        -RedirectStandardOutput $watcherRawLog `
        -RedirectStandardError (Join-Path $demoDirectory 'watcher_error.log') `
        -WindowStyle Hidden `
        -PassThru
    Wait-ForLogText $watcherRawLog 'Watcher server listening'

    Write-Host "=== SOCKET CLIENT ===" -ForegroundColor Magenta
    $client = Start-Process -FilePath $executable `
        -ArgumentList @('client', '127.0.0.1', $port) `
        -WorkingDirectory $root `
        -RedirectStandardOutput $clientRawLog `
        -RedirectStandardError (Join-Path $demoDirectory 'client_error.log') `
        -WindowStyle Hidden `
        -PassThru
    Wait-ForLogText $clientRawLog 'CONNECTED'

    New-Item -Path $eventFile -ItemType File -Force | Out-Null
    Remove-Item $eventFile -Force
    Wait-ForLogText $clientRawLog 'REMOVED'

    $watcherOutput = Remove-AnsiCodes (Get-Content $watcherRawLog -Raw)
    $clientOutput = Remove-AnsiCodes (Get-Content $clientRawLog -Raw)
    Set-Content -Path $watcherLog -Value $watcherOutput -Encoding UTF8
    Set-Content -Path $clientLog -Value $clientOutput -Encoding UTF8

    Write-Host $watcherOutput.Trim() -ForegroundColor Yellow
    Write-Host $clientOutput.Trim() -ForegroundColor Green
    Write-Host "`nAll demos passed. Captures saved in demo/." -ForegroundColor Cyan
}
finally {
    if (Test-Path $eventFile) {
        Remove-Item $eventFile -Force
    }
    if ($client -and -not $client.HasExited) {
        Stop-Process -Id $client.Id -Force
    }
    if ($watcher -and -not $watcher.HasExited) {
        Stop-Process -Id $watcher.Id -Force
    }
    Remove-Item $watcherRawLog, $clientRawLog -Force -ErrorAction SilentlyContinue
}
