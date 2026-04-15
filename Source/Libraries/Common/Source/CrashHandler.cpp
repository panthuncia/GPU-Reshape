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

#include <Common/CrashHandler.h>
#include <Common/Sink.h>

// Std
#include <mutex>

// Win32
#ifdef _WIN32
#   ifndef NDEBUG
#       define WIN32_EXCEPTION_HANDLER
#       include <iostream>
#       include <Windows.h>
#       include <dbghelp.h>
#   endif // NDEBUG
#endif // _WIN32

#ifdef WIN32_EXCEPTION_HANDLER
static void InitializeStackFrameFromContext(STACKFRAME64& stack, CONTEXT& context) {
    memset(&stack, 0, sizeof(STACKFRAME64));

#if defined(_M_AMD64)
    stack.AddrPC.Offset    = context.Rip;
    stack.AddrFrame.Offset = context.Rbp;
    stack.AddrStack.Offset = context.Rsp;
#else
    stack.AddrPC.Offset    = context.Eip;
    stack.AddrFrame.Offset = context.Ebp;
    stack.AddrStack.Offset = context.Esp;
#endif

    stack.AddrPC.Mode    = AddrModeFlat;
    stack.AddrFrame.Mode = AddrModeFlat;
    stack.AddrStack.Mode = AddrModeFlat;
}

static LONG WINAPI TopLevelExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    static std::mutex globalLock;
    std::lock_guard guard(globalLock);

    // Create console
    AllocConsole();

    FILE* stream{};
    freopen_s(&stream, "CONIN$", "r", stdin);
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);

    // Notify user
    std::cerr << "Crash detected, current frames:\n";

    // Common handles
    HANDLE thread  = GetCurrentThread();
    HANDLE process = GetCurrentProcess();

    // Ensure symbols are ready
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, NULL, TRUE);

    HMODULE lastModule{nullptr};

    // Work on a local copy of the captured context.
    CONTEXT context = *pExceptionInfo->ContextRecord;

    // Current displacement
    DWORD64 displacement{0};

    // Current frame
    STACKFRAME64 stack;
    InitializeStackFrameFromContext(stack, context);

    // Walk frame
    for (ULONG frame = 0;; frame++) {
        GRS_SINK(frame);

        BOOL result = StackWalk64(
#if defined(_M_AMD64)
            IMAGE_FILE_MACHINE_AMD64,
#else
            IMAGE_FILE_MACHINE_I386,
#endif
            process,
            thread,
            &stack,
            &context,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            NULL
        );

        // Failed?
        if (!result) {
            break;
        }

        DWORD64 programCounter = stack.AddrPC.Offset;
        if (!programCounter) {
            break;
        }

        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)]{};

        // Symbol info
        PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSymbol->MaxNameLen = MAX_SYM_NAME;
        BOOL hasSymbol = SymFromAddr(process, static_cast<ULONG64>(programCounter), &displacement, pSymbol);

        // Line info
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        // Print symbol & address
        std::cerr << "\t[" << reinterpret_cast<const void*>(programCounter) << "] ";
        if (hasSymbol) {
            std::cerr << pSymbol->Name;
        } else {
            std::cerr << "<unresolved symbol>";
        }

        HMODULE currentModule = reinterpret_cast<HMODULE>(SymGetModuleBase64(process, programCounter));
        if (currentModule != lastModule) {
            lastModule = currentModule;

            char module[MAX_PATH]{};
            if (currentModule != nullptr) {
                GetModuleFileNameA(currentModule, module, MAX_PATH);
            }

            std::cerr << "\n\t\t" << module;
        }

        // Get line, if possible
        DWORD disp{};
        if (SymGetLineFromAddr64(process, programCounter, &disp, &line)) {
            std::cerr << "\n\t\t" << line.FileName << " line " << line.LineNumber;
        }

        // Next!
        std::cerr << "\n";
    }

    std::cerr << "\nWaiting for debugger to attach... " << std::flush;

    // Wait for the debugger
    while (!IsDebuggerPresent()) {
        Sleep(100);
    }

    // Notify user
    std::cerr << "Attached." << std::endl;

    // Break!
    DebugBreak();
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void SetDebugCrashHandler() {
    // Global lock
    static std::mutex GLock;
    std::lock_guard guard(GLock);

    // Set up once
    static bool GAcquired = false;
    if (GAcquired) {
        return;
    }
    
    // Platform handler
#ifdef WIN32_EXCEPTION_HANDLER
    if (!IsDebuggerPresent()) {
        SetUnhandledExceptionFilter(TopLevelExceptionHandler);
    }
#endif // WIN32_EXCEPTION_HANDLER

    GAcquired = true;
}
