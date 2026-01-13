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

// GDScriptStruct implementation

void GDScriptStruct::_bind_methods() {
	// Struct is a blueprint class, no methods to bind directly
}

GDScriptStruct::GDScriptStruct() :
		RefCounted() {
	// RefCounted manages reference counting automatically
}

GDScriptStruct::GDScriptStruct(const StringName &p_name) :
		RefCounted(),
		name(p_name) {
	// RefCounted manages reference counting automatically
}

GDScriptStruct::~GDScriptStruct() {
	// Clean up constructor function pointer
	if (constructor != nullptr) {
		delete constructor;
		constructor = nullptr;
	}

	// Clean up method function pointers
	for (const KeyValue<StringName, MethodInfo> &E : methods) {
		if (E.value.function != nullptr) {
			delete E.value.function;
		}
	}
	methods.clear();
}

Variant GDScriptStruct::create_variant_instance(const Variant **p_args, int p_argcount) {
	// Create the instance data
	Ref<GDScriptStructInstanceData> instance_data = GDScriptStructInstanceData::create(Ref<GDScriptStruct>(this));

	// Validate argument count matches expected member count
	int expected_args = member_names.size();
	if (p_argcount > expected_args) {
		ERR_FAIL_V_MSG(Variant(), vformat("Too many arguments for struct '%s': expected %d, got %d.", name, expected_args, p_argcount));
	}

	// Apply positional arguments to struct members in declaration order
	int arg_idx = 0;
	for (const StringName &member_name : member_names) {
		if (arg_idx < p_argcount) {
			const MemberInfo *info = members.getptr(member_name);
			if (info) {
				const Variant &arg_value = *p_args[arg_idx];

				// Validate argument type against member's declared type
				bool type_valid = true;
				if (info->type != Variant::NIL && arg_value.get_type() != Variant::NIL) {
					if (info->type == Variant::OBJECT) {
						if (arg_value.get_type() != Variant::OBJECT) {
							type_valid = false;
						} else if (!info->type_name.is_empty()) {
							Object *obj = arg_value.get_validated_object();
							if (obj == nullptr) {
								type_valid = false;
							} else {
								type_valid = ClassDB::is_parent_class(obj->get_class_name(), info->type_name);
							}
						}
					} else if (arg_value.get_type() != info->type) {
						Variant converted;
						if (Variant::can_convert(arg_value.get_type(), info->type)) {
							converted = VariantUtilityFunctions::type_convert(arg_value, info->type);
							type_valid = (converted.get_type() == info->type);
							if (type_valid) {
								instance_data->set_member_direct(info->index, converted);
							}
						} else {
							type_valid = false;
						}
					}
				}

				if (!type_valid) {
					ERR_FAIL_V_MSG(Variant(), vformat("Type mismatch for struct '%s' member '%s': expected %s, got %s.", name, member_name, Variant::get_type_name(info->type), Variant::get_type_name(arg_value.get_type())));
				}

				// Type is valid (or passed validation), assign the value
				if (info->type != Variant::NIL && arg_value.get_type() != Variant::NIL && arg_value.get_type() != info->type) {
					// Already converted above
				} else {
					instance_data->set_member_direct(info->index, arg_value);
				}
			}
			arg_idx++;
		} else {
			break;
		}
	}

	// Create and return the wrapper as a Variant
	GDScriptStructInstance wrapper(instance_data);
	Variant result;
	result.type = Variant::STRUCT;
	// Use placement new to construct the wrapper in the Variant's memory
	new (result._data._mem) GDScriptStructInstance(wrapper);
	return result;
}

int GDScriptStruct::get_member_count() const {
	int count = members.size();
	if (base_struct.is_valid()) {
		count += base_struct->get_member_count();
	}
	return count;
}

