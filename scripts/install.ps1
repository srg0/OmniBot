# OmniBot — install dependencies (Windows PowerShell)
# Run from the repository root:  .\scripts\install.ps1
$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

function Test-Command($Name) {
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

Write-Host ""
Write-Host "OmniBot install" -ForegroundColor Cyan
Write-Host "---------------"
Write-Host ""

if (-not (Test-Command "python")) {
    Write-Error "Python is not on PATH. Install Python 3.10+ from https://www.python.org/downloads/ and re-run."
}
if (-not (Test-Command "node")) {
    Write-Error "Node.js is not on PATH. Install LTS from https://nodejs.org/ and re-run."
}

$pyVer = python -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')" 2>$null
if (-not $pyVer) { Write-Error "Could not read Python version." }
Write-Host "Found Python $pyVer and Node $(node -v | ForEach-Object { $_.Trim() })"

Push-Location (Join-Path $RepoRoot "app\backend")
try {
    if (-not (Test-Path ".venv")) {
        Write-Host "Creating Python venv in app\backend\.venv ..."
        python -m venv .venv
    }
    $pip = Join-Path (Get-Location) ".venv\Scripts\pip.exe"
    Write-Host "Installing Python dependencies (this may take a few minutes) ..."
    & $pip install --upgrade pip
    & $pip install -r requirements.txt
} finally {
    Pop-Location
}

Push-Location (Join-Path $RepoRoot "app\frontend")
try {
    Write-Host "Installing frontend dependencies ..."
    if (Test-Path "package-lock.json") {
        npm ci
    } else {
        npm install
    }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host ""
Write-Host "Next — start the hub and dashboard:" -ForegroundColor Yellow
Write-Host "  .\scripts\start.ps1"
Write-Host ""
Write-Host "You do not need a .env file: paste your Gemini API key in the browser on first launch."
Write-Host ""
