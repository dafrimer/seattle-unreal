$ErrorActionPreference = 'Stop'
$installPath = 'H:\Tools\VisualStudioBuildTools'
$components = @(
    'Microsoft.VisualStudio.Workload.VCTools',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
    'Microsoft.VisualStudio.Component.Windows11SDK.22621',
    'Microsoft.Net.Component.4.6.2.TargetingPack',
    'Microsoft.Net.Component.4.8.SDK'
)
$addArgs = ($components | ForEach-Object { "--add $_" }) -join ' '
$vsInstaller = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe'
if ((Test-Path (Join-Path $installPath 'VC\Tools\MSVC')) -and (Test-Path $vsInstaller)) {
    $file = $vsInstaller
    $arguments = "modify --installPath $installPath $addArgs --passive --norestart"
} else {
    $file = Join-Path $PSScriptRoot '../Prerequisites/vs_BuildTools.exe'
    $hash = (Get-FileHash $file -Algorithm SHA256).Hash
    if ($hash -ne '37bb0fb429d163ecebd272a865d11a37b906d152bef960da2ddb29c2e2fd6eeb') { throw 'Installer checksum mismatch' }
    $arguments = "--passive --wait --norestart --installPath $installPath $addArgs"
}
Write-Output 'Approve the Windows Visual Studio Installer prompt to install the C++ compiler, Windows SDK and .NET Framework SDK.'
$process = Start-Process -FilePath $file -ArgumentList $arguments -Verb RunAs -Wait -PassThru
Write-Output ('InstallerExitCode=' + $process.ExitCode)
exit $process.ExitCode
