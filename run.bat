@echo off
setlocal
cd /d "%~dp0"
for %%I in ("%~dp0..") do set "REXSDK_DIR=%%~fI"
set "GAME_EXE=%~dp0out\build\win-amd64-release\warriors_orochi_2.exe"
if not exist "%GAME_EXE%" (
  echo ERROR: No se encontro el ejecutable. Ejecuta build.bat primero.
  exit /b 1
)
set "PATH=%REXSDK_DIR%\out\win-amd64;%PATH%"
"%GAME_EXE%" --game_data_root "%REXSDK_DIR%\Warriors Orochi 2 (USA) (En,Fr,De)" --gpu_plugin xenos --no-fullscreen --resolution=720p --video_mode_refresh_rate=60 --vsync --mnk_mode --input_backend=xinput --render_target_path_d3d12=rov %*
exit /b %errorlevel%




