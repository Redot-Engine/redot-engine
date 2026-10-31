// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Copyright (c) 2008-2024 NVIDIA Corporation. All rights reserved.

#ifndef NV_NVFOUNDATION_NVPREPROCESSOR_H
#define NV_NVFOUNDATION_NVPREPROCESSOR_H

#include <stddef.h>

/** \addtogroup foundation
  @{
*/

/*
The following preprocessor identifiers specify compiler, OS, and architecture.
All definitions have a value of 1 or 0, use '#if' instead of '#ifdef'.
*/

/**
Compiler defines, see http://sourceforge.net/p/predef/wiki/Compilers/
*/
#if defined(_MSC_VER)
#if _MSC_VER >= 1900
#define NV_VC 14
#elif _MSC_VER >= 1800
#define NV_VC 12
#elif _MSC_VER >= 1700
#define NV_VC 11
#elif _MSC_VER >= 1600
#define NV_VC 10
#elif _MSC_VER >= 1500
#define NV_VC 9
#else
#error "Unknown VC version"
#endif
#elif defined(__clang__)
#define NV_CLANG 1
#elif defined(__SNC__)
#define NV_SNC 1
#elif defined(__ghs__)
#define NV_GHS 1
#elif defined(__GNUC__) // note: __clang__, __SNC__, or __ghs__ imply __GNUC__
#define NV_GCC 1
#else
#error "Unknown compiler"
#endif

/**
Operating system defines, see http://sourceforge.net/p/predef/wiki/OperatingSystems/
*/
#if defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_PARTITION_APP
#define NV_WINRT 1 // Windows Runtime, either on Windows RT or Windows 8
#elif defined(XBOXONE)
#define NV_XBOXONE 1
#elif defined(_WIN64) // note: XBOXONE implies _WIN64
#define NV_WIN64 1
#elif defined(_M_PPC)
#define NV_X360 1
#elif defined(_WIN32) // note: _M_PPC implies _WIN32
#define NV_WIN32 1
#elif defined(__ANDROID__)
#define NV_ANDROID 1
#elif defined(__linux__) // note: __ANDROID__ implies __linux__
#define NV_LINUX 1
#elif defined(__APPLE__) && (defined(__arm__) || defined(__arm64__))
#define NV_IOS 1
#elif defined(__APPLE__)
#define NV_OSX 1
#elif defined(__CELLOS_LV2__)
#define NV_PS3 1
#elif defined(__ORBIS__)
#define NV_PS4 1
#elif defined(__SNC__) && defined(__arm__)
#define NV_PSP2 1
#elif defined(__ghs__)
#define NV_WIIU 1
#else
#error "Unknown operating system"
#endif

/**
Architecture defines, see http://sourceforge.net/p/predef/wiki/Architectures/
*/
#if defined(__x86_64__) || defined(_M_X64) // ps4 compiler defines _M_X64 without value
#define NV_X64 1
#elif defined(__i386__) || defined(_M_IX86)
#define NV_X86 1
#elif defined(__arm64__) || defined(__aarch64__)
#define NV_A64 1
#elif defined(__arm__) || defined(_M_ARM)
#define NV_ARM 1
#elif defined(__SPU__)
#define NV_SPU 1
#elif defined(__ppc__) || defined(_M_PPC) || defined(__CELLOS_LV2__)
#define NV_PPC 1
#else
#error "Unknown architecture"
#endif

/**
SIMD defines
*/
#if defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
#define NV_SSE2 1
#endif
#if defined(_M_ARM) || defined(__ARM_NEON__)
#define NV_NEON 1
#endif
#if defined(_M_PPC) || defined(__CELLOS_LV2__)
#define NV_VMX 1
#endif

