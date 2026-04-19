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

#include <Backends/DX12/Compiler/ShaderCompiler.h>
#include <Backends/DX12/Compiler/IDXModule.h>
#include <Backends/DX12/States/DeviceState.h>
#include <Backends/DX12/Compiler/DXBC/DXBCModule.h>
#include <Backends/DX12/Compiler/ShaderCompilerDebug.h>
#include <Backends/DX12/Compiler/DXCompileJob.h>
#include <Backends/DX12/Compiler/DXParseJob.h>
#include <Backends/DX12/Compiler/DXStream.h>
#include <Backends/DX12/Compiler/DXIL/DXILSigner.h>
#include <Backends/DX12/Compiler/DXBC/DXBCSigner.h>
#include <Backends/DX12/Compiler/DXBC/DXBCConverter.h>
#include <Backends/DX12/Compiler/Tags.h>
#include <Backends/DX12/ShaderData/ShaderDataHost.h>
#include <Backends/DX12/Symbolizer/ShaderSGUIDHost.h>
#include <Backends/DX12/Compiler/Diagnostic/DiagnosticType.h>
#include <Backends/DX12/Compiler/DXMSCompiler.h>

// Bridge
#include <Bridge/Log/LogSeverity.h>

// Backend
#include <Backend/IFeatureHost.h>
#include <Backend/IFeature.h>
#include <Backend/IShaderFeature.h>
#include <Backend/IShaderExportHost.h>
#include <Backend/Diagnostic/DiagnosticBucketScope.h>

// Common
#include <Common/CRC.h>
#include <Common/FileSystem.h>
#include <Common/Format.h>
#include <Common/Hash.h>
#include <Common/Dispatcher/Dispatcher.h>
#include <Common/Registry.h>

// Std
#include <cstdio>
#include <fstream>

namespace {
    constexpr uint64_t kPersistentShaderArtifactMagic = 0x4843534452475047ull;
    constexpr uint32_t kPersistentShaderArtifactVersion = 2u;

    struct PersistentShaderArtifactKey {
        uint32_t sourceBytecodeHash{0};
        uint64_t featureBitSet{0};
        uint64_t combinedHash{0};
        uint64_t rootSignatureHash{0};
        uint64_t bindingHash{0};
        uint64_t localKeyHash{0};
    };

    struct PersistentShaderArtifactHeader {
        uint64_t magic{kPersistentShaderArtifactMagic};
        uint32_t version{kPersistentShaderArtifactVersion};
        uint32_t streamByteSize{0};
        uint32_t mappingCount{0};
        PersistentShaderArtifactKey key{};
        uint8_t executionInfo{0};
        uint8_t reserved[3]{};
    };

    static bool operator==(const PersistentShaderArtifactKey& lhs, const PersistentShaderArtifactKey& rhs) {
        return lhs.sourceBytecodeHash == rhs.sourceBytecodeHash &&
               lhs.featureBitSet == rhs.featureBitSet &&
               lhs.combinedHash == rhs.combinedHash &&
               lhs.rootSignatureHash == rhs.rootSignatureHash &&
               lhs.bindingHash == rhs.bindingHash &&
               lhs.localKeyHash == rhs.localKeyHash;
    }

    static uint64_t HashRootRegisterBindingInfo(const RootRegisterBindingInfo& bindingInfo) {
        uint64_t hash = 0;

        CombineHash(hash, bindingInfo.global.space);
        CombineHash(hash, bindingInfo.global.shaderExportBaseRegister);
        CombineHash(hash, bindingInfo.global.shaderExportCount);
        CombineHash(hash, bindingInfo.global.resourcePRMTBaseRegister);
        CombineHash(hash, bindingInfo.global.samplerPRMTBaseRegister);
        CombineHash(hash, bindingInfo.global.shaderDataConstantRegister);
        CombineHash(hash, bindingInfo.global.descriptorConstantBaseRegister);
        CombineHash(hash, bindingInfo.global.eventConstantBaseRegister);
        CombineHash(hash, bindingInfo.global.shaderResourceBaseRegister);
        CombineHash(hash, bindingInfo.global.shaderResourceCount);

        CombineHash(hash, bindingInfo.bindings.space);
        CombineHash(hash, bindingInfo.bindings.shaderBindingResourceBaseRegister);
        CombineHash(hash, bindingInfo.bindings.shaderBindingResourceCount);

        CombineHash(hash, bindingInfo.local.space);
        CombineHash(hash, bindingInfo.local.descriptorConstantBaseRegister);

        return hash;
    }

