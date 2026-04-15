// 
// The MIT License (MIT)
// 
// Copyright (c) 2024 Advanced Micro Devices, Inc.,
// Fatalist Development AB (Avalanche Studio Group),
// and Miguel Petersen.
// 
// All Rights Reserved.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal 
// in the Software without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
// of the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 

#include "GenTypes.h"
#include "Types.h"
#include "Name.h"

// Common
#include <Common/String.h>

// Std
#include <sstream>
#include <regex>
#include <unordered_set>
#include <string_view>

namespace {
    struct ParameterInfo {
        std::string name;
        std::string info;
    };

    static const std::unordered_set<std::string_view> kInbuiltIntrinsicNames {
        "DxOpRawBufferStoreI16"
    };

    static std::string_view TrimView(std::string_view view) {
        const size_t begin = view.find_first_not_of(" \t\r\n");
        if (begin == std::string_view::npos) {
            return {};
        }

        const size_t end = view.find_last_not_of(" \t\r\n");
        return view.substr(begin, end - begin + 1);
    }

    static std::string GetPrecedingCommentBlock(std::string_view text, size_t declarationOffset) {
        std::string block;
        size_t cursor = declarationOffset;
        bool foundComment = false;

        while (cursor > 0) {
            size_t lineStart = text.rfind('\n', cursor - 1);
            if (lineStart == std::string_view::npos) {
                lineStart = 0;
            } else {
                lineStart += 1;
            }

            const std::string_view line = TrimView(text.substr(lineStart, cursor - lineStart));

            if (line.empty()) {
                if (foundComment) {
                    break;
                }
            } else if (!line.starts_with(';')) {
                break;
            } else {
                foundComment = true;

                if (!block.empty()) {
                    block.insert(0, "\n");
                }

                block.insert(0, line.data(), line.size());
            }

            if (lineStart == 0) {
                break;
            }

            cursor = lineStart - 1;
        }

        return block;
    }
}

/// Translate a given RST type into spec type
/// \param type RST type
/// \return empty if failed
static std::string TranslateType(const std::string_view& type, std::string_view prefix = "") {
    if (std::starts_with(prefix, "DXILIntrinsicTypeSpec::ResRet")) {
        return "DXILIntrinsicTypeSpec::ResRet" + std::uppercase(std::string(type));
    }

    if (std::starts_with(prefix, "DXILIntrinsicTypeSpec::CBufRet")) {
        return "DXILIntrinsicTypeSpec::CBufRetI32" + std::uppercase(std::string(type));
    }

    if (prefix.empty()) {
        prefix = "DXILIntrinsicTypeSpec::";
    }
    
    if (type == "void") {
        return std::string(prefix) + "Void";
    } else if (type == "i64") {
        return std::string(prefix) + "I64";
    } else if (type == "i16") {
        return std::string(prefix) + "I16";
    } else if (type == "i32") {
        return std::string(prefix) + "I32";
    } else if (type == "f64") {
        return std::string(prefix) + "F64";
    } else if (type == "float" || type == "f32") {
        return std::string(prefix) + "F32";
    } else if (type == "f16") {
        return std::string(prefix) + "F16";
    } else if (type == "i8") {
        return std::string(prefix) + "I8";
    } else if (type == "i1") {
        return std::string(prefix) + "I1";
    } else if (type == "%dx.types.Handle") {
        return std::string(prefix) + "Handle";
    } else if (type == "%dx.types.ResHandle") {
        return std::string(prefix) + "Handle";
    } else if (type == "%dx.types.SamplerHandle") {
        return std::string(prefix) + "Handle";
    } else if (type == "%dx.types.Dimensions") {
        return std::string(prefix) + "Dimensions";
    } else if (type == "%dx.types.ResRet.f32") {
        return std::string(prefix) + "ResRetF32";
    } else if (type == "%dx.types.ResRet.f16") {
        return std::string(prefix) + "ResRetF16";
    } else if (type == "%dx.types.ResRet.i32") {
        return std::string(prefix) + "ResRetI32";
    } else if (type == "%dx.types.ResRet.i16") {
        return std::string(prefix) + "ResRetI16";
    } else if (type == "%dx.types.CBufRet.f32") {
        return std::string(prefix) + "CBufRetF32";
    } else if (type == "%dx.types.CBufRet.i32") {
        return std::string(prefix) + "CBufRetI32";
    } else if (type == "%dx.types.ResBind") {
        return std::string(prefix) + "ResBind";
    }

    // Unknown
    return "";
}

