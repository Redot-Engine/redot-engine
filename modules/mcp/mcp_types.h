/**************************************************************************/
/*  mcp_types.h                                                           */
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

#pragma once

/**
 * @file mcp_types.h
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

/// MCP Protocol Version
/// The latest version this server supports. Negotiated downwards if needed.
static constexpr const char *MCP_PROTOCOL_VERSION_LATEST = "2025-06-18";
/// Legacy version kept for backwards-compatibility negotiation.
static constexpr const char *MCP_PROTOCOL_VERSION_LEGACY = "2024-11-05";
static constexpr const char *MCP_SERVER_NAME = "redot-mcp";
static constexpr const char *MCP_SERVER_VERSION = "1.0.0";

/// MCP Content Types
struct MCPTextContent {
	String type = "text";
	String text;

	Dictionary to_dict() const {
		Dictionary d;
		d["type"] = type;
		d["text"] = text;
		return d;
	}
};

struct MCPImageContent {
	String type = "image";
	String data; ///< Base64 encoded
	String mime_type;

	Dictionary to_dict() const {
		Dictionary d;
		d["type"] = type;
		d["data"] = data;
		d["mimeType"] = mime_type;
		return d;
	}
};

struct MCPResourceContent {
	String type = "resource";
	String uri;
	String mime_type;
	String text;

	Dictionary to_dict() const {
		Dictionary d;
		d["type"] = type;
		d["uri"] = uri;
		if (!mime_type.is_empty()) {
			d["mimeType"] = mime_type;
		}
		if (!text.is_empty()) {
			d["text"] = text;
		}
		return d;
	}
};

/// MCP Tool Definition
struct MCPToolDefinition {
	String name;
	String description;
	Dictionary input_schema;

	Dictionary to_dict() const {
		Dictionary d;
		d["name"] = name;
		d["description"] = description;
		d["inputSchema"] = input_schema;
		return d;
	}
};

/// Helper for building JSON Schema
class MCPSchemaBuilder {
public:
	static Dictionary make_object_schema(const Dictionary &properties, const Array &required = Array()) {
		Dictionary schema;
		schema["type"] = "object";
		schema["properties"] = properties;
		if (!required.is_empty()) {
			schema["required"] = required;
		}
		return schema;
	}

	static Dictionary make_string_property(const String &description) {
		Dictionary prop;
		prop["type"] = "string";
		prop["description"] = description;
		return prop;
	}

	static Dictionary make_boolean_property(const String &description) {
		Dictionary prop;
		prop["type"] = "boolean";
		prop["description"] = description;
		return prop;
	}

	static Dictionary make_object_property(const String &description) {
		Dictionary prop;
		prop["type"] = "object";
		prop["description"] = description;
		return prop;
	}

	static Dictionary make_array_property(const String &description, const Dictionary &items = Dictionary()) {
		Dictionary prop;
		prop["type"] = "array";
		prop["description"] = description;
		if (!items.is_empty()) {
			prop["items"] = items;
		}
		return prop;
	}

	/// Build a tool annotations object (MCP 2025-06-18+).
	/// Hints are advisory; clients MUST treat them as untrusted.
	static Dictionary make_tool_annotations(bool p_read_only = false, bool p_destructive = false, bool p_idempotent = false, bool p_open_world = false) {
		Dictionary annotations;
		annotations["readOnlyHint"] = p_read_only;
		annotations["destructiveHint"] = p_destructive;
		annotations["idempotentHint"] = p_idempotent;
		annotations["openWorldHint"] = p_open_world;
		return annotations;
	}
};