/**
define anything not defined on this platform to 0
*/
#ifndef NV_VC
#define NV_VC 0
#endif
#ifndef NV_CLANG
#define NV_CLANG 0
#endif
#ifndef NV_SNC
#define NV_SNC 0
#endif
#ifndef NV_GHS
#define NV_GHS 0
#endif
#ifndef NV_GCC
#define NV_GCC 0
#endif
#ifndef NV_WINRT
#define NV_WINRT 0
#endif
#ifndef NV_XBOXONE
#define NV_XBOXONE 0
#endif
#ifndef NV_WIN64
#define NV_WIN64 0
#endif
#ifndef NV_X360
#define NV_X360 0
#endif
#ifndef NV_WIN32
#define NV_WIN32 0
#endif
#ifndef NV_ANDROID
#define NV_ANDROID 0
#endif
#ifndef NV_LINUX
#define NV_LINUX 0
#endif
#ifndef NV_IOS
#define NV_IOS 0
#endif
#ifndef NV_OSX
#define NV_OSX 0
#endif
#ifndef NV_PS3
#define NV_PS3 0
#endif
#ifndef NV_PS4
#define NV_PS4 0
#endif
#ifndef NV_PSP2
#define NV_PSP2 0
#endif
#ifndef NV_WIIU
#define NV_WIIU 0
#endif
#ifndef NV_X64
#define NV_X64 0
#endif
#ifndef NV_X86
#define NV_X86 0
#endif
#ifndef NV_A64
#define NV_A64 0
#endif
#ifndef NV_ARM
#define NV_ARM 0
#endif
#ifndef NV_SPU
#define NV_SPU 0
#endif
#ifndef NV_PPC
#define NV_PPC 0
#endif
#ifndef NV_SSE2
#define NV_SSE2 0
#endif
#ifndef NV_NEON
#define NV_NEON 0
#endif
#ifndef NV_VMX
#define NV_VMX 0
#endif

/*
define anything not defined through the command line to 0
*/
#ifndef NV_DEBUG
#define NV_DEBUG 0
#endif
#ifndef NV_CHECKED
#define NV_CHECKED 0
#endif
#ifndef NV_PROFILE
#define NV_PROFILE 0
#endif
#ifndef NV_NVTX
#define NV_NVTX 0
#endif
#ifndef NV_DOXYGEN
#define NV_DOXYGEN 0
#endif

/**
family shortcuts
*/
// compiler
#define NV_GCC_FAMILY (NV_CLANG || NV_SNC || NV_GHS || NV_GCC)
// os
#define NV_WINDOWS_FAMILY (NV_WINRT || NV_WIN32 || NV_WIN64)
#define NV_MICROSOFT_FAMILY (NV_XBOXONE || NV_X360 || NV_WINDOWS_FAMILY)
#define NV_LINUX_FAMILY (NV_LINUX || NV_ANDROID)
#define NV_APPLE_FAMILY (NV_IOS || NV_OSX) // equivalent to #if __APPLE__
#define NV_UNIX_FAMILY (NV_LINUX_FAMILY || NV_APPLE_FAMILY) // shortcut for unix/posix platforms
// architecture
#define NV_INTEL_FAMILY (NV_X64 || NV_X86)
#define NV_ARM_FAMILY (NV_ARM || NV_A64)
#define NV_P64_FAMILY (NV_X64 || NV_A64) // shortcut for 64-bit architectures

// shortcut for PS3 PPU
#define NV_PPU (NV_PS3 && NV_PPC)

/**
Assert macro
*/
#ifndef NV_ENABLE_ASSERTS
#if NV_DEBUG && !defined(__CUDACC__)
#define NV_ENABLE_ASSERTS 1
#else
#define NV_ENABLE_ASSERTS 0
#endif
#endif

/**
DLL export macros
*/
#ifndef NV_C_EXPORT
#if NV_WINDOWS_FAMILY || NV_LINUX
#define NV_C_EXPORT extern "C"
#else
#define NV_C_EXPORT
#endif
#endif

#if NV_UNIX_FAMILY && __GNUC__ >= 4
#define NV_UNIX_EXPORT __attribute__((visibility("default")))
#else
#define NV_UNIX_EXPORT
#endif

#if NV_WINDOWS_FAMILY
#define NV_DLL_EXPORT __declspec(dllexport)
#define NV_DLL_IMPORT __declspec(dllimport)
#else
#define NV_DLL_EXPORT NV_UNIX_EXPORT
#define NV_DLL_IMPORT
#endif

