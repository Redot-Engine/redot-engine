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

#include "core/variant/struct.h"
#include "core/variant/struct_info.h"
#include "core/variant/variant_internal.h"

#include "tests/test_macros.h"

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

} //namespace TestStruct
