/**************************************************************************/
/*  struct_info.h                                                         */
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

#include "core/error/error_macros.h"
#include "core/object/ref_counted.h"
#include "core/string/string_name.h"
#include "core/templates/hash_map.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

class StructInfoBuilder;

// Variant::Type suffix for every field type stored at native width in a Struct's
// packed block.
#define STRUCT_NATIVE_STORAGE_TYPES(M)          \
	M(BOOL, bool)                               \
	M(INT, int64_t)                             \
	M(FLOAT, double)                            \
	M(STRING, String)                           \
	M(VECTOR2, Vector2)                         \
	M(VECTOR2I, Vector2i)                       \
	M(RECT2, Rect2)                             \
	M(RECT2I, Rect2i)                           \
	M(VECTOR3, Vector3)                         \
	M(VECTOR3I, Vector3i)                       \
	M(TRANSFORM2D, Transform2D)                 \
	M(VECTOR4, Vector4)                         \
	M(VECTOR4I, Vector4i)                       \
	M(PLANE, Plane)                             \
	M(QUATERNION, Quaternion)                   \
	M(AABB, AABB)                               \
	M(BASIS, Basis)                             \
	M(TRANSFORM3D, Transform3D)                 \
	M(PROJECTION, Projection)                   \
	M(COLOR, Color)                             \
	M(STRING_NAME, StringName)                  \
	M(NODE_PATH, NodePath)                      \
	M(RID, RID)                                 \
	M(CALLABLE, Callable)                       \
	M(SIGNAL, Signal)                           \
	M(DICTIONARY, Dictionary)                   \
	M(ARRAY, Array)                             \
	M(PACKED_BYTE_ARRAY, PackedByteArray)       \
	M(PACKED_INT32_ARRAY, PackedInt32Array)     \
	M(PACKED_INT64_ARRAY, PackedInt64Array)     \
	M(PACKED_FLOAT32_ARRAY, PackedFloat32Array) \
	M(PACKED_FLOAT64_ARRAY, PackedFloat64Array) \
	M(PACKED_STRING_ARRAY, PackedStringArray)   \
	M(PACKED_VECTOR2_ARRAY, PackedVector2Array) \
	M(PACKED_VECTOR3_ARRAY, PackedVector3Array) \
	M(PACKED_COLOR_ARRAY, PackedColorArray)     \
	M(PACKED_VECTOR4_ARRAY, PackedVector4Array)

class StructInfo : public RefCounted {
	GDCLASS(StructInfo, RefCounted);
	friend class StructInfoBuilder;

public:
	// How a field is physically stored in a Struct's packed data block. Specialized leaf types
	// live at native width; everything else (untyped, or types not yet specialized) falls back
	// to a full Variant.
	enum FieldStorage : uint8_t {
		STORAGE_VARIANT,
#define _STRUCT_STORAGE_ENUM(m_suffix, m_type) STORAGE_##m_suffix,
		STRUCT_NATIVE_STORAGE_TYPES(_STRUCT_STORAGE_ENUM)
#undef _STRUCT_STORAGE_ENUM
	};

	struct Field {
		StringName name;
		Variant::Type type = Variant::NIL;
		bool is_typed = false;
		bool is_nullable = false;
		StringName class_name;
		StringName struct_type_id;
		Variant default_value;
		FieldStorage storage = STORAGE_VARIANT;
		uint32_t offset = 0;
	};

	static void storage_traits(FieldStorage p_storage, size_t &r_size, size_t &r_align) {
		switch (p_storage) {
#define _STRUCT_STORAGE_TRAITS(m_suffix, m_type) \
	case STORAGE_##m_suffix:                     \
		r_size = sizeof(m_type);                 \
		r_align = alignof(m_type);               \
		return;
			STRUCT_NATIVE_STORAGE_TYPES(_STRUCT_STORAGE_TRAITS)
#undef _STRUCT_STORAGE_TRAITS
			default:
				r_size = sizeof(Variant);
				r_align = alignof(Variant);
				return;
		}
	}

private:
	static FieldStorage _storage_for(const Field &p_field) {
		if (p_field.is_typed && !p_field.is_nullable) {
			switch (p_field.type) {
#define _STRUCT_STORAGE_FOR(m_suffix, m_type) \
	case Variant::m_suffix:                   \
		return STORAGE_##m_suffix;
				STRUCT_NATIVE_STORAGE_TYPES(_STRUCT_STORAGE_FOR)
#undef _STRUCT_STORAGE_FOR
				default:
					break;
			}
		}
		return STORAGE_VARIANT;
	}

	StringName logical_type_id;
	Vector<Field> fields;
	HashMap<StringName, int> index_by_name;
	uint32_t data_size = 0;
	uint32_t data_align = 1;
	uint64_t layout_hash = 0;
	bool frozen = false;