/**
Define API function declaration

NV_FOUNDATION_DLL=1 - used by the DLL library (PhysXCommon) to export the API
NV_FOUNDATION_DLL=0 - for windows configurations where the NV_FOUNDATION_API is linked through standard static linking
no definition       - this will allow DLLs and libraries to use the exported API from PhysXCommon

*/

#if NV_WINDOWS_FAMILY && !NV_ARM_FAMILY || NV_WINRT
#ifndef NV_FOUNDATION_DLL
#define NV_FOUNDATION_API NV_DLL_IMPORT
#elif NV_FOUNDATION_DLL
#define NV_FOUNDATION_API NV_DLL_EXPORT
#endif
#elif NV_UNIX_FAMILY
#ifdef NV_FOUNDATION_DLL
#define NV_FOUNDATION_API NV_UNIX_EXPORT
#endif
#endif

#ifndef NV_FOUNDATION_API
#define NV_FOUNDATION_API
#endif

/**
Calling convention
*/
#ifndef NV_CALL_CONV
#if NV_MICROSOFT_FAMILY
#define NV_CALL_CONV __cdecl
#else
#define NV_CALL_CONV
#endif
#endif

/**
Pack macros - disabled on SPU because they are not supported
*/
#if NV_VC
#define NV_PUSH_PACK_DEFAULT __pragma(pack(push, 8))
#define NV_POP_PACK __pragma(pack(pop))
#elif NV_GCC_FAMILY && !NV_SPU
#define NV_PUSH_PACK_DEFAULT _Pragma("pack(push, 8)")
#define NV_POP_PACK _Pragma("pack(pop)")
#else
#define NV_PUSH_PACK_DEFAULT
#define NV_POP_PACK
#endif

/**
Inline macro
*/
#define NV_INLINE inline
#if NV_MICROSOFT_FAMILY
#if defined(_MSC_VER) // Godot patch: MSVC-only pragma
#pragma inline_depth(255)
#endif
#endif

/**
Force inline macro
*/
#if NV_VC
#define NV_FORCE_INLINE __forceinline
#elif NV_LINUX // Workaround; Fedora Core 3 do not agree with force inline and NvcPool
#define NV_FORCE_INLINE inline
#elif NV_GCC_FAMILY
#define NV_FORCE_INLINE inline __attribute__((always_inline))
#else
#define NV_FORCE_INLINE inline
#endif

/**
Noinline macro
*/
#if NV_MICROSOFT_FAMILY
#define NV_NOINLINE __declspec(noinline)
#elif NV_GCC_FAMILY
#define NV_NOINLINE __attribute__((noinline))
#else
#define NV_NOINLINE
#endif

/**
Restrict macro
*/
#if defined(__CUDACC__)
#define NV_RESTRICT __restrict__
#else
#define NV_RESTRICT __restrict
#endif

/**
Noalias macro
*/
#if NV_MICROSOFT_FAMILY
#define NV_NOALIAS __declspec(noalias)
#else
#define NV_NOALIAS
#endif

/**
Alignment macros

NV_ALIGN_PREFIX and NV_ALIGN_SUFFIX can be used for type alignment instead of aligning individual variables as follows:
NV_ALIGN_PREFIX(16)
struct A {
...
} NV_ALIGN_SUFFIX(16);
This declaration style is parsed correctly by Visual Assist.

*/
#ifndef NV_ALIGN
#if NV_MICROSOFT_FAMILY
#define NV_ALIGN(alignment, decl) __declspec(align(alignment)) decl
#define NV_ALIGN_PREFIX(alignment) __declspec(align(alignment))
#define NV_ALIGN_SUFFIX(alignment)
#elif NV_GCC_FAMILY
#define NV_ALIGN(alignment, decl) decl __attribute__((aligned(alignment)))
#define NV_ALIGN_PREFIX(alignment)
#define NV_ALIGN_SUFFIX(alignment) __attribute__((aligned(alignment)))
#else
#define NV_ALIGN(alignment, decl)
#define NV_ALIGN_PREFIX(alignment)
#define NV_ALIGN_SUFFIX(alignment)
#endif
#endif