void GDScriptStruct::add_member(const StringName &p_name, const Variant::Type p_type, const StringName &p_type_name, const Variant &p_default_value, bool p_has_default_value) {
	ERR_FAIL_COND(members.has(p_name));

	if (base_struct.is_valid() && base_struct->has_member(p_name)) {
		ERR_FAIL_MSG(vformat("Cannot add member '%s': already defined in base struct '%s'.", p_name, base_struct->get_name()));
	}

	MemberInfo info;
	info.index = get_member_count();
	info.type = p_type;
	info.type_name = p_type_name;
	info.default_value = p_default_value;
	info.has_default_value = p_has_default_value;

	info.property_info.name = p_name;
	info.property_info.type = p_type;
	if (p_type == Variant::OBJECT && !p_type_name.is_empty()) {
		info.property_info.class_name = p_type_name;
	} else if (p_type == Variant::STRUCT && !p_type_name.is_empty()) {
		info.property_info.class_name = p_type_name;
		info.property_info.hint = PROPERTY_HINT_TYPE_STRING;
	}
	info.property_info.usage = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR;

	members[p_name] = info;
	member_names.push_back(p_name);
	member_types.push_back(p_type);
}

bool GDScriptStruct::has_member(const StringName &p_name) const {
	if (members.has(p_name)) {
		return true;
	}
	if (base_struct.is_valid()) {
		return base_struct->has_member(p_name);
	}
	return false;
}

int GDScriptStruct::get_member_index(const StringName &p_name) const {
	const MemberInfo *info = members.getptr(p_name);
	if (info) {
		return info->index;
	}
	if (base_struct.is_valid()) {
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
	if (base_struct.is_valid()) {
		return base_struct->has_method(p_name);
	}
	return false;
}

bool GDScriptStruct::is_child_of(const GDScriptStruct *p_struct) const {
	if (this == p_struct) {
		return true;
	}
	if (base_struct.is_valid()) {
		return base_struct->is_child_of(p_struct);
	}
	return false;
}

// GDScriptStructInstanceData implementation

void GDScriptStructInstanceData::_bind_methods() {
	// Instance data is internal, no methods to bind
}

GDScriptStructInstanceData::GDScriptStructInstanceData() {
}

Ref<GDScriptStructInstanceData> GDScriptStructInstanceData::create(const Ref<GDScriptStruct> &p_blueprint) {
	ERR_FAIL_COND_V_MSG(!p_blueprint.is_valid(), Ref<GDScriptStructInstanceData>(), "Cannot create struct instance data from null blueprint");

	Ref<GDScriptStructInstanceData> data;
	data.instantiate();

	data->blueprint = p_blueprint;

	// Calculate total member count including inherited members
	int total_count = p_blueprint->get_member_count();
	data->members.resize(total_count);

	// Set default values by walking the inheritance chain
	// Start from the base struct and work down to the derived struct
	Vector<Ref<GDScriptStruct>> chain;
	Ref<GDScriptStruct> current = p_blueprint;
	while (current.is_valid()) {
		chain.push_back(current);
		current = current->get_base_struct();
	}

	// Iterate in reverse order (base to derived)
	for (int i = chain.size() - 1; i >= 0; i--) {
		Ref<GDScriptStruct> struct_in_chain = chain[i];
		const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_in_chain->get_members();

		for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
			const GDScriptStruct::MemberInfo &info = E.value;
			if (info.has_default_value) {
				data->members.write[info.index] = info.default_value;
			}
		}
	}

	return data;
}

Variant GDScriptStructInstanceData::get_member_direct(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, members.size(), Variant());
	return members[p_index];
}

void GDScriptStructInstanceData::set_member_direct(int p_index, const Variant &p_value) {
	ERR_FAIL_INDEX(p_index, members.size());
	members.write[p_index] = p_value;
}

Ref<GDScriptStructInstanceData> GDScriptStructInstanceData::duplicate() const {
	Ref<GDScriptStructInstanceData> new_data;
	new_data.instantiate();

	new_data->blueprint = blueprint;
	new_data->members = members; // Vector makes a copy of Variants

	return new_data;
}