    static uint64_t HashLocalInstrumentationKeys(const ShaderInstrumentationKey& instrumentationKey) {
        uint64_t hash = 0;
        CombineHash(hash, instrumentationKey.localKeyCount);

        for (uint32_t index = 0; index < instrumentationKey.localKeyCount; ++index) {
            const ShaderLocalInstrumentationKey& localKey = instrumentationKey.localKeys[index];
            CombineHash(hash, localKey.mangledName ? StringCRC32Short(localKey.mangledName) : 0u);
            CombineHash(hash, localKey.localPhysicalMapping ? localKey.localPhysicalMapping->signatureHash : 0ull);
        }

        return hash;
    }

    static PersistentShaderArtifactKey BuildPersistentShaderArtifactKey(const ShaderJob& job) {
        PersistentShaderArtifactKey key{};
        key.sourceBytecodeHash = BufferCRC32Short(job.state->byteCode.pShaderBytecode, job.state->byteCode.BytecodeLength);
        key.featureBitSet = job.instrumentationKey.featureBitSet;
        key.combinedHash = job.instrumentationKey.combinedHash;
        key.rootSignatureHash = job.instrumentationKey.physicalMapping ? job.instrumentationKey.physicalMapping->signatureHash : 0ull;
        key.bindingHash = HashRootRegisterBindingInfo(job.instrumentationKey.bindingInfo);
        key.localKeyHash = HashLocalInstrumentationKeys(job.instrumentationKey);
        return key;
    }

    static bool CanUsePersistentShaderArtifact(const ShaderJob& job) {
        (void)job;
        return true;
    }

    static std::filesystem::path GetPersistentShaderArtifactPath(const PersistentShaderArtifactKey& key) {
        std::filesystem::path directory = GetIntermediateCachePath() / "DX12" / "ShaderInstrumentation";
        CreateDirectoryTree(directory);

        char name[192];
        std::snprintf(
            name,
            sizeof(name),
            "%08X_%016llX_%016llX_%016llX_%016llX_%016llX.bin",
            key.sourceBytecodeHash,
            static_cast<unsigned long long>(key.featureBitSet),
            static_cast<unsigned long long>(key.combinedHash),
            static_cast<unsigned long long>(key.rootSignatureHash),
            static_cast<unsigned long long>(key.bindingHash),
            static_cast<unsigned long long>(key.localKeyHash)
        );

        return directory / name;
    }

    static bool TryLoadPersistentShaderArtifact(const ShaderJob& job, ShaderInstrument& shaderInstrument) {
        const PersistentShaderArtifactKey key = BuildPersistentShaderArtifactKey(job);
        const std::filesystem::path path = GetPersistentShaderArtifactPath(key);

        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return false;
        }