	void set_logical_type_id(const StringName &p_id) {
		ERR_FAIL_COND_MSG(frozen, "Cannot modify a frozen StructInfo.");
		logical_type_id = p_id;
	}

	int add_field(const Field &p_field) {
		ERR_FAIL_COND_V_MSG(frozen, -1, "Cannot modify a frozen StructInfo.");
		ERR_FAIL_COND_V_MSG(index_by_name.has(p_field.name), -1,
				vformat(R"(Duplicate struct field "%s".)", p_field.name));
		Field stored = p_field;
		stored.default_value = p_field.default_value.duplicate(true);
		const int idx = fields.size();
		fields.push_back(stored);
		index_by_name[stored.name] = idx;
		return idx;
	}

	Error freeze() {
		ERR_FAIL_COND_V_MSG(frozen, ERR_ALREADY_EXISTS, "StructInfo is already frozen.");
		ERR_FAIL_COND_V_MSG(logical_type_id == StringName(), ERR_INVALID_DATA,
				"StructInfo requires a non-empty logical type id.");
		for (const Field &f : fields) {
			ERR_FAIL_COND_V_MSG(f.name == StringName(), ERR_INVALID_DATA,
					"StructInfo has a field with an empty name.");
			if (f.is_typed) {
				ERR_FAIL_COND_V_MSG(f.type == Variant::NIL, ERR_INVALID_DATA,
						vformat(R"(Typed struct field "%s" requires a concrete type.)", f.name));
			} else {
				ERR_FAIL_COND_V_MSG(f.type != Variant::NIL || f.class_name != StringName() || f.struct_type_id != StringName(),
						ERR_INVALID_DATA, vformat(R"(Untyped struct field "%s" carries typed metadata.)", f.name));
			}
			ERR_FAIL_COND_V_MSG(!_value_matches_field(f, f.default_value), ERR_INVALID_DATA,
					vformat(R"(Struct field "%s" default value is incompatible with its declared type.)", f.name));
		}
		for (int i = 0; i < fields.size(); i++) {
			Field &f = fields.write[i];
			f.default_value = _coerce_value(f, f.default_value);
			if (!(f.is_typed && f.type == Variant::OBJECT)) {
				f.class_name = StringName();
			}
			if (!(f.is_typed && f.type == Variant::STRUCT)) {
				f.struct_type_id = StringName();
			}
		}
		uint64_t h = hash_murmur3_one_64(fields.size());
		for (const Field &f : fields) {
			h = hash_murmur3_one_64(f.name.hash(), h);
			h = hash_murmur3_one_64((uint64_t)f.type, h);
			h = hash_murmur3_one_64(f.is_typed ? 1 : 0, h);
			h = hash_murmur3_one_64(f.class_name.hash(), h);
			h = hash_murmur3_one_64(f.struct_type_id.hash(), h);
		}
		layout_hash = h;

		size_t off = 0;
		size_t max_align = 1;

		for (int i = 0; i < fields.size(); i++) {
			Field &f = fields.write[i];
			f.storage = _storage_for(f);
			size_t sz = 0;
			size_t al = 1;

			storage_traits(f.storage, sz, al);
			off = (off + al - 1) & ~(al - 1);
			f.offset = (uint32_t)off;
			off += sz;

			if (al > max_align) {
				max_align = al;
			}
		}

		data_align = (uint32_t)max_align;
		data_size = (uint32_t)((off + max_align - 1) & ~(max_align - 1));
		frozen = true;

		return OK;
	}
	static bool _field_metadata_ok(const Field &p_field, const Variant &p_value);
	static bool _value_matches_field(const Field &p_field, const Variant &p_value) {
		if (!p_field.is_typed) {
			return true;
		}
		const Variant::Type vt = p_value.get_type();
		if (vt == Variant::NIL) {
			return true;
		}
		if (vt == p_field.type) {
			return _field_metadata_ok(p_field, p_value);
		}
		if ((p_field.type == Variant::INT || p_field.type == Variant::FLOAT) &&
				(vt == Variant::INT || vt == Variant::FLOAT)) {
			return true;
		}
		if ((p_field.type == Variant::STRING || p_field.type == Variant::STRING_NAME) &&
				(vt == Variant::STRING || vt == Variant::STRING_NAME)) {
			return true;
		}
		return false;
	}
	static Variant _coerce_value(const Field &p_field, const Variant &p_value) {
		const Variant::Type vt = p_value.get_type();
		if (!p_field.is_typed || vt == p_field.type || vt == Variant::NIL) {
			return p_value;
		}
		switch (p_field.type) {
			case Variant::INT:
				return p_value.operator int64_t();
			case Variant::FLOAT:
				return p_value.operator double();
			case Variant::STRING:
				return p_value.operator String();
			case Variant::STRING_NAME:
				return p_value.operator StringName();
			default:
				return p_value;
		}
	}

protected:
	static void _bind_methods() {}

public:
	static constexpr uint32_t SERIALIZATION_VERSION = 1;

