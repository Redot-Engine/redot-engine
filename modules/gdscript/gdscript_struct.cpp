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

#include "core/object/object.h"
#include "core/string/string_name.h"
#include "core/variant/callable.h"

GDScriptStruct::GDScriptStruct(const StringName &p_name) :
		name(p_name) {
	// Initialize with ref_count = 0
	// References will be added by owners (script, wrapper class)
	ref_count.init(0);
}

GDScriptStruct::~GDScriptStruct() {
	// Clean up children
	for (GDScriptStruct *child : children) {
		// Unreference children before deleting
		child->unreference();
		if (child->get_reference_count() == 0) {
			memdelete(child);
		}
	}
}

void GDScriptStruct::reference() {
	ref_count.ref();
}

void GDScriptStruct::unreference() {
	ref_count.unref();
}

GDScriptStructInstance *GDScriptStruct::create_instance(const Variant **p_args, int p_argcount) {
	GDScriptStructInstance *instance = memnew(GDScriptStructInstance(this));

	// Apply positional arguments to struct members in declaration order
	// This mimics constructor behavior: Struct.new(arg1, arg2, ...)
	int arg_idx = 0;
	for (const StringName &member_name : member_names) {
		if (arg_idx < p_argcount) {
			const MemberInfo *info = members.getptr(member_name);
			if (info) {
				instance->members.write[info->index] = *p_args[arg_idx];
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

void GDScriptStruct::add_member(const StringName &p_name, const Variant::Type p_type, const StringName &p_type_name) {
	ERR_FAIL_COND(members.has(p_name));

	MemberInfo info;
	info.index = members.size();
	info.type = p_type;
	info.type_name = p_type_name;

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

	// Initialize members
	const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_type->get_members();
	members.resize(struct_members.size());

	// Set default values
	for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
		const GDScriptStruct::MemberInfo &info = E.value;
		if (info.has_default_value) {
			members.write[info.index] = info.default_value;
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
	return ref_count.unref();
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
	// TODO: Implement struct method calling
	// For now, struct methods are not fully implemented
	// They need a separate calling mechanism from GDScriptFunction

	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
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

	// Serialize all members
	const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_type->get_members();

	for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
		const Variant &value = members[E.value.index];
		data[E.key] = value;
	}

	return data;
}

bool GDScriptStructInstance::deserialize(const Dictionary &p_data) {
	// For now, just deserialize members
	const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_type->get_members();

	for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
		if (p_data.has(E.key)) {
			members.write[E.value.index] = p_data[E.key];
		}
	}

	return true;
}
