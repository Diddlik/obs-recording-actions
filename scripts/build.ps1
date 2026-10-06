param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$Toolset = 'v143',
    [string]$BuildDirectory = 'build_x64',
    [string]$InnoCompiler = "${env:ProgramFiles(x86)}/Inno Setup 6/ISCC.exe",
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
if ($Package -and $Configuration -ne 'Release') { throw 'Installable packages must use the Release configuration.' }
Set-Location (Split-Path $PSScriptRoot -Parent)

function Invoke-BuildTool([string]$Executable, [string[]]$Arguments) {
    $start = [System.Diagnostics.ProcessStartInfo]::new($Executable)
    $start.UseShellExecute = $false
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    # Some agent hosts provide both PATH and Path; MSBuild rejects that environment.
    $normalized = @{}
    foreach ($entry in $start.Environment.GetEnumerator()) { $normalized[$entry.Key.ToUpperInvariant()] = $entry.Value }
    $start.Environment.Clear()
    foreach ($key in $normalized.Keys) { $start.Environment[$key] = $normalized[$key] }
    $start.Environment['MSBUILDDISABLENODEREUSE'] = '1'
    $process = [System.Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "$Executable failed with exit code $($process.ExitCode)" }
}

Invoke-BuildTool 'cmake' @('-S', '.', '-B', $BuildDirectory, '-G', $Generator, '-A', 'x64', '-T', $Toolset,
    '-DCMAKE_COMPILE_WARNING_AS_ERROR=ON')
Invoke-BuildTool 'cmake' @('--build', $BuildDirectory, '--config', $Configuration, '--parallel', '1')
Invoke-BuildTool 'ctest' @('--test-dir', $BuildDirectory, '-C', $Configuration, '--output-on-failure')
if ($Package) {
    Invoke-BuildTool 'cpack' @('--config', "$BuildDirectory/CPackConfig.cmake", '-C', $Configuration, '-B', 'dist')
    $version = (Get-Content buildspec.json -Raw | ConvertFrom-Json).version
    $stage = Join-Path $BuildDirectory "installer-stage"
    Invoke-BuildTool 'cmake' @('--install', $BuildDirectory, '--config', $Configuration, '--prefix', $stage)
    if (-not (Test-Path $InnoCompiler)) { throw "Inno Setup 6 compiler not found: $InnoCompiler" }
    Invoke-BuildTool $InnoCompiler @("/DPluginVersion=$version", "/DStageDir=$((Resolve-Path $stage).Path)", 'installer/recording-actions.iss')
    Compress-Archive -Path src, tests, data, cmake, scripts, installer, docs, .github, CMakeLists.txt, CMakePresets.json, buildspec.json,
        .clang-format, .gitignore, README.md, LICENSE, AGENTS.md, REQ.md `
        -DestinationPath "dist/obs-recording-actions-$version-source.zip" -Force
    Get-ChildItem dist -File | Where-Object { $_.Extension -in '.zip', '.exe' -and $_.Name -like "obs-recording-actions-$version-*" } | Sort-Object Name | ForEach-Object {
        '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $_.Name
    } | Set-Content dist/SHA256SUMS.txt -Encoding ascii
}
