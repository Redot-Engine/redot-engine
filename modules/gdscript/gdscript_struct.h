/**************************************************************************/
/*  gdscript_struct.h                                                     */
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

#include "core/object/object_id.h"
#include "core/templates/hash_map.h"
#include "core/templates/safe_refcount.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"
#include "gdscript_function.h"

class GDScript;
class GDScriptStructInstance;

// Lightweight struct type definition for GDScript
// Similar to GDScript but without Object overhead
class GDScriptStruct {
	friend class GDScriptStructInstance;
	friend class GDScriptCompiler;
	friend class GDScriptAnalyzer;
	friend class GDScriptStructClass;

public:
	// Member information struct
	struct MemberInfo {
		int index = 0;
		Variant::Type type = Variant::NIL;
		StringName type_name; // For custom types
		PropertyInfo property_info;
		Variant default_value;
		bool has_default_value = false;
	};

	// Method information struct
	struct MethodInfo {
		StringName name;
		GDScriptFunction *function = nullptr;
		bool is_static = false;
	};

	// Reference counting for proper lifecycle management
	// This ensures structs are only deleted when all references are gone
	void reference();
	bool unreference(); // Returns true if reference count reached zero
	int get_reference_count() const { return ref_count.get(); }

private:
	StringName name;
	GDScriptStruct *base_struct = nullptr;
	GDScript *owner = nullptr; // GDScript that owns this struct
	mutable SafeRefCount ref_count;

	// Members
	HashMap<StringName, MemberInfo> members;
	Vector<Variant::Type> member_types;
	Vector<StringName> member_names;

	// Methods
	HashMap<StringName, MethodInfo> methods;
	Vector<StringName> method_names;

	// Constructor
	GDScriptFunction *constructor = nullptr;

	// Inheritance
	// NOTE: The children vector is currently unused and not populated.
	// Child struct tracking is handled through the owning GDScript's structs HashMap.
	Vector<GDScriptStruct *> children;

	// Fully qualified name for unique identification
	String fully_qualified_name;

public:
	// Create a new struct instance
	GDScriptStructInstance *create_instance(const Variant **p_args = nullptr, int p_argcount = 0);

	// Member management
	int get_member_count() const;
	void add_member(const StringName &p_name, const Variant::Type p_type, const StringName &p_type_name = StringName());
	bool has_member(const StringName &p_name) const;
	int get_member_index(const StringName &p_name) const;
	const Vector<StringName> &get_member_names() const { return member_names; }
	const HashMap<StringName, MemberInfo> &get_members() const { return members; }

	// Method management
	void add_method(const StringName &p_name, GDScriptFunction *p_function, bool p_is_static = false);
	bool has_method(const StringName &p_name) const;
	const Vector<StringName> &get_method_names() const { return method_names; }
	const HashMap<StringName, MethodInfo> &get_methods() const { return methods; }

	// Constructor
	void set_constructor(GDScriptFunction *p_func) { constructor = p_func; }
	GDScriptFunction *get_constructor() const { return constructor; }

	// Inheritance
	bool is_child_of(const GDScriptStruct *p_struct) const;
	GDScriptStruct *get_base_struct() const { return base_struct; }

	// Identification
	void set_name(const StringName &p_name) { name = p_name; }
	StringName get_name() const { return name; }
	void set_fully_qualified_name(const String &p_name) { fully_qualified_name = p_name; }
	String get_fully_qualified_name() const { return fully_qualified_name; }

	// Owner
	void set_owner(GDScript *p_owner) { owner = p_owner; }
	GDScript *get_owner() const { return owner; }

	GDScriptStruct(const StringName &p_name);
	~GDScriptStruct();
};

// Lightweight struct instance - much less overhead than GDScriptInstance
// No Object base, just reference-counted data storage
class GDScriptStructInstance {
	friend class GDScriptStruct;

private:
	GDScriptStruct *struct_type = nullptr;
	Vector<Variant> members; // Member values
	SafeRefCount ref_count;

public:
	GDScriptStructInstance(GDScriptStruct *p_struct_type);
	~GDScriptStructInstance();

	// Reference counting for reference semantics
	bool reference();
	bool unreference();
	int get_reference_count() const { return ref_count.get(); }

	// Member access
	bool set(const StringName &p_name, const Variant &p_value);
	bool get(const StringName &p_name, Variant &r_value) const;
	Variant *get_member_ptr(const StringName &p_name);

	// Method calling
	Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error);

	// Type information
	GDScriptStruct *get_struct_type() const { return struct_type; }
	StringName get_struct_name() const;

	// Property list for editor/debugger
	void get_property_list(List<PropertyInfo> *p_list) const;

	// Serialization
	Dictionary serialize() const;
	bool deserialize(const Dictionary &p_data);
};
