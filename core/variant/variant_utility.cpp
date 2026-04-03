/**************************************************************************/
/*  variant_utility.cpp                                                   */
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

#include "variant_utility.h"

#include "core/io/marshalls.h"
#include "core/math/math_funcs.h"
#include "core/object/ref_counted.h"
#include "core/os/os.h"
#include "core/templates/a_hash_map.h"
#include "core/templates/rid.h"
#include "core/templates/rid_owner.h"
#include "core/variant/binder_common.h"
#include "core/variant/variant_parser.h"

// Math
double VariantUtilityFunctions::sin(double arg) {
	return Math::sin(arg);
}

double VariantUtilityFunctions::cos(double arg) {
	return Math::cos(arg);
}

double VariantUtilityFunctions::tan(double arg) {
	return Math::tan(arg);
}

double VariantUtilityFunctions::sinh(double arg) {
	return Math::sinh(arg);
}

double VariantUtilityFunctions::cosh(double arg) {
	return Math::cosh(arg);
}

double VariantUtilityFunctions::tanh(double arg) {
	return Math::tanh(arg);
}

double VariantUtilityFunctions::asin(double arg) {
	return Math::asin(arg);
}

double VariantUtilityFunctions::acos(double arg) {
	return Math::acos(arg);
}

double VariantUtilityFunctions::atan(double arg) {
	return Math::atan(arg);
}

double VariantUtilityFunctions::atan2(double y, double x) {
	return Math::atan2(y, x);
}

double VariantUtilityFunctions::asinh(double arg) {
	return Math::asinh(arg);
}

double VariantUtilityFunctions::acosh(double arg) {
	return Math::acosh(arg);
}

double VariantUtilityFunctions::atanh(double arg) {
	return Math::atanh(arg);
}

double VariantUtilityFunctions::sqrt(double x) {
	return Math::sqrt(x);
}

double VariantUtilityFunctions::fmod(double b, double r) {
	return Math::fmod(b, r);
}

double VariantUtilityFunctions::fposmod(double b, double r) {
	return Math::fposmod(b, r);
}

int64_t VariantUtilityFunctions::posmod(int64_t b, int64_t r) {
	return Math::posmod(b, r);
}

Variant VariantUtilityFunctions::floor(Callable::CallError &r_error, const Variant &x) {
	r_error.error = Callable::CallError::CALL_OK;
	switch (x.get_type()) {
		case Variant::INT: {
			return VariantInternalAccessor<int64_t>::get(&x);
		} break;
		case Variant::FLOAT: {
			return Math::floor(VariantInternalAccessor<double>::get(&x));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).floor();
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x);
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).floor();
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x);
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).floor();
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x);
		} break;
		default: {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
		} break;
	}
}

double VariantUtilityFunctions::floorf(double x) {
	return Math::floor(x);
}

int64_t VariantUtilityFunctions::floori(double x) {
	return int64_t(Math::floor(x));
}

Variant VariantUtilityFunctions::ceil(Callable::CallError &r_error, const Variant &x) {
	r_error.error = Callable::CallError::CALL_OK;
	switch (x.get_type()) {
		case Variant::INT: {
			return VariantInternalAccessor<int64_t>::get(&x);
		} break;
		case Variant::FLOAT: {
			return Math::ceil(VariantInternalAccessor<double>::get(&x));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).ceil();
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x);
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).ceil();
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x);
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).ceil();
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x);
		} break;
		default: {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
		} break;
	}
}

double VariantUtilityFunctions::ceilf(double x) {
	return Math::ceil(x);
}

int64_t VariantUtilityFunctions::ceili(double x) {
	return int64_t(Math::ceil(x));
}

Variant VariantUtilityFunctions::round(Callable::CallError &r_error, const Variant &x) {
	r_error.error = Callable::CallError::CALL_OK;
	switch (x.get_type()) {
		case Variant::INT: {
			return VariantInternalAccessor<int64_t>::get(&x);
		} break;
		case Variant::FLOAT: {
			return Math::round(VariantInternalAccessor<double>::get(&x));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).round();
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x);
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).round();
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x);
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).round();
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x);
		} break;
		default: {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
		} break;
	}
}

double VariantUtilityFunctions::roundf(double x) {
	return Math::round(x);
}

int64_t VariantUtilityFunctions::roundi(double x) {
	return int64_t(Math::round(x));
}

Variant VariantUtilityFunctions::abs(Callable::CallError &r_error, const Variant &x) {
	r_error.error = Callable::CallError::CALL_OK;
	switch (x.get_type()) {
		case Variant::INT: {
			return Math::abs(VariantInternalAccessor<int64_t>::get(&x));
		} break;
		case Variant::FLOAT: {
			return Math::abs(VariantInternalAccessor<double>::get(&x));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).abs();
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x).abs();
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).abs();
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x).abs();
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).abs();
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x).abs();
		} break;
		default: {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
		} break;
	}
}

double VariantUtilityFunctions::absf(double x) {
	return Math::abs(x);
}

int64_t VariantUtilityFunctions::absi(int64_t x) {
	return Math::abs(x);
}

Variant VariantUtilityFunctions::sign(Callable::CallError &r_error, const Variant &x) {
	r_error.error = Callable::CallError::CALL_OK;
	switch (x.get_type()) {
		case Variant::INT: {
			return SIGN(VariantInternalAccessor<int64_t>::get(&x));
		} break;
		case Variant::FLOAT: {
			return SIGN(VariantInternalAccessor<double>::get(&x));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).sign();
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x).sign();
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).sign();
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x).sign();
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).sign();
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x).sign();
		} break;
		default: {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
		} break;
	}
}

double VariantUtilityFunctions::signf(double x) {
	return SIGN(x);
}

int64_t VariantUtilityFunctions::signi(int64_t x) {
	return SIGN(x);
}

double VariantUtilityFunctions::pow(double x, double y) {
	return Math::pow(x, y);
}

double VariantUtilityFunctions::log(double x) {
	return Math::log(x);
}

double VariantUtilityFunctions::exp(double x) {
	return Math::exp(x);
}

bool VariantUtilityFunctions::is_nan(double x) {
	return Math::is_nan(x);
}

bool VariantUtilityFunctions::is_inf(double x) {
	return Math::is_inf(x);
}

bool VariantUtilityFunctions::is_equal_approx(double x, double y) {
	return Math::is_equal_approx(x, y);
}

bool VariantUtilityFunctions::is_zero_approx(double x) {
	return Math::is_zero_approx(x);
}

bool VariantUtilityFunctions::is_finite(double x) {
	return Math::is_finite(x);
}

double VariantUtilityFunctions::ease(float x, float curve) {
	return Math::ease(x, curve);
}

int VariantUtilityFunctions::step_decimals(float step) {
	return Math::step_decimals(step);
}

Variant VariantUtilityFunctions::snapped(Callable::CallError &r_error, const Variant &x, const Variant &step) {
	switch (x.get_type()) {
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::VECTOR2:
		case Variant::VECTOR2I:
		case Variant::VECTOR3:
		case Variant::VECTOR3I:
		case Variant::VECTOR4:
		case Variant::VECTOR4I:
			break;
		default:
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
	}

	if (x.get_type() != step.get_type()) {
		if (x.get_type() == Variant::INT || x.get_type() == Variant::FLOAT) {
			if (step.get_type() != Variant::INT && step.get_type() != Variant::FLOAT) {
				r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
				r_error.argument = 1;
				r_error.expected = Variant::NIL;
				return R"(Argument "step" must be "int" or "float".)";
			}
		} else {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 1;
			r_error.expected = x.get_type();
			return Variant();
		}
	}

	r_error.error = Callable::CallError::CALL_OK;
	switch (step.get_type()) {
		case Variant::INT: {
			return snappedi(x, VariantInternalAccessor<int64_t>::get(&step));
		} break;
		case Variant::FLOAT: {
			return snappedf(x, VariantInternalAccessor<double>::get(&step));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).snapped(VariantInternalAccessor<Vector2>::get(&step));
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x).snapped(VariantInternalAccessor<Vector2i>::get(&step));
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).snapped(VariantInternalAccessor<Vector3>::get(&step));
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x).snapped(VariantInternalAccessor<Vector3i>::get(&step));
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).snapped(VariantInternalAccessor<Vector4>::get(&step));
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x).snapped(VariantInternalAccessor<Vector4i>::get(&step));
		} break;
		default: {
			return Variant(); // Already handled.
		} break;
	}
}

double VariantUtilityFunctions::snappedf(double x, double step) {
	return Math::snapped(x, step);
}

int64_t VariantUtilityFunctions::snappedi(double x, int64_t step) {
	return Math::snapped(x, step);
}