        PersistentShaderArtifactHeader header{};
        stream.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!stream || header.magic != kPersistentShaderArtifactMagic || header.version != kPersistentShaderArtifactVersion || !(header.key == key)) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
            return false;
        }

        if (header.streamByteSize) {
            shaderInstrument.stream.Resize(header.streamByteSize);
            stream.read(reinterpret_cast<char*>(shaderInstrument.stream.GetMutableData()), header.streamByteSize);
            if (!stream) {
                std::error_code ec;
                std::filesystem::remove(path, ec);
                shaderInstrument.stream.Clear();
                return false;
            }
        }

        std::vector<ShaderSourceMapping> restoredMappings(header.mappingCount);
        if (header.mappingCount) {
            stream.read(reinterpret_cast<char*>(restoredMappings.data()), sizeof(ShaderSourceMapping) * header.mappingCount);
            if (!stream) {
                std::error_code ec;
                std::filesystem::remove(path, ec);
                shaderInstrument.stream.Clear();
                return false;
            }

            if (!job.state->parent || !job.state->parent->sguidHost || !job.state->parent->sguidHost->RestoreMappings(restoredMappings.data(), header.mappingCount)) {
                shaderInstrument.stream.Clear();
                return false;
            }
        }

        shaderInstrument.featureTable.executionInfo = header.executionInfo != 0;
        return true;
    }

    static void StorePersistentShaderArtifact(const ShaderJob& job, const ShaderInstrument& shaderInstrument) {
        const PersistentShaderArtifactKey key = BuildPersistentShaderArtifactKey(job);
        const std::filesystem::path path = GetPersistentShaderArtifactPath(key);
        const std::filesystem::path tempPath = path.string() + ".tmp";
        std::vector<ShaderSourceMapping> cachedMappings;
        if (job.state->parent && job.state->parent->sguidHost) {
            job.state->parent->sguidHost->CopyMappings(job.state->uid, cachedMappings);
        }

        PersistentShaderArtifactHeader header{};
        header.streamByteSize = shaderInstrument.stream.GetByteSize();
        header.mappingCount = static_cast<uint32_t>(cachedMappings.size());
        header.key = key;
        header.executionInfo = shaderInstrument.featureTable.executionInfo ? 1u : 0u;

        {
            std::ofstream stream(tempPath, std::ios::binary | std::ios::trunc);
            if (!stream) {
                return;
            }

            stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (header.streamByteSize) {
                stream.write(reinterpret_cast<const char*>(shaderInstrument.stream.GetData()), header.streamByteSize);
            }
            if (header.mappingCount) {
                stream.write(reinterpret_cast<const char*>(cachedMappings.data()), sizeof(ShaderSourceMapping) * header.mappingCount);
            }

            if (!stream) {
                std::error_code ec;
                std::filesystem::remove(tempPath, ec);
                return;
            }
        }

        std::error_code ec;
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(tempPath, path, ec);
        if (ec) {
            std::filesystem::remove(tempPath, ec);
        }
    }
}

ShaderCompiler::ShaderCompiler(DeviceState *device)
    : device(device),
      shaderFeatures(device->allocators.Tag(kAllocInstrumentation)),
      shaderData(device->allocators.Tag(kAllocInstrumentation)) {

}

bool ShaderCompiler::Install() {
    dispatcher = registry->Get<Dispatcher>();
    if (!dispatcher) {
        return false;
    }

    // Optional debug
    debug = registry->Get<ShaderCompilerDebug>();

    // Get the microsoft compiler
    msCompiler = registry->Get<DXMSCompiler>();

    // Get all shader features
    for (const ComRef<IFeature>& feature : device->features) {
        auto shaderFeature = Cast<IShaderFeature>(feature);

        // Append null even if not found
        shaderFeatures.push_back(shaderFeature);
    }

    // Get the export host
    auto exportHost = registry->Get<IShaderExportHost>();
    exportHost->Enumerate(&exportCount, nullptr);

    // Get the export host
    auto shaderDataHost = registry->Get<ShaderDataHost>();

    // Get number of resources
    uint32_t resourceCount;
    shaderDataHost->EnumerateShader(&resourceCount, nullptr, ShaderDataType::AllGlobal);

    // Fill resources
    shaderData.resize(resourceCount);
    shaderDataHost->EnumerateShader(&resourceCount, shaderData.data(), ShaderDataType::AllGlobal);

    // Get the signers
    dxilSigner = registry->Get<DXILSigner>();
    dxbcSigner = registry->Get<DXBCSigner>();

    // Get the converter
    dxbcConverter = registry->Get<DXBCConverter>();

    // OK
    return true;
}

void ShaderCompiler::Add(const ShaderJob& job, DispatcherBucket *bucket) {
    auto data = new(registry->GetAllocators(), kAllocInstrumentation) ShaderJob(job);
    job.diagnostic->totalJobs++;
    dispatcher->Add(BindDelegate(this, ShaderCompiler::Worker), data, bucket);
}