/**
Deprecated macro
- To deprecate a function: Place NV_DEPRECATED at the start of the function header (leftmost word).
- To deprecate a 'typedef', a 'struct' or a 'class': Place NV_DEPRECATED directly after the keywords ('typdef',
'struct', 'class').

Use these macro definitions to create warnings for deprecated functions
#define NV_DEPRECATED __declspec(deprecated) // Microsoft
#define NV_DEPRECATED __attribute__((deprecated())) // GCC
*/
#define NV_DEPRECATED

/**
General defines
*/

// static assert
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) || defined(__ORBIS__)
#define NV_COMPILE_TIME_ASSERT(exp) typedef char NvCompileTimeAssert_Dummy[(exp) ? 1 : -1] __attribute__((unused))
#else
#define NV_COMPILE_TIME_ASSERT(exp) typedef char NvCompileTimeAssert_Dummy[(exp) ? 1 : -1]
#endif

#if NV_GCC_FAMILY && !NV_SNC && !NV_GHS
#define NV_OFFSET_OF(X, Y) __builtin_offsetof(X, Y)
#else
#define NV_OFFSET_OF(X, Y) offsetof(X, Y)
#endif

#define NV_OFFSETOF_BASE 0x100 // casting the null ptr takes a special-case code path, which we don't want
#define NV_OFFSET_OF_RT(Class, Member) \
	(reinterpret_cast<size_t>(&reinterpret_cast<Class *>(NV_OFFSETOF_BASE)->Member) - size_t(NV_OFFSETOF_BASE))

// check that exactly one of NDEBUG and _DEBUG is defined
#if !defined(NDEBUG) ^ defined(_DEBUG)
#error Exactly one of NDEBUG and _DEBUG needs to be defined!
#endif

// make sure NV_CHECKED is defined in all _DEBUG configurations as well
#if !NV_CHECKED && NV_DEBUG
#error NV_CHECKED must be defined when NV_DEBUG is defined
#endif

#ifdef __CUDACC__
#define NV_CUDA_CALLABLE __host__ __device__
#else
#define NV_CUDA_CALLABLE
#endif

// avoid unreferenced parameter warning
// preferred solution: omit the parameter's name from the declaration
template <class T>
NV_CUDA_CALLABLE NV_INLINE void NV_UNUSED(T const &) {
}

// Ensure that the application hasn't tweaked the pack value to less than 8, which would break
// matching between the API headers and the binaries
// This assert works on win32/win64/360/ps3, but may need further specialization on other platforms.
// Some GCC compilers need the compiler flag -malign-double to be set.
// Apparently the apple-clang-llvm compiler doesn't support malign-double.
#if NV_PS4 || NV_APPLE_FAMILY
struct NvPackValidation {
	char _;
	long a;
};
#elif NV_ANDROID
struct NvPackValidation {
	char _;
	double a;
};
#else
struct NvPackValidation {
	char _;
	long long a;
};
#endif
#if !NV_APPLE_FAMILY
NV_COMPILE_TIME_ASSERT(NV_OFFSET_OF(NvPackValidation, a) == 8);
#endif

// use in a cpp file to suppress LNK4221
#if NV_VC
#define NV_DUMMY_SYMBOL \
	namespace {         \
	char NvDummySymbol; \
	}
#else
#define NV_DUMMY_SYMBOL
#endif

#if NV_GCC_FAMILY && !NV_GHS
#define NV_WEAK_SYMBOL __attribute__((weak)) // this is to support SIMD constant merging in template specialization
#else
#define NV_WEAK_SYMBOL
#endif

// Macro for avoiding default assignment and copy, because doing this by inheritance can increase class size on some
// platforms.
#define NV_NOCOPY(Class)  \
                          \
protected:                \
	Class(const Class &); \
	Class &operator=(const Class &);

#define NV_STRINGIZE_HELPER(X) #X
#define NV_STRINGIZE(X) NV_STRINGIZE_HELPER(X)

#define NV_CONCAT_HELPER(X, Y) X##Y
#define NV_CONCAT(X, Y) NV_CONCAT_HELPER(X, Y)

// C-style API declaration.
#define NV_C_API NV_C_EXPORT NV_DLL_EXPORT

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVPREPROCESSOR_H
