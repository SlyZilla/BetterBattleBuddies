# Independent BBB acceptance runner. Loads BBB + base game, never ACA.
[CmdletBinding()]
param(
 [ValidateSet('vanilla','rearm','look','firemaneuverperf','compile')][string]$Preset = 'vanilla',
 [string]$AddonRoot,
 [string]$ServerPath
)
$ErrorActionPreference = 'Stop'
if (!$AddonRoot) { $AddonRoot = Split-Path $PSScriptRoot -Parent }
$AddonRoot = [IO.Path]::GetFullPath($AddonRoot)
$project = Join-Path $AddonRoot 'addon.gproj'
if (!(Test-Path -LiteralPath $project) -or (Get-Content -LiteralPath $project -Raw) -notmatch 'GUID "BBB0261015000001"') { throw 'Not a BBB addon root.' }
$serverWait = 0
while ((Get-Process ArmaReforgerServer -ErrorAction SilentlyContinue) -and $serverWait -lt 300) {
 Start-Sleep -Milliseconds 400
 $serverWait++
}
if (Get-Process ArmaReforgerServer -ErrorAction SilentlyContinue) { throw 'Another server holds port 2001; let it finish first.' }
if (!$ServerPath) {
 $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
 $libraries = @($steam, 'E:\SteamLibrary', 'C:\Program Files (x86)\Steam')
 if ($steam) {
  $vdf = Join-Path $steam 'steamapps/libraryfolders.vdf'
  if (Test-Path -LiteralPath $vdf) {
   foreach ($match in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"')) { $libraries += $match.Groups[1].Value.Replace('\\', '\') }
  }
 }
 foreach ($library in $libraries) {
  if (!$library) { continue }
  $candidate = Join-Path $library 'steamapps/common/Arma Reforger Server/ArmaReforgerServer.exe'
  if (Test-Path -LiteralPath $candidate) { $ServerPath = $candidate; break }
 }
}
if (!$ServerPath -or !(Test-Path -LiteralPath $ServerPath)) { throw 'Install Arma Reforger Server or supply -ServerPath.' }
$root = Split-Path $PSScriptRoot -Parent
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + $Preset
$profile = Join-Path $root ('.local-testserver/' + $id)
$resultDir = Join-Path $root '.bbb-results'
New-Item -ItemType Directory -Path $profile,$resultDir -Force | Out-Null
$flags = @{ vanilla = '-bbbVanillaTest 1'; rearm = '-bbbRearmTest 1'; look = '-acrBbbLookTest 1'; firemaneuverperf = '-acrBbbFireManeuverPerfTest 1'; compile = '' }
$timeouts = @{ vanilla = 65; rearm = 65; look = 90; firemaneuverperf = 160; compile = 45 }
$world = '{AC230926A00100E0}Worlds/BBB_TestArland.ent'
if ($Preset -eq 'compile') { $world = '{77C41EA429B6AC35}worlds/GameMaster/GM_Arland.ent' }
$argsLine = '-server "' + $world + '" -addonsDir "' + (Split-Path $AddonRoot -Parent) + '" -addons BBB0261015000001 -profile "' + $profile + '" -maxFPS 60 -noBackend ' + $flags[$Preset]
$process = Start-Process -FilePath $ServerPath -ArgumentList $argsLine -WorkingDirectory (Split-Path $ServerPath -Parent) -WindowStyle Hidden -PassThru
$started = Get-Date
$status = 'timeout'
$log = $null
$body = ''
try {
 while (((Get-Date) - $started).TotalSeconds -lt $timeouts[$Preset]) {
  Start-Sleep -Seconds 2
  if (!$log) { $log = Get-ChildItem -LiteralPath $profile -Filter console.log -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 }
  if ($log) {
   $body = Get-Content -LiteralPath $log.FullName -Raw
   if ($body -match "SCRIPT\s+\(E\)|Can't compile|Unable to start replication") { $status = 'error'; break }
   if ($Preset -eq 'compile' -and $body -match '\[BBB\] started on its own') { $status = 'completed'; break }
   if ($body -match '\[ACR\] TEST COMPLETE') {
    $status = 'failed'
    if ($body -match '\[ACR\] VERDICT PASS\b' -and $body -notmatch '\[ACR\] VERDICT FAIL\b') { $status = 'completed' }
    break
   }
  }
  if ($process.HasExited) { $status = 'server exited'; break }
 }
} finally {
 if (!$process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
}
$result = Join-Path $resultDir ($id + '.result.txt')
$sha = 'uncommitted extraction'
if (Test-Path -LiteralPath (Join-Path $root '.git/refs/heads/main')) { $sha = & git -C $root rev-parse HEAD }
@("status=$status", "preset=$Preset", "source=$sha", "addonRoot=$AddonRoot", "args=$argsLine", "log=$log", $body) | Set-Content -LiteralPath $result -Encoding UTF8
Write-Output "BBB $Preset : $status; result=$result"
if ($status -ne 'completed') { exit 1 }
