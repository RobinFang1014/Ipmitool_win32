@echo off
setlocal EnableExtensions

pushd "%~dp0" >nul || (
    echo Failed to enter script directory.
    exit /b 1
)

echo Cleaning local Visual Studio and build-generated files...
echo.

call :RemoveDir ".vs"
call :RemoveDir "bin"
call :RemoveDir "obj"

call :RemoveFile "Resource.aps"
call :RemoveFile "ipmitool-windows.vcxproj.user"

for %%F in (
    "*.aps"
    "*.suo"
    "*.user"
    "*.VC.db"
    "*.opendb"
) do call :RemoveFile "%%~F"

echo.
echo Cleanup complete.
popd >nul
exit /b 0

:RemoveDir
if exist "%~1\" (
    echo Removing directory: %~1
    rmdir /s /q "%~1"
) else (
    echo Skipping missing directory: %~1
)
exit /b 0

:RemoveFile
if exist "%~1" (
    echo Removing file: %~1
    del /f /q "%~1"
) else (
    echo Skipping missing file: %~1
)
exit /b 0
