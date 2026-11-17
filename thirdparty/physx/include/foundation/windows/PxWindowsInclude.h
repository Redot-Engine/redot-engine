// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_WINDOWS_INCLUDE_H
#define PX_WINDOWS_INCLUDE_H

#ifndef _WIN32
#error "This file should only be included by Windows builds!!"
#endif

#ifdef _WINDOWS_ // windows already included
#error "Only include windows.h through this file!!"
#endif

// We only support >= Windows 7, and we need this for critical section and
// Setting this hides some important APIs (e.g. LoadPackagedLibrary), so don't do it
#define _WIN32_WINNT 0x0601

// turn off as much as we can for windows. All we really need is the thread functions(critical sections/Interlocked*
// etc)
#define NOGDICAPMASKS
#define NOVIRTUALKEYCODES
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOGDI
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define WIN32_LEAN_AND_MEAN
// We need a slightly wider API surface for e.g. MultiByteToWideChar
#define NOUSER
#define NONLS
#define NOMSG

#pragma warning(push)
#pragma warning(disable : 4668) //'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#include <windows.h>
#pragma warning(pop)

#if PX_SSE2
#include <xmmintrin.h>
#endif

#endif

