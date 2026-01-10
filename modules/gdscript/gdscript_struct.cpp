/**************************************************************************/
/*  gdscript_struct.cpp                                                   */
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

#include "gdscript_struct.h"

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/string_name.h"
#include "core/variant/callable.h"
#include "core/variant/variant_utility.h"

GDScriptStruct::GDScriptStruct(const StringName &p_name) :
		name(p_name) {
	// Initialize with ref_count = 1
	// The first reference represents the initial creator's ownership
	// Subsequent owners must call reference() to take ownership
	ref_count.init(1);
}

GDScriptStruct::~GDScriptStruct() {
}

void GDScriptStruct::reference() {
	ref_count.ref();
}

bool GDScriptStruct::unreference() {
	return ref_count.unref();
}

int GDScriptStruct::get_member_count() const {
	// Return total member count including inherited members
	int count = members.size();
	if (base_struct) {
		count += base_struct->get_member_count();
	}
	return count;
}

GDScriptStructInstance *GDScriptStruct::create_instance(const Variant **p_args, int p_argcount) {
	// Allocate instance
	GDScriptStructInstance *instance = memnew(GDScriptStructInstance(this));
	if (instance == nullptr) {
		ERR_FAIL_V_MSG(nullptr, vformat("Failed to allocate memory for struct '%s'.", name));
	}

	// Validate argument count matches expected member count
	int expected_args = member_names.size();
	if (p_argcount > expected_args) {
		memdelete(instance);
		ERR_FAIL_V_MSG(nullptr, vformat("Too many arguments for struct '%s': expected %d, got %d.", name, expected_args, p_argcount));
	}

	// Apply positional arguments to struct members in declaration order
	// This mimics constructor behavior: Struct.new(arg1, arg2, ...)
	int arg_idx = 0;
	for (const StringName &member_name : member_names) {
		if (arg_idx < p_argcount) {
			const MemberInfo *info = members.getptr(member_name);
			if (info) {
				const Variant &arg_value = *p_args[arg_idx];

				// Validate argument type against member's declared type
				bool type_valid = true;
				if (info->type != Variant::NIL && arg_value.get_type() != Variant::NIL) {
					// Check if the argument type matches the expected type
					if (info->type == Variant::OBJECT) {
						// For Object types, require the argument to be an Object
						if (arg_value.get_type() != Variant::OBJECT) {
							type_valid = false;
						} else if (!info->type_name.is_empty()) {
							// If a specific class is requested, validate the object's class
							Object *obj = arg_value.get_validated_object();
							if (obj == nullptr) {
								type_valid = false;
							} else {
								// Check if the object is an instance of the expected class
								type_valid = ClassDB::is_parent_class(obj->get_class_name(), info->type_name);
							}
						}
						// If type_name is empty, accept any Object
					} else if (arg_value.get_type() != info->type) {
						// Type mismatch - try to convert
						Variant converted;
						if (Variant::can_convert(arg_value.get_type(), info->type)) {
							converted = VariantUtilityFunctions::type_convert(arg_value, info->type);
							type_valid = (converted.get_type() == info->type);
							if (type_valid) {
								instance->members.write[info->index] = converted;
							}
						} else {
							type_valid = false;
						}
					}
				}

				if (!type_valid) {
					memdelete(instance);
					ERR_FAIL_V_MSG(nullptr, vformat("Type mismatch for struct '%s' member '%s': expected %s, got %s.", name, member_name, Variant::get_type_name(info->type), Variant::get_type_name(arg_value.get_type())));
				}

				// Type is valid (or passed validation), assign the value
				if (info->type != Variant::NIL && arg_value.get_type() != Variant::NIL && arg_value.get_type() != info->type) {
					// Already converted above, or will use the original value
				} else {
					instance->members.write[info->index] = arg_value;
				}
			}
			arg_idx++;
		} else {
			break; // No more arguments
		}
	}

	// TODO: Call actual _init constructor function if it exists
	// For now, we just apply the arguments directly to members

	return instance;
}

void GDScriptStruct::add_member(const StringName &p_name, const Variant::Type p_type, const StringName &p_type_name, const Variant &p_default_value, bool p_has_default_value) {
	ERR_FAIL_COND(members.has(p_name));

	MemberInfo info;
	info.index = get_member_count(); // Index after all inherited members
	info.type = p_type;
	info.type_name = p_type_name;
	info.default_value = p_default_value;
	info.has_default_value = p_has_default_value;

	members[p_name] = info;
	member_names.push_back(p_name);
	member_types.push_back(p_type);
}

bool GDScriptStruct::has_member(const StringName &p_name) const {
	if (members.has(p_name)) {
		return true;
	}

	// Check base struct
	if (base_struct) {
		return base_struct->has_member(p_name);
	}

	return false;
}

int GDScriptStruct::get_member_index(const StringName &p_name) const {
	const MemberInfo *info = members.getptr(p_name);
	if (info) {
		return info->index;
	}

	// Check base struct
	if (base_struct) {
		return base_struct->get_member_index(p_name);
	}

	return -1;
}

void GDScriptStruct::add_method(const StringName &p_name, GDScriptFunction *p_function, bool p_is_static) {
	ERR_FAIL_COND(methods.has(p_name));

	MethodInfo info;
	info.name = p_name;
	info.function = p_function;
	info.is_static = p_is_static;

	methods[p_name] = info;
	method_names.push_back(p_name);
}

bool GDScriptStruct::has_method(const StringName &p_name) const {
	if (methods.has(p_name)) {
		return true;
	}

	// Check base struct
	if (base_struct) {
		return base_struct->has_method(p_name);
	}

	return false;
}

