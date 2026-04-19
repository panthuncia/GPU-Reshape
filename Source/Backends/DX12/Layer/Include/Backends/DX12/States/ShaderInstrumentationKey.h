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

#pragma once

// Layer
#include "RootRegisterBindingInfo.h"
#include "RootSignaturePhysicalMapping.h"

// Std
#include <cstring>
#include <cstdint>
#include <tuple>

struct ShaderLocalInstrumentationKey {
    /// Name for which this key applies
    const char* mangledName{nullptr};
    
    /// Local root signature mapping
    RootSignaturePhysicalMapping* localPhysicalMapping{nullptr};
};

struct ShaderInstrumentationKey {
    static uint64_t GetPhysicalMappingHash(const RootSignaturePhysicalMapping* mapping) {
        return mapping ? mapping->signatureHash : 0ull;
    }

    static auto BindingInfoAsTuple(const RootRegisterBindingInfo& bindingInfo) {
        return std::make_tuple(
            bindingInfo.global.space,
            bindingInfo.global.shaderExportBaseRegister,
            bindingInfo.global.shaderExportCount,
            bindingInfo.global.resourcePRMTBaseRegister,
            bindingInfo.global.samplerPRMTBaseRegister,
            bindingInfo.global.shaderDataConstantRegister,
            bindingInfo.global.descriptorConstantBaseRegister,
            bindingInfo.global.eventConstantBaseRegister,
            bindingInfo.global.shaderResourceBaseRegister,
            bindingInfo.global.shaderResourceCount,
            bindingInfo.bindings.space,
            bindingInfo.bindings.shaderBindingResourceBaseRegister,
            bindingInfo.bindings.shaderBindingResourceCount,
            bindingInfo.local.space,
            bindingInfo.local.descriptorConstantBaseRegister
        );
    }

    static int CompareLocalKeyStrings(const char* lhs, const char* rhs) {
        if (lhs == rhs) {
            return 0;
        }

        if (!lhs) {
            return -1;
        }

        if (!rhs) {
            return 1;
        }

        const int result = std::strcmp(lhs, rhs);
        if (result < 0) {
            return -1;
        }

        if (result > 0) {
            return 1;
        }

        return 0;
    }

    auto AsTuple() const {
        return std::make_tuple(
            featureBitSet,
            combinedHash,
            GetPhysicalMappingHash(physicalMapping),
            BindingInfoAsTuple(bindingInfo),
            localKeyCount
        );
    }

    bool operator<(const ShaderInstrumentationKey& key) const {
        const auto lhsTuple = AsTuple();
        const auto rhsTuple = key.AsTuple();
        if (lhsTuple != rhsTuple) {
            return lhsTuple < rhsTuple;
        }

        for (uint32_t index = 0; index < localKeyCount; ++index) {
            const ShaderLocalInstrumentationKey& lhsLocalKey = localKeys[index];
            const ShaderLocalInstrumentationKey& rhsLocalKey = key.localKeys[index];

            const int nameOrder = CompareLocalKeyStrings(lhsLocalKey.mangledName, rhsLocalKey.mangledName);
            if (nameOrder != 0) {
                return nameOrder < 0;
            }

            const uint64_t lhsLocalHash = GetPhysicalMappingHash(lhsLocalKey.localPhysicalMapping);
            const uint64_t rhsLocalHash = GetPhysicalMappingHash(rhsLocalKey.localPhysicalMapping);
            if (lhsLocalHash != rhsLocalHash) {
                return lhsLocalHash < rhsLocalHash;
            }
        }

        return false;
    }

    /// Feature bit set
    uint64_t featureBitSet{0};

    /// Final hash, includes stream data and physical mappings
    uint64_t combinedHash{0};

    /// Root signature mapping
    RootSignaturePhysicalMapping* physicalMapping{nullptr};

    /// Optional, local keys for libraries
    ShaderLocalInstrumentationKey* localKeys{nullptr};

    /// Number of local keys
    uint32_t localKeyCount = 0;

    /// Signature root binding info
    RootRegisterBindingInfo bindingInfo{};
};
