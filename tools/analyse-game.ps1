# Builds the qilin tool and runs the first analysis on your copy of the game.
#
#   powershell -ExecutionPolicy Bypass -File tools\analyse-game.ps1 [-Game <path>] [-ChooseGame] [-Pull]
#
# On Windows, "Analyse game.bat" in the project's folder runs it for you:
# double-click it. It needs Visual Studio 2022 17.10 or newer (with "Desktop
# development with C++") and finds its CMake and Git itself, from any window.
# -Game is your copy: a disc image (.iso), an extracted disc's folder (the one
# holding default.xbe) or default.xbe itself. With no -Game it uses the one it
# used last (build\game-path.txt), else asks with a file picker. Everything it
# writes goes to analysis-out\ (never committed): the disc's file list and
# default.xbe if your copy is an image, the executable's summary and SHA-256,
# the kernel functions it imports, its functions and control flow, how its
# functions are aligned, the byte accounting, and the instruction listings.
#
# Keep this file ASCII-only and Windows PowerShell 5.1-compatible.
param(
    [string]$Game = "",
    [switch]$ChooseGame,
    [switch]$Pull
)
$ErrorActionPreference = "Stop"
trap {   # any failure: say what in one red line, and end with an error code
    Write-Host ""
    Write-Host "Stopped: $_" -ForegroundColor Red
    exit 1
}
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$out = Join-Path $root "analysis-out"
$gameFile = Join-Path $build "game-path.txt"
$onWindows = ($PSVersionTable.PSEdition -eq "Desktop") -or $IsWindows   # Windows PowerShell 5.1 has no $IsWindows

function Invoke-Step([string]$what, [string]$exe, [string[]]$arguments) {
    Write-Host "== $what" -ForegroundColor Cyan
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)" }
}

# qilin's output to a file. Its messages (stderr) stay on the screen.
# $alsoOk: one more exit code that is a result, not a failure.
function Save-Output([string]$what, [string]$exe, [string[]]$arguments, [string]$file, [int]$alsoOk = 0) {
    Write-Host "== $what" -ForegroundColor Cyan
    $lines = & $exe @arguments
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $alsoOk) { throw "$what failed (exit code $LASTEXITCODE)" }
    Set-Content -Path $file -Value $lines -Encoding UTF8
}

# CMake and Git: on the PATH, else the ones Visual Studio installs, else the usual places.
function Add-Tool([string]$name, [string[]]$folders) {
    if (Get-Command $name -ErrorAction SilentlyContinue) { return $true }
    foreach ($f in $folders) {
        if ($f -and (Test-Path (Join-Path $f "$name.exe"))) {
            $env:PATH = "$f;$env:PATH"
            return $true
        }
    }
    return $false
}
if ($onWindows) {
    $vs = ""
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) { $vs = & $vswhere -latest -products * -property installationPath | Select-Object -First 1 }
    $ide = if ($vs) { Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft" } else { "" }
    $haveCmake = Add-Tool "cmake" @($(if ($ide) { Join-Path $ide "CMake\CMake\bin" }), (Join-Path $env:ProgramFiles "CMake\bin"))
    $haveGit = Add-Tool "git" @((Join-Path $env:ProgramFiles "Git\cmd"), $(if ($ide) { Join-Path $ide "TeamFoundation\Team Explorer\Git\cmd" }))
    if (-not $vs) { throw "Visual Studio 2022 wasn't found. Install it (the free Community edition) with 'Desktop development with C++'." }
    if (-not $haveCmake) { throw "CMake wasn't found. In the Visual Studio Installer, add 'C++ CMake tools for Windows'." }
    if (-not $haveGit) { throw "Git wasn't found. Install Git for Windows (git-scm.com), or add 'Git for Windows' in the Visual Studio Installer." }
} elseif (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake isn't installed."
}

if ($Pull) {
    Write-Host "== Getting the latest code" -ForegroundColor Cyan
    & git -C $root pull --ff-only
    if ($LASTEXITCODE -ne 0) { throw "git pull failed (exit code $LASTEXITCODE). If you changed files in the project folder, that's why." }
}

# Your copy of the game: the one used last, else ask.
if (-not $Game -and -not $ChooseGame -and (Test-Path $gameFile)) { $Game = (Get-Content $gameFile -Raw).Trim() }
if (-not $Game -or -not (Test-Path $Game)) {
    if ($Game) { Write-Host "$Game isn't there any more: choose your copy again." -ForegroundColor Yellow }
    if (-not $onWindows) { throw "give your copy of the game with -Game <path>" }
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title = "Choose your copy of Barbie Horse Adventures: Wild Horse Rescue (a disc image, or default.xbe in an extracted disc's folder)"
    $dialog.Filter = "Xbox game (*.iso;*.xiso;*.xbe)|*.iso;*.xiso;*.xbe|All files (*.*)|*.*"
    if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { throw "no game chosen" }
    $Game = $dialog.FileName
}

$configure = @("-S", $root, "-B", $build)
if ($onWindows -and -not (Test-Path (Join-Path $build "CMakeCache.txt"))) { $configure += @("-G", "Visual Studio 17 2022", "-A", "x64") }
if (-not $onWindows) { $configure += "-DCMAKE_BUILD_TYPE=Release" }
Invoke-Step "Configuring" "cmake" $configure
Set-Content -Path $gameFile -Value $Game -Encoding UTF8
Invoke-Step "Building the qilin tool" "cmake" @("--build", $build, "--config", "Release", "--target", "qilin", "--parallel")
$exe = if ($onWindows) { ".exe" } else { "" }
$qilin = @((Join-Path $build "Release/qilin$exe"), (Join-Path $build "qilin$exe")) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $qilin) { throw "built, but qilin$exe isn't where it should be" }

New-Item -ItemType Directory -Force -Path $out | Out-Null
$isFolder = Test-Path $Game -PathType Container
if (-not $isFolder -and $Game -notmatch '\.xbe$') {
    # A disc image: its file list, and default.xbe out of it (a few megabytes;
    # the rest of the game stays in the image).
    Save-Output "What the disc holds" $qilin @("disc", $Game) (Join-Path $out "disc.txt")
    Invoke-Step "Extracting default.xbe" $qilin @("extract", $Game, "--out", (Join-Path $out "extracted"), "--only", "default.xbe")
}
Save-Output "The executable" $qilin @("info", $Game) (Join-Path $out "info.txt")
Save-Output "Its kernel imports" $qilin @("imports", $Game) (Join-Path $out "imports.txt")
Save-Output "Its functions and control flow (this one can take a few minutes)" $qilin @("cfg", $Game) (Join-Path $out "cfg.txt")
Save-Output "How its functions are aligned" $qilin @("align", $Game) (Join-Path $out "align.txt")
Save-Output "The byte accounting" $qilin @("coverage", $Game, "--list") (Join-Path $out "coverage.txt") 3
Save-Output "Functions to review" $qilin @("suspects", $Game) (Join-Path $out "suspects.txt")
Invoke-Step "Every reachable instruction" $qilin @("insns", $Game, "-o", (Join-Path $out "reach.insns.txt"))
Invoke-Step "Every code section, swept" $qilin @("insns", $Game, "--linear", "-o", (Join-Path $out "linear.insns.txt"))

Write-Host ""
Write-Host "Done. The results are in $out" -ForegroundColor Green
Write-Host "They are derived from the game: keep them on this PC (the folder is never committed)."
