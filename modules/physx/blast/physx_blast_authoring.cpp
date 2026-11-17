/**************************************************************************/
/*  physx_blast_authoring.cpp                                             */
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
/*  physx_blast_authoring.cpp                                           */
//////////////////////////////////////////////////////////////////////////
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/* Copyright (c) 2024-present Redot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */

#include "physx_blast_authoring.h"

#include "../physx_server.h"
#include "physx_blast_runtime.h"

#include "core/io/image.h"
#include "core/object/class_db.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"

#include <NvBlast.h>
#include <NvBlastExtAuthoring.h>
#include <NvBlastExtAuthoringBondGenerator.h>
#include <NvBlastExtAuthoringConvexMeshBuilder.h>
#include <NvBlastExtAuthoringCutout.h>
#include <NvBlastExtAuthoringFractureTool.h>
#include <NvBlastExtAuthoringMesh.h>
#include <NvBlastExtAuthoringMeshCleaner.h>
#include <NvBlastExtAuthoringTypes.h>
#include <NvBlastTypes.h>
#include <PxPhysicsAPI.h>

#include <cstdlib>
#include <vector>

// Deliberately NOT `using namespace Nv::Blast;` -- Nv::Blast::Mesh collides
// with Godot's own ::Mesh (scene/resources/mesh.h), which this file also
// uses for the p_mesh parameter. Every Blast SDK type below is qualified
// explicitly instead.
using namespace physx;

namespace {

void blast_authoring_log(int32_t p_type, const char *p_msg, const char *p_file, int32_t p_line) {
	if (p_type <= 1) {
		ERR_PRINT(vformat("Blast authoring: %s (%s:%d)", p_msg, p_file, p_line));
	}
}

// ConvexMeshBuilder ships as a pure interface in this SDK (no built-in
// implementation), so every caller has to provide one. The contract: cook a
// real hull so ProcessFracture doesn't choke on a degenerate one, but the
// result is never actually consumed downstream -- PhysXDestructible3D cooks
// its own PhysX convex hulls from each chunk's render-mesh points, not from
// Blast's authored CollisionHull data.
class GodotConvexMeshBuilder : public Nv::Blast::ConvexMeshBuilder {
public:
	explicit GodotConvexMeshBuilder(PxPhysics *p_physics) :
			physics(p_physics) {}

	void release() override { delete this; }

	Nv::Blast::CollisionHull *buildCollisionGeometry(uint32_t p_count, const NvcVec3 *p_verts) override {
		std::vector<PxVec3> pts(p_count);
		for (uint32_t i = 0; i < p_count; i++) {
			pts[i] = PxVec3(p_verts[i].x, p_verts[i].y, p_verts[i].z);
		}
		PxConvexMeshDesc desc;
		desc.points.count = p_count;
		desc.points.stride = sizeof(PxVec3);
		desc.points.data = pts.data();
		desc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
		PxCookingParams cook_params(physics->getTolerancesScale());
		PxConvexMesh *mesh = PxCreateConvexMesh(cook_params, desc, *PxGetStandaloneInsertionCallback());

		Nv::Blast::CollisionHull *hull = new Nv::Blast::CollisionHull();
		memset(hull, 0, sizeof(Nv::Blast::CollisionHull));
		if (!mesh) {
			return hull; // empty hull -- fine, unused downstream
		}

		const PxU32 nb_verts = mesh->getNbVertices();
		const PxVec3 *src_verts = mesh->getVertices();
		hull->pointsCount = nb_verts;
		hull->points = new NvcVec3[nb_verts];
		for (PxU32 i = 0; i < nb_verts; i++) {
			hull->points[i] = { src_verts[i].x, src_verts[i].y, src_verts[i].z };
		}

		const PxU32 nb_polys = mesh->getNbPolygons();
		hull->polygonDataCount = nb_polys;
		hull->polygonData = new Nv::Blast::HullPolygon[nb_polys];
		std::vector<PxU8> all_indices;
		for (PxU32 i = 0; i < nb_polys; i++) {
			PxHullPolygon pd;
			mesh->getPolygonData(i, pd);
			hull->polygonData[i].plane[0] = pd.mPlane[0];
			hull->polygonData[i].plane[1] = pd.mPlane[1];
			hull->polygonData[i].plane[2] = pd.mPlane[2];
			hull->polygonData[i].plane[3] = pd.mPlane[3];
			hull->polygonData[i].vertexCount = pd.mNbVerts;
			hull->polygonData[i].indexBase = (uint16_t)all_indices.size();
			const PxU8 *ib = mesh->getIndexBuffer();
			for (PxU16 j = 0; j < pd.mNbVerts; j++) {
				all_indices.push_back(ib[pd.mIndexBase + j]);
			}
		}
		hull->indicesCount = (uint32_t)all_indices.size();
		hull->indices = new uint32_t[all_indices.size()];
		for (size_t i = 0; i < all_indices.size(); i++) {
			hull->indices[i] = all_indices[i];
		}

		mesh->release();
		return hull;
	}

