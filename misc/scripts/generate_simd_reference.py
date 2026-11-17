#!/usr/bin/env python3

"""Generates tests/core/math/simd_reference.h: verbatim copies of the pre-SIMD
implementations, renamed with a ref_ prefix and wrapped in subclasses, so the tests compare the
current engine code against the exact scalar originals."""

import argparse
import os
import re
import subprocess
import sys
from typing import List

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..") + os.sep


# Keep the oracle independent after this branch is merged or master moves.
SCALAR_BASE = "4e31c91ea328667fcc8fee3da27dc10a4d0811fe"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("base", nargs="?", default=SCALAR_BASE, help="pre-SIMD scalar revision")
parser.add_argument("--check", action="store_true", help="verify the committed reference without writing it")
parser.add_argument("--output", default=os.path.join(ROOT, "tests/core/math/simd_reference.h"))
args = parser.parse_args()
BASE = (
    subprocess.check_output(["git", "rev-parse", "--verify", "--end-of-options", args.base + "^{commit}"], cwd=ROOT)
    .decode()
    .strip()
)


def head(path):
    source = subprocess.check_output(["git", "show", BASE + ":" + path], cwd=ROOT).decode()
    if re.search(r"\b(?:GODOT_(?:SIMD|STRING_SIMD|IMAGE_SIMD)\w*|SIMDGeometry|AudioMixKernels)\b", source):
        sys.exit("Refusing SIMD-bearing scalar baseline: " + BASE + ":" + path)
    return source


def extract(text, start_pat):
    lines = text.split("\n")
    for i, line in enumerate(lines):
        if re.match(start_pat, line):
            indent = re.match(r"^(\s*)", line).group(1)
            for j in range(i + 1, len(lines)):
                if lines[j] == indent + "}":
                    return "\n".join(lines[i : j + 1])
            raise Exception("no end for " + start_pat)
    raise Exception("no start for " + start_pat)


def member(text, start_pat, cls, new_name, old_name=None, out_of_line=False):
    """Turns an in-class or out-of-class member definition into an in-class ref_ member.

    out_of_line marks functions that were defined in a .cpp file: the copy is made non-inlinable so
    benchmarks compare it with the engine function under the same calling conditions."""
    fn = extract(text, start_pat)
    lines = fn.split("\n")
    first = lines[0]
    in_class = first.startswith("\t")
    first = first.strip()
    first = re.sub(r"^(_FORCE_INLINE_ |_ALWAYS_INLINE_ |inline |constexpr |static )+", "", first)
    if old_name is None:
        m = re.search(r"(" + re.escape(cls) + r"::)?(operator==|[A-Za-z_]\w*)\(", first)
        old = m.group(0)
    else:
        old = old_name
    assert old in first, (old, first)
    first = first.replace(old, new_name + "(", 1)
    body = lines[1:]
    if not in_class:
        body = ["\t" + b if b else b for b in body]
    prefix = "_NO_INLINE_ " if out_of_line else ""
    return "\t" + prefix + first + "\n" + "\n".join(body) + "\n"


out: List[str] = []
w = out.append

w("""/**************************************************************************/
/*  simd_reference.h                                                      */
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

// Scalar reference implementations for the SIMD tests and benchmarks.
// Generated by misc/scripts/generate_simd_reference.py; do not edit by hand.
// Everything in the Ref* structs is a verbatim copy of the code that existed before the SIMD paths
// were added (generated from git, with the function renamed to ref_*), so the tests compare the
// current engine output against the original scalar output bit for bit.

#include "core/io/image.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/bvh_abb.h"
#include "core/math/color.h"
#include "core/math/dynamic_bvh.h"
#include "core/math/plane.h"
#include "core/math/projection.h"
#include "core/math/quaternion.h"
#include "core/math/rect2.h"
#include "core/math/transform_2d.h"
#include "core/math/transform_3d.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/string/ucaps.h"
#include "core/string/ustring.h"
#include "servers/audio/audio_server.h"
#include "servers/audio/effects/eq_filter.h"
#include "servers/rendering/renderer_scene_cull.h"

namespace SIMDReference {
""")

