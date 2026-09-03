/**************************************************************************/
/*  struct.cpp                                                            */
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

#include "struct.h"

#include "core/os/memory.h"
#include "core/variant/struct_info.h"
#include "core/variant/variant.h"

#include <type_traits>

static_assert(!std::is_copy_constructible_v<StructInfoBuilder>);
static_assert(!std::is_copy_assignable_v<StructInfoBuilder>);
static_assert(std::is_move_constructible_v<StructInfoBuilder>);
static_assert(std::is_move_assignable_v<StructInfoBuilder>);

static_assert(std::is_nothrow_default_constructible_v<Struct>);
static_assert(std::is_nothrow_move_constructible_v<Struct>);
static_assert(std::is_nothrow_move_assignable_v<Struct>);
static_assert(std::is_nothrow_destructible_v<Struct>);

struct StructData {
	Ref<StructInfo> info;
	Variant *values = nullptr;
	int field_count = 0;

	static StructData *create(const Ref<StructInfo> &p_info) {
		ERR_FAIL_COND_V_MSG(p_info.is_null(), nullptr,
				"Cannot construct a Struct without a schema.");
		ERR_FAIL_COND_V_MSG(!p_info->is_frozen(), nullptr,
				"Cannot construct a Struct from an unfinished (unfrozen) schema.");

		StructData *d = memnew(StructData);
		d->info = p_info;
		d->field_count = p_info->get_field_count();
		if (d->field_count > 0) {
			d->values = memnew_arr(Variant, d->field_count);
			for (int i = 0; i < d->field_count; i++) {
				d->values[i] = p_info->instantiate_default(i);
			}
		}
		return d;
	}

	~StructData() {
		if (values) {
			memdelete_arr(values);
			values = nullptr;
		}
	}
};

StructData *Struct::_copy_data(const StructData *p_from) {
	if (!p_from) {
		return nullptr;
	}
	DEV_ASSERT(p_from->info.is_valid());
	DEV_ASSERT(p_from->info->is_frozen());
	DEV_ASSERT(p_from->field_count >= 0);

	StructData *d = memnew(StructData);
	d->info = p_from->info;
	d->field_count = p_from->field_count;
	if (d->field_count > 0) {
		d->values = memnew_arr(Variant, d->field_count);
		for (int i = 0; i < d->field_count; i++) {
			d->values[i] = p_from->values[i];
		}
	}
	return d;
}

void Struct::_free_data() noexcept {
	if (_p) {
		memdelete(_p);
		_p = nullptr;
	}
}

Struct::Struct(const Ref<StructInfo> &p_info) {
	_p = StructData::create(p_info);
}

Struct::Struct(const Struct &p_from) {
	_p = _copy_data(p_from._p);
}

Struct::Struct(Struct &&p_from) noexcept {
	_p = p_from._p;
	p_from._p = nullptr;
}

Struct &Struct::operator=(const Struct &p_from) {
	if (this == &p_from) {
		return *this;
	}
	_free_data();
	_p = _copy_data(p_from._p);
	return *this;
}

Struct &Struct::operator=(Struct &&p_from) noexcept {
	if (this == &p_from) {
		return *this;
	}
	_free_data();
	_p = p_from._p;
	p_from._p = nullptr;
	return *this;
}

Struct::~Struct() noexcept {
	_free_data();
}

Ref<StructInfo> Struct::get_info() const {
	return _p ? _p->info : Ref<StructInfo>();
}

StringName Struct::get_type_id() const {
	// A non-null _p always has a valid, frozen info (see create()/_copy_data()).
	return _p ? _p->info->get_logical_type_id() : StringName();
}

uint64_t Struct::get_layout_hash() const noexcept {
	return _p ? _p->info->get_layout_hash() : 0;
}

int Struct::get_field_count() const noexcept {
	return _p ? _p->field_count : 0;
}

Variant Struct::get_member(int p_index) const {
	ERR_FAIL_NULL_V(_p, Variant());
	ERR_FAIL_INDEX_V(p_index, _p->field_count, Variant());
	return _p->values[p_index];
}

Variant Struct::_make_serializable(const Variant &p_value) {
	if (p_value.get_type() == Variant::OBJECT && p_value.get_validated_object() == nullptr) {
		return Variant();
	}
	return p_value;
}

