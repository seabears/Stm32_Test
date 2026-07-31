param(
  [string]$DfuUtil = "C:/Users/haewoong/AppData/Local/Arduino15/packages/arduino/tools/dfu-util/0.11.0-arduino5/dfu-util.exe",
  [string]$Bin = "Debug/stm32f103c8t6.bin",
  [string]$Device = "1EAF:0003",
  [int]$Alt = 0,
  [string]$Address = "0x08002000"
)

$ErrorActionPreference = "Continue"

Write-Host "Waiting for USB DFU device $Device ..."
Write-Host "Press and release RESET on the board now."
Write-Host "Uploading $Bin to $Address when DFU appears ..."
Write-Host "Progress will be shown below. Erase and download can take about a minute."

$args = @("-w", "-d", $Device, "-a", "$Alt", "-s", $Address, "-D", $Bin)
& $DfuUtil @args 2>&1 | Tee-Object -Variable uploadLines
$exitCode = $LASTEXITCODE
$upload = $uploadLines -join [Environment]::NewLine
$success = $upload -match "File downloaded successfully"

if ($success) {
  Write-Host "USB DFU upload finished successfully."
  exit 0
}

if ($upload -match "LIBUSB_ERROR_NOT_SUPPORTED") {
  Write-Error "DFU device was found, but the Windows driver is not WinUSB/libusb compatible. Install the driver with Zadig."
  exit 2
}

if ($exitCode -ne 0) {
  Write-Error "USB DFU upload failed with exit code $exitCode."
}

exit $exitCode
