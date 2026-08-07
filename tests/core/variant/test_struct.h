/**************************************************************************/
/*  test_struct.h                                                         */
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

#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/variant/struct.h"
#include "core/variant/struct_info.h"
#include "core/variant/variant_internal.h"
#include "core/variant/variant_parser.h"

#include "tests/test_macros.h"
#include "tests/test_utils.h"

#include <iterator>
#include <utility>

namespace TestStruct {

static Ref<StructInfo> make_point() {
	StructInfoBuilder b;
	b.set_logical_type_id("Point");
	StructInfo::Field fx;
	fx.name = "x";
	fx.type = Variant::INT;
	fx.is_typed = true;
	fx.default_value = 0;
	b.add_field(fx);
	StructInfo::Field fy;
	fy.name = "y";
	fy.type = Variant::INT;
	fy.is_typed = true;
	fy.default_value = 0;
	b.add_field(fy);
	return b.build();
}

static Ref<StructInfo> make_bag(const Array &p_default = Array()) {
	StructInfoBuilder b;
	b.set_logical_type_id("Bag");
	StructInfo::Field f;
	f.name = "items";
	f.type = Variant::ARRAY;
	f.is_typed = true;
	f.default_value = p_default;
	b.add_field(f);
	return b.build();
}

TEST_CASE("[Struct] Construction and basic field access") {
	Ref<StructInfo> info = make_point();
	Struct s(info);
	CHECK_FALSE(s.is_null());
	CHECK(s.get_field_count() == 2);
	CHECK(s.get_type_id() == StringName("Point"));

	s.set_member(0, 5);
	CHECK(int(s.get_member(0)) == 5);

	CHECK(s.set_named("y", 7));
	Variant v;
	CHECK(s.get_named("y", v));
	CHECK(int(v) == 7);
}

TEST_CASE("[Struct] A null schema yields a null struct") {
	Ref<StructInfo> null_info;
	ERR_PRINT_OFF;
	Struct s(null_info);
	ERR_PRINT_ON;
	CHECK(s.is_null());
}

TEST_CASE("[Struct] An unfrozen schema is rejected") {
	Ref<StructInfo> raw;
	raw.instantiate();
	ERR_PRINT_OFF;
	Struct s(raw);
	ERR_PRINT_ON;
	CHECK(s.is_null());
}

TEST_CASE("[Struct] The builder is single-use and non-reusable") {
	StructInfoBuilder b;
	b.set_logical_type_id("Point");
	StructInfo::Field f;
	f.name = "x";
	f.type = Variant::INT;
	f.is_typed = true;
	b.add_field(f);
	Ref<StructInfo> info = b.build();
	CHECK(info.is_valid());

	ERR_PRINT_OFF;
	StructInfo::Field g;
	g.name = "y";
	CHECK(b.add_field(g) == -1);
	CHECK(b.build().is_null());
	ERR_PRINT_ON;
}

TEST_CASE("[Struct] Moving a builder transfers construction ownership") {
	StructInfoBuilder source;
	source.set_logical_type_id("MovedSchema");
	StructInfo::Field f;
	f.name = "value";
	f.type = Variant::INT;
	f.is_typed = true;
	source.add_field(f);

	StructInfoBuilder destination = std::move(source);
	Ref<StructInfo> info = destination.build();
	CHECK(info.is_valid());

	ERR_PRINT_OFF;
	CHECK(source.build().is_null());
	ERR_PRINT_ON;
}

TEST_CASE("[Struct] Duplicate field names are rejected") {
	StructInfoBuilder b;
	b.set_logical_type_id("Dup");
	StructInfo::Field f;
	f.name = "x";
	f.type = Variant::INT;
	f.is_typed = true;
	CHECK(b.add_field(f) == 0);
	ERR_PRINT_OFF;
	CHECK(b.add_field(f) == -1);
	ERR_PRINT_ON;
}

TEST_CASE("[Struct] An empty logical type id cannot be built") {
	StructInfoBuilder b;
	StructInfo::Field f;
	f.name = "x";
	f.type = Variant::INT;
	f.is_typed = true;
	b.add_field(f);
	ERR_PRINT_OFF;
	Ref<StructInfo> info = b.build();
	ERR_PRINT_ON;
	CHECK(info.is_null());
}

TEST_CASE("[Struct] An empty field name cannot be built") {
	StructInfoBuilder b;
	b.set_logical_type_id("Invalid");
	StructInfo::Field f;
	f.type = Variant::INT;
	f.is_typed = true;
	b.add_field(f);
	ERR_PRINT_OFF;
	Ref<StructInfo> info = b.build();
	ERR_PRINT_ON;
	CHECK(info.is_null());
}

TEST_CASE("[Struct] Untyped fields cannot carry typed metadata") {
	StructInfoBuilder b;
	b.set_logical_type_id("InvalidUntyped");
	StructInfo::Field f;
	f.name = "value";
	f.type = Variant::INT;
	f.is_typed = false;
	b.add_field(f);
	ERR_PRINT_OFF;
	Ref<StructInfo> info = b.build();
	ERR_PRINT_ON;
	CHECK(info.is_null());
}

TEST_CASE("[Struct] Typed fields require a concrete type") {
	StructInfoBuilder b;
	b.set_logical_type_id("InvalidTyped");
	StructInfo::Field f;
	f.name = "value";
	f.type = Variant::NIL;
	f.is_typed = true;
	b.add_field(f);
	ERR_PRINT_OFF;
	Ref<StructInfo> info = b.build();
	ERR_PRINT_ON;
	CHECK(info.is_null());
}

TEST_CASE("[Struct] A typed field's default must match its declared type") {
	StructInfoBuilder b;
	b.set_logical_type_id("BadDefault");
	StructInfo::Field f;
	f.name = "count";
	f.type = Variant::INT;
	f.is_typed = true;
	f.default_value = "not an int";
	b.add_field(f);
	ERR_PRINT_OFF;
	Ref<StructInfo> info = b.build();
	ERR_PRINT_ON;
	CHECK(info.is_null());
}

TEST_CASE("[Struct] Typed field assignment rejects incompatible values") {
	Ref<StructInfo> info = make_point();
	Struct s(info);

	s.set_member(0, 42);
	CHECK(int(s.get_member(0)) == 42);
	CHECK(s.set_named("y", 7));

	ERR_PRINT_OFF;
	s.set_member(0, "text");
	CHECK_FALSE(s.set_named("y", "text"));
	ERR_PRINT_ON;

	CHECK(int(s.get_member(0)) == 42);
	Variant v;
	s.get_named("y", v);
	CHECK(int(v) == 7);
}

TEST_CASE("[Struct] Two fresh instances receive independent default containers") {
	Ref<StructInfo> info = make_bag();
	Struct s1(info);
	Struct s2(info);

	Array a1 = s1.get_member(0);
	a1.push_back(1);

	Array a2 = s2.get_member(0);
	CHECK(a1.size() == 1);
	CHECK(a2.size() == 0);
}

TEST_CASE("[Struct] Copy shares reference-like members (Variant-standard)") {
	Ref<StructInfo> info = make_bag();
	Struct s1(info);
	Array a = s1.get_member(0);
	a.push_back(7);

	Struct s2 = s1;
	Array a2 = s2.get_member(0);
	CHECK(a2.size() == 1);

	a2.push_back(8);
	Array a1 = s1.get_member(0);
	CHECK(a1.size() == 2);
}

TEST_CASE("[Struct] Copy keeps value-like members independent") {
	Ref<StructInfo> info = make_point();
	Struct s1(info);
	s1.set_member(0, 5);

	Struct s2 = s1;
	s2.set_member(0, 9);
	CHECK(int(s1.get_member(0)) == 5);
	CHECK(int(s2.get_member(0)) == 9);
}

TEST_CASE("[Struct] Move construction and assignment leave the source null") {
	Ref<StructInfo> info = make_point();

	Struct s(info);
	Struct moved = std::move(s);
	CHECK(s.is_null());
	CHECK_FALSE(moved.is_null());
	CHECK(moved.get_field_count() == 2);

	Struct a(info);
	Struct b(info);
	b = std::move(a);
	CHECK(a.is_null());
	CHECK_FALSE(b.is_null());
}

TEST_CASE("[Struct] Named and indexed access are bounds-checked") {
	Ref<StructInfo> info = make_point();
	Struct s(info);

	Variant v;
	CHECK_FALSE(s.get_named("nope", v));
	CHECK_FALSE(s.set_named("nope", 1));

	ERR_PRINT_OFF;
	CHECK(s.get_member(99).get_type() == Variant::NIL);
	s.set_member(99, 1);
	ERR_PRINT_ON;
}

TEST_CASE("[Struct] A builder caller cannot mutate the canonical default afterward") {
	Array outside;
	Ref<StructInfo> info = make_bag(outside);

	outside.push_back(99);

	Struct s(info);
	Array items = s.get_member(0);
	CHECK(items.size() == 0);
}

TEST_CASE("[Struct] Equality uses logical identity + exact layout, not pointer") {
	Ref<StructInfo> p1 = make_point();
	Ref<StructInfo> p2 = make_point();

	Struct a(p1);
	a.set_member(0, 1);
	a.set_member(1, 2);
	Struct b(p2);
	b.set_member(0, 1);
	b.set_member(1, 2);
	CHECK(a == b);

	b.set_member(1, 3);
	CHECK(a != b);

	StructInfoBuilder rb;
	rb.set_logical_type_id("Point");
	StructInfo::Field fy;
	fy.name = "y";
	fy.type = Variant::INT;
	fy.is_typed = true;
	fy.default_value = 0;
	rb.add_field(fy);
	StructInfo::Field fx;
	fx.name = "x";
	fx.type = Variant::INT;
	fx.is_typed = true;
	fx.default_value = 0;
	rb.add_field(fx);
	Ref<StructInfo> reordered = rb.build();

	Struct c(reordered);
	c.set_member(0, 2);
	c.set_member(1, 1);
	CHECK(a != c);
}

TEST_CASE("[Struct] Round-trips through a Variant with value semantics") {
	Ref<StructInfo> info = make_point();
	Struct s(info);
	s.set_member(0, 3);
	s.set_member(1, 4);

	Variant v = s;
	CHECK(v.get_type() == Variant::STRUCT);

	Variant v2 = v;
	Struct back = v2;
	CHECK(int(back.get_member(0)) == 3);
	CHECK(int(back.get_member(1)) == 4);

	back.set_member(0, 99);
	CHECK(int(s.get_member(0)) == 3);

	Variant same = s;
	CHECK(v == same);
	Struct other(info);
	other.set_member(0, 3);
	other.set_member(1, 5);
	Variant different = other;
	CHECK(v != different);
}

TEST_CASE("[Struct] Variant copy allocates independent StructData") {
	Ref<StructInfo> info = make_point();
	Variant v = Struct(info);
	VariantInternal::get_struct(&v)->set_member(0, 3);

	Variant v2 = v;

	VariantInternal::get_struct(&v2)->set_member(0, 99);
	CHECK(int(VariantInternal::get_struct(&v)->get_member(0)) == 3);
	CHECK(int(VariantInternal::get_struct(&v2)->get_member(0)) == 99);
}

TEST_CASE("[Struct] Same-type Variant assignment keeps independent storage") {
	Ref<StructInfo> info = make_point();
	Variant v = Struct(info);
	VariantInternal::get_struct(&v)->set_member(0, 3);

	Variant assigned = Struct(info);
	assigned = v;

	VariantInternal::get_struct(&assigned)->set_member(0, 77);
	CHECK(int(VariantInternal::get_struct(&v)->get_member(0)) == 3);
	CHECK(int(VariantInternal::get_struct(&assigned)->get_member(0)) == 77);
}

TEST_CASE("[Struct] Works as a dictionary key (hash + equality)") {
	Ref<StructInfo> info = make_point();
	Struct k(info);
	k.set_member(0, 1);
	k.set_member(1, 2);

	Dictionary d;
	d[Variant(k)] = 42;

	Struct same(info);
	same.set_member(0, 1);
	same.set_member(1, 2);
	CHECK(d.has(Variant(same)));
	CHECK(int(d[Variant(same)]) == 42);

	Struct different(info);
	different.set_member(0, 9);
	different.set_member(1, 9);
	CHECK_FALSE(d.has(Variant(different)));

	StructInfoBuilder rb;
	rb.set_logical_type_id("Point");
	StructInfo::Field fy;
	fy.name = "y";
	fy.type = Variant::INT;
	fy.is_typed = true;
	fy.default_value = 0;
	rb.add_field(fy);
	StructInfo::Field fx;
	fx.name = "x";
	fx.type = Variant::INT;
	fx.is_typed = true;
	fx.default_value = 0;
	rb.add_field(fx);
	Struct reordered(rb.build());
	reordered.set_member(0, 2);
	reordered.set_member(1, 1);
	CHECK_FALSE(d.has(Variant(reordered)));
}

TEST_CASE("[Struct] Variant is keyed and supports has_key") {
	Ref<StructInfo> info = make_point();
	Variant v = Struct(info);
	CHECK(Variant::is_keyed(Variant::STRUCT));

	bool valid = false;
	CHECK(v.has_key(StringName("x"), valid));
	CHECK(valid);
	CHECK(v.has_key("y", valid));
	CHECK(valid);
	CHECK_FALSE(v.has_key("missing", valid));
	CHECK(valid);
	CHECK(v.has_key(0, valid));
	CHECK(valid);
	CHECK_FALSE(v.has_key(99, valid));
	CHECK(valid);
	CHECK_FALSE(v.has_key(Vector2(), valid));
	CHECK(valid);
}

TEST_CASE("[Struct] Variant keyed get/set by field name") {
	Ref<StructInfo> info = make_point();
	Variant v = Struct(info);
	bool valid = false;

	v.set(StringName("x"), 5, &valid);
	CHECK(valid);
	CHECK(int(v.get(StringName("x"), &valid)) == 5);
	CHECK(valid);

	v.set("y", 9, &valid);
	CHECK(valid);
	CHECK(int(v.get("y", &valid)) == 9);
	CHECK(valid);

	v.get("nope", &valid);
	CHECK_FALSE(valid);
	v.set("nope", 1, &valid);
	CHECK_FALSE(valid);
}

TEST_CASE("[Struct] Variant keyed get/set by index") {
	Ref<StructInfo> info = make_point();
	Variant v = Struct(info);
	bool valid = false;

	v.set(0, 11, &valid);
	CHECK(valid);
	CHECK(int(v.get(0, &valid)) == 11);
	CHECK(valid);

	v.get(99, &valid);
	CHECK_FALSE(valid);
	v.set(99, 1, &valid);
	CHECK_FALSE(valid);
}

TEST_CASE("[Struct] Variant keyed set rejects incompatible type without emitting an error") {
	Ref<StructInfo> info = make_point();
	Variant v = Struct(info);
	bool valid = true;

	v.set("x", "not an int", &valid);
	CHECK_FALSE(valid);
	CHECK(int(v.get("x", &valid)) == 0);
	CHECK(valid);
}

TEST_CASE("[Struct] normalize_value rejects an out-of-range index without a diagnostic") {
	Ref<StructInfo> info = make_point();
	Variant out = 123;
	CHECK_FALSE(info->normalize_value(99, 5, out));
	CHECK(int(out) == 123);
	CHECK_FALSE(info->normalize_value(-1, 5, out));
	CHECK(int(out) == 123);
}

TEST_CASE("[Struct] Hash is member-aware and consistent with equality") {
	Ref<StructInfo> info = make_point();
	Struct a(info);
	a.set_member(0, 1);
	a.set_member(1, 2);
	Struct b(info);
	b.set_member(0, 1);
	b.set_member(1, 2);
	Struct c(info);
	c.set_member(0, 9);
	c.set_member(1, 9);

	CHECK(a == b);
	CHECK(Variant(a).hash() == Variant(b).hash());
	CHECK(a != c);
	CHECK(Variant(a).hash() != Variant(c).hash());
}

static Ref<StructInfo> make_typed(const StringName &p_id, const StringName &p_field, Variant::Type p_type, const Variant &p_default) {
	StructInfoBuilder b;
	b.set_logical_type_id(p_id);
	StructInfo::Field f;
	f.name = p_field;
	f.type = p_type;
	f.is_typed = true;
	f.default_value = p_default;
	b.add_field(f);
	return b.build();
}

TEST_CASE("[Struct] Typed field coerces between int and float (both directions)") {
	Struct fs(make_typed("F", "x", Variant::FLOAT, 0.0));
	fs.set_member(0, 5);
	CHECK(fs.get_member(0).get_type() == Variant::FLOAT);
	CHECK(double(fs.get_member(0)) == 5.0);

	Struct is(make_typed("I", "x", Variant::INT, 0));
	is.set_member(0, 5.7);
	CHECK(is.get_member(0).get_type() == Variant::INT);
	CHECK(int(is.get_member(0)) == 5);
}

TEST_CASE("[Struct] Typed field coerces between String and StringName (both directions)") {
	Struct sn(make_typed("SN", "n", Variant::STRING_NAME, StringName()));
	sn.set_member(0, String("hello"));
	CHECK(sn.get_member(0).get_type() == Variant::STRING_NAME);
	CHECK(sn.get_member(0) == Variant(StringName("hello")));

	Struct st(make_typed("ST", "s", Variant::STRING, String()));
	st.set_member(0, StringName("world"));
	CHECK(st.get_member(0).get_type() == Variant::STRING);
	CHECK(st.get_member(0) == Variant(String("world")));
}

TEST_CASE("[Struct] Typed field default is coerced at freeze") {
	Struct s(make_typed("Vec", "x", Variant::FLOAT, 0));
	CHECK(s.get_member(0).get_type() == Variant::FLOAT);
}

TEST_CASE("[Struct] instantiate_default isolates mutable container defaults") {
	StructInfoBuilder b;
	b.set_logical_type_id("MutableDefaults");
	Array arr_default;
	arr_default.push_back(1);
	StructInfo::Field fa;
	fa.name = "items";
	fa.default_value = arr_default;
	b.add_field(fa);
	Dictionary dict_default;
	dict_default["k"] = 1;
	StructInfo::Field fd;
	fd.name = "map";
	fd.default_value = dict_default;
	b.add_field(fd);
	Ref<StructInfo> info = b.build();

	Array first_arr = info->instantiate_default(0);
	first_arr.push_back(2);
	Array second_arr = info->instantiate_default(0);
	CHECK(second_arr.size() == 1);
	CHECK(int(second_arr[0]) == 1);

	Dictionary first_dict = info->instantiate_default(1);
	first_dict["k"] = 99;
	Dictionary second_dict = info->instantiate_default(1);
	CHECK(int(second_dict["k"]) == 1);

	Struct s(info);
	CHECK(Array(s.get_member(0)).size() == 1);
	CHECK(int(Dictionary(s.get_member(1))["k"]) == 1);
}

TEST_CASE("[Struct] Typed STRUCT field enforces struct_type_id") {
	StructInfoBuilder b;
	b.set_logical_type_id("Line");
	StructInfo::Field f;
	f.name = "a";
	f.type = Variant::STRUCT;
	f.is_typed = true;
	f.struct_type_id = "Point";
	b.add_field(f);
	Struct line(b.build());

	Struct point(make_point());
	CHECK(line.set_named("a", point));

	Struct other(make_typed("Other", "z", Variant::INT, 0));
	ERR_PRINT_OFF;
	CHECK_FALSE(line.set_named("a", other));
	ERR_PRINT_ON;
}

TEST_CASE("[Struct] Typed OBJECT field enforces class_name (subclasses and null accepted)") {
	StructInfoBuilder b;
	b.set_logical_type_id("Holder");
	StructInfo::Field f;
	f.name = "o";
	f.type = Variant::OBJECT;
	f.is_typed = true;
	f.class_name = "RefCounted";
	b.add_field(f);
	Struct holder(b.build());

	Ref<RefCounted> rc;
	rc.instantiate();
	CHECK(holder.set_named("o", rc));

	Ref<Resource> res;
	res.instantiate();
	CHECK(holder.set_named("o", res));

	CHECK(holder.set_named("o", Variant()));

	Object *obj = memnew(Object);
	ERR_PRINT_OFF;
	CHECK_FALSE(holder.set_named("o", Variant(obj)));
	ERR_PRINT_ON;
	memdelete(obj);
}

TEST_CASE("[Struct] Typed STRUCT field is a nominal (type-id) constraint, not exact-layout") {
	StructInfoBuilder b;
	b.set_logical_type_id("Container");
	StructInfo::Field f;
	f.name = "p";
	f.type = Variant::STRUCT;
	f.is_typed = true;
	f.struct_type_id = "Point";
	b.add_field(f);
	Struct container(b.build());

	StructInfoBuilder ob;
	ob.set_logical_type_id("Point");
	StructInfo::Field of;
	of.name = "different";
	of.type = Variant::INT;
	of.is_typed = true;
	of.default_value = 0;
	ob.add_field(of);
	Struct fake_point(ob.build());

	CHECK(container.set_named("p", fake_point));
}

TEST_CASE("[Struct] Deep duplicate recurses into container members; shallow shares them") {
	Struct s(make_bag());
	Array nested;
	nested.push_back(5);
	s.set_member(0, nested);

	Variant deep = Variant(s).duplicate(true);
	Array deep_arr = VariantInternal::get_struct(&deep)->get_member(0);
	deep_arr.push_back(99);
	CHECK(Array(s.get_member(0)).size() == 1);

	Variant shallow = Variant(s).duplicate(false);
	Array shallow_arr = VariantInternal::get_struct(&shallow)->get_member(0);
	shallow_arr.push_back(7);
	CHECK(Array(s.get_member(0)).size() == 2);
}

TEST_CASE("[Struct] Struct casts to itself (cast tables)") {
	CHECK(Variant::can_convert(Variant::STRUCT, Variant::STRUCT));
	CHECK(Variant::can_convert_strict(Variant::STRUCT, Variant::STRUCT));
}

TEST_CASE("[Struct] Typed Array of structs accepts structs and rejects others") {
	Array arr;
	arr.set_typed(Variant::STRUCT, StringName(), Variant());
	CHECK(arr.is_typed());
	CHECK(arr.get_typed_builtin() == (uint32_t)Variant::STRUCT);

	arr.push_back(Variant(Struct(make_point())));
	CHECK(arr.size() == 1);
	CHECK(arr[0].get_type() == Variant::STRUCT);

	ERR_PRINT_OFF;
	arr.push_back(5);
	ERR_PRINT_ON;
	CHECK(arr.size() == 1);
}

TEST_CASE("[Struct][D8] Type tokens round-trip for every Variant type") {
	for (int t = 0; t < Variant::VARIANT_MAX; t++) {
		const Variant::Type type = (Variant::Type)t;
		CHECK(StructInfo::type_from_token(StructInfo::type_to_token(type)) == type);
	}
	CHECK(StructInfo::type_from_token("not_a_real_token") == Variant::VARIANT_MAX);
	CHECK(String(StructInfo::type_to_token(Variant::STRUCT)) == "struct");
}

TEST_CASE("[Struct][D8] Fingerprints are deterministic, 128-bit, and separate layout from schema") {
	Ref<StructInfo> p1 = make_point();
	Ref<StructInfo> p2 = make_point();

	CHECK(p1->get_schema_fingerprint() == p2->get_schema_fingerprint());
	CHECK(p1->get_schema_fingerprint().length() == 32);
	CHECK(p1->get_layout_fingerprint().length() == 32);

	StructInfoBuilder b;
	b.set_logical_type_id("Coord");
	StructInfo::Field fx;
	fx.name = "x";
	fx.type = Variant::INT;
	fx.is_typed = true;
	fx.default_value = 0;
	b.add_field(fx);
	StructInfo::Field fy;
	fy.name = "y";
	fy.type = Variant::INT;
	fy.is_typed = true;
	fy.default_value = 0;
	b.add_field(fy);
	Ref<StructInfo> coord = b.build();

	CHECK(coord->get_layout_fingerprint() == p1->get_layout_fingerprint());
	CHECK(coord->get_schema_fingerprint() != p1->get_schema_fingerprint());
}

TEST_CASE("[Struct][D8] freeze() canonicalizes stray field metadata (§5)") {
	StructInfoBuilder b;
	b.set_logical_type_id("Strays");
	StructInfo::Field f;
	f.name = "n";
	f.type = Variant::INT;
	f.is_typed = true;
	f.class_name = "Node";
	f.struct_type_id = "Point";
	f.default_value = 0;
	b.add_field(f);
	Ref<StructInfo> info = b.build();

	CHECK(info->get_field_class_name(0) == StringName());
	CHECK(info->get_field_struct_type_id(0) == StringName());
}

TEST_CASE("[Struct][D8] JSON round-trip preserves schema identity and values") {
	Ref<StructInfo> info = make_point();
	Struct s(info);
	s.set_member(0, 3);
	s.set_member(1, 4);

	Variant back = JSON::to_native(JSON::from_native(Variant(s), true), true);
	CHECK(back.get_type() == Variant::STRUCT);
	Struct r = back;
	CHECK(r.get_type_id() == StringName("Point"));
	CHECK(r.get_field_count() == 2);
	CHECK(int(r.get_member(0)) == 3);
	CHECK(int(r.get_member(1)) == 4);
	CHECK(r == s);
	CHECK(r.get_info()->get_schema_fingerprint() == info->get_schema_fingerprint());
}

TEST_CASE("[Struct][D8] JSON text round-trip (stringify -> parse -> reconstruct)") {
	Ref<StructInfo> info = make_point();
	Struct s(info);
	s.set_member(0, 7);
	s.set_member(1, 8);

	const String text = JSON::stringify(JSON::from_native(Variant(s), true));
	Struct r = JSON::to_native(JSON::parse_string(text), true);
	CHECK(r == s);
}

TEST_CASE("[Struct][D8] Binary marshalls round-trip (encode/decode_variant)") {
	Ref<StructInfo> info = make_point();
	Struct s(info);
	s.set_member(0, 3);
	s.set_member(1, 4);

	int len = 0;
	CHECK(encode_variant(Variant(s), nullptr, len, true) == OK);
	Vector<uint8_t> buf;
	buf.resize(len);
	CHECK(encode_variant(Variant(s), buf.ptrw(), len, true) == OK);
	CHECK(len % 4 == 0);

	Variant decoded;
	int used = 0;
	CHECK(decode_variant(decoded, buf.ptr(), buf.size(), &used, true) == OK);
	CHECK(used == buf.size());
	CHECK(decoded.get_type() == Variant::STRUCT);
	Struct r = decoded;
	CHECK(r == s);
	CHECK(r.get_info()->get_schema_fingerprint() == info->get_schema_fingerprint());
}

TEST_CASE("[Struct][D8] Binary marshalls round-trip with a nested container member") {
	Struct s(make_bag());
	Array a;
	a.push_back(1);
	a.push_back(2);
	s.set_member(0, a);

	int len = 0;
	encode_variant(Variant(s), nullptr, len, true);
	Vector<uint8_t> buf;
	buf.resize(len);
	encode_variant(Variant(s), buf.ptrw(), len, true);

	Variant decoded;
	int used = 0;
	CHECK(decode_variant(decoded, buf.ptr(), buf.size(), &used, true) == OK);
	Struct r = decoded;
	CHECK(Array(r.get_member(0)).size() == 2);
	CHECK(int(Array(r.get_member(0))[0]) == 1);
}

TEST_CASE("[Struct][D8] Text (.tres/variant_parser) round-trip") {
	Ref<StructInfo> info = make_point();
	Struct s(info);
	s.set_member(0, 3);
	s.set_member(1, 4);

	String text;
	CHECK(VariantWriter::write_to_string(Variant(s), text) == OK);

	VariantParser::StreamString ss;
	ss.s = text;
	Variant result;
	String err_str;
	int err_line = 0;
	CHECK(VariantParser::parse(&ss, result, err_str, err_line) == OK);
	CHECK(result.get_type() == Variant::STRUCT);
	Struct r = result;
	CHECK(r == s);
	CHECK(r.get_info()->get_schema_fingerprint() == info->get_schema_fingerprint());
}

TEST_CASE("[Struct][D8] Null struct round-trips through every format") {
	const Variant null_struct = Struct();

	CHECK(Struct(JSON::to_native(JSON::from_native(null_struct, true), true)).is_null());
	CHECK(Struct(JSON::to_native(JSON::parse_string(JSON::stringify(JSON::from_native(null_struct, true))), true)).is_null());

	int len = 0;
	encode_variant(null_struct, nullptr, len, true);
	Vector<uint8_t> buf;
	buf.resize(len);
	encode_variant(null_struct, buf.ptrw(), len, true);
	Variant decoded;
	int used = 0;
	CHECK(decode_variant(decoded, buf.ptr(), buf.size(), &used, true) == OK);
	CHECK(Struct(decoded).is_null());

	String text;
	VariantWriter::write_to_string(null_struct, text);
	VariantParser::StreamString ss;
	ss.s = text;
	Variant tv;
	String es;
	int el = 0;
	CHECK(VariantParser::parse(&ss, tv, es, el) == OK);
	CHECK(Struct(tv).is_null());
}

TEST_CASE("[Struct][D8] Field metadata (class_name / struct_type_id) survives serialization") {
	StructInfoBuilder b;
	b.set_logical_type_id("Meta");
	StructInfo::Field field_obj;
	field_obj.name = "o";
	field_obj.type = Variant::OBJECT;
	field_obj.is_typed = true;
	field_obj.class_name = "RefCounted";
	b.add_field(field_obj);
	StructInfo::Field field_struct;
	field_struct.name = "p";
	field_struct.type = Variant::STRUCT;
	field_struct.is_typed = true;
	field_struct.struct_type_id = "Point";
	b.add_field(field_struct);
	Ref<StructInfo> info = b.build();

	Struct r = JSON::to_native(JSON::from_native(Variant(Struct(info)), true), true);
	Ref<StructInfo> ri = r.get_info();
	CHECK(ri->get_field_class_name(0) == StringName("RefCounted"));
	CHECK(ri->get_field_struct_type_id(1) == StringName("Point"));
	CHECK(ri->get_schema_fingerprint() == info->get_schema_fingerprint());
}

TEST_CASE("[Struct][D8] Non-trivial defaults are reconstructed from the serialized schema") {
	Struct s(make_typed("D", "n", Variant::INT, 5));
	Struct r = JSON::to_native(JSON::from_native(Variant(s), true), true);

	Struct fresh(r.get_info());
	CHECK(int(fresh.get_member(0)) == 5);
}

TEST_CASE("[Struct][D8] Reader rejects an unknown field type token") {
	Variant native = JSON::from_native(Variant(Struct(make_point())), true);
	Dictionary env = native;
	Dictionary args = env["args"];
	Dictionary schema = args["schema"];
	Array fields = schema["fields"];
	Dictionary field0 = fields[0];
	field0["type"] = "bogus_token";

	ERR_PRINT_OFF;
	const Variant back = JSON::to_native(native, true);
	ERR_PRINT_ON;
	CHECK(back.get_type() == Variant::NIL);
}

TEST_CASE("[Struct][D8] Reader rejects an incompatible serialized value") {
	Variant native = JSON::from_native(Variant(Struct(make_point())), true);
	Dictionary env = native;
	Dictionary args = env["args"];
	Array values = args["values"];
	values[0] = Array();

	ERR_PRINT_OFF;
	const Variant back = JSON::to_native(native, true);
	ERR_PRINT_ON;
	CHECK(back.get_type() == Variant::NIL);
}

TEST_CASE("[Struct][D8] A freed object member serializes as NIL") {
	StructInfoBuilder b;
	b.set_logical_type_id("Holder");
	StructInfo::Field f;
	f.name = "o";
	f.type = Variant::NIL;
	f.is_typed = false;
	b.add_field(f);
	Struct s(b.build());

	Object *obj = memnew(Object);
	s.set_member(0, Variant(obj));
	memdelete(obj);

	Struct r = JSON::to_native(JSON::from_native(Variant(s), true), true);
	CHECK(r.get_member(0).get_type() == Variant::NIL);
}

TEST_CASE("[Struct][D8] A dead Object schema default serializes as NIL in every format") {
	StructInfoBuilder b;
	b.set_logical_type_id("DeadDefault");
	StructInfo::Field f;
	f.name = "o";
	f.type = Variant::NIL;
	f.is_typed = false;
	Object *obj = memnew(Object);
	f.default_value = Variant(obj);
	b.add_field(f);
	Ref<StructInfo> info = b.build();

	Struct s(info);
	s.set_member(0, Variant());
	memdelete(obj);

	{
		Struct r = JSON::to_native(JSON::from_native(Variant(s), true), true);
		CHECK(Struct(r.get_info()).get_member(0).get_type() == Variant::NIL);
	}
	{
		int len = 0;
		encode_variant(Variant(s), nullptr, len, true);
		Vector<uint8_t> buf;
		buf.resize(len);
		encode_variant(Variant(s), buf.ptrw(), len, true);
		Variant decoded;
		int used = 0;
		CHECK(decode_variant(decoded, buf.ptr(), buf.size(), &used, true) == OK);
		CHECK(Struct(Struct(decoded).get_info()).get_member(0).get_type() == Variant::NIL);
	}
	{
		String text;
		VariantWriter::write_to_string(Variant(s), text);
		VariantParser::StreamString ss;
		ss.s = text;
		Variant tv;
		String es;
		int el = 0;
		CHECK(VariantParser::parse(&ss, tv, es, el) == OK);
		CHECK(Struct(Struct(tv).get_info()).get_member(0).get_type() == Variant::NIL);
	}
	{
		Ref<Resource> res = memnew(Resource);
		res->set_meta("s", Variant(s));
		static const char *exts[] = { "res", "tres" };
		for (const char *ext : exts) {
			const String path = TestUtils::get_temp_path(String("struct_dead_default.") + ext);
			REQUIRE(ResourceSaver::save(res, path) == OK);
			const Ref<Resource> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
			REQUIRE(loaded.is_valid());
			Struct rs = loaded->get_meta("s");
			CHECK(Struct(rs.get_info()).get_member(0).get_type() == Variant::NIL);
		}
	}
}

TEST_CASE("[Struct][D8] ResourceSaver/ResourceLoader round-trip (.res and .tres)") {
	Ref<StructInfo> info = make_point();
	Struct s(info);
	s.set_member(0, 3);
	s.set_member(1, 4);

	Ref<Resource> res = memnew(Resource);
	res->set_meta("s", Variant(s));

	const String p_bin = TestUtils::get_temp_path("struct_res.res");
	const String p_txt = TestUtils::get_temp_path("struct_res.tres");
	CHECK(ResourceSaver::save(res, p_bin) == OK);
	CHECK(ResourceSaver::save(res, p_txt) == OK);

	const Ref<Resource> lb = ResourceLoader::load(p_bin, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	const Ref<Resource> lt = ResourceLoader::load(p_txt, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(lb.is_valid());
	REQUIRE(lt.is_valid());
	CHECK(Struct(lb->get_meta("s")) == s);
	CHECK(Struct(lt->get_meta("s")) == s);
}

TEST_CASE("[Struct][D8] Type tokens match the frozen vocabulary exactly") {
	struct TokenPair {
		Variant::Type type;
		const char *token;
	};
	const TokenPair expected[] = {
		{ Variant::NIL, "nil" },
		{ Variant::BOOL, "bool" },
		{ Variant::INT, "int" },
		{ Variant::FLOAT, "float" },
		{ Variant::STRING, "string" },
		{ Variant::VECTOR2, "vector2" },
		{ Variant::VECTOR2I, "vector2i" },
		{ Variant::RECT2, "rect2" },
		{ Variant::RECT2I, "rect2i" },
		{ Variant::VECTOR3, "vector3" },
		{ Variant::VECTOR3I, "vector3i" },
		{ Variant::TRANSFORM2D, "transform2d" },
		{ Variant::VECTOR4, "vector4" },
		{ Variant::VECTOR4I, "vector4i" },
		{ Variant::PLANE, "plane" },
		{ Variant::QUATERNION, "quaternion" },
		{ Variant::AABB, "aabb" },
		{ Variant::BASIS, "basis" },
		{ Variant::TRANSFORM3D, "transform3d" },
		{ Variant::PROJECTION, "projection" },
		{ Variant::COLOR, "color" },
		{ Variant::STRING_NAME, "string_name" },
		{ Variant::NODE_PATH, "node_path" },
		{ Variant::RID, "rid" },
		{ Variant::OBJECT, "object" },
		{ Variant::CALLABLE, "callable" },
		{ Variant::SIGNAL, "signal" },
		{ Variant::DICTIONARY, "dictionary" },
		{ Variant::ARRAY, "array" },
		{ Variant::PACKED_BYTE_ARRAY, "packed_byte_array" },
		{ Variant::PACKED_INT32_ARRAY, "packed_int32_array" },
		{ Variant::PACKED_INT64_ARRAY, "packed_int64_array" },
		{ Variant::PACKED_FLOAT32_ARRAY, "packed_float32_array" },
		{ Variant::PACKED_FLOAT64_ARRAY, "packed_float64_array" },
		{ Variant::PACKED_STRING_ARRAY, "packed_string_array" },
		{ Variant::PACKED_VECTOR2_ARRAY, "packed_vector2_array" },
		{ Variant::PACKED_VECTOR3_ARRAY, "packed_vector3_array" },
		{ Variant::PACKED_COLOR_ARRAY, "packed_color_array" },
		{ Variant::PACKED_VECTOR4_ARRAY, "packed_vector4_array" },
		{ Variant::STRUCT, "struct" },
	};
	CHECK(std::size(expected) == (size_t)Variant::VARIANT_MAX);
	for (const TokenPair &e : expected) {
		CHECK(String(StructInfo::type_to_token(e.type)) == e.token);
		CHECK(StructInfo::type_from_token(e.token) == e.type);
	}
}

TEST_CASE("[Struct][D8] Fingerprints are frozen to known golden values (portable)") {
	Ref<StructInfo> info = make_point();
	CHECK(info->get_layout_fingerprint() == "ed9ca167c5baca2d5534eed31d0c1f87");
	CHECK(info->get_schema_fingerprint() == "2d4d71795eee415fa7349f8a5ff53c8f");
}

TEST_CASE("[Struct][D8] A malformed nested struct fails the whole JSON decode") {
	StructInfoBuilder ob;
	ob.set_logical_type_id("Outer");
	StructInfo::Field f;
	f.name = "inner";
	f.type = Variant::STRUCT;
	f.is_typed = true;
	f.struct_type_id = "Point";
	ob.add_field(f);
	Struct outer(ob.build());
	outer.set_named("inner", Struct(make_point()));

	Variant native = JSON::from_native(Variant(outer), true);
	Dictionary env = native;
	Dictionary args = env["args"];
	Array values = args["values"];
	Dictionary nested_env = values[0];
	Dictionary nested_args = nested_env["args"];
	Dictionary nested_schema = nested_args["schema"];
	Array nested_fields = nested_schema["fields"];
	Dictionary nested_field0 = nested_fields[0];
	nested_field0["type"] = "bogus_token";

	ERR_PRINT_OFF;
	const Variant back = JSON::to_native(native, true);
	ERR_PRINT_ON;
	CHECK(back.get_type() == Variant::NIL);
}

TEST_CASE("[Struct][D8] A malformed struct nested inside an array field fails the whole JSON decode") {
	StructInfoBuilder ob;
	ob.set_logical_type_id("ArrHolder");
	StructInfo::Field f;
	f.name = "items";
	f.type = Variant::NIL;
	f.is_typed = false;
	ob.add_field(f);
	Struct outer(ob.build());
	Array arr;
	arr.push_back(Struct(make_point()));
	outer.set_member(0, arr);

	Variant native = JSON::from_native(Variant(outer), true);
	Dictionary env = native;
	Dictionary args = env["args"];
	Array values = args["values"];
	Array elems = values[0];
	Dictionary nested_struct_env = elems[0];
	Dictionary ns_args = nested_struct_env["args"];
	Dictionary ns_schema = ns_args["schema"];
	Array ns_fields = ns_schema["fields"];
	Dictionary ns_field0 = ns_fields[0];
	ns_field0["type"] = "bogus_token";

	ERR_PRINT_OFF;
	const Variant back = JSON::to_native(native, true);
	ERR_PRINT_ON;
	CHECK(back.get_type() == Variant::NIL);
}

TEST_CASE("[Struct][D8] Text parser rejects a field missing its default") {
	const String text = R"(Struct(1, "P", [{"name": "x", "type": "int", "typed": true}], [5]))";
	VariantParser::StreamString ss;
	ss.s = text;
	Variant v;
	String es;
	int el = 0;
	ERR_PRINT_OFF;
	const Error err = VariantParser::parse(&ss, v, es, el);
	ERR_PRINT_ON;
	CHECK(err != OK);
}

TEST_CASE("[Struct][D8] Resource referenced only by a struct member is discovered, saved and loaded") {
	Ref<Resource> child = memnew(Resource);
	child->set_name("member_child");

	StructInfoBuilder b;
	b.set_logical_type_id("MemHolder");
	StructInfo::Field f;
	f.name = "r";
	f.type = Variant::NIL;
	f.is_typed = false;
	b.add_field(f);
	Struct s(b.build());
	s.set_member(0, child);

	Ref<Resource> res = memnew(Resource);
	res->set_meta("s", Variant(s));

	static const char *exts[] = { "res", "tres" };
	for (const char *ext : exts) {
		const String path = TestUtils::get_temp_path(String("struct_member_res.") + ext);
		REQUIRE(ResourceSaver::save(res, path) == OK);
		const Ref<Resource> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		Struct rs = loaded->get_meta("s");
		const Ref<Resource> rchild = rs.get_member(0);
		REQUIRE(rchild.is_valid());
		CHECK(rchild->get_name() == "member_child");
	}
}

TEST_CASE("[Struct][D8] Resource referenced only by a schema default is discovered, saved and loaded") {
	Ref<Resource> defchild = memnew(Resource);
	defchild->set_name("default_child");

	StructInfoBuilder b;
	b.set_logical_type_id("DefHolder");
	StructInfo::Field f;
	f.name = "r";
	f.type = Variant::NIL;
	f.is_typed = false;
	f.default_value = defchild;
	b.add_field(f);
	Struct s(b.build());
	s.set_member(0, Variant());

	Ref<Resource> res = memnew(Resource);
	res->set_meta("s", Variant(s));

	static const char *exts[] = { "res", "tres" };
	for (const char *ext : exts) {
		const String path = TestUtils::get_temp_path(String("struct_default_res.") + ext);
		REQUIRE(ResourceSaver::save(res, path) == OK);
		const Ref<Resource> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		Struct rs = loaded->get_meta("s");
		Struct fresh(rs.get_info());
		const Ref<Resource> rdef = fresh.get_member(0);
		REQUIRE(rdef.is_valid());
		CHECK(rdef->get_name() == "default_child");
	}
}

TEST_CASE("[Struct][D8] Text parser rejects malformed Struct arguments") {
	const char *bad[] = {
		R"(Struct("P", [], []))",
		R"(Struct("x", "P", [], []))",
		R"(Struct(2, "P", [], []))",
		R"(Struct(1, 123, [], []))",
		R"(Struct(1, "P", 12, []))",
		R"(Struct(1, "P", [], 7))",
		R"(Struct(1, "P", [42], []))",
	};
	for (const char *text : bad) {
		VariantParser::StreamString ss;
		ss.s = text;
		Variant v;
		String es;
		int el = 0;
		ERR_PRINT_OFF;
		const Error err = VariantParser::parse(&ss, v, es, el);
		ERR_PRINT_ON;
		CHECK(err != OK);
	}
}

TEST_CASE("[Struct][D8] JSON reader rejects malformed envelopes") {
	{
		Dictionary env = JSON::from_native(Variant(Struct(make_point())), true);
		env["args"] = 42;
		ERR_PRINT_OFF;
		const Variant back = JSON::to_native(env, true);
		ERR_PRINT_ON;
		CHECK(back.get_type() == Variant::NIL);
	}
	{
		Dictionary env = JSON::from_native(Variant(Struct(make_point())), true);
		Dictionary args = env["args"];
		Dictionary schema = args["schema"];
		schema["version"] = 999;
		ERR_PRINT_OFF;
		const Variant back = JSON::to_native(env, true);
		ERR_PRINT_ON;
		CHECK(back.get_type() == Variant::NIL);
	}
	{
		Dictionary env = JSON::from_native(Variant(Struct(make_point())), true);
		Dictionary args = env["args"];
		Dictionary schema = args["schema"];
		schema["fields"] = 7;
		ERR_PRINT_OFF;
		const Variant back = JSON::to_native(env, true);
		ERR_PRINT_ON;
		CHECK(back.get_type() == Variant::NIL);
	}
	{
		Dictionary env = JSON::from_native(Variant(Struct(make_point())), true);
		Dictionary args = env["args"];
		Dictionary schema = args["schema"];
		Array fields = schema["fields"];
		fields[0] = 5;
		ERR_PRINT_OFF;
		const Variant back = JSON::to_native(env, true);
		ERR_PRINT_ON;
		CHECK(back.get_type() == Variant::NIL);
	}
	{
		Dictionary env = JSON::from_native(Variant(Struct(make_point())), true);
		Dictionary args = env["args"];
		args["values"] = 9;
		ERR_PRINT_OFF;
		const Variant back = JSON::to_native(env, true);
		ERR_PRINT_ON;
		CHECK(back.get_type() == Variant::NIL);
	}
}

TEST_CASE("[Struct][D8] Binary decoder rejects truncated and oversized struct data") {
	Struct s(make_point());
	s.set_member(0, 3);
	s.set_member(1, 4);
	int len = 0;
	encode_variant(Variant(s), nullptr, len, true);
	Vector<uint8_t> buf;
	buf.resize(len);
	encode_variant(Variant(s), buf.ptrw(), len, true);

	Variant decoded;
	int used = 0;
	ERR_PRINT_OFF;
	const Error truncated = decode_variant(decoded, buf.ptr(), buf.size() / 2, &used, true);
	ERR_PRINT_ON;
	CHECK(truncated != OK);

	Vector<uint8_t> raw;
	auto put32 = [&raw](uint32_t p_v) {
		for (int i = 0; i < 4; i++) {
			raw.push_back((p_v >> (8 * i)) & 0xFF);
		}
	};
	put32(Variant::STRUCT);
	put32(1);
	raw.push_back('X');
	raw.push_back(0);
	raw.push_back(0);
	raw.push_back(0);
	put32(StructInfo::SERIALIZATION_VERSION);
	put32(0x7FFFFFFF);

	Variant oversized;
	used = 0;
	ERR_PRINT_OFF;
	const Error huge = decode_variant(oversized, raw.ptr(), raw.size(), &used, true);
	ERR_PRINT_ON;
	CHECK(huge != OK);
}

TEST_CASE("[Struct][D8] Schema fingerprint reflects structure, not default values") {
	Ref<StructInfo> a = make_typed("Point", "x", Variant::INT, 0);
	Ref<StructInfo> b = make_typed("Point", "x", Variant::INT, 100);
	CHECK(a->get_schema_fingerprint() == b->get_schema_fingerprint());
	CHECK(a->get_layout_fingerprint() == b->get_layout_fingerprint());
}

TEST_CASE("[Struct][D8] Every native JSON type still round-trips nested in arrays and dictionaries") {
	Array samples;
	samples.push_back(Variant());
	samples.push_back(true);
	samples.push_back(42);
	samples.push_back(3.5);
	samples.push_back(String("hi"));
	samples.push_back(Vector2(1, 2));
	samples.push_back(Vector3(1, 2, 3));
	samples.push_back(Color(0.1f, 0.2f, 0.3f, 0.4f));
	samples.push_back(StringName("sn"));
	samples.push_back(NodePath("a/b"));
	{
		PackedInt32Array p;
		p.push_back(1);
		p.push_back(2);
		samples.push_back(p);
	}
	{
		PackedStringArray p;
		p.push_back("x");
		samples.push_back(p);
	}

	for (int i = 0; i < samples.size(); i++) {
		const Variant v = samples[i];

		CHECK(JSON::to_native(JSON::from_native(v, true), true) == v);

		Array arr;
		arr.push_back(v);
		const Variant arr_back = JSON::to_native(JSON::from_native(arr, true), true);
		REQUIRE(arr_back.get_type() == Variant::ARRAY);
		REQUIRE(Array(arr_back).size() == 1);
		CHECK(Array(arr_back)[0] == v);

		Dictionary d;
		d["k"] = v;
		const Variant d_back = JSON::to_native(JSON::from_native(d, true), true);
		REQUIRE(d_back.get_type() == Variant::DICTIONARY);
		CHECK(Dictionary(d_back)["k"] == v);
	}
}

} //namespace TestStruct
