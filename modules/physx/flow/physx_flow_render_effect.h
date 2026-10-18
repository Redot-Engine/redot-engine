/**************************************************************************/
/*  physx_flow_render_effect.h                                            */
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

#ifdef GODOT_PHYSX_FLOW

#include "core/templates/hash_map.h"
#include "flow_scatter_rd.h"
#include "core/templates/local_vector.h"
#include "scene/resources/compositor.h"

class Camera3D;
class PhysXFlowSimulation3D;
class World3D;
class RenderingDevice;
class RenderData;

// Draws every PhysXFlowSimulation3D into the frame with Flow's own ray
// marcher (self-shadowed, straight from Flow's sparse grid on the GPU),
// after transparent geometry.
//
// While a world has flows, the effect is made to run without touching
// anything the user owns or saves: with no Compositor on the world, one is
// created on the World3D (never on the WorldEnvironment, never saved); a
// user's Compositor (world or camera) gets the effect added at the
// RenderingServer level only -- its effects array stays as the user made
// it -- unless it already holds a PhysXFlowRenderEffect. When the world's
// last flow leaves, all of that is undone.
class PhysXFlowRenderEffect : public CompositorEffect {
public:
	FlowScatterRD &get_scatter() { return scatter; }

	GDCLASS(PhysXFlowRenderEffect, CompositorEffect);

	static LocalVector<PhysXFlowSimulation3D *> flows;

	struct WorldState {
		int flows = 0;
		Ref<Compositor> created; // installed on the World3D by us
		// User compositors the effect was added to, with their own effect
		// RIDs as last seen (a change means the user re-set them).
		HashMap<ObjectID, LocalVector<RID>> injected;
	};
	static HashMap<ObjectID, WorldState> worlds;
	static void _inject(WorldState &r_state, const Ref<Compositor> &p_compositor);
	static void _restore(const ObjectID &p_compositor);

	// Copies Flow's composited result into Godot's color buffer, which
	// RenderingDevice won't copy into directly (no copy-to usage) but a
	// compute pass can write.
	FlowScatterRD scatter; // the light scattering passes (point/spot glow)
	RID copy_shader;
	RID copy_pipeline;
	bool copy_failed = false;
	bool _ensure_copy_pipeline(RenderingDevice *p_rd);

	void _render(int p_callback_type, const RenderData *p_render_data);

protected:
	static void _bind_methods();

public:
	// Writes p_src over p_dst (both rgba16f, p_size), on the render thread.
	void copy_texture(RenderingDevice *p_rd, const RID &p_src, const RID &p_dst, const Size2i &p_size);

	static void register_flow(PhysXFlowSimulation3D *p_flow);
	static void unregister_flow(PhysXFlowSimulation3D *p_flow);

	// A world gained / lost a flow (counted); install keeps the effect
	// running there (call every step: the user may swap compositors).
	static void attach_world(World3D *p_world);
	static void detach_world(const ObjectID &p_world);
	static void install(World3D *p_world, Camera3D *p_camera);
	// Diagnostics: the Compositor a World3D currently renders with (World3D
	// doesn't expose it to scripts).
	static Ref<Compositor> get_world_compositor(const Ref<World3D> &p_world);
	// The effect every PhysXFlowSimulation3D installs (one is enough: it
	// draws them all).
	static Ref<PhysXFlowRenderEffect> get_shared();
	static void free_shared();

	PhysXFlowRenderEffect();
	~PhysXFlowRenderEffect();
};

#endif // GODOT_PHYSX_FLOW