bool Generators::DXILIntrinsics(const GeneratorInfo &info, TemplateEngine &templateEngine) {
    std::stringstream intrinsics;
    std::unordered_set<std::string> emittedNames;

    // Current UID
    uint32_t uid = 0;

    // Regex patterns
    std::regex declarePattern("declare (%?[A-Za-z.0-9]+) @([A-Za-z.0-9]+)\\(");
    std::regex overloadPattern("(f64|f32|f16|i64|i32|i16|i8|i1)");

    // For all declarations
    for(std::sregex_iterator m = std::sregex_iterator(info.dxilRST.begin(), info.dxilRST.end(), declarePattern); m != std::sregex_iterator(); m++) {
        // Any error?
        bool wasError = false;

        // Overload status
        std::string overloadStr = GetPrecedingCommentBlock(info.dxilRST, static_cast<size_t>(m->position(0)));

        // All overloads
        std::vector<std::string> overloads;

        // For all overloads
        for(std::sregex_iterator p = std::sregex_iterator(overloadStr.begin(), overloadStr.end(), overloadPattern); p != std::sregex_iterator(); p++) {
            std::string overload = (*p)[1].str();

            // Ignore duplicates
            if (std::find(overloads.begin(), overloads.end(), overload) != overloads.end()) {
                continue;
            }

            // Add overload
            overloads.push_back(overload);
        }

        // Translate return type
        std::string returnType = TranslateType((*m)[1].str());
        if (returnType.empty()) {
            continue;
        }

        // Dummy overload
        if (overloads.empty()) {
            overloads.push_back("");
        }

        // Push all overloads
        for (const std::string& overload : overloads) {
            // Get name
            std::string name;
            if (overload == "") {
                name = (*m)[2].str();
            } else {
                name = (*m)[2].str();

                // Remove default overload
                name.erase(name.begin() + name.find_last_of('.') + 1, name.end());

                // Add overload
                name += overload;
            }

            // Convert key wise name
            std::string keyName;
            for (size_t i = 0; i < name.length(); i++) {
                if (name[i] == '.') {
                    continue;
                }

                // Capitalize first character and after '.'
                keyName.push_back((i == 0 || name[i - 1] == '.') ? std::toupper(name[i]) : name[i]);
            }

            // Optional overload type
            std::string overloadType;

            // Translate overload
            if (!overload.empty()) {
                size_t start = 0;

                if (returnType != "DXILIntrinsicTypeSpec::Void") {
                    start = returnType.size() - 1;

                    while (isdigit(returnType[start])) {
                        start--;
                    }
                }

                // Translate overload type
                overloadType = TranslateType(overload, returnType.substr(0, start));
                if (overloadType.empty()) {
                    continue;
                }
            }

            // Override return type if need be
            std::string candidateType = returnType;
            if (!overload.empty() && returnType != "DXILIntrinsicTypeSpec::Void") {
                candidateType = overloadType;
            }

            // All parameters
            std::vector<ParameterInfo> parameters;

            // For all parameters
            size_t parameterOffset = static_cast<size_t>(m->position(0) + m->length(0));
            const size_t firstLineEnd = info.dxilRST.find('\n', parameterOffset);
            const size_t firstLineLength = (firstLineEnd == std::string::npos ? info.dxilRST.length() : firstLineEnd) - parameterOffset;
            const std::string_view firstLine = TrimView(std::string_view(info.dxilRST).substr(parameterOffset, firstLineLength));

            // Skip shorthand example declarations such as declare foo(...)
            if (firstLine.starts_with("...")) {
                continue;
            }

            while (parameterOffset < info.dxilRST.length()) {
                size_t lineEnd = info.dxilRST.find('\n', parameterOffset);
                if (lineEnd == std::string::npos) {
                    lineEnd = info.dxilRST.length();
                }

                std::string_view line = TrimView(std::string_view(info.dxilRST).substr(parameterOffset, lineEnd - parameterOffset));
                parameterOffset = lineEnd + 1;

                if (line.empty()) {
                    continue;
                }

                const size_t terminalPos = line.find_first_of(",)");
                if (terminalPos == std::string_view::npos) {
                    break;
                }

                std::string paramType = TranslateType(TrimView(line.substr(0, terminalPos)));
                if (paramType.empty()) {
                    wasError = true;
                    break;
                }

                // Get info
                std::string paramInfo;
                const size_t commentPos = line.find(';');
                if (commentPos != std::string_view::npos) {
                    paramInfo = std::string(TrimView(line.substr(commentPos + 1)));
                }

                // Extremely crude overload deduction
                if (paramInfo.find("value") != std::string::npos) {
                    paramType = overloadType;
                }

                // Add parameter
                parameters.push_back(ParameterInfo {
                    .name = paramType,
                    .info = paramInfo
                });

                // End of intrinsic?
                if (line[terminalPos] == ')') {
                    break;
                }
            }

            // Failed parsing?
            if (wasError) {
                continue;
            }

            // Skip intrinsics that are already maintained in the inbuilt header.
            if (kInbuiltIntrinsicNames.contains(keyName)) {
                continue;
            }

            // DXIL.rst contains both examples and formal signatures. Skip duplicate declarations.
            if (!emittedNames.insert(keyName).second) {
                continue;
            }

            // Emit intrinsic spec
            intrinsics << "\tstatic DXILIntrinsicSpec " << keyName << " {\n";
            intrinsics << "\t\t.uid = kInbuiltCount + " << (uid++) << ",\n";
            intrinsics << "\t\t.name = \"" << name << "\",\n";
            intrinsics << "\t\t.returnType = " << candidateType << ",\n";
            intrinsics << "\t\t.parameterTypes = {\n";

            // Emit parameters
            for (const ParameterInfo& param : parameters) {
                intrinsics << "\t\t\t" << param.name << ", // " << param.info << "\n";
            }

            // Close
            intrinsics << "\t\t}\n";
            intrinsics << "\t};\n\n";
        }
    }

    // Substitute keys
    templateEngine.Substitute("$INTRINSICS", intrinsics.str().c_str());

    // OK
    return true;
}
