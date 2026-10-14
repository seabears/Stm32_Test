param(
    [ValidateSet("all", "clean", "flash", "reset", "halt")]
    [string]$Target = "all",
    [string]$CubeIdeRoot = "C:/ST/STM32CubeIDE_1.15.0/STM32CubeIDE"
)
$ErrorActionPreference = "Stop"
# CubeIDE의 GNU make/GCC를 찾는다. 다른 설치 경로는 매개변수로 지정한다.
$plugins = Join-Path $CubeIdeRoot "plugins"
$makeTool = Get-ChildItem "$plugins/com.st.stm32cube.ide.mcu.externaltools.make.win32_*/tools/bin/make.exe" | Select-Object -First 1
$gccTool = Get-ChildItem "$plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin/arm-none-eabi-gcc.exe" | Select-Object -First 1
if (!$makeTool -or !$gccTool) { throw "CubeIDE make/GCC not found. Specify -CubeIdeRoot." }
$previousPath = $env:PATH
Push-Location (Join-Path $PSScriptRoot "..")
try {
    $env:PATH = "$($makeTool.DirectoryName);$previousPath"
    $toolchain = $gccTool.DirectoryName.Replace("\", "/")
    & $makeTool.FullName -j4 $Target "TOOLCHAIN_BIN=$toolchain"
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
} finally {
    $env:PATH = $previousPath
    Pop-Location
}
