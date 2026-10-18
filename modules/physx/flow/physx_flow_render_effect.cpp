/**************************************************************************/
/*  physx_flow_render_effect.cpp                                          */
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

#include "physx_flow_render_effect.h"

#ifdef GODOT_PHYSX_FLOW

#include "flow_simulation.h"
#include "physx_flow_simulation_3d.h"

#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"
#include "scene/3d/camera_3d.h"
#include "scene/resources/3d/world_3d.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/light_storage.h"#include "servers/rendering/rendering_server.h"
#include "servers/rendering/storage/render_data.h"

LocalVector<PhysXFlowSimulation3D *> PhysXFlowRenderEffect::flows;
HashMap<ObjectID, PhysXFlowRenderEffect::WorldState> PhysXFlowRenderEffect::worlds;

namespace {

Ref<PhysXFlowRenderEffect> shared_effect;

const char *COPY_SHADER = R"(
#[compute]
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba16f) uniform restrict readonly image2D src_image;
layout(set = 0, binding = 1, rgba16f) uniform restrict writeonly image2D dst_image;

layout(push_constant, std430) uniform Params {
	ivec2 size;
	ivec2 pad;
} params;

void main() {
	ivec2 p = ivec2(gl_GlobalInvocationID.xy);
	if (p.x >= params.size.x || p.y >= params.size.y) {
		return;
	}
	imageStore(dst_image, p, imageLoad(src_image, p));
}
)";

// Godot's column-vector matrix as Flow's row-vector one: Flow's row i is
// Godot's column i, which is exactly Projection's memory layout.
void to_flow_matrix(const Projection &p_m, float r_out[16]) {
	for (int c = 0; c < 4; c++) {
		for (int r = 0; r < 4; r++) {
			r_out[c * 4 + r] = (float)p_m.columns[c][r];
		}
	}
}

} // namespace

void PhysXFlowRenderEffect::register_flow(PhysXFlowSimulation3D *p_flow) {
	if (flows.find(p_flow) < 0) {
		flows.push_back(p_flow);
	}
}

void PhysXFlowRenderEffect::unregister_flow(PhysXFlowSimulation3D *p_flow) {
	flows.erase(p_flow);
}

namespace {

// The RIDs of a compositor's own (valid) effects, in order; whether one of
// them is a PhysXFlowRenderEffect.
LocalVector<RID> own_effect_rids(const Ref<Compositor> &p_compositor, bool &r_has_flow_effect) {
	LocalVector<RID> rids;
	r_has_flow_effect = false;
	const TypedArray<CompositorEffect> effects = p_compositor->get_compositor_effects();
	for (int i = 0; i < effects.size(); i++) {
		Ref<CompositorEffect> e = effects[i];
		if (e.is_valid()) {
			rids.push_back(e->get_rid());
			r_has_flow_effect = r_has_flow_effect || Object::cast_to<PhysXFlowRenderEffect>(e.ptr()) != nullptr;
		}
	}
	return rids;
}

bool same_rids(const LocalVector<RID> &p_a, const LocalVector<RID> &p_b) {
	if (p_a.size() != p_b.size()) {
		return false;
	}
	for (uint32_t i = 0; i < p_a.size(); i++) {
		if (p_a[i] != p_b[i]) {
			return false;
		}
	}
	return true;
}

void push_effects(RID p_compositor, const LocalVector<RID> &p_rids, RID p_extra) {
	TypedArray<RID> rids;
	for (const RID &r : p_rids) {
		rids.push_back(r);
	}
	if (p_extra.is_valid()) {
		rids.push_back(p_extra);
	}
	RenderingServer::get_singleton()->compositor_set_compositor_effects(p_compositor, rids);
}

} // namespace

void PhysXFlowRenderEffect::_inject(WorldState &r_state, const Ref<Compositor> &p_compositor) {
	if (p_compositor.is_null() || p_compositor == r_state.created) {
		return;
	}
	bool has_flow_effect = false;
	const LocalVector<RID> own = own_effect_rids(p_compositor, has_flow_effect);
	if (has_flow_effect) {
		return; // the user added it themselves
	}
	const ObjectID id = p_compositor->get_instance_id();
	LocalVector<RID> *seen = r_state.injected.getptr(id);
	if (seen != nullptr && same_rids(*seen, own)) {
		return; // already in, and the user hasn't re-set the effects since
	}
	push_effects(p_compositor->get_rid(), own, get_shared()->get_rid());
	r_state.injected.insert(id, own);
}

void PhysXFlowRenderEffect::_restore(const ObjectID &p_compositor) {
	Compositor *compositor = Object::cast_to<Compositor>(ObjectDB::get_instance(p_compositor));
	if (compositor == nullptr) {
		return;
	}
	// Exactly what the user's resource says, i.e. without ours.
	bool has_flow_effect = false;
	push_effects(compositor->get_rid(), own_effect_rids(Ref<Compositor>(compositor), has_flow_effect), RID());
}