# ------------------------------------------------------------------ math types
aabb_h = head("core/math/aabb.h")
aabb_cpp = head("core/math/aabb.cpp")
w("struct RefAABB : public AABB {\n\tRefAABB(const AABB &p_aabb) :\n\t\t\tAABB(p_aabb) {}\n\n")
w(member(aabb_h, r"^bool AABB::intersects_convex_shape\(", "AABB", "ref_intersects_convex_shape") + "\n")
w(member(aabb_h, r"^bool AABB::inside_convex_shape\(", "AABB", "ref_inside_convex_shape") + "\n")
w(member(aabb_h, r"^bool AABB::smits_intersect_ray\(", "AABB", "ref_smits_intersect_ray") + "\n")
w(member(aabb_cpp, r"^bool AABB::intersects_plane\(", "AABB", "ref_intersects_plane", out_of_line=True))
w("};\n\n")

basis_cpp = head("core/math/basis.cpp")
w("struct RefBasis : public Basis {\n\tRefBasis(const Basis &p_basis) :\n\t\t\tBasis(p_basis) {}\n\n")
w(member(basis_cpp, r"^Vector3 Basis::get_scale_abs\(", "Basis", "ref_get_scale_abs", out_of_line=True))
w("};\n\n")

t3_h = head("core/math/transform_3d.h")
w(
    "struct RefTransform3D : public Transform3D {\n\tRefTransform3D(const Transform3D &p_transform) :\n\t\t\tTransform3D(p_transform) {}\n\n"
)
w(
    member(
        t3_h,
        r"^_FORCE_INLINE_ AABB Transform3D::xform_inv\(const AABB &p_aabb\)",
        "Transform3D",
        "ref_xform_inv_aabb",
        "Transform3D::xform_inv(",
    )
)
w("};\n\n")

v4_cpp = head("core/math/vector4.cpp")
w("struct RefVector4 : public Vector4 {\n\tRefVector4(const Vector4 &p_v) :\n\t\t\tVector4(p_v) {}\n\n")
for name in [
    "is_equal_approx",
    "is_zero_approx",
    "sign",
    "floor",
    "ceil",
    "round",
    "clamp",
    "clampf",
    "cubic_interpolate",
    "cubic_interpolate_in_time",
]:
    w(member(v4_cpp, r"^\S+ Vector4::" + name + r"\(", "Vector4", "ref_" + name, out_of_line=True) + "\n")
w("};\n\n")

v3_h = head("core/math/vector3.h")
v2_h = head("core/math/vector2.h")
w("struct RefVector3 : public Vector3 {\n\tRefVector3(const Vector3 &p_v) :\n\t\t\tVector3(p_v) {}\n\n")
w(member(v3_h, r"^Vector3 Vector3::cubic_interpolate\(", "Vector3", "ref_cubic_interpolate") + "\n")
w(member(v3_h, r"^Vector3 Vector3::cubic_interpolate_in_time\(", "Vector3", "ref_cubic_interpolate_in_time"))
w("};\n\n")
w("struct RefVector2 : public Vector2 {\n\tRefVector2(const Vector2 &p_v) :\n\t\t\tVector2(p_v) {}\n\n")
w(member(v2_h, r"^Vector2 Vector2::cubic_interpolate\(", "Vector2", "ref_cubic_interpolate"))
w("};\n\n")

q_h = head("core/math/quaternion.h")
w("struct RefQuaternion : public Quaternion {\n\tRefQuaternion(const Quaternion &p_q) :\n\t\t\tQuaternion(p_q) {}\n\n")
w(member(q_h, r"^\t_FORCE_INLINE_ Vector3 xform\(const Vector3 &p_v\) const \{", "Quaternion", "ref_xform"))
w("};\n\n")

plane_cpp = head("core/math/plane.cpp")
w("struct RefPlane : public Plane {\n\tRefPlane(const Plane &p_plane) :\n\t\t\tPlane(p_plane) {}\n\n")
w(member(plane_cpp, r"^bool Plane::intersect_3\(", "Plane", "ref_intersect_3", out_of_line=True))
w("};\n\n")

proj_cpp = head("core/math/projection.cpp")
w(
    "struct RefProjection : public Projection {\n\tRefProjection(const Projection &p_projection) :\n\t\t\tProjection(p_projection) {}\n\n"
)
w(member(proj_cpp, r"^Vector4 Projection::xform_inv\(const Vector4", "Projection", "ref_xform_inv", out_of_line=True))
w("};\n\n")

