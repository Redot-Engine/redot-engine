/**************************************************************************/
/*  struct_info.cpp                                                       */
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

#include "struct_info.h"

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/struct.h"
#include "core/variant/variant_internal.h"

const char *StructInfo::type_to_token(Variant::Type p_type) {
	switch (p_type) {
		case Variant::NIL:
			return "nil";
		case Variant::BOOL:
			return "bool";
		case Variant::INT:
			return "int";
		case Variant::FLOAT:
			return "float";
		case Variant::STRING:
			return "string";
		case Variant::VECTOR2:
			return "vector2";
		case Variant::VECTOR2I:
			return "vector2i";
		case Variant::RECT2:
			return "rect2";
		case Variant::RECT2I:
			return "rect2i";
		case Variant::VECTOR3:
			return "vector3";
		case Variant::VECTOR3I:
			return "vector3i";
		case Variant::TRANSFORM2D:
			return "transform2d";
		case Variant::VECTOR4:
			return "vector4";
		case Variant::VECTOR4I:
			return "vector4i";
		case Variant::PLANE:
			return "plane";
		case Variant::QUATERNION:
			return "quaternion";
		case Variant::AABB:
			return "aabb";
		case Variant::BASIS:
			return "basis";
		case Variant::TRANSFORM3D:
			return "transform3d";
		case Variant::PROJECTION:
			return "projection";
		case Variant::COLOR:
			return "color";
		case Variant::STRING_NAME:
			return "string_name";
		case Variant::NODE_PATH:
			return "node_path";
		case Variant::RID:
			return "rid";
		case Variant::OBJECT:
			return "object";
		case Variant::CALLABLE:
			return "callable";
		case Variant::SIGNAL:
			return "signal";
		case Variant::DICTIONARY:
			return "dictionary";
		case Variant::ARRAY:
			return "array";
		case Variant::PACKED_BYTE_ARRAY:
			return "packed_byte_array";
		case Variant::PACKED_INT32_ARRAY:
			return "packed_int32_array";
		case Variant::PACKED_INT64_ARRAY:
			return "packed_int64_array";
		case Variant::PACKED_FLOAT32_ARRAY:
			return "packed_float32_array";
		case Variant::PACKED_FLOAT64_ARRAY:
			return "packed_float64_array";
		case Variant::PACKED_STRING_ARRAY:
			return "packed_string_array";
		case Variant::PACKED_VECTOR2_ARRAY:
			return "packed_vector2_array";
		case Variant::PACKED_VECTOR3_ARRAY:
			return "packed_vector3_array";
		case Variant::PACKED_COLOR_ARRAY:
			return "packed_color_array";
		case Variant::PACKED_VECTOR4_ARRAY:
			return "packed_vector4_array";
		case Variant::STRUCT:
			return "struct";
		case Variant::VARIANT_MAX:
			return "";
	}
	return "";
}

Variant::Type StructInfo::type_from_token(const String &p_token) {
	for (int t = 0; t < Variant::VARIANT_MAX; t++) {
		if (p_token == type_to_token((Variant::Type)t)) {
			return (Variant::Type)t;
		}
	}
	return Variant::VARIANT_MAX;
}

static void _append_u32_le(Vector<uint8_t> &r_buf, uint32_t p_value) {
	r_buf.push_back(p_value & 0xFF);
	r_buf.push_back((p_value >> 8) & 0xFF);
	r_buf.push_back((p_value >> 16) & 0xFF);
	r_buf.push_back((p_value >> 24) & 0xFF);
}

static void _append_lp(Vector<uint8_t> &r_buf, const String &p_str) {
	const CharString utf8 = p_str.utf8();
	_append_u32_le(r_buf, (uint32_t)utf8.length());
	const uint8_t *data = (const uint8_t *)utf8.get_data();
	for (int i = 0; i < utf8.length(); i++) {
		r_buf.push_back(data[i]);
	}
}

static uint32_t _murmur3_32_le(const uint8_t *p_data, int p_len, uint32_t p_seed) {
	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;
	uint32_t h = p_seed;
	const int nblocks = p_len / 4;
	for (int i = 0; i < nblocks; i++) {
		const int o = i * 4;
		uint32_t k = uint32_t(p_data[o]) | (uint32_t(p_data[o + 1]) << 8) | (uint32_t(p_data[o + 2]) << 16) | (uint32_t(p_data[o + 3]) << 24);
		k *= c1;
		k = hash_rotl32(k, 15);
		k *= c2;
		h ^= k;
		h = hash_rotl32(h, 13);
		h = h * 5 + 0xe6546b64;
	}
	const int tail = nblocks * 4;
	uint32_t k = 0;
	switch (p_len & 3) {
		case 3:
			k ^= uint32_t(p_data[tail + 2]) << 16;
			[[fallthrough]];
		case 2:
			k ^= uint32_t(p_data[tail + 1]) << 8;
			[[fallthrough]];
		case 1:
			k ^= uint32_t(p_data[tail + 0]);
			k *= c1;
			k = hash_rotl32(k, 15);
			k *= c2;
			h ^= k;
			break;
		default:
			break;
	}
	h ^= uint32_t(p_len);
	return hash_fmix32(h);
}

static String _h128_hex(const Vector<uint8_t> &p_bytes) {
	static const uint32_t seeds[4] = { 0x00000000, 0x9E3779B9, 0x85EBCA6B, 0xC2B2AE35 };
	uint8_t out[16];
	for (int k = 0; k < 4; k++) {
		const uint32_t h = _murmur3_32_le(p_bytes.ptr(), p_bytes.size(), seeds[k]);
		out[k * 4 + 0] = h & 0xFF;
		out[k * 4 + 1] = (h >> 8) & 0xFF;
		out[k * 4 + 2] = (h >> 16) & 0xFF;
		out[k * 4 + 3] = (h >> 24) & 0xFF;
	}
	return String::hex_encode_buffer(out, 16);
}

Vector<uint8_t> StructInfo::get_layout_descriptor() const {
	Vector<uint8_t> buf;
	buf.push_back(1);
	_append_u32_le(buf, (uint32_t)fields.size());
	for (const Field &f : fields) {
		_append_lp(buf, String(f.name));
		_append_lp(buf, String(type_to_token(f.type)));
		buf.push_back(f.is_typed ? 1 : 0);
		_append_lp(buf, String(f.class_name));
		_append_lp(buf, String(f.struct_type_id));
	}
	return buf;
}

String StructInfo::get_layout_fingerprint() const {
	return _h128_hex(get_layout_descriptor());
}

String StructInfo::get_schema_fingerprint() const {
	Vector<uint8_t> buf;
	_append_lp(buf, String(logical_type_id));
	buf.append_array(get_layout_descriptor());
	return _h128_hex(buf);
}

bool StructInfo::_field_metadata_ok(const Field &p_field, const Variant &p_value) {
	if (p_field.type == Variant::OBJECT && p_field.class_name != StringName()) {
		Object *obj = p_value.get_validated_object();
		if (obj == nullptr) {
			return true;
		}
		const StringName &obj_class = obj->get_class_name();
		return obj_class == p_field.class_name || ClassDB::is_parent_class(obj_class, p_field.class_name);
	}
	if (p_field.type == Variant::STRUCT && p_field.struct_type_id != StringName()) {
		const Struct *s = VariantInternal::get_struct(&p_value);
		return s->is_null() || s->get_type_id() == p_field.struct_type_id;
	}
	return true;
}