void PhysXFlowRenderEffect::attach_world(World3D *p_world) {
	ERR_FAIL_NULL(p_world);
	WorldState &state = worlds[p_world->get_instance_id()];
	state.flows++;
}

void PhysXFlowRenderEffect::detach_world(const ObjectID &p_world) {
	WorldState *state = worlds.getptr(p_world);
	if (state == nullptr || --state->flows > 0) {
		return;
	}
	for (const KeyValue<ObjectID, LocalVector<RID>> &kv : state->injected) {
		_restore(kv.key);
	}
	World3D *world = Object::cast_to<World3D>(ObjectDB::get_instance(p_world));
	if (world != nullptr && state->created.is_valid() && world->get_compositor() == state->created) {
		world->set_compositor(Ref<Compositor>());
	}
	worlds.erase(p_world);
}

void PhysXFlowRenderEffect::install(World3D *p_world, Camera3D *p_camera) {
	ERR_FAIL_NULL(p_world);
	WorldState *state = worlds.getptr(p_world->get_instance_id());
	ERR_FAIL_NULL(state);
	Ref<Compositor> world_compositor = p_world->get_compositor();
	if (world_compositor.is_null()) {
		if (state->created.is_null()) {
			state->created.instantiate();
			TypedArray<CompositorEffect> effects;
			effects.push_back(get_shared());
			state->created->set_compositor_effects(effects);
		}
		p_world->set_compositor(state->created);
	} else {
		_inject(*state, world_compositor);
	}
	// A camera's own compositor replaces the world's for that camera.
	if (p_camera != nullptr) {
		_inject(*state, p_camera->get_compositor());
	}
}

Ref<Compositor> PhysXFlowRenderEffect::get_world_compositor(const Ref<World3D> &p_world) {
	return p_world.is_valid() ? p_world->get_compositor() : Ref<Compositor>();
}

void PhysXFlowRenderEffect::_bind_methods() {
	ClassDB::bind_static_method("PhysXFlowRenderEffect", D_METHOD("get_world_compositor", "world"), &PhysXFlowRenderEffect::get_world_compositor);
}

Ref<PhysXFlowRenderEffect> PhysXFlowRenderEffect::get_shared() {
	if (shared_effect.is_null()) {
		shared_effect.instantiate();
	}
	return shared_effect;
}

void PhysXFlowRenderEffect::free_shared() {
	// Godot's Compositor holds Refs to its effects (a TypedArray next to the
	// RID list), and WorldState.created holds the Compositor we made. Releasing
	// only the shared Ref here would leave that cycle alive: the static worlds
	// map outlives the whole shutdown, so the Compositor and CompositorEffect
	// RIDs would still be alive at RenderingServer::finish (reported as exit
	// leaks) and this object's destructor would run afterwards, too late to
	// free its RenderingDevice copies. Tear the world state down ourselves,
	// while every server can still free what it owns -- quit paths that never
	// delivered EXIT_WORLD (script mode) end here too.
	for (const KeyValue<ObjectID, WorldState> &kv : worlds) {
		for (const KeyValue<ObjectID, LocalVector<RID>> &inj : kv.value.injected) {
			_restore(inj.key);
		}
		World3D *world = Object::cast_to<World3D>(ObjectDB::get_instance(kv.key));
		if (world != nullptr && kv.value.created.is_valid() && world->get_compositor() == kv.value.created) {
			world->set_compositor(Ref<Compositor>());
		}
	}
	worlds.clear();
	// Raw node pointers; nothing dereferences them past this point.
	flows.clear();
	// Last: drops the last Ref (the compositors above are gone), so the
	// destructor's RenderingDevice frees happen against a live device.
	shared_effect.unref();
}

PhysXFlowRenderEffect::PhysXFlowRenderEffect() {
	set_effect_callback_type(CompositorEffect::EFFECT_CALLBACK_TYPE_POST_TRANSPARENT);
	set_access_resolved_color(true);
	set_access_resolved_depth(true);
	// Straight to C++, instead of the script-facing _render_callback.
	RenderingServer::get_singleton()->compositor_effect_set_callback(get_rid(), RS::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_TRANSPARENT,
			callable_mp(this, &PhysXFlowRenderEffect::_render));
}

PhysXFlowRenderEffect::~PhysXFlowRenderEffect() {
	RenderingDevice *rd = RenderingServer::get_singleton() != nullptr ? RenderingServer::get_singleton()->get_rendering_device() : nullptr;
	if (rd != nullptr) {
		if (copy_pipeline.is_valid()) {
			rd->free(copy_pipeline);
		}
		if (copy_shader.is_valid()) {
			rd->free(copy_shader);
		}
		scatter.free(rd);
	}
}

