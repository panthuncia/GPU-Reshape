:: 
:: The MIT License (MIT)
:: 
:: Copyright (c) 2024 Advanced Micro Devices, Inc.,
:: Fatalist Development AB (Avalanche Studio Group),
:: and Miguel Petersen.
:: 
:: All Rights Reserved.
:: 
:: Permission is hereby granted, free of charge, to any person obtaining a copy 
:: of this software and associated documentation files (the "Software"), to deal 
:: in the Software without restriction, including without limitation the rights 
:: to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
:: of the Software, and to permit persons to whom the Software is furnished to do so, 
:: subject to the following conditions:
:: 
:: The above copyright notice and this permission notice shall be included in all 
:: copies or substantial portions of the Software.
:: 
:: THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
:: INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
:: PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
:: FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
:: ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
:: 

@echo off
setlocal EnableExtensions

set "BUILD_CONFIG=%~1"
if "%BUILD_CONFIG%"=="" set "BUILD_CONFIG=Debug"

set "BUILD_DIR=%~2"
if "%BUILD_DIR%"=="" set "BUILD_DIR=cmake-build-vs2026"

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo Failed!
    echo.
    echo Could not find "%BUILD_DIR%\CMakeCache.txt".
    exit /b 1
)

findstr /C:"ENABLE_UIX:BOOL=ON" "%BUILD_DIR%\CMakeCache.txt" >NUL
if errorlevel 1 (
    echo UIX is disabled in %BUILD_DIR%. Skipping UIX .NET build pass.
    exit /b 0
)

echo Building UIX .NET projects for %BUILD_CONFIG%^|x64...

call :BuildProject "Source\UIX\Runtime\Runtime.csproj"
if errorlevel 1 exit /b 1

call :BuildProject "Source\Features\Concurrency\Frontend\UIX\UIX.csproj"
if errorlevel 1 exit /b 1
call :BuildProject "Source\Features\Debug\Frontend\UIX\UIX.csproj"
if errorlevel 1 exit /b 1
call :BuildProject "Source\Features\Descriptor\Frontend\UIX\UIX.csproj"
if errorlevel 1 exit /b 1
call :BuildProject "Source\Features\DeviceCommands\Frontend\UIX\UIX.csproj"
if errorlevel 1 exit /b 1
call :BuildProject "Source\Features\ExportStability\Frontend\UIX\UIX.csproj"
if errorlevel 1 exit /b 1
call :BuildProject "Source\Features\Initialization\Frontend\UIX\UIX.csproj"
if errorlevel 1 exit /b 1
call :BuildProject "Source\Features\Loop\Frontend\UIX\UIX.csproj"
if errorlevel 1 exit /b 1
call :BuildProject "Source\Features\ResourceBounds\Frontend\UIX\UIX.csproj"
if errorlevel 1 exit /b 1
call :BuildProject "Source\Features\Waterfall\Frontend\UIX\UIX.csproj"
if errorlevel 1 exit /b 1

call :BuildProject "Source\Services\Discovery\DotNet\NotifyIcon\NotifyIcon.csproj"
if errorlevel 1 exit /b 1
call :BuildProject "Source\UIX\Studio\Studio.csproj"
if errorlevel 1 exit /b 1

echo UIX .NET build completed.
exit /b 0

:BuildProject
echo   - %~1
dotnet build "%~1" -c %BUILD_CONFIG% -p:Platform=x64 -nologo -v:minimal
exit /b %ERRORLEVEL%