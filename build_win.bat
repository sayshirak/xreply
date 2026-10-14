@echo off
rem Build xreply (Release x64) with VS2026 + Qt 6.10.3 MSVC + CEF 154
rem Output: D:\program\xreply\xreply

call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

set "PATH=D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
if exist "D:\program\xfollowing\tools\cmake\bin\cmake.exe" set "PATH=D:\program\xfollowing\tools\cmake\bin;%PATH%"
if exist "D:\program\xreply\tools\cmake\bin\cmake.exe" set "PATH=D:\program\xreply\tools\cmake\bin;%PATH%"

cd /d D:\program\xreply

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build build --target xreply
if errorlevel 1 exit /b 1

set "OUT=D:\program\xreply\xreply"
set "SYS32=%SystemRoot%\System32"
for %%F in (
  msvcp140.dll msvcp140_1.dll msvcp140_2.dll msvcp140_atomic_wait.dll msvcp140_codecvt_ids.dll
  vcruntime140.dll vcruntime140_1.dll concrt140.dll vccorlib140.dll
) do (
  if exist "%SYS32%\%%F" copy /Y "%SYS32%\%%F" "%OUT%\" >nul
)

echo XREPLY_BUILD_OK
