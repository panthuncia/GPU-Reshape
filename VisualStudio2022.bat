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

set "VS_GENERATOR="
set "VS_BUILD_DIR="
set "BUILD_DIR="
set "EXPECT_BUILD_DIR="

cmake --help | findstr /C:"Visual Studio 18 2026" >NUL
if not errorlevel 1 (
    set "VS_GENERATOR=Visual Studio 18 2026"
    set "VS_BUILD_DIR=cmake-build-vs2026"
) else (
    cmake --help | findstr /C:"Visual Studio 17 2022" >NUL
    if not errorlevel 1 (
        set "VS_GENERATOR=Visual Studio 17 2022"
        set "VS_BUILD_DIR=cmake-build-vs2022"
    ) else (
        echo Failed!
        echo.
        echo Could not find a supported Visual Studio generator in CMake.
        echo Expected one of:
        echo   - Visual Studio 18 2026
        echo   - Visual Studio 17 2022
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

rem Info
echo GPU Reshape - Solution generator
echo:
echo For the full set of configuration switches and options, see the build documentation.
echo Using generator: !VS_GENERATOR!
echo Using build directory: !BUILD_DIR!
echo:

rem Print all generation inline in the command window
echo Generating from cmake...
echo ------------------------
echo:

if /I "!BUILD_DIR!"=="!VS_BUILD_DIR!" (
    cmake -G "!VS_GENERATOR!" -A x64 -DCMAKE_CONFIGURATION_TYPES:STRING="Debug;Release;MinSizeRel;RelWithDebInfo" -DINSTALL_THIRD_PARTY:BOOL="1" -B "!BUILD_DIR!" %*
) else (
    cmake -G "!VS_GENERATOR!" -A x64 -DCMAKE_CONFIGURATION_TYPES:STRING="Debug;Release;MinSizeRel;RelWithDebInfo" -DINSTALL_THIRD_PARTY:BOOL="1" %*
)
	
echo:
echo -------------------------

rem Succeeded?
if %ERRORLEVEL% == 0 (
	echo OK!
	echo:
) else (
	echo Failed!
	exit /b 1
)

echo|set /p="Solution patching... "
cmake -P Build/Utils/CSProjPatch.cmake !BUILD_DIR!
echo OK!
endlocal