rect_h = head("core/math/rect2.h")
w("struct RefRect2 : public Rect2 {\n\tRefRect2(const Rect2 &p_rect) :\n\t\t\tRect2(p_rect) {}\n\n")
w(member(rect_h, r"^\tinline bool intersects\(const Rect2 &p_rect", "Rect2", "ref_intersects"))
w("};\n\n")

t2_h = head("core/math/transform_2d.h")
w(
    "struct RefTransform2D : public Transform2D {\n\tRefTransform2D(const Transform2D &p_transform) :\n\t\t\tTransform2D(p_transform) {}\n\n"
)
w(
    member(
        t2_h,
        r"^Rect2 Transform2D::xform\(const Rect2 &p_rect\)",
        "Transform2D",
        "ref_xform_rect",
        "Transform2D::xform(",
    )
)
w("};\n\n")

color_h = head("core/math/color.h")
w("struct RefColor : public Color {\n\tRefColor(const Color &p_color) :\n\t\t\tColor(p_color) {}\n\n")
for name in ["to_rgba32", "to_argb32", "to_abgr32", "to_rgba64", "to_argb64", "to_abgr64"]:
    w(member(color_h, r"^\tconstexpr \S+ " + name + r"\(\) const noexcept \{", "Color", "ref_" + name) + "\n")
w(
    member(color_h, r"^\tconstexpr bool operator==\(const Color &p_color\) const noexcept \{", "Color", "ref_equals")
    + "\n"
)
w(member(color_h, r"^\tconstexpr Color clamp\(", "Color", "ref_clamp") + "\n")
w(member(color_h, r"^\tconstexpr Color darkened\(", "Color", "ref_darkened") + "\n")
w(member(color_h, r"^\tconstexpr Color lightened\(", "Color", "ref_lightened"))
w("};\n\n")

# ------------------------------------------------------------------ BVH and culling
bvh_h = head("core/math/bvh_abb.h")
w(
    "template <typename BOUNDS = AABB, typename POINT = Vector3>\nstruct RefBVH_ABB : public BVH_ABB<BOUNDS, POINT> {\n\tusing Base = BVH_ABB<BOUNDS, POINT>;\n\tusing Base::min;\n\tusing Base::neg_max;\n\tusing Base::_any_lessthan;\n\tusing Base::_any_morethan;\n\tRefBVH_ABB(const Base &p_abb) :\n\t\t\tBase(p_abb) {}\n\n"
)
w(
    member(bvh_h, r"^\tbool intersects\(const BVH_ABB &p_o\) const \{", "BVH_ABB", "ref_intersects").replace(
        "const BVH_ABB &", "const Base &"
    )
    + "\n"
)
w(
    member(
        bvh_h, r"^\tbool intersects_swizzled\(const BVH_ABB &p_o\) const \{", "BVH_ABB", "ref_intersects_swizzled"
    ).replace("const BVH_ABB &", "const Base &")
    + "\n"
)
w(
    member(bvh_h, r"^\tbool is_other_within\(const BVH_ABB &p_o\) const \{", "BVH_ABB", "ref_is_other_within").replace(
        "const BVH_ABB &", "const Base &"
    )
)
w("};\n\n")

cull_h = head("servers/rendering/renderer_scene_cull.h")
w(
    "struct RefInstanceBounds : public RendererSceneCull::InstanceBounds {\n\tusing Frustum = RendererSceneCull::Frustum;\n\tRefInstanceBounds(const RendererSceneCull::InstanceBounds &p_bounds) :\n\t\t\tRendererSceneCull::InstanceBounds(p_bounds) {}\n\n"
)
for name in ["in_frustum"]:
    fn = extract(cull_h, r"^\t\t_ALWAYS_INLINE_ bool " + name + r"\(")
    lines = [line[1:] if line.startswith("\t") else line for line in fn.split("\n")]
    lines[0] = lines[0].replace("_ALWAYS_INLINE_ bool " + name + "(", "bool ref_" + name + "(")
    w("\n".join(lines) + "\n\n")
w("};\n\n")

# ------------------------------------------------------------------ strings
ustr = head("core/string/ustring.cpp")


def string_member(start_pat, new_name, old):
    fn = member(ustr, start_pat, "String", new_name, old, out_of_line=True)
    fn = fn.replace("_replacement_char", "char32_t(0xfffd)")
    return fn


