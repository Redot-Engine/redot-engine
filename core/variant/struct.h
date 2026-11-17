/**************************************************************************/
/*  struct.h                                                              */
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

#include "core/string/string_name.h"
#include "core/variant/variant_deep_duplicate.h"

template <typename T>
class Ref;
class Variant;
class StructInfo;
struct StructData;

class Struct {
	StructData *_p = nullptr;

	void _free_data() noexcept;
	static StructData *_copy_data(const StructData *p_from);

public:
	Struct() noexcept = default;
	explicit Struct(const Ref<StructInfo> &p_info);
	Struct(const Struct &p_from);
	Struct(Struct &&p_from) noexcept;
	Struct &operator=(const Struct &p_from);
	Struct &operator=(Struct &&p_from) noexcept;
	~Struct() noexcept;

	bool is_null() const noexcept { return _p == nullptr; }

	Ref<StructInfo> get_info() const;
	StringName get_type_id() const;
	uint64_t get_layout_hash() const noexcept;
	int get_field_count() const noexcept;

	Variant get_member(int p_index) const;
	void set_member(int p_index, const Variant &p_value);

	static Variant _make_serializable(const Variant &p_value);
	Variant get_member_serializable(int p_index) const;

	bool get_named(const StringName &p_name, Variant &r_value) const;
	bool set_named(const StringName &p_name, const Variant &p_value);

	int index_of(const StringName &p_name) const;
	bool try_set_member(int p_index, const Variant &p_value);

	uint32_t recursive_hash(int recursion_count) const;
	Struct recursive_duplicate(bool p_deep, ResourceDeepDuplicateMode p_deep_subresources_mode, int recursion_count) const;

	bool operator==(const Struct &p_other) const;
	bool operator!=(const Struct &p_other) const { return !(*this == p_other); }
};