	bool is_frozen() const noexcept { return frozen; }
	StringName get_logical_type_id() const { return logical_type_id; }
	uint64_t get_layout_hash() const noexcept { return layout_hash; }

	int get_field_count() const noexcept { return fields.size(); }

	uint32_t get_data_size() const noexcept { return data_size; }
	uint32_t get_data_align() const noexcept { return data_align; }
	FieldStorage get_field_storage(int p_index) const {
		CRASH_BAD_INDEX(p_index, fields.size());
		return fields[p_index].storage;
	}
	uint32_t get_field_offset(int p_index) const {
		CRASH_BAD_INDEX(p_index, fields.size());
		return fields[p_index].offset;
	}

	StringName get_field_name(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, fields.size(), StringName());
		return fields[p_index].name;
	}
	Variant::Type get_field_type(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, fields.size(), Variant::NIL);
		return fields[p_index].type;
	}
	bool is_field_typed(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, fields.size(), false);
		return fields[p_index].is_typed;
	}
	StringName get_field_class_name(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, fields.size(), StringName());
		return fields[p_index].class_name;
	}
	StringName get_field_struct_type_id(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, fields.size(), StringName());
		return fields[p_index].struct_type_id;
	}

	int index_of(const StringName &p_name) const {
		const int *idx = index_by_name.getptr(p_name);
		return idx ? *idx : -1;
	}
	bool has_field(const StringName &p_name) const { return index_by_name.has(p_name); }

	Variant instantiate_default(int p_index) const {
		ERR_FAIL_COND_V_MSG(!frozen, Variant(), "Cannot instantiate a default from an unfinished schema.");
		ERR_FAIL_INDEX_V(p_index, fields.size(), Variant());
		return fields[p_index].default_value.duplicate(true);
	}

	const Variant &_get_field_default_raw(int p_index) const {
		CRASH_BAD_INDEX(p_index, fields.size());
		return fields[p_index].default_value;
	}

	bool is_value_compatible(int p_index, const Variant &p_value) const {
		ERR_FAIL_INDEX_V(p_index, fields.size(), false);
		return _value_matches_field(fields[p_index], p_value);
	}

	bool normalize_value(int p_index, const Variant &p_value, Variant &r_normalized) const {
		if (p_index < 0 || p_index >= fields.size()) {
			return false;
		}
		if (!_value_matches_field(fields[p_index], p_value)) {
			return false;
		}
		r_normalized = _coerce_value(fields[p_index], p_value);
		return true;
	}

	Vector<uint8_t> get_layout_descriptor() const;
	String get_layout_fingerprint() const;
	String get_schema_fingerprint() const;
	static const char *type_to_token(Variant::Type p_type);
	static Variant::Type type_from_token(const String &p_token);

	bool is_same_layout_as(const StructInfo &p_other) const {
		if (!frozen || !p_other.frozen) {
			return false;
		}
		if (logical_type_id != p_other.logical_type_id ||
				layout_hash != p_other.layout_hash ||
				fields.size() != p_other.fields.size()) {
			return false; // Fast reject.
		}
		for (int i = 0; i < fields.size(); i++) {
			const Field &a = fields[i];
			const Field &b = p_other.fields[i];
			if (a.name != b.name || a.type != b.type || a.is_typed != b.is_typed ||
					a.class_name != b.class_name || a.struct_type_id != b.struct_type_id) {
				return false;
			}
		}
		return true;
	}
};

class StructInfoBuilder {
	Ref<StructInfo> info;

public:
	StructInfoBuilder() { info.instantiate(); }

	StructInfoBuilder(const StructInfoBuilder &) = delete;
	StructInfoBuilder &operator=(const StructInfoBuilder &) = delete;

	StructInfoBuilder(StructInfoBuilder &&p_other) {
		info = p_other.info;
		p_other.info = Ref<StructInfo>();
	}
	StructInfoBuilder &operator=(StructInfoBuilder &&p_other) {
		if (this != &p_other) {
			info = p_other.info;
			p_other.info = Ref<StructInfo>();
		}
		return *this;
	}

	void set_logical_type_id(const StringName &p_id) {
		ERR_FAIL_COND(info.is_null());
		info->set_logical_type_id(p_id);
	}
	int add_field(const StructInfo::Field &p_field) {
		ERR_FAIL_COND_V(info.is_null(), -1);
		return info->add_field(p_field);
	}

	Ref<StructInfo> build() {
		ERR_FAIL_COND_V_MSG(info.is_null(), Ref<StructInfo>(),
				"StructInfoBuilder has already been consumed.");
		ERR_FAIL_COND_V_MSG(info->freeze() != OK, Ref<StructInfo>(),
				"Failed to finalize StructInfo.");
		Ref<StructInfo> result = info;
		info = Ref<StructInfo>();
		return result;
	}
};
