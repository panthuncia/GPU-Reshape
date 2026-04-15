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
setlocal EnableExtensions EnableDelayedExpansion

set "VS_BUILD_DIR="
set "BUILD_DIR="
set "EXPECT_BUILD_DIR="

cmake --help | findstr /C:"Visual Studio 18 2026" >NUL
if not errorlevel 1 (
    set "VS_BUILD_DIR=cmake-build-vs2026"
) else (
    cmake --help | findstr /C:"Visual Studio 17 2022" >NUL
    if not errorlevel 1 (
        set "VS_BUILD_DIR=cmake-build-vs2022"
    ) else (
        echo Failed!
        echo.
        echo Could not find a supported Visual Studio generator in CMake.
        exit /b 1
    )
)

set "BUILD_DIR=%VS_BUILD_DIR%"
for %%I in (%*) do (
    if defined EXPECT_BUILD_DIR (
        set "BUILD_DIR=%%~I"
        set "EXPECT_BUILD_DIR="
    ) else (
        set "ARG_TOKEN=%%~I"
        if /I "!ARG_TOKEN!"=="-B" (
            set "EXPECT_BUILD_DIR=1"
        ) else if /I "!ARG_TOKEN:~0,2!"=="-B" (
            set "BUILD_DIR=!ARG_TOKEN:~2!"
        )
    )
)

echo GPU Reshape - Full build
echo:
echo Configuring solution...
call "%~dp0VisualStudio2022.bat" %*
if errorlevel 1 (
    exit /b %ERRORLEVEL%
)

echo Building Debug x64...
cmake --build "!BUILD_DIR!" --config Debug --parallel
if errorlevel 1 (
    echo Failed!
    exit /b %ERRORLEVEL%
)

call "%~dp0BuildUIX.bat" Debug "!BUILD_DIR!"
if errorlevel 1 (
    echo Failed!
    exit /b %ERRORLEVEL%
)

echo:
echo Full Debug x64 build completed.
endlocal