bool GDScriptStruct::is_child_of(const GDScriptStruct *p_struct) const {
	if (this == p_struct) {
		return true;
	}

	if (base_struct) {
		return base_struct->is_child_of(p_struct);
	}

	return false;
}

// GDScriptStructInstance implementation

GDScriptStructInstance::GDScriptStructInstance(GDScriptStruct *p_struct_type) :
		struct_type(p_struct_type) {
	ERR_FAIL_NULL(struct_type);

	// Calculate total member count including inherited members
	int total_count = struct_type->get_member_count();
	members.resize(total_count);

	// Set default values by walking the inheritance chain
	// Start from the base struct and work down to the derived struct
	// This ensures derived struct defaults override base struct defaults
	Vector<GDScriptStruct *> chain;
	GDScriptStruct *current = struct_type;
	while (current) {
		chain.push_back(current);
		current = current->get_base_struct();
	}

	// Iterate in reverse order (base to derived)
	for (int i = chain.size() - 1; i >= 0; i--) {
		GDScriptStruct *struct_in_chain = chain[i];
		const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_in_chain->get_members();

		for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
			const GDScriptStruct::MemberInfo &info = E.value;
			if (info.has_default_value) {
				members.write[info.index] = info.default_value;
			}
		}
	}
}

GDScriptStructInstance::~GDScriptStructInstance() {
	// Variant members will clean themselves up
}

bool GDScriptStructInstance::reference() {
	return ref_count.ref();
}

bool GDScriptStructInstance::unreference() {
	if (ref_count.unref()) {
		// Reference count reached zero, delete this instance
		memdelete(this);
		return true;
	}
	return false;
}

bool GDScriptStructInstance::set(const StringName &p_name, const Variant &p_value) {
	int index = struct_type->get_member_index(p_name);
	if (index < 0) {
		return false;
	}

	members.write[index] = p_value;
	return true;
}

bool GDScriptStructInstance::get(const StringName &p_name, Variant &r_value) const {
	int index = struct_type->get_member_index(p_name);
	if (index < 0) {
		return false;
	}

	r_value = members[index];
	return true;
}

Variant *GDScriptStructInstance::get_member_ptr(const StringName &p_name) {
	int index = struct_type->get_member_index(p_name);
	if (index < 0 || index >= members.size()) {
		return nullptr;
	}

	return &members.write[index];
}

Variant GDScriptStructInstance::call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	// Look up the method in the struct type
	const GDScriptStruct::MethodInfo *method_info = nullptr;

	// Check current struct
	if (struct_type->get_methods().has(p_method)) {
		method_info = &struct_type->get_methods().get(p_method);
	}

	// Check base struct if not found
	if (method_info == nullptr && struct_type->get_base_struct() != nullptr) {
		GDScriptStruct *base = struct_type->get_base_struct();
		while (base != nullptr) {
			if (base->get_methods().has(p_method)) {
				method_info = &base->get_methods().get(p_method);
				break;
			}
			base = base->get_base_struct();
		}
	}

	if (method_info == nullptr) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		r_error.argument = 0;
		r_error.expected = 0;
		return Variant();
	}

	// Check if this is a static method
	// For now, only static methods are supported on structs
	// Non-static methods would require VM integration to provide 'self'
	if (!method_info->is_static) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		r_error.argument = 0;
		r_error.expected = 0;
		ERR_FAIL_V_MSG(Variant(), vformat("Non-static struct methods are not yet supported. Cannot call instance method '%s' on struct '%s'.", p_method, struct_type->get_name()));
	}

	// Static method - call via GDScriptFunction
	if (method_info->function != nullptr) {
		// For static methods, we can pass nullptr as the instance
		// Static methods don't access instance members
		return method_info->function->call(nullptr, p_args, p_argcount, r_error);
	}

	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	r_error.argument = 0;
	r_error.expected = 0;
	return Variant();
}

StringName GDScriptStructInstance::get_struct_name() const {
	return struct_type->get_name();
}

void GDScriptStructInstance::get_property_list(List<PropertyInfo> *p_list) const {
	const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_type->get_members();

	for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
		p_list->push_back(E.value.property_info);
	}
}

Dictionary GDScriptStructInstance::serialize() const {
	Dictionary data;

	// Store type name
	data["__type__"] = struct_type->get_fully_qualified_name();

	// Serialize all members from the entire inheritance chain
	// Walk from base to derived to ensure consistent ordering
	Vector<GDScriptStruct *> chain;
	GDScriptStruct *current = struct_type;
	while (current) {
		chain.push_back(current);
		current = current->get_base_struct();
	}

	// Iterate in reverse order (base to derived)
	for (int i = chain.size() - 1; i >= 0; i--) {
		GDScriptStruct *struct_in_chain = chain[i];
		const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_in_chain->get_members();

		for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
			const Variant &value = members[E.value.index];
			data[E.key] = value;
		}
	}

	return data;
}

bool GDScriptStructInstance::deserialize(const Dictionary &p_data) {
	// Deserialize all members from the entire inheritance chain
	// Walk from base to derived to ensure consistent ordering
	Vector<GDScriptStruct *> chain;
	GDScriptStruct *current = struct_type;
	while (current) {
		chain.push_back(current);
		current = current->get_base_struct();
	}

	// Iterate in reverse order (base to derived)
	for (int i = chain.size() - 1; i >= 0; i--) {
		GDScriptStruct *struct_in_chain = chain[i];
		const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_in_chain->get_members();

		for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
			if (p_data.has(E.key)) {
				members.write[E.value.index] = p_data[E.key];
			}
		}
	}

	return true;
}