void ShaderCompiler::Worker(void *data) {
    auto *job = static_cast<ShaderJob *>(data);

    // Try to compile, if failed remove the reservation
    if (!CompileShader(*job)) {
        job->state->RemoveInstrument(job->instrumentationKey);
    }

    // Cleanup keys
    if (job->instrumentationKey.localKeys) {
        destroy(job->instrumentationKey.localKeys, allocators);
    }
    
    destroy(job, allocators);
}

bool ShaderCompiler::InitializeModule(ShaderState *state) {
    // Instrumented pipelines are unique, however, originating modules may not be
    std::lock_guard moduleGuad(state->mutex);
    return InitializeModuleNoLock(state);
}

bool ShaderCompiler::InitializeModuleNoLock(ShaderState *state) {
    // Create the module on demand
    if (state->module) {
        return true;
    }

    // Get type
    uint32_t type = *static_cast<const uint32_t *>(state->byteCode.pShaderBytecode);

    // Create the module
    switch (type) {
        default: {
            // Unknown type, just skip the job
            return false;
        }
        case 'CBXD': {
            state->module = new (allocators, kAllocModuleDXBC) DXBCModule(allocators.Tag(kAllocModuleDXBC), state->uid, GlobalUID::New());
            break;
        }
    }

    // Prepare job
    DXParseJob job;
    job.byteCode = state->byteCode.pShaderBytecode;
    job.byteLength = state->byteCode.BytecodeLength;
    job.pdbController = device->pdbController;
    job.dxbcConverter = dxbcConverter;

    // Try to parse the bytecode
    if (!state->module->Parse(job)) {
        return false;
    }

    // If this contains a slim debug module, recompile it all
    if (!state->module->GetDebug() && state->module->IsSlimDebugModule()) {
        if (IDXModule* slimModule = CompileSlimModule(state->module)) {
            destroy(state->module, allocators);
            state->module = slimModule;
            if (state->module->GetDebug()) {
                device->logBuffer->Add("DX12", LogSeverity::Info, Format("Recovered embedded shader debug information for shader UID {} from a slim debug module.", state->uid));
            }
        } else {
            device->logBuffer->Add("DX12", LogSeverity::Warning, Format("Failed to recover embedded shader debug information for shader UID {} from a slim debug module.", state->uid));
        }
    }

    if (!state->module->GetDebug()) {
        device->logBuffer->Add("DX12", LogSeverity::Warning, Format("Shader UID {} initialized without DX12 debug metadata; SGUID source mapping will remain unresolved.", state->uid));
    }

    // OK
    return true;
}

