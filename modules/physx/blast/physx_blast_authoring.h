/**************************************************************************/
/*  physx_blast_authoring.h                                               */
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

#include "physx_blast_asset.h"

#include "core/object/ref_counted.h"

class Mesh;
class Texture2D;

// In-engine mesh fracturing. Ports the proven-correct authoring call
// sequence (NvBlastExtAuthoringCreateFractureTool -> VoronoiSitesGenerator ->
// voronoiFracturing -> NvBlastExtAuthoringProcessFracture) into real module
// code, using the module's own PxPhysics (PhysXServer3D) to cook collision
// hulls.
//
// A RefCounted rather than a set of static functions so it's directly
// callable from GDScript (ClassDB.instantiate("PhysXBlastAuthoring")) for a
// headless test today, and so a future editor dock has an obvious object to
// hold onto without inventing a second calling convention.
class PhysXBlastAuthoring : public RefCounted {
	GDCLASS(PhysXBlastAuthoring, RefCounted);

public:
	// NvBlastExtAuthoring's FractureTool supports more than one fracture
	// algorithm -- PATTERN_VORONOI (organic/rock-like chunks, the original
	// only option), PATTERN_SLICING (regular, brick-like pieces from cutting
	// planes -- much better suited to man-made/architectural shapes like
	// crates or walls than Voronoi's random cells), and PATTERN_CUTOUT (a 2D
	// pattern extruded through the mesh -- glass-pane spiderweb cracks, brick
	// walls, anything with a real authored break-up shape). Unlike the other
	// two, NvBlast doesn't generate the Cutout pattern itself -- same as
	// Unreal's own Blast integration (BlastFractureSettingsCutout::Pattern,
	// a plain UTexture2D artists import), it's a grayscale image the caller
	// supplies via p_cutout_pattern; PATTERN_CUTOUT with no pattern set fails
	// the same way Unreal's does with no Pattern texture assigned.
	//
	// Pattern authoring rule, found the hard way: every crack line must
	// reach the image boundary. A pattern whose lines stop short (e.g. a
	// spiderweb with cracks that fade out before the edge) leaves one
	// single connected "background" region wrapping the whole pattern with
	// a hole in the middle -- unlike a real broken-glass pane, where every
	// shard is a closed polygon. That topology crashed two different ways
	// deep in the SDK's own Cutout code (FractureToolImpl::cutout's
	// SweepingAccelerator construction), not this module's integration --
	// confirmed by testing the exact same pattern with lines extended to
	// the edge, which fractures and fully verifies clean. A straight grid/
	// brick pattern is naturally edge-to-edge already and was never
	// affected; only curved/radial patterns are at risk of this if authored
	// with cracks that stop short.
	enum FracturePattern {
		PATTERN_VORONOI,
		PATTERN_SLICING,
		PATTERN_CUTOUT,
	};

protected:
	static void _bind_methods();

public:
	// Fractures p_mesh's surface 0 into roughly p_site_count pieces (plus the
	// implicit root chunk = the whole unfractured mesh, chunk 0 -- same
	// convention PhysXDestructible3D assumes),
	// using p_pattern's algorithm. For PATTERN_SLICING, p_site_count is only
	// approximate -- it's converted to a roughly-cube-root split across the
	// 3 slicing axes, since slicing works in slice-counts-per-axis, not a
	// single site count. p_site_count is ignored entirely for PATTERN_CUTOUT
	// -- piece count there comes from however many distinct regions
	// p_cutout_pattern actually contains, and p_cutout_pattern is required
	// (a null Ref fails, same as PATTERN_VORONOI/PATTERN_SLICING would fail
	// with a null p_mesh). Returns a null Ref on failure (mesh has no
	// surfaces, cooking failed, etc).
	Ref<PhysXBlastAsset> fracture_mesh(const Ref<Mesh> &p_mesh, int p_site_count, int p_seed, FracturePattern p_pattern = PATTERN_VORONOI, const Ref<Texture2D> &p_cutout_pattern = Ref<Texture2D>());
};

VARIANT_ENUM_CAST(PhysXBlastAuthoring::FracturePattern);
