param(
  [string]$DfuUtil = "C:/Users/haewoong/AppData/Local/Arduino15/packages/arduino/tools/dfu-util/0.11.0-arduino5/dfu-util.exe",
  [string]$Bin = "Debug/stm32f103c8t6.bin",
  [string]$Device = "1EAF:0003",
  [int]$Alt = 2,
  [int]$TimeoutSeconds = 20
)

$ErrorActionPreference = "Continue"
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
Write-Host "Waiting for USB DFU device $Device ..."
Write-Host "Press and release RESET on the board now."

while ((Get-Date) -lt $deadline) {
  $list = & $DfuUtil -l 2>&1 | Out-String

  if ($list -match "LIBUSB_ERROR_NOT_SUPPORTED") {
    Write-Host ""
    Write-Host "DFU device $Device was found, but Windows driver is not usable by dfu-util."
    Write-Host "Install WinUSB/libusbK driver for the $Device device with Zadig, then run this task again."
    Write-Host "Zadig path example: Options > List All Devices > select Maple DFU / STM32duino DFU / $Device > WinUSB > Replace Driver."
    Write-Host ""
    Write-Host $list
    exit 2
  }

  if ($list -match [regex]::Escape($Device)) {
    Write-Host "DFU device found. Uploading $Bin ..."
    $upload = & $DfuUtil -d $Device -a $Alt -D $Bin 2>&1 | Out-String
    Write-Host $upload
    if ($upload -match "LIBUSB_ERROR_NOT_SUPPORTED") {
      Write-Error "DFU device was found, but the Windows driver is not WinUSB/libusb compatible. Install the driver with Zadig."
      exit 2
    }
    exit $LASTEXITCODE
  }

  Start-Sleep -Milliseconds 250
}

Write-Error "DFU device $Device was not found within $TimeoutSeconds seconds. Try holding RESET, start this task, then release RESET."
exit 1