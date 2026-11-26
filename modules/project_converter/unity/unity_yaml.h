/**************************************************************************/
/*  unity_yaml.h                                                          */
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

#include "core/math/color.h"
#include "core/math/quaternion.h"
#include "core/math/vector3.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

/// One `--- !u!<classID> &<fileID>` document from a Unity YAML asset.
struct UnityYamlDocument {
	int class_id = 0;
	int64_t file_id = 0;
	bool stripped = false;
	String type_name;
	Dictionary body;
};

/// Parser for the YAML subset used by Unity's text serialization.
///
/// Unity emits a predictable dialect: indentation based maps and sequences,
/// flow maps such as `{fileID: 1, guid: abc, type: 3}`, and scalar values.
/// A full YAML implementation would pull in a large dependency for no gain,
/// so this parser implements exactly that dialect (plus block scalars and
/// quoted strings) and reports the document metadata the scene builder needs.
class UnityYaml {
public:
	static Error parse_text(const String &p_text, Vector<UnityYamlDocument> &r_documents);
	static Error parse_file(const String &p_path, Vector<UnityYamlDocument> &r_documents);

	static Dictionary parse_flow_mapping(const String &p_text);
	static Variant parse_scalar(const String &p_text);

	static int64_t ref_file_id(const Variant &p_value);
	static String ref_guid(const Variant &p_value);

	static Vector3 get_vector3(const Dictionary &p_dict, const String &p_key, const Vector3 &p_default = Vector3());
	static Quaternion get_quaternion(const Dictionary &p_dict, const String &p_key, const Quaternion &p_default = Quaternion());
	static Color get_color(const Dictionary &p_dict, const String &p_key, const Color &p_default = Color());
	static bool get_bool(const Dictionary &p_dict, const String &p_key, bool p_default = false);
	static double get_number(const Dictionary &p_dict, const String &p_key, double p_default = 0.0);
	static int64_t get_int(const Dictionary &p_dict, const String &p_key, int64_t p_default = 0);
};