w(
    "struct RefString : public String {\n\tRefString() {}\n\tRefString(const String &p_string) :\n\t\t\tString(p_string) {}\n\n"
)
w(string_member(r"^uint32_t String::hash\(\) const \{", "ref_hash", "String::hash(") + "\n")
w(string_member(r"^bool String::operator==\(const char \*p_str\) const \{", "ref_equals", "String::operator==(") + "\n")
w(string_member(r"^String String::to_upper\(\) const \{", "ref_to_upper", "String::to_upper(") + "\n")
w(string_member(r"^String String::to_lower\(\) const \{", "ref_to_lower", "String::to_lower(") + "\n")
w(
    string_member(
        r"^CharString String::utf8\(Vector<uint8_t> \*r_ch_length_map\) const \{", "ref_utf8", "String::utf8("
    )
    + "\n"
)
w(
    string_member(
        r"^Error String::append_utf8\(const char \*p_utf8, int p_len, bool p_skip_cr\) \{",
        "ref_append_utf8",
        "String::append_utf8(",
    )
)
w("};\n\n")

# ------------------------------------------------------------------ image templates (file-static in image.cpp)
img = head("core/io/image.cpp")
for pat in [
    r"^template <uint32_t read_bytes, bool read_alpha, uint32_t write_bytes",
    r"^template <int CC, typename T>\nstatic void _scale_bilinear",
]:
    pass
lines = img.split("\n")


def extract_template(first_line_pat, func_pat):
    for i, line in enumerate(lines):
        if re.match(first_line_pat, line) and any(
            re.match(func_pat, lines[k]) for k in range(i + 1, min(i + 4, len(lines)))
        ):
            for j in range(i + 1, len(lines)):
                if lines[j] == "}":
                    return "\n".join(lines[i : j + 1])
    raise Exception("template not found " + func_pat)


w(
    extract_template(r"^template <uint32_t read_bytes", r"^static void _convert\(").replace(
        "static void _convert(", "static void ref_convert("
    )
    + "\n\n"
)
w(
    extract_template(r"^template <int CC, typename T>", r"^static void _scale_bilinear\(").replace(
        "static void _scale_bilinear(", "static void ref_scale_bilinear("
    )
    + "\n\n"
)
w(
    extract_template(
        r"^template <typename Component, int CC, bool renormalize,", r"^static void _generate_po2_mipmap\("
    ).replace("static void _generate_po2_mipmap(", "static void ref_generate_po2_mipmap(")
    + "\n\n"
)
for name in [
    "average_4_uint8",
    "average_4_float",
    "average_4_half",
    "renormalize_uint8",
    "renormalize_float",
    "renormalize_half",
]:
    fn = extract(img, r"^void Image::" + name + r"\(")
    w(fn.replace("void Image::" + name + "(", "static void ref_" + name + "(", 1) + "\n\n")


# ------------------------------------------------------------------ audio
# AudioEffectStereoEnhanceInstance::process on stand-in state.
se = head("servers/audio/effects/audio_effect_stereo_enhance.cpp")
w("""struct RefStereoEnhance {
	struct Params {
		float pan_pullout = 1.0f;
		float time_pullout = 0.0f;
		float surround = 0.0f;
	};
	Params params;
	Params *base = &params;
	float *delay_ringbuff = nullptr;
	unsigned int ringbuff_pos = 0;
	unsigned int ringbuff_mask = 0;

""")
w(
    member(
        se,
        r"^void AudioEffectStereoEnhanceInstance::process\(",
        "AudioEffectStereoEnhanceInstance",
        "process",
        "AudioEffectStereoEnhanceInstance::process(",
    )
)
w("};\n\n")


# ------------------------------------------------------------------ image dispatchers (from Image::convert / _generate_mipmap_from_format)
conv = extract(img, r"^Error Image::convert\(Format p_new_format\) \{|^void Image::convert\(Format p_new_format\) \{")
sw = conv[conv.index("\t\tswitch (conversion_type) {") :]
sw_lines = sw.split("\n")
cases = []
for i, line in enumerate(sw_lines):
    if "_convert<" in line:
        cases.append(sw_lines[i - 1].strip())
        cases.append("\t\t" + line.strip().replace("_convert<", "ref_convert<"))
        cases.append("\t\tbreak;")