	void releaseCollisionHull(Nv::Blast::CollisionHull *hull) const override {
		if (!hull) {
			return;
		}
		delete[] hull->points;
		delete[] hull->indices;
		delete[] hull->polygonData;
		delete hull;
	}

private:
	PxPhysics *physics;
};

class GodotRandomGenerator : public Nv::Blast::RandomGeneratorBase {
public:
	void seed(int32_t p_seed) override { std::srand((unsigned)p_seed); }
	float getRandomValue() override { return (float)std::rand() / (float)RAND_MAX; }
};

// Pre-flight check for PATTERN_CUTOUT patterns, added after two different
// real crashes deep inside NvBlastExtAuthoringFractureToolImpl::cutout()
// (SweepingAccelerator construction) -- both traced to the same cause: a
// crack/wall shape that doesn't reach the pattern's edge leaves the fill
// region wrapping around it with a hole in the middle (unlike a real broken
// pane, where every shard is a closed polygon and every crack reaches a
// boundary). Confirmed by fixing exactly that -- extending the crack lines
// to the image edge -- and nothing else, which made an otherwise-identical
// pattern fracture cleanly.
//
// Rule enforced here, directly from that finding: every connected wall
// (crack-line) shape must touch the image boundary somewhere. A wall
// component with zero boundary-touching pixels is a "floating island" --
// the exact topology that crashed. Uses the same weighted-RGB threshold
// NvBlastExtAuthoringCutoutImpl.cpp's own createCutoutSet() applies (see
// PhysXBlastAuthoring::fracture_mesh()'s own comment on why it's 3 bytes/
// pixel, not the 1-byte/pixel the public header claims) so this reads the
// pattern exactly the way the SDK itself will.
bool cutout_pattern_has_floating_wall_island(const PackedByteArray &p_gray, uint32_t p_w, uint32_t p_h) {
	const int64_t grid_vertex_count = (int64_t)p_w * (int64_t)p_h;
	if (grid_vertex_count == 0) {
		return false;
	}

	LocalVector<bool> is_wall;
	is_wall.resize((uint32_t)grid_vertex_count);
	{
		const uint8_t *g = p_gray.ptr();
		for (int64_t i = 0; i < grid_vertex_count; i++) {
			const uint32_t pix = 16777216u * (uint32_t)g[i]; // (5033165+9898557+1845494) * gray, R=G=B
			is_wall[(uint32_t)i] = (pix >> 28) != 0;
		}
	}

	LocalVector<int32_t> visited; // -1 = unvisited wall pixel, else already assigned to a checked component
	visited.resize((uint32_t)grid_vertex_count);
	for (uint32_t i = 0; i < (uint32_t)grid_vertex_count; i++) {
		visited[i] = -1;
	}

	LocalVector<uint32_t> stack;
	for (uint32_t y = 0; y < p_h; y++) {
		for (uint32_t x = 0; x < p_w; x++) {
			const uint32_t start = y * p_w + x;
			if (!is_wall[start] || visited[start] != -1) {
				continue;
			}

			bool touches_border = false;
			stack.clear();
			stack.push_back(start);
			visited[start] = 1;
			while (!stack.is_empty()) {
				const uint32_t cur = stack[stack.size() - 1];
				stack.remove_at(stack.size() - 1);
				const uint32_t cx = cur % p_w;
				const uint32_t cy = cur / p_w;
				if (cx == 0 || cx == p_w - 1 || cy == 0 || cy == p_h - 1) {
					touches_border = true;
				}
				if (cx > 0) {
					const uint32_t nb = cur - 1;
					if (is_wall[nb] && visited[nb] == -1) {
						visited[nb] = 1;
						stack.push_back(nb);
					}
				}
				if (cx + 1 < p_w) {
					const uint32_t nb = cur + 1;
					if (is_wall[nb] && visited[nb] == -1) {
						visited[nb] = 1;
						stack.push_back(nb);
					}
				}
				if (cy > 0) {
					const uint32_t nb = cur - p_w;
					if (is_wall[nb] && visited[nb] == -1) {
						visited[nb] = 1;
						stack.push_back(nb);
					}
				}
				if (cy + 1 < p_h) {
					const uint32_t nb = cur + p_w;
					if (is_wall[nb] && visited[nb] == -1) {
						visited[nb] = 1;
						stack.push_back(nb);
					}
				}
			}

			if (!touches_border) {
				return true; // a floating island -- reject the whole pattern
			}
		}
	}
	return false;
}

} //namespace

