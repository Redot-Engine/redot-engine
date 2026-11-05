// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_STRING_H
#define PX_STRING_H

#include "foundation/PxPreprocessor.h"
#include "foundation/PxSimpleTypes.h"
#include "foundation/PxFoundationConfig.h"
#include <stdarg.h>

#if !PX_DOXYGEN
namespace physx
{
#endif

// the following functions have C99 semantics. Note that C99 requires for snprintf and vsnprintf:
// * the resulting string is always NULL-terminated regardless of truncation.
// * in the case of truncation the return value is the number of characters that would have been created.

PX_FOUNDATION_API int32_t Pxsscanf(const char* buffer, const char* format, ...);
PX_FOUNDATION_API int32_t Pxstrcmp(const char* str1, const char* str2);
PX_FOUNDATION_API int32_t Pxstrncmp(const char* str1, const char* str2, size_t count);
PX_FOUNDATION_API int32_t Pxsnprintf(char* dst, size_t dstSize, const char* format, ...);
PX_FOUNDATION_API int32_t Pxvsnprintf(char* dst, size_t dstSize, const char* src, va_list arg);

// strlcat and strlcpy have BSD semantics:
// * dstSize is always the size of the destination buffer
// * the resulting string is always NULL-terminated regardless of truncation
// * in the case of truncation the return value is the length of the string that would have been created

PX_FOUNDATION_API size_t Pxstrlcat(char* dst, size_t dstSize, const char* src);
PX_FOUNDATION_API size_t Pxstrlcpy(char* dst, size_t dstSize, const char* src);

// case-insensitive string comparison
PX_FOUNDATION_API int32_t Pxstricmp(const char* str1, const char* str2);
PX_FOUNDATION_API int32_t Pxstrnicmp(const char* str1, const char* str2, size_t count);

// in-place string case conversion
PX_FOUNDATION_API void Pxstrlwr(char* str);
PX_FOUNDATION_API void Pxstrupr(char* str);


/**
\brief Prints the string literally (does not consume % specifier), trying to make sure it's visible to the app
programmer
*/
PX_FOUNDATION_API void PxPrintString(const char*);

#if !PX_DOXYGEN
} // namespace physx
#endif
#endif

