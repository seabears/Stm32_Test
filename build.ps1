param(
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$MakeArgs
)

$ErrorActionPreference = "Stop"

if ($MakeArgs.Count -eq 0)
{
  $MakeArgs = @("-j12", "all")
}

$cubeRoot = "C:\ST"
$gcc = Get-ChildItem -Path $cubeRoot -Filter arm-none-eabi-gcc.exe -File -Recurse |
  Sort-Object FullName -Descending |
  Select-Object -First 1
$make = Get-ChildItem -Path $cubeRoot -Filter make.exe -File -Recurse |
  Sort-Object FullName -Descending |
  Select-Object -First 1
$openocd = Get-ChildItem -Path $cubeRoot -Filter openocd.exe -File -Recurse |
  Sort-Object FullName -Descending |
  Select-Object -First 1
$openocdTarget = Get-ChildItem -Path $cubeRoot -Filter stm32f4x.cfg -File -Recurse |
  Where-Object FullName -Like "*openocd*st_scripts\target\stm32f4x.cfg" |
  Sort-Object FullName -Descending |
  Select-Object -First 1

if ($null -eq $gcc -or $null -eq $make)
{
  throw "STM32CubeIDE GNU Arm toolchain or Make was not found below C:\ST."
}

$makeBin = $make.DirectoryName
$toolchainBin = $gcc.DirectoryName -replace "\\", "/"
$env:PATH = "$makeBin;$env:PATH"

$overrides = @("TOOLCHAIN_BIN=$toolchainBin")
if ($null -ne $openocd)
{
  $overrides += "OPENOCD=$($openocd.FullName -replace '\\', '/')"
}
if ($null -ne $openocdTarget)
{
  $scripts = Split-Path -Parent (Split-Path -Parent $openocdTarget.FullName)
  $overrides += "OPENOCD_SCRIPTS=$($scripts -replace '\\', '/')"
}

& $make.FullName @MakeArgs @overrides
exit $LASTEXITCODE
