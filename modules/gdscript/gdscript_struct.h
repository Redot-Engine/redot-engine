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

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"
#include "gdscript_function.h"

class GDScript;
class GDScriptStructInstance;

// Forward declaration for the instance data
class GDScriptStructInstanceData;

// Lightweight struct type definition for GDScript
// The blueprint/type definition - shared across all instances
class GDScriptStruct : public RefCounted {
	GDCLASS(GDScriptStruct, RefCounted);
	friend class GDScriptStructInstance;
	friend class GDScriptCompiler;
	friend class GDScriptAnalyzer;
	friend class GDScriptStructClass;
	friend class GDScriptStructInstanceData;

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

private:
	StringName name;
	Ref<GDScriptStruct> base_struct;
	GDScript *owner = nullptr; // GDScript that owns this struct

	// Members
	HashMap<StringName, MemberInfo> members;
	Vector<Variant::Type> member_types;
	Vector<StringName> member_names;

	// Methods
	HashMap<StringName, MethodInfo> methods;
	Vector<StringName> method_names;

	// Constructor
	GDScriptFunction *constructor = nullptr;

	// Fully qualified name for unique identification
	String fully_qualified_name;

protected:
	static void _bind_methods();

public:
	// Create a new struct instance (returns Variant containing GDScriptStructInstance wrapper)
	Variant create_variant_instance(const Variant **p_args = nullptr, int p_argcount = 0);

	// Member management
	int get_member_count() const;
	void add_member(const StringName &p_name, const Variant::Type p_type, const StringName &p_type_name = StringName(), const Variant &p_default_value = Variant(), bool p_has_default_value = false);
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
	Ref<GDScriptStruct> get_base_struct() const { return base_struct; }
	void set_base_struct(const Ref<GDScriptStruct> &p_base) { base_struct = p_base; }

	// Identification
	void set_name(const StringName &p_name) { name = p_name; }
	StringName get_name() const { return name; }
	void set_fully_qualified_name(const String &p_name) { fully_qualified_name = p_name; }
	String get_fully_qualified_name() const { return fully_qualified_name; }

	// Owner
	void set_owner(GDScript *p_owner) { owner = p_owner; }
	GDScript *get_owner() const { return owner; }

	GDScriptStruct();
	GDScriptStruct(const StringName &p_name);
	~GDScriptStruct();
};

// The actual instance data - this is what gets COW-managed
// Inherits from RefCounted for automatic reference counting
class GDScriptStructInstanceData : public RefCounted {
	GDCLASS(GDScriptStructInstanceData, RefCounted);
	friend class GDScriptStructInstance;

private:
	Ref<GDScriptStruct> blueprint; // Pointer to the type definition
	Vector<Variant> members; // The actual data

protected:
	static void _bind_methods();

public:
	// Factory method
	static Ref<GDScriptStructInstanceData> create(const Ref<GDScriptStruct> &p_blueprint);

	// Direct member access (bypasses COW - only use when you know you have unique ownership)
	Variant get_member_direct(int p_index) const;
	void set_member_direct(int p_index, const Variant &p_value);

	// Get the blueprint
	Ref<GDScriptStruct> get_blueprint() const { return blueprint; }

	// Get member array (for serialization)
	const Vector<Variant> &get_members() const { return members; }
	Vector<Variant> &get_members_mut() { return members; }

	// Duplicate this data for COW
	Ref<GDScriptStructInstanceData> duplicate() const;

	// Serialization
	Dictionary serialize() const;
	bool deserialize(const Dictionary &p_data);

	// Property list for editor/debugger
	void get_property_list(List<PropertyInfo> *p_list) const;

	GDScriptStructInstanceData();
};

// A lightweight wrapper class that the Variant stores by value
// This class implements the COW logic
// Designed to be small and copyable - stored directly in Variant's union
class GDScriptStructInstance {
private:
	// The Variant stores this object by value
	// The Ref<> inside manages the actual data with automatic reference counting
	Ref<GDScriptStructInstanceData> data;

public:
	// Default constructor - creates null instance
	GDScriptStructInstance() = default;

	// Constructor from blueprint - creates new instance data
	GDScriptStructInstance(const Ref<GDScriptStruct> &p_blueprint);

	// Constructor from instance data - wraps existing data (used internally)
	GDScriptStructInstance(const Ref<GDScriptStructInstanceData> &p_data);

	// Copy constructor - just copies the Ref (fast, no deep copy)
	GDScriptStructInstance(const GDScriptStructInstance &p_other) = default;

	// Move constructor
	GDScriptStructInstance(GDScriptStructInstance &&p_other) noexcept = default;

	// Assignment operator - implements COW
	GDScriptStructInstance &operator=(const GDScriptStructInstance &p_other);

	// Move assignment
	GDScriptStructInstance &operator=(GDScriptStructInstance &&p_other) noexcept = default;

	// Destructor - automatic cleanup through Ref<>
	~GDScriptStructInstance() = default;

	// Check if instance is valid
	bool is_valid() const { return data.is_valid(); }
	bool is_null() const { return data.is_null(); }

	// Ensure we have a unique copy (call this before modifying if needed)
	void _ensure_unique();

	// Member access - const getter (no COW needed)
	bool get(const StringName &p_name, Variant &r_value) const;
	Variant get_by_index(int p_index) const;

	// Member access - setter (implements COW)
	bool set(const StringName &p_name, const Variant &p_value);
	void set_by_index(int p_index, const Variant &p_value);

	// Direct pointer access (for VM optimization - caller must ensure unique first)
	// Returns pointer to member value, or nullptr if not found
	Variant *get_member_ptr(const StringName &p_name);
	Variant *get_member_ptr_by_index(int p_index);

	// Method calling
	Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) const;

	// Type information
	Ref<GDScriptStruct> get_struct_type() const;
	StringName get_struct_name() const;

	// Property list for editor/debugger
	void get_property_list(List<PropertyInfo> *p_list) const;

	// Serialization
	Dictionary serialize() const;
	bool deserialize(const Dictionary &p_data);

	// Get the underlying data (for advanced use)
	const Ref<GDScriptStructInstanceData> &get_data() const { return data; }
};
