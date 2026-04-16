# 
# The MIT License (MIT)
# 
# Copyright (c) 2024 Advanced Micro Devices, Inc.,
# Fatalist Development AB (Avalanche Studio Group),
# and Miguel Petersen.
# 
# All Rights Reserved.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy 
# of this software and associated documentation files (the "Software"), to deal 
# in the Software without restriction, including without limitation the rights 
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
# of the Software, and to permit persons to whom the Software is furnished to do so, 
# subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all 
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
# PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
# FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# 


# Add external projects
include_external_msproject(UIX.Runtime ${CMAKE_CURRENT_SOURCE_DIR}/Runtime/Runtime.csproj)
include_external_msproject(UIX.Studio ${CMAKE_CURRENT_SOURCE_DIR}/Studio/Studio.csproj)

# IDE discovery
set_target_properties(UIX.Runtime PROPERTIES FOLDER Source/UIX/Runtime)
set_target_properties(UIX.Studio PROPERTIES FOLDER Source/UIX/Studio)

# Add runtime dependencies
add_dependencies(
    UIX.Runtime
    GRS.Libraries.Bridge.DotNet
    GRS.Libraries.Message.DotNet
    GRS.Services.HostResolver.DotNet
    GRS.Services.Discovery.DotNet
    GRS.Libraries.Bridge.Schema.DotNet
    GRS.Libraries.Backend.Schema.DotNet
)

# Add studio dependencies
add_dependencies(
    UIX.Studio
    UIX.Runtime
    GRS.Services.Discovery.NotifyIcon
)

# Visual Studio does not reliably build the external UIX .csproj entries as part of
# the generated solution build, so wire the supported UIX batch pass into ALL.
add_custom_target(
    UIX.Build ALL
    COMMAND "${CMAKE_SOURCE_DIR}/BuildUIX.bat" $<CONFIG> "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
)

set_target_properties(UIX.Build PROPERTIES FOLDER Source/UIX)

add_dependencies(
    UIX.Build
    GRS.Features.Concurrency.Backend.DotNet
    GRS.Features.Debug.Backend.DotNet
    GRS.Features.Descriptor.Backend.DotNet
    GRS.Features.DeviceCommands.Backend.DotNet
    GRS.Features.ExportStability.Backend.DotNet
    GRS.Features.Initialization.Backend.DotNet
    GRS.Features.Loop.Backend.DotNet
    GRS.Features.ResourceBounds.Backend.DotNet
    GRS.Features.Waterfall.Backend.DotNet
    GRS.Libraries.Backend.Schema.DotNet
    GRS.Libraries.Bridge.DotNet
    GRS.Libraries.Bridge.Schema.DotNet
    GRS.Libraries.Message.DotNet
    GRS.Services.Discovery.DotNet
    GRS.Services.HostResolver.DotNet
)