Variant VariantUtilityFunctions::lerp(Callable::CallError &r_error, const Variant &from, const Variant &to, double weight) {
	switch (from.get_type()) {
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::VECTOR2:
		case Variant::VECTOR3:
		case Variant::VECTOR4:
		case Variant::QUATERNION:
		case Variant::BASIS:
		case Variant::COLOR:
		case Variant::TRANSFORM2D:
		case Variant::TRANSFORM3D:
			break;
		default:
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "from" must be "int", "float", "Vector2", "Vector3", "Vector4", "Color", "Quaternion", "Basis", "Transform2D", or "Transform3D".)";
	}

	if (from.get_type() != to.get_type()) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 1;
		r_error.expected = from.get_type();
		return Variant();
	}

	r_error.error = Callable::CallError::CALL_OK;
	switch (from.get_type()) {
		case Variant::INT: {
			return lerpf(VariantInternalAccessor<int64_t>::get(&from), to, weight);
		} break;
		case Variant::FLOAT: {
			return lerpf(VariantInternalAccessor<double>::get(&from), to, weight);
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&from).lerp(VariantInternalAccessor<Vector2>::get(&to), weight);
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&from).lerp(VariantInternalAccessor<Vector3>::get(&to), weight);
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&from).lerp(VariantInternalAccessor<Vector4>::get(&to), weight);
		} break;
		case Variant::QUATERNION: {
			return VariantInternalAccessor<Quaternion>::get(&from).slerp(VariantInternalAccessor<Quaternion>::get(&to), weight);
		} break;
		case Variant::BASIS: {
			return VariantInternalAccessor<Basis>::get(&from).slerp(VariantInternalAccessor<Basis>::get(&to), weight);
		} break;
		case Variant::TRANSFORM2D: {
			return VariantInternalAccessor<Transform2D>::get(&from).interpolate_with(VariantInternalAccessor<Transform2D>::get(&to), weight);
		} break;
		case Variant::TRANSFORM3D: {
			return VariantInternalAccessor<Transform3D>::get(&from).interpolate_with(VariantInternalAccessor<Transform3D>::get(&to), weight);
		} break;
		case Variant::COLOR: {
			return VariantInternalAccessor<Color>::get(&from).lerp(VariantInternalAccessor<Color>::get(&to), weight);
		} break;
		default: {
			return Variant(); // Already handled.
		} break;
	}
}

double VariantUtilityFunctions::lerpf(double from, double to, double weight) {
	return Math::lerp(from, to, weight);
}

double VariantUtilityFunctions::cubic_interpolate(double from, double to, double pre, double post, double weight) {
	return Math::cubic_interpolate(from, to, pre, post, weight);
}

double VariantUtilityFunctions::cubic_interpolate_angle(double from, double to, double pre, double post, double weight) {
	return Math::cubic_interpolate_angle(from, to, pre, post, weight);
}

double VariantUtilityFunctions::cubic_interpolate_in_time(double from, double to, double pre, double post, double weight,
		double to_t, double pre_t, double post_t) {
	return Math::cubic_interpolate_in_time(from, to, pre, post, weight, to_t, pre_t, post_t);
}

double VariantUtilityFunctions::cubic_interpolate_angle_in_time(double from, double to, double pre, double post, double weight,
		double to_t, double pre_t, double post_t) {
	return Math::cubic_interpolate_angle_in_time(from, to, pre, post, weight, to_t, pre_t, post_t);
}

double VariantUtilityFunctions::bezier_interpolate(double p_start, double p_control_1, double p_control_2, double p_end, double p_t) {
	return Math::bezier_interpolate(p_start, p_control_1, p_control_2, p_end, p_t);
}

double VariantUtilityFunctions::bezier_derivative(double p_start, double p_control_1, double p_control_2, double p_end, double p_t) {
	return Math::bezier_derivative(p_start, p_control_1, p_control_2, p_end, p_t);
}

double VariantUtilityFunctions::angle_difference(double from, double to) {
	return Math::angle_difference(from, to);
}

double VariantUtilityFunctions::lerp_angle(double from, double to, double weight) {
	return Math::lerp_angle(from, to, weight);
}

double VariantUtilityFunctions::inverse_lerp(double from, double to, double weight) {
	return Math::inverse_lerp(from, to, weight);
}

double VariantUtilityFunctions::remap(double value, double istart, double istop, double ostart, double ostop) {
	return Math::remap(value, istart, istop, ostart, ostop);
}

double VariantUtilityFunctions::remap_default(double value, double istart, double istop, double ostart, double ostop, double default_value) {
	double result = Math::remap(value, istart, istop, ostart, ostop);
	if (Math::is_finite(result)) {
		return result;
	}

	return default_value;
}

double VariantUtilityFunctions::smoothstep(double from, double to, double val) {
	return Math::smoothstep(from, to, val);
}

double VariantUtilityFunctions::move_toward(double from, double to, double delta) {
	return Math::move_toward(from, to, delta);
}

double VariantUtilityFunctions::rotate_toward(double from, double to, double delta) {
	return Math::rotate_toward(from, to, delta);
}

double VariantUtilityFunctions::deg_to_rad(double angle_deg) {
	return Math::deg_to_rad(angle_deg);
}

double VariantUtilityFunctions::rad_to_deg(double angle_rad) {
	return Math::rad_to_deg(angle_rad);
}

double VariantUtilityFunctions::linear_to_db(double linear) {
	return Math::linear_to_db(linear);
}

double VariantUtilityFunctions::db_to_linear(double db) {
	return Math::db_to_linear(db);
}

Variant VariantUtilityFunctions::wrap(Callable::CallError &r_error, const Variant &p_x, const Variant &p_min, const Variant &p_max) {
	Variant::Type x_type = p_x.get_type();
	if (x_type != Variant::INT && x_type != Variant::FLOAT) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::FLOAT;
		return Variant();
	}

	Variant::Type min_type = p_min.get_type();
	if (min_type != Variant::INT && min_type != Variant::FLOAT) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 1;
		r_error.expected = x_type;
		return Variant();
	}

	Variant::Type max_type = p_max.get_type();
	if (max_type != Variant::INT && max_type != Variant::FLOAT) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 2;
		r_error.expected = x_type;
		return Variant();
	}

	Variant value;

	switch (x_type) {
		case Variant::INT: {
			if (x_type != min_type || x_type != max_type) {
				value = wrapf((double)p_x, (double)p_min, (double)p_max);
			} else {
				value = wrapi((int)p_x, (int)p_min, (int)p_max);
			}
		} break;
		case Variant::FLOAT: {
			value = wrapf((double)p_x, (double)p_min, (double)p_max);
		} break;
		default:
			break;
	}

	r_error.error = Callable::CallError::CALL_OK;
	return value;
}

int64_t VariantUtilityFunctions::wrapi(int64_t value, int64_t min, int64_t max) {
	return Math::wrapi(value, min, max);
}

double VariantUtilityFunctions::wrapf(double value, double min, double max) {
	return Math::wrapf(value, min, max);
}

double VariantUtilityFunctions::pingpong(double value, double length) {
	return Math::pingpong(value, length);
}

double VariantUtilityFunctions::sigmoid(double x) {
	return Math::sigmoid_affine(x, 1.0, 0.0);
}

double VariantUtilityFunctions::sigmoid_approx(double x) {
	return Math::sigmoid_affine_approx(x, 1.0, 0.0);
}

double VariantUtilityFunctions::sigmoid_affine(double x, double amplitude, double y_translation) {
	return Math::sigmoid_affine(x, amplitude, y_translation);
}

double VariantUtilityFunctions::sigmoid_affine_approx(double x, double amplitude, double y_translation) {
	return Math::sigmoid_affine_approx(x, amplitude, y_translation);
}

Variant VariantUtilityFunctions::max(Callable::CallError &r_error, const Variant **p_args, int p_argcount) {
	if (p_argcount < 2) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 2;
		return Variant();
	}
	Variant base = *p_args[0];
	Variant ret;

	for (int i = 0; i < p_argcount; i++) {
		Variant::Type arg_type = p_args[i]->get_type();
		if (arg_type != Variant::INT && arg_type != Variant::FLOAT) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = i;
			r_error.expected = Variant::FLOAT;
			return Variant();
		}
		if (i == 0) {
			continue;
		}
		bool valid;
		Variant::evaluate(Variant::OP_LESS, base, *p_args[i], ret, valid);
		if (!valid) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = i;
			r_error.expected = base.get_type();
			return Variant();
		}
		if (ret.booleanize()) {
			base = *p_args[i];
		}
	}
	r_error.error = Callable::CallError::CALL_OK;
	return base;
}

double VariantUtilityFunctions::maxf(double x, double y) {
	return MAX(x, y);
}

