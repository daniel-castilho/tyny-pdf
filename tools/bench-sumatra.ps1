#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Runs SumatraPDF 3.6.1 and 3.7pre benchmarks in one command.
.DESCRIPTION
    Expects SumatraPDF.exe and SumatraPDF-3.7pre.exe in the PATH (or C:\tmp).
    Outputs two JSON files to C:\tmp\sumatra361.json and C:\tmp\sumatra37pre.json.
#>

param()

$ErrorActionPreference = "Stop"

# Repo root = parent of this script's directory
$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Definition
$RepoRoot   = Split-Path -Parent $ScriptDir
$Harness    = Join-Path $RepoRoot "tests\bench\harness\run_benchmark.py"
$Corpus     = Join-Path $RepoRoot "tests\bench\corpus"
$OutDir     = "C:\tmp"
$ExeDir     = "C:\tmp"   # onde estão SumatraPDF.exe e SumatraPDF-3.7pre.exe

# Garante que os .exe estão no PATH
$env:Path += ";$ExeDir"

# Verifica pré-requisitos
if (-not (Test-Path $Harness)) { throw "Harness não encontrado: $Harness" }
if (-not (Test-Path $Corpus))  { throw "Corpus não encontrado: $Corpus" }
if (-not (Get-Command SumatraPDF.exe -ErrorAction SilentlyContinue))       { throw "SumatraPDF.exe não está no PATH (coloque em $ExeDir ou adicione ao PATH)" }
if (-not (Get-Command SumatraPDF-3.7pre.exe -ErrorAction SilentlyContinue)) { throw "SumatraPDF-3.7pre.exe não está no PATH (coloque em $ExeDir ou adicione ao PATH)" }

Write-Host "=== Benchmark SumatraPDF 3.6.1 ===" -ForegroundColor Cyan
python $Harness --target sumatra-3.6.1 --corpus $Corpus --runs 5 --output (Join-Path $OutDir "sumatra361.json")

Write-Host "`n=== Benchmark SumatraPDF 3.7 pre-release ===" -ForegroundColor Cyan
python $Harness --target sumatra-3.7pre --corpus $Corpus --runs 5 --output (Join-Path $OutDir "sumatra37pre.json")

Write-Host "`n=== Concluído ===" -ForegroundColor Green
Write-Host "Arquivos gerados:"
Write-Host "  $OutDir\sumatra361.json"
Write-Host "  $OutDir\sumatra37pre.json"
Write-Host "`nMande esses dois arquivos para mim que eu atualizo o baseline.json."