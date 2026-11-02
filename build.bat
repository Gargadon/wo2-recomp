@echo off
setlocal
cd /d "%~dp0"
for %%I in ("%~dp0..") do set "REXSDK_DIR=%%~fI"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo ERROR: No se encontro vswhere. Instala Visual Studio con C++ y LLVM.
  exit /b 1
)
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_DIR=%%I"
if not defined VS_DIR (
  echo ERROR: No se encontro Visual Studio con herramientas C++.
  exit /b 1
)
call "%VS_DIR%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
set "PATH=%VS_DIR%\VC\Tools\Llvm\x64\bin;%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
where clang++ >nul 2>&1
if errorlevel 1 (
  echo ERROR: Instala las herramientas LLVM de Visual Studio.
  exit /b 1
)
cmake --preset win-amd64-release "-DREXSDK_DIR=%REXSDK_DIR%" -DCMAKE_CXX_FLAGS=-mssse3
if errorlevel 1 exit /b 1
cmake --build --preset win-amd64-release -j 6 %*
exit /b %errorlevel%