Variant Struct::get_member_serializable(int p_index) const {
	return _make_serializable(get_member(p_index));
}

void Struct::set_member(int p_index, const Variant &p_value) {
	ERR_FAIL_NULL(_p);
	ERR_FAIL_INDEX(p_index, _p->field_count);
	Variant normalized;
	ERR_FAIL_COND_MSG(!_p->info->normalize_value(p_index, p_value, normalized),
			vformat(R"(Value of type "%s" is incompatible with struct field %d.)",
					Variant::get_type_name(p_value.get_type()), p_index));
	_p->values[p_index] = normalized;
}

bool Struct::get_named(const StringName &p_name, Variant &r_value) const {
	// A non-null _p always has a valid info; a valid index_of() result is always
	// within [0, field_count) since both derive from the same frozen schema.
	if (!_p) {
		return false;
	}
	const int idx = _p->info->index_of(p_name);
	if (idx < 0) {
		return false;
	}
	DEV_ASSERT(idx < _p->field_count);
	r_value = _p->values[idx];
	return true;
}

bool Struct::set_named(const StringName &p_name, const Variant &p_value) {
	if (!_p) {
		return false;
	}
	const int idx = _p->info->index_of(p_name);
	if (idx < 0) {
		return false;
	}
	DEV_ASSERT(idx < _p->field_count);
	Variant normalized;
	ERR_FAIL_COND_V_MSG(!_p->info->normalize_value(idx, p_value, normalized), false,
			vformat(R"(Value of type "%s" is incompatible with struct field "%s".)",
					Variant::get_type_name(p_value.get_type()), p_name));
	_p->values[idx] = normalized;
	return true;
}

uint32_t Struct::recursive_hash(int recursion_count) const {
	if (recursion_count > MAX_RECURSION) {
		ERR_PRINT("Max recursion reached");
		return 0;
	}
	uint32_t h = hash_murmur3_one_32(Variant::STRUCT);
	if (_p) {
		h = hash_murmur3_one_32(_p->info->get_logical_type_id().hash(), h);
		h = hash_murmur3_one_64(_p->info->get_layout_hash(), h);
		recursion_count++;
		for (int i = 0; i < _p->field_count; i++) {
			h = hash_murmur3_one_32(_p->values[i].recursive_hash(recursion_count), h);
		}
	}
	return hash_fmix32(h);
}

int Struct::index_of(const StringName &p_name) const {
	return _p ? _p->info->index_of(p_name) : -1;
}

bool Struct::try_set_member(int p_index, const Variant &p_value) {
	Variant normalized;
	if (!_p || p_index < 0 || p_index >= _p->field_count || !_p->info->normalize_value(p_index, p_value, normalized)) {
		return false;
	}
	_p->values[p_index] = normalized;
	return true;
}

Struct Struct::recursive_duplicate(bool p_deep, ResourceDeepDuplicateMode p_deep_subresources_mode, int recursion_count) const {
	if (!p_deep || !_p) {
		return *this;
	}
	if (recursion_count > MAX_RECURSION) {
		ERR_PRINT("Max recursion reached");
		return *this;
	}
	recursion_count++;
	Struct dup = *this;
	for (int i = 0; i < dup._p->field_count; i++) {
		dup._p->values[i] = dup._p->values[i].recursive_duplicate(p_deep, p_deep_subresources_mode, recursion_count);
	}
	return dup;
}

bool Struct::operator==(const Struct &p_other) const {
	if (_p == p_other._p) {
		return true;
	}
	if (!_p || !p_other._p) {
		return false;
	}
	const StructInfo *lhs_info = _p->info.ptr();
	const StructInfo *rhs_info = p_other._p->info.ptr();
	// Same frozen schema => same layout (and same field_count); skip the
	// structural comparison in the common case where instances share a schema.
	if (lhs_info != rhs_info && !lhs_info->is_same_layout_as(*rhs_info)) {
		return false;
	}
	for (int i = 0; i < _p->field_count; i++) {
		if (!(_p->values[i] == p_other._p->values[i])) {
			return false;
		}
	}
	return true;
}