// Printed as before, and kept for get_last_error()/get_last_warnings()
// (the fracture dialog shows them).
Ref<PhysXBlastAsset> PhysXBlastAuthoring::_fail(const String &p_message) {
	_error(p_message);
	return Ref<PhysXBlastAsset>();
}

void PhysXBlastAuthoring::_error(const String &p_message) {
	ERR_PRINT(p_message);
	last_error = p_message.trim_prefix("PhysXBlastAuthoring: ");
}

void PhysXBlastAuthoring::_warn(const String &p_message) {
	WARN_PRINT(p_message);
	last_warnings.push_back(p_message.trim_prefix("PhysXBlastAuthoring: "));
}

void PhysXBlastAuthoring::_bind_methods() {
	ClassDB::bind_method(D_METHOD("fracture_mesh", "mesh", "site_count", "seed", "pattern", "cutout_pattern"), &PhysXBlastAuthoring::fracture_mesh, DEFVAL(PATTERN_VORONOI), DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_method(D_METHOD("get_last_error"), &PhysXBlastAuthoring::get_last_error);
	ClassDB::bind_method(D_METHOD("get_last_warnings"), &PhysXBlastAuthoring::get_last_warnings);

	BIND_ENUM_CONSTANT(PATTERN_VORONOI);
	BIND_ENUM_CONSTANT(PATTERN_SLICING);
	BIND_ENUM_CONSTANT(PATTERN_CUTOUT);
}

Ref<PhysXBlastAsset> PhysXBlastAuthoring::fracture_mesh(const Ref<Mesh> &p_mesh, int p_site_count, int p_seed, FracturePattern p_pattern, const Ref<Texture2D> &p_cutout_pattern) {
	if (!blast_runtime_load()) {
		return _fail("PhysXBlastAuthoring: the Blast runtime libraries are not installed; install the PhysX runtime bundle first.");
	}
	last_error = String();
	last_warnings.clear();
	if (p_mesh.is_null() || p_mesh->get_surface_count() < 1) {
		return _fail("PhysXBlastAuthoring: mesh is null or has no surfaces.");
	}
	PxPhysics *physics = PhysXServer3D::get_singleton() ? PhysXServer3D::get_singleton()->try_get_physics() : nullptr;
	if (!physics) {
		return _fail("PhysXBlastAuthoring: PhysX not initialized.");
	}
	if (p_mesh->surface_get_primitive_type(0) != Mesh::PRIMITIVE_TRIANGLES) {
		return _fail("PhysXBlastAuthoring: the mesh's first surface isn't a triangle list.");
	}

	const Array arrays = p_mesh->surface_get_arrays(0);
	const PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
	if (verts.size() < 4) {
		return _fail("PhysXBlastAuthoring: mesh has too few vertices.");
	}
	PackedVector3Array src_normals = arrays[Mesh::ARRAY_NORMAL];
	PackedInt32Array src_indices = arrays[Mesh::ARRAY_INDEX];

	LocalVector<NvcVec3> positions;
	LocalVector<NvcVec3> normals;
	LocalVector<NvcVec2> uvs;
	positions.resize(verts.size());
	normals.resize(verts.size());
	uvs.resize(verts.size());
	for (int i = 0; i < verts.size(); i++) {
		positions[i] = { verts[i].x, verts[i].y, verts[i].z };
		// A flat placeholder if the mesh has none -- Blast's Voronoi split
		// doesn't need correct normals, and nothing downstream ever reads
		// them back (see GodotConvexMeshBuilder's own comment).
		const Vector3 vertex_normal = (i < src_normals.size()) ? src_normals[i] : Vector3(0, 1, 0);
		normals[i] = { vertex_normal.x, vertex_normal.y, vertex_normal.z };
		uvs[i] = { 0.0f, 0.0f };
	}

	LocalVector<uint32_t> indices;
	if (src_indices.size() >= 3) {
		indices.resize(src_indices.size());
		for (int i = 0; i < src_indices.size(); i++) {
			indices[i] = (uint32_t)src_indices[i];
		}
	} else {
		// Non-indexed mesh -- vertices are already one triangle list.
		indices.resize(verts.size());
		for (uint32_t i = 0; i < (uint32_t)verts.size(); i++) {
			indices[i] = i;
		}
	}

	// Blast trusts its input: an index past the vertices reads out of
	// bounds, a partial triangle crashes, a NaN vertex or a mesh collapsed
	// to a point spins the mesh cleaner forever.
	if (indices.size() < 3 || indices.size() % 3 != 0) {
		return _fail(vformat("PhysXBlastAuthoring: %d indices isn't a whole number of triangles.", (int)indices.size()));
	}
	for (uint32_t i = 0; i < indices.size(); i++) {
		if (indices[i] >= (uint32_t)verts.size()) {
			return _fail(vformat("PhysXBlastAuthoring: index %d points past the mesh's %d vertices.", (int)indices[i], verts.size()));
		}
	}
	AABB bounds(verts[0], Vector3());
	for (int i = 0; i < verts.size(); i++) {
		if (!verts[i].is_finite()) {
			return _fail(vformat("PhysXBlastAuthoring: vertex %d isn't a finite position.", i));
		}
		bounds.expand_to(verts[i]);
	}
	const real_t thinnest = bounds.size[bounds.get_shortest_axis_index()];
	if (thinnest <= MAX(bounds.get_longest_axis_size(), (real_t)1.0) * (real_t)1e-5) {
		return _fail(vformat("PhysXBlastAuthoring: the mesh has no volume to fracture (its bounds are %s).", bounds.size));
	}

	// Inside out (its triangles face inward: a negative volume): Blast keeps
	// the winding it's given, so every piece's outer faces would face in and
	// read as culled/hollow. Flip the winding instead.
	real_t signed_volume = 0.0;
	for (uint32_t t = 0; t + 2 < indices.size(); t += 3) {
		const Vector3 &a = verts[indices[t]];
		const Vector3 &b = verts[indices[t + 1]];
		const Vector3 &c = verts[indices[t + 2]];
		signed_volume += a.dot(c.cross(b));
	}
	if (signed_volume < 0.0) {
		for (uint32_t t = 0; t + 2 < indices.size(); t += 3) {
			SWAP(indices[t + 1], indices[t + 2]);
		}
		_warn("PhysXBlastAuthoring: the mesh was inside out (its triangles faced inward); fractured with its triangles flipped.");
	}

	Nv::Blast::Mesh *raw_mesh = NvBlastExtAuthoringCreateMesh(positions.ptr(), normals.ptr(), uvs.ptr(),
			(uint32_t)positions.size(), indices.ptr(), (uint32_t)indices.size());
	if (!raw_mesh) {
		return _fail("PhysXBlastAuthoring: NvBlastExtAuthoringCreateMesh failed.");
	}

	// FractureTool requires a closed (watertight), self-intersection-free,
	// open-edge-free mesh (NvBlastExtAuthoringMeshCleaner.h) -- a Godot
	// PrimitiveMesh like BoxMesh doesn't weld vertices across face
	// boundaries (24 verts for 6 faces, not 8 shared corners), which reads
	// as "open edges" to Blast. Voronoi/Slicing tolerated that in practice;
	// Cutout's boolean CSG did not (crashed/produced a 1-chunk degenerate
	// result with "Not equal number of starting and ending vertices" until
	// this was added) -- clean unconditionally rather than special-case it
	// per pattern, since a strictly-valid mesh can only help the others too.
	Nv::Blast::MeshCleaner *mesh_cleaner = NvBlastExtAuthoringCreateMeshCleaner();
	Nv::Blast::Mesh *cleaned_mesh = mesh_cleaner->cleanMesh(raw_mesh);
	if (!cleaned_mesh) {
		_warn("PhysXBlastAuthoring: the mesh cleaner failed; fracturing the mesh as it is.");
	} else {
		// The cleaner's result is checked against the source: empty or
		// reaching outside the source's bounds (it broke the shape) falls
		// back to the raw mesh; smaller than the source (it dropped pieces
		// that weren't part of a closed solid, a stray triangle) is kept.
		const NvcBounds3 &cb = cleaned_mesh->getBoundingBox();
		const AABB cleaned_bounds(Vector3(cb.minimum.x, cb.minimum.y, cb.minimum.z),
				Vector3(cb.maximum.x - cb.minimum.x, cb.maximum.y - cb.minimum.y, cb.maximum.z - cb.minimum.z));
		const real_t tolerance = bounds.get_longest_axis_size() * (real_t)0.01;
		if (cleaned_mesh->getFacetCount() == 0 || !bounds.grow(tolerance).encloses(cleaned_bounds)) {
			_warn(vformat("PhysXBlastAuthoring: the mesh cleaner broke the mesh (bounds %s, was %s); fracturing the mesh as it is.", cleaned_bounds, bounds));
			cleaned_mesh->release();
			cleaned_mesh = nullptr;
		} else if (!cleaned_bounds.grow(tolerance).encloses(bounds)) {
			_warn(vformat("PhysXBlastAuthoring: the mesh cleaner dropped parts of the mesh that weren't a closed solid (bounds %s, was %s).", cleaned_bounds, bounds));
		}
	}
	Nv::Blast::Mesh *blast_mesh = cleaned_mesh ? cleaned_mesh : raw_mesh;

	Nv::Blast::FractureTool *fTool = NvBlastExtAuthoringCreateFractureTool();
	const Nv::Blast::Mesh *src_meshes[1] = { blast_mesh };
	fTool->setSourceMeshes(src_meshes, 1);

	GodotRandomGenerator rng;
	rng.seed(p_seed);

	Ref<PhysXBlastAsset> result;
	Nv::Blast::VoronoiSitesGenerator *sites_gen = nullptr;
	int32_t frac_result;

	if (p_pattern == PATTERN_SLICING) {
		// slicing() works in slice-counts-per-axis, not a single site count --
		// approximate p_site_count with a roughly-cube-root split so the
		// property still reads as "about this many pieces" regardless of
		// which pattern is picked. A small amount of offset/angle variation
		// by default so the cuts don't look like a perfectly uniform grid.
		int per_axis = 1;
		const int target = MAX(p_site_count, 1);
		while ((per_axis + 1) * (per_axis + 1) * (per_axis + 1) <= target) {
			per_axis++;
		}
		Nv::Blast::SlicingConfiguration conf;
		conf.x_slices = per_axis;
		conf.y_slices = per_axis;
		conf.z_slices = per_axis;
		conf.offset_variations = 0.2f;
		conf.angle_variations = 0.2f;
		frac_result = fTool->slicing(0, conf, false, &rng);
		if (frac_result != 0) {
			_error(vformat("PhysXBlastAuthoring: slicing failed, code=%d.", frac_result));
		}
	} else if (p_pattern == PATTERN_CUTOUT) {
		// Unlike Voronoi/Slicing, NvBlast doesn't generate this pattern itself
		// -- same as Unreal's own Blast integration, the caller supplies a
		// grayscale bitmap (see this class's header for why) and
		// NvBlastExtAuthoringBuildCutoutSet segments it into per-region loops.
		if (p_cutout_pattern.is_null()) {
			_error("PhysXBlastAuthoring: PATTERN_CUTOUT requires a cutout_pattern texture.");
			frac_result = -1;
		} else {
			Ref<Image> pattern_img = p_cutout_pattern->get_image();
			if (pattern_img.is_null() || pattern_img->is_empty()) {
				_error("PhysXBlastAuthoring: cutout_pattern texture has no image data.");
				frac_result = -1;
			} else {
				if (pattern_img->is_compressed()) {
					pattern_img->decompress();
				}
				// NvBlastExtAuthoringCutout.h's own doc comment claims "each
				// pixel is represented by one byte," but this SDK build's
				// actual createCutoutSet() (NvBlastExtAuthoringCutoutImpl.cpp)
				// reads 3 bytes/pixel -- a weighted RGB luminance, thresholded
				// near-binary (roughly: value below ~1/16 of max is interior/
				// fill, anything brighter is a wall/crack line). Confirmed by
				// reading that source directly after the stale 1-byte-per-
				// pixel assumption crashed with an out-of-bounds read -- it
				// walks 3 bytes per pixel regardless of what the header says.
				// Matches Unreal's own Blast integration, which independently
				// packs the same 3-bytes-per-pixel format for the same call.
				pattern_img->convert(Image::FORMAT_L8);
				const PackedByteArray gray = pattern_img->get_data();
				const uint32_t pw = (uint32_t)pattern_img->get_width();
				const uint32_t ph = (uint32_t)pattern_img->get_height();
				PackedByteArray pixels;
				pixels.resize((int64_t)pw * ph * 3);
				{
					uint8_t *dst = pixels.ptrw();
					const uint8_t *src = gray.ptr();
					const int64_t expanded_vertex_count = (int64_t)pw * ph;
					for (int64_t i = 0; i < expanded_vertex_count; i++) {
						dst[i * 3 + 0] = src[i];
						dst[i * 3 + 1] = src[i];
						dst[i * 3 + 2] = src[i];
					}
				}

				// Refuse rather than risk the crash a floating (edge-
				// unreached) crack island caused twice while building this
				// pattern -- see cutout_pattern_has_floating_wall_island()'s
				// own comment for the full story. A native SDK crash can't
				// be caught from here (it takes the whole process down), so
				// this has to be ruled out before ever calling cutout().
				if (cutout_pattern_has_floating_wall_island(gray, pw, ph)) {
					_error("PhysXBlastAuthoring: cutout_pattern rejected -- it has a crack/wall shape that "
						   "doesn't reach the image edge (a 'floating island'). Every crack line needs to "
						   "reach the pattern's boundary, the same way a real broken pane's cracks do -- "
						   "otherwise the fill region wraps around it with a hole in the middle, which is "
						   "known to crash the underlying Blast SDK's Cutout code rather than fail cleanly.");
					frac_result = -1;
				} else {
					Nv::Blast::CutoutSet *cutout_set = NvBlastExtAuthoringCreateCutoutSet();
					NvBlastExtAuthoringBuildCutoutSet(*cutout_set, pixels.ptr(), pw, ph,
							/*segmentationErrorThreshold*/ 1e-3f, /*snapThreshold*/ 1.0f,
							/*periodic*/ false, /*expandGaps*/ false);

					Nv::Blast::CutoutConfiguration conf;
					conf.cutoutSet = cutout_set;
					// Defaults otherwise: scale (-1,-1) auto-fits the pattern
					// to the chunk's own AABB, isRelativeTransform=true
					// centers it on the chunk -- exactly what a "just works"
					// first pass wants without per-mesh sizing math of our own.
					frac_result = fTool->cutout(0, conf, false, &rng);
					if (frac_result != 0) {
						_error(vformat("PhysXBlastAuthoring: cutout failed, code=%d.", frac_result));
					}
					cutout_set->release();
				}
			}
		}
	} else {
		sites_gen = NvBlastExtAuthoringCreateVoronoiSitesGenerator(blast_mesh, &rng);
		sites_gen->uniformlyGenerateSitesInMesh((uint32_t)MAX(p_site_count, 1));
		const NvcVec3 *sites = nullptr;
		const uint32_t site_count = sites_gen->getVoronoiSites(sites);
		frac_result = fTool->voronoiFracturing(0, site_count, sites, false);
		if (frac_result != 0) {
			_error(vformat("PhysXBlastAuthoring: voronoiFracturing failed, code=%d.", frac_result));
		}
	}

	if (frac_result == 0) {
		fTool->finalizeFracturing();

		GodotConvexMeshBuilder collision_builder(physics);
		Nv::Blast::BlastBondGenerator *bond_gen = NvBlastExtAuthoringCreateBondGenerator(&collision_builder);
		Nv::Blast::ConvexDecompositionParams cparams;
		// Voronoi cells and slicing planes of a convex source stay convex,
		// but a cutout region can easily be concave (an L-shaped brick, a
		// crack loop that isn't itself convex) -- allow real decomposition
		// there instead of silently producing a wrong/degenerate hull.
		cparams.maximumNumberOfHulls = (p_pattern == PATTERN_CUTOUT) ? 8 : 1;

		Nv::Blast::AuthoringResult *ares = NvBlastExtAuthoringProcessFracture(*fTool, *bond_gen, collision_builder, cparams);
		if (!ares) {
			_error("PhysXBlastAuthoring: NvBlastExtAuthoringProcessFracture failed.");
		} else {
			const uint32_t asset_size = NvBlastAssetGetSize(ares->asset, blast_authoring_log);
			PackedByteArray asset_bytes;
			asset_bytes.resize(asset_size);
			memcpy(asset_bytes.ptrw(), ares->asset, asset_size);

			Array chunk_points;
			chunk_points.resize(ares->chunkCount);
			for (uint32_t c = 0; c < ares->chunkCount; c++) {
				const uint32_t begin = ares->geometryOffset[c];
				const uint32_t end = ares->geometryOffset[c + 1];
				PackedVector3Array points;
				points.resize((end - begin) * 3);
				for (uint32_t t = begin; t < end; t++) {
					const Nv::Blast::Triangle &tri = ares->geometry[t];
					points.write[(t - begin) * 3 + 0] = Vector3(tri.a.p.x, tri.a.p.y, tri.a.p.z);
					points.write[(t - begin) * 3 + 1] = Vector3(tri.b.p.x, tri.b.p.y, tri.b.p.z);
					points.write[(t - begin) * 3 + 2] = Vector3(tri.c.p.x, tri.c.p.y, tri.c.p.z);
				}
				chunk_points[c] = points;
			}

			result.instantiate();
			result->set_asset_bytes(asset_bytes);
			result->set_chunk_points(chunk_points);

			NvBlastExtAuthoringReleaseAuthoringResult(collision_builder, ares);
		}
		bond_gen->release();
	}

	if (sites_gen) {
		sites_gen->release();
	}
	fTool->release();
	if (cleaned_mesh) {
		cleaned_mesh->release();
	}
	raw_mesh->release();
	mesh_cleaner->release();
	return result;
}