int64_t VariantUtilityFunctions::maxi(int64_t x, int64_t y) {
	return MAX(x, y);
}

Variant VariantUtilityFunctions::min(Callable::CallError &r_error, const Variant **p_args, int p_argcount) {
	if (p_argcount < 2) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 2;
		return Variant();
	}
	Variant base = *p_args[0];
	Variant ret;

	for (int i = 0; i < p_argcount; i++) {
		Variant::Type arg_type = p_args[i]->get_type();
		if (arg_type != Variant::INT && arg_type != Variant::FLOAT) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = i;
			r_error.expected = Variant::FLOAT;
			return Variant();
		}
		if (i == 0) {
			continue;
		}
		bool valid;
		Variant::evaluate(Variant::OP_GREATER, base, *p_args[i], ret, valid);
		if (!valid) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = i;
			r_error.expected = base.get_type();
			return Variant();
		}
		if (ret.booleanize()) {
			base = *p_args[i];
		}
	}
	r_error.error = Callable::CallError::CALL_OK;
	return base;
}

double VariantUtilityFunctions::minf(double x, double y) {
	return MIN(x, y);
}

int64_t VariantUtilityFunctions::mini(int64_t x, int64_t y) {
	return MIN(x, y);
}

Variant VariantUtilityFunctions::clamp(Callable::CallError &r_error, const Variant &x, const Variant &min, const Variant &max) {
	Variant value = x;

	Variant ret;

	bool valid;
	Variant::evaluate(Variant::OP_LESS, value, min, ret, valid);
	if (!valid) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 1;
		r_error.expected = value.get_type();
		return Variant();
	}
	if (ret.booleanize()) {
		value = min;
	}
	Variant::evaluate(Variant::OP_GREATER, value, max, ret, valid);
	if (!valid) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 2;
		r_error.expected = value.get_type();
		return Variant();
	}
	if (ret.booleanize()) {
		value = max;
	}

	r_error.error = Callable::CallError::CALL_OK;

	return value;
}

double VariantUtilityFunctions::clampf(double x, double min, double max) {
	return CLAMP(x, min, max);
}

int64_t VariantUtilityFunctions::clampi(int64_t x, int64_t min, int64_t max) {
	return CLAMP(x, min, max);
}

int64_t VariantUtilityFunctions::nearest_po2(int64_t x) {
	return nearest_power_of_2_templated(uint64_t(x));
}

// Random

void VariantUtilityFunctions::randomize() {
	Math::randomize();
}

int64_t VariantUtilityFunctions::randi() {
	return Math::rand();
}

double VariantUtilityFunctions::randf() {
	return Math::randf();
}

double VariantUtilityFunctions::randfn(double mean, double deviation) {
	return Math::randfn(mean, deviation);
}

int64_t VariantUtilityFunctions::randi_range(int64_t from, int64_t to) {
	return Math::random((int32_t)from, (int32_t)to);
}

double VariantUtilityFunctions::randf_range(double from, double to) {
	return Math::random(from, to);
}

void VariantUtilityFunctions::seed(int64_t s) {
	return Math::seed(s);
}

PackedInt64Array VariantUtilityFunctions::rand_from_seed(int64_t seed) {
	uint64_t s = seed;
	PackedInt64Array arr;
	arr.resize(2);
	arr.write[0] = Math::rand_from_seed(&s);
	arr.write[1] = s;
	return arr;
}

// Utility

Variant VariantUtilityFunctions::weakref(Callable::CallError &r_error, const Variant &obj) {
	if (obj.get_type() == Variant::OBJECT) {
		r_error.error = Callable::CallError::CALL_OK;
		if (obj.is_ref_counted()) {
			Ref<WeakRef> wref = memnew(WeakRef);
			Ref<RefCounted> r = obj;
			if (r.is_valid()) {
				wref->set_ref(r);
			}
			return wref;
		} else {
			Ref<WeakRef> wref = memnew(WeakRef);
			Object *o = obj.get_validated_object();
			if (o) {
				wref->set_obj(o);
			}
			return wref;
		}
	} else if (obj.get_type() == Variant::NIL) {
		r_error.error = Callable::CallError::CALL_OK;
		Ref<WeakRef> wref = memnew(WeakRef);
		return wref;
	} else {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::OBJECT;
		return Variant();
	}
}

int64_t VariantUtilityFunctions::_typeof(const Variant &obj) {
	return obj.get_type();
}

Variant VariantUtilityFunctions::type_convert(const Variant &p_variant, const Variant::Type p_type) {
	switch (p_type) {
		case Variant::Type::NIL:
			return Variant();
		case Variant::Type::BOOL:
			return p_variant.operator bool();
		case Variant::Type::INT:
			return p_variant.operator int64_t();
		case Variant::Type::FLOAT:
			return p_variant.operator double();
		case Variant::Type::STRING:
			return p_variant.operator String();
		case Variant::Type::VECTOR2:
			return p_variant.operator Vector2();
		case Variant::Type::VECTOR2I:
			return p_variant.operator Vector2i();
		case Variant::Type::RECT2:
			return p_variant.operator Rect2();
		case Variant::Type::RECT2I:
			return p_variant.operator Rect2i();
		case Variant::Type::VECTOR3:
			return p_variant.operator Vector3();
		case Variant::Type::VECTOR3I:
			return p_variant.operator Vector3i();
		case Variant::Type::TRANSFORM2D:
			return p_variant.operator Transform2D();
		case Variant::Type::VECTOR4:
			return p_variant.operator Vector4();
		case Variant::Type::VECTOR4I:
			return p_variant.operator Vector4i();
		case Variant::Type::PLANE:
			return p_variant.operator Plane();
		case Variant::Type::QUATERNION:
			return p_variant.operator Quaternion();
		case Variant::Type::AABB:
			return p_variant.operator ::AABB();
		case Variant::Type::BASIS:
			return p_variant.operator Basis();
		case Variant::Type::TRANSFORM3D:
			return p_variant.operator Transform3D();
		case Variant::Type::PROJECTION:
			return p_variant.operator Projection();
		case Variant::Type::COLOR:
			return p_variant.operator Color();
		case Variant::Type::STRING_NAME:
			return p_variant.operator StringName();
		case Variant::Type::NODE_PATH:
			return p_variant.operator NodePath();
		case Variant::Type::RID:
			return p_variant.operator ::RID();
		case Variant::Type::OBJECT:
			return p_variant.operator Object *();
		case Variant::Type::CALLABLE:
			return p_variant.operator Callable();
		case Variant::Type::SIGNAL:
			return p_variant.operator Signal();
		case Variant::Type::DICTIONARY:
			return p_variant.operator Dictionary();
		case Variant::Type::ARRAY:
			return p_variant.operator Array();
		case Variant::Type::PACKED_BYTE_ARRAY:
			return p_variant.operator PackedByteArray();
		case Variant::Type::PACKED_INT32_ARRAY:
			return p_variant.operator PackedInt32Array();
		case Variant::Type::PACKED_INT64_ARRAY:
			return p_variant.operator PackedInt64Array();
		case Variant::Type::PACKED_FLOAT32_ARRAY:
			return p_variant.operator PackedFloat32Array();
		case Variant::Type::PACKED_FLOAT64_ARRAY:
			return p_variant.operator PackedFloat64Array();
		case Variant::Type::PACKED_STRING_ARRAY:
			return p_variant.operator PackedStringArray();
		case Variant::Type::PACKED_VECTOR2_ARRAY:
			return p_variant.operator PackedVector2Array();
		case Variant::Type::PACKED_VECTOR3_ARRAY:
			return p_variant.operator PackedVector3Array();
		case Variant::Type::PACKED_COLOR_ARRAY:
			return p_variant.operator PackedColorArray();
		case Variant::Type::PACKED_VECTOR4_ARRAY:
			return p_variant.operator PackedVector4Array();
		case Variant::Type::VARIANT_MAX:
			ERR_PRINT("Invalid type argument to type_convert(), use the TYPE_* constants. Returning the unconverted Variant.");
	}
	return p_variant;
}

String VariantUtilityFunctions::str(Callable::CallError &r_error, const Variant **p_args, int p_arg_count) {
	if (p_arg_count < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
		return String();
	}

	r_error.error = Callable::CallError::CALL_OK;

	return join_string(p_args, p_arg_count);
}

String VariantUtilityFunctions::error_string(Error error) {
	if (error < 0 || error >= ERR_MAX) {
		return String("(invalid error code)");
	}

	return String(error_names[error]);
}

String VariantUtilityFunctions::type_string(Variant::Type p_type) {
	ERR_FAIL_INDEX_V_MSG((int)p_type, (int)Variant::VARIANT_MAX, "<invalid type>", "Invalid type argument to type_string(), use the TYPE_* constants.");
	return Variant::get_type_name(p_type);
}