Dictionary GDScriptStructInstanceData::serialize() const {
	Dictionary data;

	if (blueprint.is_valid()) {
		data["__type__"] = blueprint->get_fully_qualified_name();

		// Serialize all members from the entire inheritance chain
		Vector<Ref<GDScriptStruct>> chain;
		Ref<GDScriptStruct> current = blueprint;
		while (current.is_valid()) {
			chain.push_back(current);
			current = current->get_base_struct();
		}

		// Iterate in reverse order (base to derived)
		for (int i = chain.size() - 1; i >= 0; i--) {
			Ref<GDScriptStruct> struct_in_chain = chain[i];
			const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_in_chain->get_members();

			for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
				const Variant &value = members[E.value.index];
				data[E.key] = value;
			}
		}
	}

	return data;
}

bool GDScriptStructInstanceData::deserialize(const Dictionary &p_data) {
	if (!blueprint.is_valid()) {
		return false;
	}

	// Check type safety
	if (p_data.has("__type__")) {
		String stored_type = p_data["__type__"];
		String expected_type = blueprint->get_fully_qualified_name();
		if (stored_type != expected_type) {
			return false;
		}
	}

	// Deserialize all members from the entire inheritance chain
	Vector<Ref<GDScriptStruct>> chain;
	Ref<GDScriptStruct> current = blueprint;
	while (current.is_valid()) {
		chain.push_back(current);
		current = current->get_base_struct();
	}

	// Iterate in reverse order (base to derived)
	for (int i = chain.size() - 1; i >= 0; i--) {
		Ref<GDScriptStruct> struct_in_chain = chain[i];
		const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_in_chain->get_members();

		for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
			if (p_data.has(E.key)) {
				members.write[E.value.index] = p_data[E.key];
			}
		}
	}

	return true;
}

void GDScriptStructInstanceData::get_property_list(List<PropertyInfo> *p_list) const {
	if (blueprint.is_valid()) {
		// Walk the inheritance chain to include inherited members
		Vector<Ref<GDScriptStruct>> chain;
		Ref<GDScriptStruct> current = blueprint;
		while (current.is_valid()) {
			chain.push_back(current);
			current = current->get_base_struct();
		}

		// Iterate in reverse order (base to derived) so derived members come last
		for (int i = chain.size() - 1; i >= 0; i--) {
			Ref<GDScriptStruct> struct_in_chain = chain[i];
			const HashMap<StringName, GDScriptStruct::MemberInfo> &struct_members = struct_in_chain->get_members();

			for (const KeyValue<StringName, GDScriptStruct::MemberInfo> &E : struct_members) {
				p_list->push_back(E.value.property_info);
			}
		}
	}
}

// GDScriptStructInstance implementation

GDScriptStructInstance::GDScriptStructInstance(const Ref<GDScriptStruct> &p_blueprint) {
	if (p_blueprint.is_valid()) {
		data = GDScriptStructInstanceData::create(p_blueprint);
	}
}

GDScriptStructInstance::GDScriptStructInstance(const Ref<GDScriptStructInstanceData> &p_data) :
		data(p_data) {
	// Just wrap the existing data - Ref<> handles reference counting
}

GDScriptStructInstance &GDScriptStructInstance::operator=(const GDScriptStructInstance &p_other) {
	data = p_other.data;
	return *this;
}

void GDScriptStructInstance::_ensure_unique() {
	if (data.is_valid() && data->get_reference_count() > 1) {
		// Make a unique copy for COW
		data = data->duplicate();
	}
}

bool GDScriptStructInstance::get(const StringName &p_name, Variant &r_value) const {
	if (data.is_null()) {
		return false;
	}

	Ref<GDScriptStruct> struct_type = data->get_blueprint();
	if (struct_type.is_null()) {
		return false;
	}

	int index = struct_type->get_member_index(p_name);
	if (index < 0) {
		return false;
	}

	if (index >= data->get_members().size()) {
		return false;
	}

	r_value = data->get_member_direct(index);
	return true;
}