bool PhysXFlowRenderEffect::_ensure_copy_pipeline(RenderingDevice *p_rd) {
	if (copy_pipeline.is_valid()) {
		return true;
	}
	if (copy_failed) {
		return false;
	}
	Ref<RDShaderFile> file;
	file.instantiate();
	if (file->parse_versions_from_text(String(COPY_SHADER)) != OK) {
		copy_failed = true;
		ERR_FAIL_V_MSG(false, "PhysXFlowRenderEffect: copy shader failed to compile.");
	}
	copy_shader = p_rd->shader_create_from_spirv(file->get_spirv_stages());
	if (copy_shader.is_valid()) {
		copy_pipeline = p_rd->compute_pipeline_create(copy_shader);
	}
	copy_failed = !copy_pipeline.is_valid();
	return !copy_failed;
}

void PhysXFlowRenderEffect::copy_texture(RenderingDevice *p_rd, const RID &p_src, const RID &p_dst, const Size2i &p_size) {
	if (!_ensure_copy_pipeline(p_rd)) {
		return;
	}
	RD::Uniform src(RD::UNIFORM_TYPE_IMAGE, 0, p_src);
	RD::Uniform dst(RD::UNIFORM_TYPE_IMAGE, 1, p_dst);
	LocalVector<RD::Uniform> uniforms;
	uniforms.push_back(src);
	uniforms.push_back(dst);
	const RID set = p_rd->uniform_set_create(uniforms, copy_shader, 0);
	ERR_FAIL_COND(!set.is_valid());
	const int32_t push[4] = { p_size.x, p_size.y, 0, 0 };
	const RD::ComputeListID list = p_rd->compute_list_begin();
	p_rd->compute_list_bind_compute_pipeline(list, copy_pipeline);
	p_rd->compute_list_bind_uniform_set(list, set, 0);
	p_rd->compute_list_set_push_constant(list, push, sizeof(push));
	p_rd->compute_list_dispatch(list, (p_size.x + 7) / 8, (p_size.y + 7) / 8, 1);
	p_rd->compute_list_end();
	p_rd->free(set);
}

void PhysXFlowRenderEffect::_render(int p_callback_type, const RenderData *p_render_data) {
	if (flows.is_empty() || p_render_data == nullptr) {
		return;
	}
	RenderSceneBuffersRD *buffers = Object::cast_to<RenderSceneBuffersRD>(p_render_data->get_render_scene_buffers().ptr());
	RenderSceneDataRD *scene = Object::cast_to<RenderSceneDataRD>(p_render_data->get_render_scene_data());
	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
	if (buffers == nullptr || scene == nullptr || rd == nullptr) {
		return;
	}
	const Size2i size = buffers->get_internal_size();
	const RID color = buffers->get_internal_texture();
	const RID depth = buffers->get_depth_texture();
	if (size.x <= 0 || size.y <= 0 || !color.is_valid() || !depth.is_valid()) {
		return;
	}

	// The projection the depth buffer was rendered with: Godot's camera
	// projection already carries its depth correction (reverse Z in 0..1, Y
	// as this target needs) and TAA jitter, so Flow reconstructs scene
	// positions from depth exactly.
	Projection projection = scene->get_cam_projection();
	// Flow reads its targets with row 0 at clip-space +Y; Godot's are the
	// other way up (checked on screen: without this the plume is upside
	// down).
	Projection y_flip;
	y_flip.columns[1][1] = -1.0;
	projection = y_flip * projection;
	// A grid's world is the node's world: one Godot unit is one Flow unit,
	// so the view is the camera's world->view transform directly.
	const Projection view = Projection(scene->get_cam_transform().affine_inverse());
	float view_f[16];
	float projection_f[16];
	to_flow_matrix(view, view_f);
	to_flow_matrix(projection, projection_f);

	// Each flow composites over what's drawn so far, so far ones go first.
	const Vector3 eye = scene->get_cam_transform().origin;
	LocalVector<PhysXFlowSimulation3D *> sorted;
	LocalVector<real_t> distance;
	for (PhysXFlowSimulation3D *flow : flows) {
		sorted.push_back(flow);
		distance.push_back(eye.distance_squared_to(flow->get_draw_center()));
	}
	for (uint32_t i = 1; i < sorted.size(); i++) {
		for (uint32_t j = i; j > 0 && distance[j - 1] < distance[j]; j--) {
			SWAP(distance[j - 1], distance[j]);
			SWAP(sorted[j - 1], sorted[j]);
		}
	}
	for (PhysXFlowSimulation3D *flow : sorted) {
		// For the light scattering pass: clip space (as the depth buffer has it)
		// straight back to world meters, plus the frame's lights and shadow
		// atlas for the scattering pass's shadows.
		const Projection inv_view_projection = (projection * view).inverse();
		const RenderDataRD *render_data_rd = dynamic_cast<const RenderDataRD *>(p_render_data);
		flow->render_rd(this, rd, view_f, projection_f, size, depth, color, inv_view_projection, eye,
				render_data_rd != nullptr ? render_data_rd->lights : nullptr,
				render_data_rd != nullptr ? render_data_rd->shadow_atlas : RID());
	}
}

#endif // GODOT_PHYSX_FLOW