void VariantUtilityFunctions::print(Callable::CallError &r_error, const Variant **p_args, int p_arg_count) {
	print_line(join_string(p_args, p_arg_count));
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::print_rich(Callable::CallError &r_error, const Variant **p_args, int p_arg_count) {
	print_line_rich(join_string(p_args, p_arg_count));
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::_print_verbose(Callable::CallError &r_error, const Variant **p_args, int p_arg_count) {
	if (OS::get_singleton()->is_stdout_verbose()) {
		// No need to use `print_verbose()` as this call already only happens
		// when verbose mode is enabled. This avoids performing string argument concatenation
		// when not needed.
		print_line(join_string(p_args, p_arg_count));
	}

	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::printerr(Callable::CallError &r_error, const Variant **p_args, int p_arg_count) {
	print_error(join_string(p_args, p_arg_count));
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::printt(Callable::CallError &r_error, const Variant **p_args, int p_arg_count) {
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		if (i) {
			s += "\t";
		}
		s += p_args[i]->operator String();
	}

	print_line(s);
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::prints(Callable::CallError &r_error, const Variant **p_args, int p_arg_count) {
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		if (i) {
			s += " ";
		}
		s += p_args[i]->operator String();
	}

	print_line(s);
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::printraw(Callable::CallError &r_error, const Variant **p_args, int p_arg_count) {
	print_raw(join_string(p_args, p_arg_count));
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::push_error(Callable::CallError &r_error, const Variant **p_args, int p_arg_count) {
	if (p_arg_count < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
	}

	ERR_PRINT(join_string(p_args, p_arg_count));
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::push_warning(Callable::CallError &r_error, const Variant **p_args, int p_arg_count) {
	if (p_arg_count < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
	}

	WARN_PRINT(join_string(p_args, p_arg_count));
	r_error.error = Callable::CallError::CALL_OK;
}

String VariantUtilityFunctions::var_to_str(const Variant &p_var) {
	String vars;
	VariantWriter::write_to_string(p_var, vars, nullptr, nullptr, true, false);
	return vars;
}

String VariantUtilityFunctions::var_to_str_with_objects(const Variant &p_var) {
	String vars;
	VariantWriter::write_to_string(p_var, vars, nullptr, nullptr, true, true);
	return vars;
}

Variant VariantUtilityFunctions::str_to_var(const String &p_var) {
	VariantParser::StreamString ss;
	ss.s = p_var;

	String errs;
	int line = 1;
	Variant ret;
	Error err = VariantParser::parse(&ss, ret, errs, line, nullptr, false);
	if (err != OK) {
		ERR_PRINT("Parse error at line " + itos(line) + ": " + errs + ".");
	}

	return ret;
}

Variant VariantUtilityFunctions::str_to_var_with_objects(const String &p_var) {
	VariantParser::StreamString ss;
	ss.s = p_var;

	String errs;
	int line = 1;
	Variant ret;
	Error err = VariantParser::parse(&ss, ret, errs, line, nullptr, true);
	if (err != OK) {
		ERR_PRINT("Parse error at line " + itos(line) + ": " + errs + ".");
	}

	return ret;
}

PackedByteArray VariantUtilityFunctions::var_to_bytes(const Variant &p_var) {
	int len;
	Error err = encode_variant(p_var, nullptr, len, false);
	if (err != OK) {
		return PackedByteArray();
	}

	PackedByteArray barr;
	barr.resize(len);
	{
		uint8_t *w = barr.ptrw();
		err = encode_variant(p_var, w, len, false);
		if (err != OK) {
			return PackedByteArray();
		}
	}

	return barr;
}

PackedByteArray VariantUtilityFunctions::var_to_bytes_with_objects(const Variant &p_var) {
	int len;
	Error err = encode_variant(p_var, nullptr, len, true);
	if (err != OK) {
		return PackedByteArray();
	}

	PackedByteArray barr;
	barr.resize(len);
	{
		uint8_t *w = barr.ptrw();
		err = encode_variant(p_var, w, len, true);
		if (err != OK) {
			return PackedByteArray();
		}
	}

	return barr;
}

Variant VariantUtilityFunctions::bytes_to_var(const PackedByteArray &p_arr) {
	Variant ret;
	{
		const uint8_t *r = p_arr.ptr();
		Error err = decode_variant(ret, r, p_arr.size(), nullptr, false);
		if (err != OK) {
			return Variant();
		}
	}
	return ret;
}

Variant VariantUtilityFunctions::bytes_to_var_with_objects(const PackedByteArray &p_arr) {
	Variant ret;
	{
		const uint8_t *r = p_arr.ptr();
		Error err = decode_variant(ret, r, p_arr.size(), nullptr, true);
		if (err != OK) {
			return Variant();
		}
	}
	return ret;
}

int64_t VariantUtilityFunctions::hash(const Variant &p_arr) {
	return p_arr.hash();
}

Object *VariantUtilityFunctions::instance_from_id(int64_t p_id) {
	ObjectID id = ObjectID((uint64_t)p_id);
	Object *ret = ObjectDB::get_instance(id);
	return ret;
}

bool VariantUtilityFunctions::is_instance_id_valid(int64_t p_id) {
	return ObjectDB::get_instance(ObjectID((uint64_t)p_id)) != nullptr;
}

bool VariantUtilityFunctions::is_instance_valid(const Variant &p_instance) {
	if (p_instance.get_type() != Variant::OBJECT) {
		return false;
	}
	return p_instance.get_validated_object() != nullptr;
}

uint64_t VariantUtilityFunctions::rid_allocate_id() {
	return RID_AllocBase::_gen_id();
}

RID VariantUtilityFunctions::rid_from_int64(uint64_t p_base) {
	return RID::from_uint64(p_base);
}

bool VariantUtilityFunctions::is_same(const Variant &p_a, const Variant &p_b) {
	return p_a.identity_compare(p_b);
}

String VariantUtilityFunctions::join_string(const Variant **p_args, int p_arg_count) {
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		String os = p_args[i]->operator String();
		s += os;
	}
	return s;
}

#ifdef DEBUG_ENABLED
#define VCALLR *ret = p_func(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...)
#define VCALL p_func(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...)
#else
#define VCALLR *ret = p_func(VariantCaster<P>::cast(*p_args[Is])...)
#define VCALL p_func(VariantCaster<P>::cast(*p_args[Is])...)
#endif // DEBUG_ENABLED

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void call_helperpr(R (*p_func)(Callable::CallError &, P...), Variant *ret, const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
	*ret = p_func(r_error, VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
	(void)p_args; // avoid gcc warning
}

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void call_helperpr(R (*p_func)(P...), Variant *ret, const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;
	VCALLR;
	(void)p_args; // avoid gcc warning
	(void)r_error;
}

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void validated_call_helperpr(R (*p_func)(P...), Variant *ret, const Variant **p_args, IndexSequence<Is...>) {
	*ret = p_func(VariantCaster<P>::cast(*p_args[Is])...);
	(void)p_args;
}

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void validated_call_helperpr(R (*p_func)(Callable::CallError &, P...), Variant *ret, const Variant **p_args, IndexSequence<Is...>) {
	Callable::CallError err;
	*ret = p_func(err, VariantCaster<P>::cast(*p_args[Is])...);
	(void)p_args;
}

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void ptr_call_helperpr(R (*p_func)(P...), void *ret, const void **p_args, IndexSequence<Is...>) {
	PtrToArg<R>::encode(p_func(PtrToArg<P>::convert(p_args[Is])...), ret);
	(void)p_args;
}

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void ptr_call_helperpr(R (*p_func)(Callable::CallError &, P...), void *ret, const void **p_args, IndexSequence<Is...>) {
	Callable::CallError err;
	PtrToArg<R>::encode(p_func(err, PtrToArg<P>::convert(p_args[Is])...), ret);
	(void)p_args;
}

template <typename R, typename... P>
static _FORCE_INLINE_ void call_helperr(R (*p_func)(Callable::CallError &, P...), Variant *ret, const Variant **p_args, Callable::CallError &r_error) {
	call_helperpr(p_func, ret, p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ void call_helperr(R (*p_func)(P...), Variant *ret, const Variant **p_args, Callable::CallError &r_error) {
	call_helperpr(p_func, ret, p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ void validated_call_helperr(R (*p_func)(P...), Variant *ret, const Variant **p_args) {
	validated_call_helperpr(p_func, ret, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ void validated_call_helperr(R (*p_func)(Callable::CallError &, P...), Variant *ret, const Variant **p_args) {
	validated_call_helperpr(p_func, ret, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ void ptr_call_helperr(R (*p_func)(P...), void *ret, const void **p_args) {
	ptr_call_helperpr(p_func, ret, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ void ptr_call_helperr(R (*p_func)(Callable::CallError &, P...), void *ret, const void **p_args) {
	ptr_call_helperpr(p_func, ret, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ int get_arg_count_helperr(R (*p_func)(P...)) {
	return sizeof...(P);
}

template <typename R, typename... P>
static _FORCE_INLINE_ int get_arg_count_helperr(R (*p_func)(Callable::CallError &, P...)) {
	return sizeof...(P);
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type get_arg_type_helperr(R (*p_func)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type get_arg_type_helperr(R (*p_func)(Callable::CallError &, P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg + 1);
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type get_ret_type_helperr(R (*p_func)(P...)) {
	return GetTypeInfo<R>::VARIANT_TYPE;
}

// WITHOUT RET

template <typename... P, size_t... Is>
static _FORCE_INLINE_ void call_helperp(void (*p_func)(P...), const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;
	VCALL;
	(void)p_args;
	(void)r_error;
}

// struct CallHelperVoid
// {
// 	static void call()
// 	{
// 	}
// };
//
// struct CallHelperRet
// {
// 	static void call()
// 	{
// 	}
// };
//
// template <typename TRet, typename... TArgs>
// static _FORCE_INLINE_ void _call_helper()
// {
// 	std::conditional<std::is_same<TRet, void>::value, CallHelperVoid, CallHelperRet>::call();
// }
//
// template <typename... P, size_t... Is>
// static _FORCE_INLINE_ void call_helperp2(void (*p_func)(P...), const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
// 	r_error.error = Callable::CallError::CALL_OK;
// 	p_func(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...);
// 	(void)p_args;
// 	(void)r_error;
// }

template <typename... P, size_t... Is>
static _FORCE_INLINE_ void validated_call_helperp(void (*p_func)(P...), const Variant **p_args, IndexSequence<Is...>) {
	p_func(VariantCaster<P>::cast(*p_args[Is])...);
	(void)p_args;
}

template <typename... P, size_t... Is>
static _FORCE_INLINE_ void ptr_call_helperp(void (*p_func)(P...), const void **p_args, IndexSequence<Is...>) {
	p_func(PtrToArg<P>::convert(p_args[Is])...);
	(void)p_args;
}

template <typename... P>
static _FORCE_INLINE_ void call_helper(void (*p_func)(P...), const Variant **p_args, Callable::CallError &r_error) {
	call_helperp(p_func, p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
static _FORCE_INLINE_ void validated_call_helper(void (*p_func)(P...), const Variant **p_args) {
	validated_call_helperp(p_func, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
static _FORCE_INLINE_ void ptr_call_helper(void (*p_func)(P...), const void **p_args) {
	ptr_call_helperp(p_func, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
static _FORCE_INLINE_ int get_arg_count_helper(void (*p_func)(P...)) {
	return sizeof...(P);
}

template <typename... P>
static _FORCE_INLINE_ Variant::Type get_arg_type_helper(void (*p_func)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename... P>
static _FORCE_INLINE_ Variant::Type get_ret_type_helper(void (*p_func)(P...)) {
	return Variant::NIL;
}

template <typename... P>
static _FORCE_INLINE_ Variant::Type get_ret_type_helper(Variant (*p_func)(P...)) {
	return Variant::NIL;
}

struct VariantUtilityFunctionInfo {
	void (*call_utility)(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) = nullptr;
	Variant::ValidatedUtilityFunction validated_call_utility = nullptr;
	Variant::PTRUtilityFunction ptr_call_utility = nullptr;
	Vector<String> argnames;
	bool is_vararg = false;
	bool returns_value = false;
	int argcount = 0;
	Variant::Type (*get_arg_type)(int) = nullptr;
	Variant::Type return_type;
	Variant::UtilityFunctionType type;
};

static AHashMap<StringName, VariantUtilityFunctionInfo> utility_function_table;
static List<StringName> utility_function_name_table;

template <typename T>
static void register_utility_function(const String &p_name, const Vector<String> &argnames) {
	String name = p_name;
	if (name.begins_with("_")) {
		name = name.substr(1);
	}
	StringName sname = name;
	ERR_FAIL_COND(utility_function_table.has(sname));

	VariantUtilityFunctionInfo bfi;
	bfi.call_utility = T::call;
	bfi.validated_call_utility = T::validated_call;
	bfi.ptr_call_utility = T::ptrcall;
	bfi.is_vararg = T::is_vararg();
	bfi.argnames = argnames;
	bfi.argcount = T::get_argument_count();
	if (!bfi.is_vararg) {
		ERR_FAIL_COND_MSG(argnames.size() != bfi.argcount, vformat("Wrong number of arguments binding utility function: '%s'.", name));
	}
	bfi.get_arg_type = T::get_argument_type;
	bfi.return_type = T::get_return_type();
	bfi.type = T::get_type();
	bfi.returns_value = T::has_return_type();

	utility_function_table.insert(sname, bfi);
	utility_function_name_table.push_back(sname);
}

template <typename _Signature>
struct Func;

template <typename... TArgs>
struct Func<void(TArgs...)> {
	template <void (*m_func)(TArgs...), Variant::UtilityFunctionType m_category>
	struct FuncInner {
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
			call_helper(m_func, p_args, r_error);
		}
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {
			validated_call_helper(m_func, p_args);
		}
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {
			ptr_call_helper(m_func, p_args);
		}
		static int get_argument_count() {
			return get_arg_count_helper(m_func);
		}
		static Variant::Type get_argument_type(int p_arg) {
			return get_arg_type_helper(m_func, p_arg);
		}
		static Variant::Type get_return_type() {
			return get_ret_type_helper(m_func);
		}
		static bool has_return_type() {
			return false;
		}
		static bool is_vararg() {
			return false;
		}
		static Variant::UtilityFunctionType get_type() {
			return m_category;
		}
		static void register_fn(const String &name, const Vector<String> &args) {
			register_utility_function<FuncInner<m_func, m_category>>(name, args);
		}
	};
};

template <>
struct Func<void(Callable::CallError &, const Variant **, int)> {
	template <void (*m_func)(Callable::CallError &, const Variant **, int), Variant::UtilityFunctionType m_category>
	struct FuncInner {
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
			r_error.error = Callable::CallError::CALL_OK;
			m_func(r_error, p_args, p_argcount);
		}
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {
			Callable::CallError c;
			m_func(c, p_args, p_argcount);
		}
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {
			Vector<Variant> args;
			for (int i = 0; i < p_argcount; i++) {
				args.push_back(PtrToArg<Variant>::convert(p_args[i]));
			}
			Vector<const Variant *> argsp;
			for (int i = 0; i < p_argcount; i++) {
				argsp.push_back(&args[i]);
			}
			Variant r;
			validated_call(&r, (const Variant **)argsp.ptr(), p_argcount);
		}
		static int get_argument_count() {
			return 1;
		}
		static Variant::Type get_argument_type(int p_arg) {
			return Variant::NIL;
		}
		static Variant::Type get_return_type() {
			return Variant::NIL;
		}
		static bool has_return_type() {
			return false;
		}
		static bool is_vararg() {
			return true;
		}
		static Variant::UtilityFunctionType get_type() {
			return m_category;
		}
		static void register_fn(const String &name, const Vector<String> &args) {
			register_utility_function<FuncInner<m_func, m_category>>(name, args);
		}
	};
};

template <typename TRet>
struct Func<TRet(Callable::CallError &, const Variant **, int)> {
	template <TRet (*m_func)(Callable::CallError &, const Variant **, int), Variant::UtilityFunctionType m_category>
	struct FuncInner {
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
			r_error.error = Callable::CallError::CALL_OK;
			*r_ret = m_func(r_error, p_args, p_argcount);
		}
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {
			Callable::CallError c;
			*r_ret = m_func(c, p_args, p_argcount);
		}
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {
			Vector<Variant> args;
			for (int i = 0; i < p_argcount; i++) {
				args.push_back(PtrToArg<Variant>::convert(p_args[i]));
			}
			Vector<const Variant *> argsp;
			for (int i = 0; i < p_argcount; i++) {
				argsp.push_back(&args[i]);
			}
			Variant r;
			validated_call(&r, (const Variant **)argsp.ptr(), p_argcount);
			PtrToArg<String>::encode(r.operator String(), ret);
		}
		static int get_argument_count() {
			return 1;
		}
		static Variant::Type get_argument_type(int p_arg) {
			return Variant::NIL;
		}
		static Variant::Type get_return_type() {
			return Variant::STRING;
		}
		static bool has_return_type() {
			return true;
		}
		static bool is_vararg() {
			return true;
		}
		static Variant::UtilityFunctionType get_type() {
			return m_category;
		}
		static void register_fn(const String &name, const Vector<String> &args) {
			register_utility_function<FuncInner<m_func, m_category>>(name, args);
		}
	};
};

template <typename TRet, typename... TArgs>
struct Func<TRet(TArgs...)> {
	template <TRet (*m_func)(TArgs...), Variant::UtilityFunctionType m_category>
	struct FuncInner {
		static void call(Variant *ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
			call_helperr(m_func, ret, p_args, r_error);
		}
		static void validated_call(Variant *ret, const Variant **p_args, int p_argcount) {
			validated_call_helperr(m_func, ret, p_args);
		}
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {
			ptr_call_helperr(m_func, ret, p_args);
		}
		static int get_argument_count() {
			return get_arg_count_helperr(m_func);
		}
		static Variant::Type get_argument_type(int p_arg) {
			return get_arg_type_helperr(m_func, p_arg);
		}
		static Variant::Type get_return_type() {
			return get_ret_type_helperr(m_func);
		}
		static bool has_return_type() {
			return true;
		}
		static bool is_vararg() {
			return false;
		}
		static Variant::UtilityFunctionType get_type() {
			return m_category;
		}
		static void register_fn(const String &name, const Vector<String> &args) {
			register_utility_function<FuncInner<m_func, m_category>>(name, args);
		}
	};
};

void Variant::_register_variant_utility_functions() {
	// Math

	Func<typeof(VariantUtilityFunctions::sin)>::FuncInner<VariantUtilityFunctions::sin, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("sin", sarray("angle_rad"));
	Func<typeof(VariantUtilityFunctions::cos)>::FuncInner<VariantUtilityFunctions::cos, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("cos", sarray("angle_rad"));
	Func<typeof(VariantUtilityFunctions::tan)>::FuncInner<VariantUtilityFunctions::tan, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("tan", sarray("angle_rad"));

	Func<typeof(VariantUtilityFunctions::sinh)>::FuncInner<VariantUtilityFunctions::sinh, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("sinh", sarray("x"));
	Func<typeof(VariantUtilityFunctions::cosh)>::FuncInner<VariantUtilityFunctions::cosh, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("cosh", sarray("x"));
	Func<typeof(VariantUtilityFunctions::tanh)>::FuncInner<VariantUtilityFunctions::tanh, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("tanh", sarray("x"));

	Func<typeof(VariantUtilityFunctions::asin)>::FuncInner<VariantUtilityFunctions::asin, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("asin", sarray("x"));
	Func<typeof(VariantUtilityFunctions::acos)>::FuncInner<VariantUtilityFunctions::acos, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("acos", sarray("x"));
	Func<typeof(VariantUtilityFunctions::atan)>::FuncInner<VariantUtilityFunctions::atan, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("atan", sarray("x"));

	Func<typeof(VariantUtilityFunctions::atan2)>::FuncInner<VariantUtilityFunctions::atan2, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("atan2", sarray("y", "x"));

	Func<typeof(VariantUtilityFunctions::asinh)>::FuncInner<VariantUtilityFunctions::asinh, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("asinh", sarray("x"));
	Func<typeof(VariantUtilityFunctions::acosh)>::FuncInner<VariantUtilityFunctions::acosh, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("acosh", sarray("x"));
	Func<typeof(VariantUtilityFunctions::atanh)>::FuncInner<VariantUtilityFunctions::atanh, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("atanh", sarray("x"));

	Func<typeof(VariantUtilityFunctions::sqrt)>::FuncInner<VariantUtilityFunctions::sqrt, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("sqrt", sarray("x"));
	Func<typeof(VariantUtilityFunctions::fmod)>::FuncInner<VariantUtilityFunctions::fmod, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("fmod", sarray("x", "y"));
	Func<typeof(VariantUtilityFunctions::fposmod)>::FuncInner<VariantUtilityFunctions::fposmod, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("fposmod", sarray("x", "y"));
	Func<typeof(VariantUtilityFunctions::posmod)>::FuncInner<VariantUtilityFunctions::posmod, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("posmod", sarray("x", "y"));

	Func<typeof(VariantUtilityFunctions::floor)>::FuncInner<VariantUtilityFunctions::floor, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("floor", sarray("x"));
	Func<typeof(VariantUtilityFunctions::floorf)>::FuncInner<VariantUtilityFunctions::floorf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("floorf", sarray("x"));
	Func<typeof(VariantUtilityFunctions::floori)>::FuncInner<VariantUtilityFunctions::floori, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("floori", sarray("x"));

	Func<typeof(VariantUtilityFunctions::ceil)>::FuncInner<VariantUtilityFunctions::ceil, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("ceil", sarray("x"));
	Func<typeof(VariantUtilityFunctions::ceilf)>::FuncInner<VariantUtilityFunctions::ceilf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("ceilf", sarray("x"));
	Func<typeof(VariantUtilityFunctions::ceili)>::FuncInner<VariantUtilityFunctions::ceili, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("ceili", sarray("x"));

	Func<typeof(VariantUtilityFunctions::round)>::FuncInner<VariantUtilityFunctions::round, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("round", sarray("x"));
	Func<typeof(VariantUtilityFunctions::roundf)>::FuncInner<VariantUtilityFunctions::roundf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("roundf", sarray("x"));
	Func<typeof(VariantUtilityFunctions::roundi)>::FuncInner<VariantUtilityFunctions::roundi, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("roundi", sarray("x"));

	Func<typeof(VariantUtilityFunctions::abs)>::FuncInner<VariantUtilityFunctions::abs, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("abs", sarray("x"));
	Func<typeof(VariantUtilityFunctions::absf)>::FuncInner<VariantUtilityFunctions::absf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("absf", sarray("x"));
	Func<typeof(VariantUtilityFunctions::absi)>::FuncInner<VariantUtilityFunctions::absi, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("absi", sarray("x"));

	Func<typeof(VariantUtilityFunctions::sign)>::FuncInner<VariantUtilityFunctions::sign, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("sign", sarray("x"));
	Func<typeof(VariantUtilityFunctions::signf)>::FuncInner<VariantUtilityFunctions::signf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("signf", sarray("x"));
	Func<typeof(VariantUtilityFunctions::signi)>::FuncInner<VariantUtilityFunctions::signi, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("signi", sarray("x"));

	Func<typeof(VariantUtilityFunctions::snapped)>::FuncInner<VariantUtilityFunctions::snapped, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("snapped", sarray("x", "step"));
	Func<typeof(VariantUtilityFunctions::snappedf)>::FuncInner<VariantUtilityFunctions::snappedf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("snappedf", sarray("x", "step"));
	Func<typeof(VariantUtilityFunctions::snappedi)>::FuncInner<VariantUtilityFunctions::snappedi, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("snappedi", sarray("x", "step"));

	Func<typeof(VariantUtilityFunctions::pow)>::FuncInner<VariantUtilityFunctions::pow, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("pow", sarray("base", "exp"));
	Func<typeof(VariantUtilityFunctions::log)>::FuncInner<VariantUtilityFunctions::log, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("log", sarray("x"));
	Func<typeof(VariantUtilityFunctions::exp)>::FuncInner<VariantUtilityFunctions::exp, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("exp", sarray("x"));

	Func<typeof(VariantUtilityFunctions::is_nan)>::FuncInner<VariantUtilityFunctions::is_nan, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("is_nan", sarray("x"));
	Func<typeof(VariantUtilityFunctions::is_inf)>::FuncInner<VariantUtilityFunctions::is_inf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("is_inf", sarray("x"));

	Func<typeof(VariantUtilityFunctions::is_equal_approx)>::FuncInner<VariantUtilityFunctions::is_equal_approx, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("is_equal_approx", sarray("a", "b"));
	Func<typeof(VariantUtilityFunctions::is_zero_approx)>::FuncInner<VariantUtilityFunctions::is_zero_approx, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("is_zero_approx", sarray("x"));
	Func<typeof(VariantUtilityFunctions::is_finite)>::FuncInner<VariantUtilityFunctions::is_finite, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("is_finite", sarray("x"));

	Func<typeof(VariantUtilityFunctions::ease)>::FuncInner<VariantUtilityFunctions::ease, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("ease", sarray("x", "curve"));
	Func<typeof(VariantUtilityFunctions::step_decimals)>::FuncInner<VariantUtilityFunctions::step_decimals, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("step_decimals", sarray("x"));

	Func<typeof(VariantUtilityFunctions::lerp)>::FuncInner<VariantUtilityFunctions::lerp, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("lerp", sarray("from", "to", "weight"));
	Func<typeof(VariantUtilityFunctions::lerpf)>::FuncInner<VariantUtilityFunctions::lerpf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("lerpf", sarray("from", "to", "weight"));
	Func<typeof(VariantUtilityFunctions::cubic_interpolate)>::FuncInner<VariantUtilityFunctions::cubic_interpolate, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("cubic_interpolate", sarray("from", "to", "pre", "post", "weight"));
	Func<typeof(VariantUtilityFunctions::cubic_interpolate_angle)>::FuncInner<VariantUtilityFunctions::cubic_interpolate_angle, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("cubic_interpolate_angle", sarray("from", "to", "pre", "post", "weight"));
	Func<typeof(VariantUtilityFunctions::cubic_interpolate_in_time)>::FuncInner<VariantUtilityFunctions::cubic_interpolate_in_time, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("cubic_interpolate_in_time", sarray("from", "to", "pre", "post", "weight", "to_t", "pre_t", "post_t"));
	Func<typeof(VariantUtilityFunctions::cubic_interpolate_angle_in_time)>::FuncInner<VariantUtilityFunctions::cubic_interpolate_angle_in_time, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("cubic_interpolate_angle_in_time", sarray("from", "to", "pre", "post", "weight", "to_t", "pre_t", "post_t"));
	Func<typeof(VariantUtilityFunctions::bezier_interpolate)>::FuncInner<VariantUtilityFunctions::bezier_interpolate, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("bezier_interpolate", sarray("start", "control_1", "control_2", "end", "t"));
	Func<typeof(VariantUtilityFunctions::bezier_derivative)>::FuncInner<VariantUtilityFunctions::bezier_derivative, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("bezier_derivative", sarray("start", "control_1", "control_2", "end", "t"));
	Func<typeof(VariantUtilityFunctions::angle_difference)>::FuncInner<VariantUtilityFunctions::angle_difference, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("angle_difference", sarray("from", "to"));
	Func<typeof(VariantUtilityFunctions::lerp_angle)>::FuncInner<VariantUtilityFunctions::lerp_angle, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("lerp_angle", sarray("from", "to", "weight"));
	Func<typeof(VariantUtilityFunctions::inverse_lerp)>::FuncInner<VariantUtilityFunctions::inverse_lerp, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("inverse_lerp", sarray("from", "to", "weight"));
	Func<typeof(VariantUtilityFunctions::remap)>::FuncInner<VariantUtilityFunctions::remap, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("remap", sarray("value", "istart", "istop", "ostart", "ostop"));
	Func<typeof(VariantUtilityFunctions::remap_default)>::FuncInner<VariantUtilityFunctions::remap_default, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("remap_default", sarray("value", "istart", "istop", "ostart", "ostop", "default_value"));

	Func<typeof(VariantUtilityFunctions::smoothstep)>::FuncInner<VariantUtilityFunctions::smoothstep, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("smoothstep", sarray("from", "to", "x"));
	Func<typeof(VariantUtilityFunctions::move_toward)>::FuncInner<VariantUtilityFunctions::move_toward, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("move_toward", sarray("from", "to", "delta"));
	Func<typeof(VariantUtilityFunctions::rotate_toward)>::FuncInner<VariantUtilityFunctions::rotate_toward, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("rotate_toward", sarray("from", "to", "delta"));

	Func<typeof(VariantUtilityFunctions::deg_to_rad)>::FuncInner<VariantUtilityFunctions::deg_to_rad, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("deg_to_rad", sarray("deg"));
	Func<typeof(VariantUtilityFunctions::rad_to_deg)>::FuncInner<VariantUtilityFunctions::rad_to_deg, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("rad_to_deg", sarray("rad"));
	Func<typeof(VariantUtilityFunctions::linear_to_db)>::FuncInner<VariantUtilityFunctions::linear_to_db, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("linear_to_db", sarray("lin"));
	Func<typeof(VariantUtilityFunctions::db_to_linear)>::FuncInner<VariantUtilityFunctions::db_to_linear, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("db_to_linear", sarray("db"));

	Func<typeof(VariantUtilityFunctions::wrap)>::FuncInner<VariantUtilityFunctions::wrap, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("wrap", sarray("value", "min", "max"));
	Func<typeof(VariantUtilityFunctions::wrapi)>::FuncInner<VariantUtilityFunctions::wrapi, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("wrapi", sarray("value", "min", "max"));
	Func<typeof(VariantUtilityFunctions::wrapf)>::FuncInner<VariantUtilityFunctions::wrapf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("wrapf", sarray("value", "min", "max"));

	Func<typeof(VariantUtilityFunctions::max)>::FuncInner<VariantUtilityFunctions::max, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("max", sarray());
	Func<typeof(VariantUtilityFunctions::maxi)>::FuncInner<VariantUtilityFunctions::maxi, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("maxi", sarray("a", "b"));
	Func<typeof(VariantUtilityFunctions::maxf)>::FuncInner<VariantUtilityFunctions::maxf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("maxf", sarray("a", "b"));

	Func<typeof(VariantUtilityFunctions::min)>::FuncInner<VariantUtilityFunctions::min, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("min", sarray());
	Func<typeof(VariantUtilityFunctions::mini)>::FuncInner<VariantUtilityFunctions::mini, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("mini", sarray("a", "b"));
	Func<typeof(VariantUtilityFunctions::minf)>::FuncInner<VariantUtilityFunctions::minf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("minf", sarray("a", "b"));

	Func<typeof(VariantUtilityFunctions::clamp)>::FuncInner<VariantUtilityFunctions::clamp, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("clamp", sarray("value", "min", "max"));
	Func<typeof(VariantUtilityFunctions::clampi)>::FuncInner<VariantUtilityFunctions::clampi, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("clampi", sarray("value", "min", "max"));
	Func<typeof(VariantUtilityFunctions::clampf)>::FuncInner<VariantUtilityFunctions::clampf, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("clampf", sarray("value", "min", "max"));

	Func<typeof(VariantUtilityFunctions::nearest_po2)>::FuncInner<VariantUtilityFunctions::nearest_po2, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("nearest_po2", sarray("value"));
	Func<typeof(VariantUtilityFunctions::pingpong)>::FuncInner<VariantUtilityFunctions::pingpong, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("pingpong", sarray("value", "length"));

	Func<typeof(VariantUtilityFunctions::sigmoid)>::FuncInner<VariantUtilityFunctions::sigmoid, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("sigmoid", sarray("x"));
	Func<typeof(VariantUtilityFunctions::sigmoid_approx)>::FuncInner<VariantUtilityFunctions::sigmoid_approx, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("sigmoid_approx", sarray("x"));
	Func<typeof(VariantUtilityFunctions::sigmoid_affine)>::FuncInner<VariantUtilityFunctions::sigmoid_affine, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("sigmoid_affine", sarray("x", "amplitude", "y_translation"));
	Func<typeof(VariantUtilityFunctions::sigmoid_affine_approx)>::FuncInner<VariantUtilityFunctions::sigmoid_affine_approx, Variant::UTILITY_FUNC_TYPE_MATH>::register_fn("sigmoid_affine_approx", sarray("x", "amplitude", "y_translation"));

	// Random

	Func<void()>::FuncInner<VariantUtilityFunctions::randomize, Variant::UTILITY_FUNC_TYPE_RANDOM>::register_fn("randomize", sarray());
	Func<typeof(VariantUtilityFunctions::randi)>::FuncInner<VariantUtilityFunctions::randi, Variant::UTILITY_FUNC_TYPE_RANDOM>::register_fn("randi", sarray());
	Func<typeof(VariantUtilityFunctions::randf)>::FuncInner<VariantUtilityFunctions::randf, Variant::UTILITY_FUNC_TYPE_RANDOM>::register_fn("randf", sarray());
	Func<typeof(VariantUtilityFunctions::randi_range)>::FuncInner<VariantUtilityFunctions::randi_range, Variant::UTILITY_FUNC_TYPE_RANDOM>::register_fn("randi_range", sarray("from", "to"));
	Func<typeof(VariantUtilityFunctions::randf_range)>::FuncInner<VariantUtilityFunctions::randf_range, Variant::UTILITY_FUNC_TYPE_RANDOM>::register_fn("randf_range", sarray("from", "to"));
	Func<typeof(VariantUtilityFunctions::randfn)>::FuncInner<VariantUtilityFunctions::randfn, Variant::UTILITY_FUNC_TYPE_RANDOM>::register_fn("randfn", sarray("mean", "deviation"));
	Func<void(int64_t)>::FuncInner<VariantUtilityFunctions::seed, Variant::UTILITY_FUNC_TYPE_RANDOM>::register_fn("seed", sarray("base"));
	Func<typeof(VariantUtilityFunctions::rand_from_seed)>::FuncInner<VariantUtilityFunctions::rand_from_seed, Variant::UTILITY_FUNC_TYPE_RANDOM>::register_fn("rand_from_seed", sarray("seed"));

	// Utility

	Func<typeof(VariantUtilityFunctions::weakref)>::FuncInner<VariantUtilityFunctions::weakref, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("weakref", sarray("obj"));
	Func<typeof(VariantUtilityFunctions::_typeof)>::FuncInner<VariantUtilityFunctions::_typeof, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("_typeof", sarray("variable"));
	Func<typeof(VariantUtilityFunctions::type_convert)>::FuncInner<VariantUtilityFunctions::type_convert, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("type_convert", sarray("variant", "type"));
	Func<typeof(VariantUtilityFunctions::str)>::FuncInner<VariantUtilityFunctions::str, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("str", sarray());
	Func<typeof(VariantUtilityFunctions::error_string)>::FuncInner<VariantUtilityFunctions::error_string, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("error_string", sarray("error"));
	Func<typeof(VariantUtilityFunctions::type_string)>::FuncInner<VariantUtilityFunctions::type_string, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("type_string", sarray("type"));
	Func<typeof(VariantUtilityFunctions::print)>::FuncInner<VariantUtilityFunctions::print, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("print", sarray());
	Func<typeof(VariantUtilityFunctions::print_rich)>::FuncInner<VariantUtilityFunctions::print_rich, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("print_rich", sarray());
	Func<typeof(VariantUtilityFunctions::printerr)>::FuncInner<VariantUtilityFunctions::printerr, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("printerr", sarray());
	Func<typeof(VariantUtilityFunctions::printt)>::FuncInner<VariantUtilityFunctions::printt, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("printt", sarray());
	Func<typeof(VariantUtilityFunctions::prints)>::FuncInner<VariantUtilityFunctions::prints, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("prints", sarray());
	Func<typeof(VariantUtilityFunctions::printraw)>::FuncInner<VariantUtilityFunctions::printraw, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("printraw", sarray());
	Func<typeof(VariantUtilityFunctions::_print_verbose)>::FuncInner<VariantUtilityFunctions::_print_verbose, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("print_verbose", sarray());
	Func<typeof(VariantUtilityFunctions::push_error)>::FuncInner<VariantUtilityFunctions::push_error, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("push_error", sarray());
	Func<typeof(VariantUtilityFunctions::push_warning)>::FuncInner<VariantUtilityFunctions::push_warning, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("push_warning", sarray());

	Func<typeof(VariantUtilityFunctions::var_to_str)>::FuncInner<VariantUtilityFunctions::var_to_str, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("var_to_str", sarray("variable"));
	Func<typeof(VariantUtilityFunctions::str_to_var)>::FuncInner<VariantUtilityFunctions::str_to_var, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("str_to_var", sarray("string"));

	Func<typeof(VariantUtilityFunctions::var_to_str_with_objects)>::FuncInner<VariantUtilityFunctions::var_to_str_with_objects, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("var_to_str_with_objects", sarray("variable"));
	Func<typeof(VariantUtilityFunctions::str_to_var_with_objects)>::FuncInner<VariantUtilityFunctions::str_to_var_with_objects, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("str_to_var_with_objects", sarray("string"));

	Func<typeof(VariantUtilityFunctions::var_to_bytes)>::FuncInner<VariantUtilityFunctions::var_to_bytes, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("var_to_bytes", sarray("variable"));
	Func<typeof(VariantUtilityFunctions::bytes_to_var)>::FuncInner<VariantUtilityFunctions::bytes_to_var, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("bytes_to_var", sarray("bytes"));

	Func<typeof(VariantUtilityFunctions::var_to_bytes_with_objects)>::FuncInner<VariantUtilityFunctions::var_to_bytes_with_objects, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("var_to_bytes_with_objects", sarray("variable"));
	Func<typeof(VariantUtilityFunctions::bytes_to_var_with_objects)>::FuncInner<VariantUtilityFunctions::bytes_to_var_with_objects, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("bytes_to_var_with_objects", sarray("bytes"));

	Func<typeof(VariantUtilityFunctions::hash)>::FuncInner<VariantUtilityFunctions::hash, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("hash", sarray("variable"));

	Func<typeof(VariantUtilityFunctions::instance_from_id)>::FuncInner<VariantUtilityFunctions::instance_from_id, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("instance_from_id", sarray("instance_id"));
	Func<typeof(VariantUtilityFunctions::is_instance_id_valid)>::FuncInner<VariantUtilityFunctions::is_instance_id_valid, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("is_instance_id_valid", sarray("id"));
	Func<typeof(VariantUtilityFunctions::is_instance_valid)>::FuncInner<VariantUtilityFunctions::is_instance_valid, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("is_instance_valid", sarray("instance"));

	Func<typeof(VariantUtilityFunctions::rid_allocate_id)>::FuncInner<VariantUtilityFunctions::rid_allocate_id, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("rid_allocate_id", Vector<String>());
	Func<typeof(VariantUtilityFunctions::rid_from_int64)>::FuncInner<VariantUtilityFunctions::rid_from_int64, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("rid_from_int64", sarray("base"));

	Func<typeof(VariantUtilityFunctions::is_same)>::FuncInner<VariantUtilityFunctions::is_same, Variant::UTILITY_FUNC_TYPE_GENERAL>::register_fn("is_same", sarray("a", "b"));
}

void Variant::_unregister_variant_utility_functions() {
	utility_function_table.clear();
	utility_function_name_table.clear();
}

void Variant::call_utility_function(const StringName &p_name, Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		r_error.argument = 0;
		r_error.expected = 0;
		return;
	}

	if (unlikely(!bfi->is_vararg && p_argcount < bfi->argcount)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = bfi->argcount;
		return;
	}

	if (unlikely(!bfi->is_vararg && p_argcount > bfi->argcount)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = bfi->argcount;
		return;
	}

	bfi->call_utility(r_ret, p_args, p_argcount, r_error);
}

bool Variant::has_utility_function(const StringName &p_name) {
	return utility_function_table.has(p_name);
}

Variant::ValidatedUtilityFunction Variant::get_validated_utility_function(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return nullptr;
	}

	return bfi->validated_call_utility;
}

Variant::PTRUtilityFunction Variant::get_ptr_utility_function(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return nullptr;
	}

	return bfi->ptr_call_utility;
}

Variant::UtilityFunctionType Variant::get_utility_function_type(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return Variant::UTILITY_FUNC_TYPE_MATH;
	}

	return bfi->type;
}

MethodInfo Variant::get_utility_function_info(const StringName &p_name) {
	MethodInfo info;
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (bfi) {
		info.name = p_name;
		if (bfi->returns_value && bfi->return_type == Variant::NIL) {
			info.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
		}
		info.return_val.type = bfi->return_type;
		if (bfi->is_vararg) {
			info.flags |= METHOD_FLAG_VARARG;
		}
		for (int i = 0; i < bfi->argnames.size(); ++i) {
			PropertyInfo arg;
			arg.type = bfi->get_arg_type(i);
			arg.name = bfi->argnames[i];
			info.arguments.push_back(arg);
		}
	}
	return info;
}

int Variant::get_utility_function_argument_count(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return 0;
	}

	return bfi->argcount;
}

Variant::Type Variant::get_utility_function_argument_type(const StringName &p_name, int p_arg) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return Variant::NIL;
	}

	return bfi->get_arg_type(p_arg);
}

String Variant::get_utility_function_argument_name(const StringName &p_name, int p_arg) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return String();
	}
	ERR_FAIL_INDEX_V(p_arg, bfi->argnames.size(), String());
	ERR_FAIL_COND_V(bfi->is_vararg, String());
	return bfi->argnames[p_arg];
}

bool Variant::has_utility_function_return_value(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return false;
	}
	return bfi->returns_value;
}

Variant::Type Variant::get_utility_function_return_type(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return Variant::NIL;
	}

	return bfi->return_type;
}

bool Variant::is_utility_function_vararg(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return false;
	}

	return bfi->is_vararg;
}

uint32_t Variant::get_utility_function_hash(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.getptr(p_name);
	ERR_FAIL_NULL_V(bfi, 0);

	uint32_t hash = hash_murmur3_one_32(bfi->is_vararg);
	hash = hash_murmur3_one_32(bfi->returns_value, hash);
	if (bfi->returns_value) {
		hash = hash_murmur3_one_32(bfi->return_type, hash);
	}
	hash = hash_murmur3_one_32(bfi->argcount, hash);
	for (int i = 0; i < bfi->argcount; i++) {
		hash = hash_murmur3_one_32(bfi->get_arg_type(i), hash);
	}

	return hash_fmix32(hash);
}

void Variant::get_utility_function_list(List<StringName> *r_functions) {
	for (const StringName &E : utility_function_name_table) {
		r_functions->push_back(E);
	}
}

int Variant::get_utility_function_count() {
	return utility_function_name_table.size();
}
