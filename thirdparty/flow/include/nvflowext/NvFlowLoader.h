// SPDX-FileCopyrightText: Copyright (c) 2014-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
//
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

#ifndef NV_FLOW_LOADER_H
#define NV_FLOW_LOADER_H

#if defined(_WIN32)
// Godot patch: lowercase include so case-sensitive filesystems (cross builds) find it.
#include <windows.h>
static void *NvFlowLoadLibrary(const char *winName, const char *linuxName) {
	return (void *)LoadLibraryA(winName);
}
static void *NvFlowGetProcAddress(void *module, const char *name) {
	return (void *)GetProcAddress((HMODULE)module, name); // Godot patch: explicit cast; clang rejects implicit FARPROC->void*
}
static void NvFlowFreeLibrary(void *module) {
	FreeLibrary((HMODULE)module);
}
static const char *NvFlowLoadLibraryError() {
	DWORD lastError = GetLastError();
	static char buf[1024];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf), NULL);
	return buf;
}
#else
#include <dlfcn.h>
static void *NvFlowLoadLibrary(const char *winName, const char *linuxName) {
	void *module = dlopen(linuxName, RTLD_NOW);
	//if (!module)
	//{
	//    fprintf(stderr, "Module %s failed to load : %s\n", linuxName, dlerror());
	//}
	return module;
}
static void *NvFlowGetProcAddress(void *module, const char *name) {
	return dlsym(module, name);
}
static void NvFlowFreeLibrary(void *module) {
	dlclose(module);
}
static const char *NvFlowLoadLibraryError() {
	return dlerror();
}
#endif

#include "NvFlowExt.h"

struct NvFlowLoader {
	void *module_nvflow;
	void *module_nvflowext;

	NvFlowOpList opList;
	NvFlowExtOpList extOpList;
	NvFlowGridInterface gridInterface;
	NvFlowGridParamsInterface gridParamsInterface;
	NvFlowContextOptInterface contextOptInterface;
	NvFlowDeviceInterface deviceInterface;

	NvFlowOpList *opList_orig;
	NvFlowExtOpList *extOpList_orig;
};

static void NvFlowLoaderInitDeviceAPICustom(
		NvFlowLoader *ptr,
		void (*printError)(const char *str, void *userdata),
		void *userdata,
		NvFlowContextApi deviceAPI,
		const char *nvflow_dll,
		const char *nvflow_so,
		const char *nvflowext_dll,
		const char *nvflowext_so) {
	NvFlowReflectClear(ptr, sizeof(NvFlowLoader));

	/// Load nvflow and nvflowext
	ptr->module_nvflow = NvFlowLoadLibrary(nvflow_dll, nvflow_so);
	if (ptr->module_nvflow) {
		PFN_NvFlowGetOpList getOpList = (PFN_NvFlowGetOpList)NvFlowGetProcAddress(ptr->module_nvflow, "NvFlowGetOpList");

		if (getOpList) {
			NvFlowOpList_duplicate(&ptr->opList, getOpList());
		}

		if (getOpList) {
			ptr->opList_orig = getOpList();
		}
	} else if (printError) {
		printError(NvFlowLoadLibraryError(), userdata);
	}

	ptr->module_nvflowext = NvFlowLoadLibrary(nvflowext_dll, nvflowext_so);
	if (ptr->module_nvflowext) {
		PFN_NvFlowGetExtOpList getExtOpList = (PFN_NvFlowGetExtOpList)NvFlowGetProcAddress(ptr->module_nvflowext, "NvFlowGetExtOpList");
		PFN_NvFlowGetGridInterface getGridInterface = (PFN_NvFlowGetGridInterface)NvFlowGetProcAddress(ptr->module_nvflowext, "NvFlowGetGridInterface");
		PFN_NvFlowGetGridParamsInterface getGridParamsInterface = (PFN_NvFlowGetGridParamsInterface)NvFlowGetProcAddress(ptr->module_nvflowext, "NvFlowGetGridParamsInterface");
		PFN_NvFlowGetContextOptInterface getContextOptInterface = (PFN_NvFlowGetContextOptInterface)NvFlowGetProcAddress(ptr->module_nvflowext, "NvFlowGetContextOptInterface");
		PFN_NvFlowGetDeviceInterface getDeviceInterface = (PFN_NvFlowGetDeviceInterface)NvFlowGetProcAddress(ptr->module_nvflowext, "NvFlowGetDeviceInterface");

		if (getExtOpList) {
			NvFlowExtOpList_duplicate(&ptr->extOpList, getExtOpList());
		}
		if (getGridInterface) {
			NvFlowGridInterface_duplicate(&ptr->gridInterface, getGridInterface());
		}
		if (getGridParamsInterface) {
			NvFlowGridParamsInterface_duplicate(&ptr->gridParamsInterface, getGridParamsInterface());
		}
		if (getContextOptInterface) {
			NvFlowContextOptInterface_duplicate(&ptr->contextOptInterface, getContextOptInterface());
		}
		if (getDeviceInterface) {
			NvFlowDeviceInterface_duplicate(&ptr->deviceInterface, getDeviceInterface(deviceAPI));
		}

		if (getExtOpList) {
			ptr->extOpList_orig = getExtOpList();
		}
	} else if (printError) {
		printError(NvFlowLoadLibraryError(), userdata);
	}
}

static void NvFlowLoaderInitDeviceAPI(NvFlowLoader *ptr, void (*printError)(const char *str, void *userdata), void *userdata, NvFlowContextApi deviceAPI) {
	NvFlowLoaderInitDeviceAPICustom(ptr, printError, userdata, deviceAPI,
			"nvflow.dll", "libnvflow.so", "nvflowext.dll", "libnvflowext.so");
}

static inline void NvFlowLoaderInit(NvFlowLoader *ptr, void (*printError)(const char *str, void *userdata), void *userdata) {
	NvFlowLoaderInitDeviceAPI(ptr, printError, userdata, eNvFlowContextApi_vulkan);
}

static inline void NvFlowLoaderInitCustom(
		NvFlowLoader *ptr,
		void (*printError)(const char *str, void *userdata),
		void *userdata,
		const char *nvflow_dll,
		const char *nvflow_so,
		const char *nvflowext_dll,
		const char *nvflowext_so) {
	NvFlowLoaderInitDeviceAPICustom(ptr, printError, userdata, eNvFlowContextApi_vulkan,
			nvflow_dll, nvflow_so, nvflowext_dll, nvflowext_so);
}

static void NvFlowLoaderDestroy(NvFlowLoader *ptr) {
	NvFlowFreeLibrary(ptr->module_nvflow);
	NvFlowFreeLibrary(ptr->module_nvflowext);
}

#endif
