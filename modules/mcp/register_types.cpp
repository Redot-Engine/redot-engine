/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "register_types.h"

#include "mcp_bridge.h"
#include "mcp_protocol.h"
#include "mcp_server.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"

static MCPServer *mcp_server_singleton = nullptr;
static MCPBridge *mcp_bridge_singleton = nullptr;

void initialize_mcp_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(MCPProtocol);
	GDREGISTER_CLASS(MCPServer);
	GDREGISTER_CLASS(MCPBridge);

	mcp_server_singleton = memnew(MCPServer);
	Engine::get_singleton()->add_singleton(
			Engine::Singleton("MCPServer", MCPServer::get_singleton()));

	mcp_bridge_singleton = memnew(MCPBridge);
	Engine::get_singleton()->add_singleton(
			Engine::Singleton("MCPBridge", MCPBridge::get_singleton()));
}

void uninitialize_mcp_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	if (mcp_bridge_singleton) {
		Engine::get_singleton()->remove_singleton("MCPBridge");
		memdelete(mcp_bridge_singleton);
		mcp_bridge_singleton = nullptr;
	}

	if (mcp_server_singleton) {
		Engine::get_singleton()->remove_singleton("MCPServer");
		memdelete(mcp_server_singleton);
		mcp_server_singleton = nullptr;
	}
}
