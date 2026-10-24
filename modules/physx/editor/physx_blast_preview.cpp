/**************************************************************************/
/*  physx_blast_preview.cpp                                               */
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

//////////////////////////////////////////////////////////////////////////
/*  physx_blast_preview.cpp                                             */
//////////////////////////////////////////////////////////////////////////
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/* Copyright (c) 2024-present Redot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */

#include "physx_blast_preview.h"

#ifdef GODOT_PHYSX_BLAST

#include "../blast/physx_blast_asset.h"

#include "scene/resources/material.h"
#include "scene/resources/mesh.h"

static Color _blast_preview_chunk_color(int p_index) {
	// Golden-ratio hue step: successive chunks get visually distinct colors
	// with no lookup table and no risk of two adjacent chunks landing on the
	// same hue.
	const double hue = Math::fmod(p_index * 0.6180339887, 1.0);
	return Color::from_hsv(hue, 0.55, 0.85);
}

Ref<ArrayMesh> physx_blast_build_preview_mesh(const Ref<PhysXBlastAsset> &p_asset) {
	Ref<ArrayMesh> preview_mesh;
	preview_mesh.instantiate();
	if (p_asset.is_null()) {
		return preview_mesh;
	}

	const Array points = p_asset->get_chunk_points();
	for (int i = 1; i < points.size(); i++) {
		const PackedVector3Array tri = points[i];
		if (tri.size() < 3) {
			continue;
		}

		PackedVector3Array normals;
		normals.resize(tri.size());
		for (int t = 0; t + 2 < tri.size(); t += 3) {
			// Same winding fix PhysXDestructible3D uses for this same
			// triangle-soup data -- (c-a).cross(b-a), not the more intuitive
			// (b-a).cross(c-a) -- see that node's normal computation.
			const Vector3 n = (tri[t + 2] - tri[t]).cross(tri[t + 1] - tri[t]).normalized();
			normals.set(t, n);
			normals.set(t + 1, n);
			normals.set(t + 2, n);
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = tri;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		preview_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_albedo(_blast_preview_chunk_color(i));
		preview_mesh->surface_set_material(preview_mesh->get_surface_count() - 1, mat);
	}

	return preview_mesh;
}

#endif // GODOT_PHYSX_BLAST