bool ShaderCompiler::CompileShader(const ShaderJob &job) {
#if SHADER_COMPILER_SERIAL
    static std::mutex mutex;
    std::lock_guard guard(mutex);
#endif

    // Diagnostic scope
    DiagnosticBucketScope scope(job.diagnostic->messages, job.state->uid);

    // Try to reuse a persisted artifact before touching the source module.
    ShaderInstrument cachedInstrument(allocators);
    if (CanUsePersistentShaderArtifact(job) && TryLoadPersistentShaderArtifact(job, cachedInstrument)) {
        job.state->AddInstrument(job.instrumentationKey, new (allocators) ShaderInstrument(std::move(cachedInstrument)));
        ++job.diagnostic->passedJobs;
        return true;
    }

    // Initialize module
    if (!InitializeModule(job.state)) {
        scope.Add(DiagnosticType::ShaderUnknownHeader, *static_cast<const uint32_t *>(job.state->byteCode.pShaderBytecode));
        ++job.diagnostic->failedJobs;
        return false;
    }

    // Create a copy of the module, don't modify the source
    IDXModule *module = job.state->module->Copy();

    // Assign the instrumentation hash, may be used within features for tracking
    module->GetProgram()->SetShaderInstrumentationHash(job.instrumentationKey.combinedHash);

    // Debugging
    std::filesystem::path debugPath;
    if (debug) {
        // Allocate path
        debugPath = debug->AllocatePath(module);

        // Add source module
        debug->Add(debugPath, "source", module);
    }

    // Get user map
    IL::ShaderDataMap& shaderDataMap = module->GetProgram()->GetShaderDataMap();

    // Add resources
    for (const ShaderDataInfo& info : shaderData) {
        shaderDataMap.Add(info);
    }

    // Pre-injection
    for (size_t i = 0; i < shaderFeatures.size(); i++) {
        if (!(job.instrumentationKey.featureBitSet & (1ull << i)) || !shaderFeatures[i]) {
            continue;
        }

        // Pre-inject marked shader feature
        shaderFeatures[i]->PreInject(*module->GetProgram(), *job.dependentSpecialization);
    }

    // Pass through all features
    for (size_t i = 0; i < shaderFeatures.size(); i++) {
        if (!(job.instrumentationKey.featureBitSet & (1ull << i)) || !shaderFeatures[i]) {
            continue;
        }

        // Inject marked shader feature
        shaderFeatures[i]->Inject(*module->GetProgram(), *job.dependentSpecialization);
    }

    // Instrumentation job
    DXCompileJob compileJob;
    compileJob.instrumentationKey = job.instrumentationKey;
    compileJob.streamCount = exportCount;
    compileJob.dxilSigner = dxilSigner;
    compileJob.dxbcSigner = dxbcSigner;
    compileJob.messages = scope;

    // Instrumented data
    ShaderInstrument shaderInstrument(allocators);
    shaderInstrument.featureTable = module->GetProgram()->GetFeatureTable();

    // Debugging
    if (!debugPath.empty()) {
        // Add instrumented module
        debug->Add(debugPath, "user", module);
    }

    // Attempt to recompile
    if (!module->Compile(compileJob, shaderInstrument.stream)) {
        ++job.diagnostic->failedJobs;
        return false;
    }

    // Debugging
    if (!debugPath.empty()) {
        // Add instrumented module
        debug->Add(debugPath, "instrumented", module);
    }

    if (CanUsePersistentShaderArtifact(job)) {
        StorePersistentShaderArtifact(job, shaderInstrument);
    }

    // Assign the instrument
    job.state->AddInstrument(job.instrumentationKey, new (allocators) ShaderInstrument(shaderInstrument));

    // Mark as passed
    ++job.diagnostic->passedJobs;

    // Destroy the module
    destroy(module, allocators);

    // OK
    return true;
}

IDXModule* ShaderCompiler::CompileSlimModule(IDXModule* sourceModule) {
    // Try to compile a new module from the source blob
    Microsoft::WRL::ComPtr<IDxcResult> result = msCompiler->CompileWithEmbeddedDebug(sourceModule);
    if (!result) {
        return nullptr;
    }

    // Get the status
    HRESULT status;
    result->GetStatus(&status);

    // May have failed
    if (FAILED(status)) {
#ifndef NDEBUG
        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errorBlob;
        if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), &errorBlob, nullptr))) {
            // Format
            std::stringstream stream;
            stream << "Failed to compile slim module:\n";
            stream.write(errorBlob->GetStringPointer(), errorBlob->GetStringLength());

            // Dump to console
            OutputDebugStringA(stream.str().c_str());
        }
#endif // NDEBUG

        // Treat as complete failure
        return nullptr;
    }

    // Get the shader blob
    Microsoft::WRL::ComPtr<IDxcBlob> blob;
    result->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), &blob, nullptr);
    
    // Get type
    uint32_t type = *static_cast<const uint32_t *>(blob->GetBufferPointer());
    
    // Create the module
    IDXModule* slimModule;
    switch (type) {
        default: {
            // Unknown type
            return nullptr;
        }
        case 'CBXD': {
            slimModule = new (allocators, kAllocModuleDXBC) DXBCModule(
                allocators.Tag(kAllocModuleDXBC),
                 sourceModule->GetProgram()->GetShaderGUID(),
                 sourceModule->GetInstrumentationGUID()
            );
            break;
        }
    }

    // Prepare job
    DXParseJob job;
    job.byteCode = blob->GetBufferPointer();
    job.byteLength = blob->GetBufferSize();
    job.pdbController = device->pdbController;
    job.dxbcConverter = dxbcConverter;

    // Try to parse the bytecode
    if (!slimModule->Parse(job)) {
        destroy(slimModule, allocators);
        return nullptr;
    }

    // OK
    return slimModule;
}