for k in ["average_4_rgbe9995", "renormalize_rgbe9995"]:
    fn = extract(img, r"^void Image::" + k + r"\(")
    w(fn.replace("void Image::" + k + "(", "static void ref_" + k + "(", 1) + "\n\n")
body = "\n".join(("\t" + c) if c.startswith("case") else ("\t" + c) for c in cases)
body = body.replace("FORMAT_", "Image::FORMAT_")
w(
    "static void ref_convert_dispatch(Image::Format format, Image::Format p_new_format, int mip_width, int mip_height, const uint8_t *rptr, uint8_t *wptr) {\n\tconst int conversion_type = format | p_new_format << 8;\n\tswitch (conversion_type) {\n"
    + body
    + "\n\t\tdefault:\n\t\t\tbreak;\n\t}\n}\n\n"
)
mip = extract(img, r"^void Image::_generate_mipmap_from_format\(")
mip = mip.replace("void Image::_generate_mipmap_from_format(", "static void ref_generate_mipmap_from_format(", 1)
mip = (
    mip.replace("_generate_po2_mipmap<", "ref_generate_po2_mipmap<")
    .replace("Image::average_4_", "ref_average_4_")
    .replace("Image::renormalize_", "ref_renormalize_")
)
w(mip + "\n\n")

# ------------------------------------------------------------------ DynamicBVH::Volume (private struct in the engine)
dbvh = head("core/math/dynamic_bvh.h")
vol_inter = extract(dbvh, r"^\t\t_FORCE_INLINE_ bool intersects\(const Volume &b\) const \{")
vol_conv = extract(dbvh, r"^\t\t_FORCE_INLINE_ bool intersects_convex\(")


def dedent(t):
    return "\n".join(line[1:] if line.startswith("\t") else line for line in t.split("\n"))


w(
    "struct RefVolume {\n\tVector3 min, max;\n\n"
    + dedent(vol_inter).replace("const Volume &", "const RefVolume &")
    + "\n\n"
    + dedent(vol_conv)
    + "\n};\n\n"
)


# ------------------------------------------------------------------ DynamicBVH: the whole original class
def copy_full_class(h_path, cpp_path, name, new_name):
    h = head(h_path)
    decl = h[h.index("class " + name + " {") :]
    cpp = head(cpp_path)
    body = cpp[cpp.index("\n", cpp.rindex("#include")) + 1 :]
    text = re.sub(r"\b" + name + r"\b", new_name, decl + "\n" + body)
    lines = text.split("\n")
    for i, line in enumerate(lines):
        top_level = line and not line[0].isspace() and not line.startswith(("inline ", "template", "//", "#", "}"))
        if top_level and (new_name + "::") in line and line.rstrip().endswith("{"):
            lines[i] = "inline " + line
    return "\n".join(lines) + "\n"


w(copy_full_class("core/math/dynamic_bvh.h", "core/math/dynamic_bvh.cpp", "DynamicBVH", "RefDynamicBVH"))

w("} // namespace SIMDReference\n")

OUTPUT = args.output
# Free functions are inline (not static) so including the header without using them does not warn.
text = re.sub(r"\bstatic (void|bool|int) ref_", r"inline \1 ref_", "".join(out))
text = text.replace(
    "// Generated by misc/scripts/generate_simd_reference.py; do not edit by hand.",
    "// Generated by misc/scripts/generate_simd_reference.py; do not edit by hand.\n"
    "// Scalar baseline revision: " + BASE,
)
try:
    text = subprocess.run(
        ["clang-format", "--assume-filename=" + os.path.join(ROOT, "tests/core/math/simd_reference.h")],
        input=text,
        text=True,
        capture_output=True,
        check=True,
    ).stdout
except (OSError, subprocess.CalledProcessError) as error:
    sys.exit("clang-format is required for reproducible reference generation: " + str(error))
if args.check:
    with open(OUTPUT, encoding="utf-8") as source:
        if source.read() != text:
            sys.exit("Scalar reference is out of date: " + OUTPUT)
    print("Scalar reference matches", BASE)
    sys.exit(0)
with open(OUTPUT, "w", encoding="utf-8", newline="\n") as destination:
    destination.write(text)
print("Generated", OUTPUT, "from", BASE)
