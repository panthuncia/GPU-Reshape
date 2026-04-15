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

# Visual studio solution generation patchup
#   See https://gitlab.kitware.com/cmake/cmake/-/issues/23513

# Expected path
set(SolutionRoot "${CMAKE_ARGV3}")

if (EXISTS "${SolutionRoot}/GPU-Reshape.sln")
	set(CSProjPath "${SolutionRoot}/GPU-Reshape.sln")
elseif(EXISTS "${SolutionRoot}/GPU-Reshape.slnx")
	set(CSProjPath "${SolutionRoot}/GPU-Reshape.slnx")
else()
	message(FATAL_ERROR "Could not locate GPU-Reshape.sln or GPU-Reshape.slnx in '${SolutionRoot}'")
endif()

# Replace all instances of "Any CPU" with "x64"
file(READ "${CSProjPath}" SolutionContents)
string(REPLACE "Any CPU" "x64" FILE_CONTENTS "${SolutionContents}")
file(WRITE "${CSProjPath}" "${FILE_CONTENTS}")

# Prevent the generated ALL_BUILD target from racing external C# projects.
# These projects are built explicitly and sequentially by BuildUIX.bat.
set(AllBuildPath "${SolutionRoot}/ALL_BUILD.vcxproj")
if (EXISTS "${AllBuildPath}")
	file(STRINGS "${AllBuildPath}" AllBuildLines)
	set(PatchedAllBuildContents "")
	set(SkipProjectReference OFF)

	foreach(Line IN LISTS AllBuildLines)
		if (SkipProjectReference)
			if (Line MATCHES "^[ \t]*</ProjectReference>")
				set(SkipProjectReference OFF)
			endif()
		elseif(Line MATCHES "^[ \t]*<ProjectReference Include=\"[^\"]+\\.csproj\">")
			set(SkipProjectReference ON)
		else()
			string(APPEND PatchedAllBuildContents "${Line}\r\n")
		endif()
	endforeach()

	file(WRITE "${AllBuildPath}" "${PatchedAllBuildContents}")
endif()