Variant GDScriptStructInstance::get_by_index(int p_index) const {
	if (data.is_null()) {
		return Variant();
	}

	if (p_index < 0 || p_index >= data->get_members().size()) {
		return Variant();
	}

	return data->get_member_direct(p_index);
}

bool GDScriptStructInstance::set(const StringName &p_name, const Variant &p_value) {
	if (data.is_null()) {
		return false;
	}

	// COW: Ensure unique copy before modifying
	_ensure_unique();

	Ref<GDScriptStruct> struct_type = data->get_blueprint();
	if (struct_type.is_null()) {
		return false;
	}

	int index = struct_type->get_member_index(p_name);
	if (index < 0) {
		return false;
	}

	data->set_member_direct(index, p_value);
	return true;
}

void GDScriptStructInstance::set_by_index(int p_index, const Variant &p_value) {
	if (data.is_null()) {
		return;
	}

	// COW: Ensure unique copy before modifying
	_ensure_unique();

	if (p_index < 0 || p_index >= data->get_members().size()) {
		return;
	}

	data->set_member_direct(p_index, p_value);
}

Variant *GDScriptStructInstance::get_member_ptr(const StringName &p_name) {
	if (data.is_null()) {
		return nullptr;
	}

	Ref<GDScriptStruct> struct_type = data->get_blueprint();
	if (struct_type.is_null()) {
		return nullptr;
	}

	int index = struct_type->get_member_index(p_name);
	if (index < 0) {
		return nullptr;
	}

	return get_member_ptr_by_index(index);
}

Variant *GDScriptStructInstance::get_member_ptr_by_index(int p_index) {
	if (data.is_null()) {
		return nullptr;
	}

	if (p_index < 0 || p_index >= data->get_members().size()) {
		return nullptr;
	}

	// COW: Ensure unique copy before returning a modifiable pointer
	_ensure_unique();

	return &data->get_members_mut().write[p_index];
}

Variant GDScriptStructInstance::call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) const {
	if (data.is_null()) {
		r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}

	Ref<GDScriptStruct> struct_type = data->get_blueprint();
	if (struct_type.is_null()) {
		r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}

	// Look up the method
	const GDScriptStruct::MethodInfo *method_info = nullptr;

	if (struct_type->get_methods().has(p_method)) {
		method_info = &struct_type->get_methods().get(p_method);
	}

	if (method_info == nullptr && struct_type->get_base_struct().is_valid()) {
		Ref<GDScriptStruct> base = struct_type->get_base_struct();
		while (base.is_valid()) {
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

	if (!method_info->is_static) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		r_error.argument = 0;
		r_error.expected = 0;
		ERR_FAIL_V_MSG(Variant(), vformat("Non-static struct methods are not yet supported. Cannot call instance method '%s' on struct '%s'.", p_method, struct_type->get_name()));
	}

	if (method_info->function != nullptr) {
		return method_info->function->call(nullptr, p_args, p_argcount, r_error);
	}

	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	r_error.argument = 0;
	r_error.expected = 0;
	return Variant();
}

Ref<GDScriptStruct> GDScriptStructInstance::get_struct_type() const {
	if (data.is_null()) {
		return Ref<GDScriptStruct>();
	}
	return data->get_blueprint();
}

StringName GDScriptStructInstance::get_struct_name() const {
	Ref<GDScriptStruct> struct_type = get_struct_type();
	if (struct_type.is_null()) {
		return StringName();
	}
	return struct_type->get_name();
}

void GDScriptStructInstance::get_property_list(List<PropertyInfo> *p_list) const {
	if (data.is_valid()) {
		data->get_property_list(p_list);
	}
}

Dictionary GDScriptStructInstance::serialize() const {
	if (data.is_null()) {
		return Dictionary();
	}
	return data->serialize();
}

bool GDScriptStructInstance::deserialize(const Dictionary &p_data) {
	if (data.is_null()) {
		return false;
	}
	return data->deserialize(p_data);
}
