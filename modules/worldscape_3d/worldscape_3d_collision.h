/**************************************************************************/
/*  worldscape_3d_collision.h                                             */
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

// Terrain3D Godot plugin: Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#pragma once

#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/physics_material.h"

#include <vector>

#include "constants.h"
#include "worldscape_3d_util.h"

class WorldScape3D;

class WorldScape3DCollision : public Object {
	GDCLASS(WorldScape3DCollision, Object);
	CLASS_NAME();

public: // Constants
	enum CollisionMode {
		DISABLED,
		DYNAMIC_GAME,
		DYNAMIC_EDITOR,
		FULL_GAME,
		FULL_EDITOR,
	};

private:
	WorldScape3D *_terrain = nullptr;

	// Public settings
	CollisionMode _mode = DYNAMIC_GAME;
	uint16_t _shape_size = 16;
	uint16_t _radius = 64;
	uint32_t _layer = 1;
	uint32_t _mask = 1;
	real_t _priority = 1.f;
	Ref<PhysicsMaterial> _physics_material;

	// Work data
	RID _static_body_rid; // Physics Server Static Body
	StaticBody3D *_static_body = nullptr; // Editor mode StaticBody3D
	std::vector<CollisionShape3D *> _shapes; // All CollisionShape3Ds

	bool _initialized = false;
	Vector2i _last_snapped_pos = V2I_MAX;

	Vector2i _snap_to_grid(const Vector2i &p_pos) const;
	Vector2i _snap_to_grid(const Vector3 &p_pos) const;
	Dictionary _get_shape_data(const Vector2i &p_position, const int p_size);

	void _shape_set_disabled(const int p_shape_id, const bool p_disabled);
	void _shape_set_transform(const int p_shape_id, const Transform3D &p_xform);
	Vector3 _shape_get_position(const int p_shape_id) const;
	void _shape_set_data(const int p_shape_id, const Dictionary &p_dict);

	void _reload_physics_material();

public:
	WorldScape3DCollision() {}
	~WorldScape3DCollision() { destroy(); }
	void initialize(WorldScape3D *p_terrain);

	void build();
	//void reset_target_position() { _last_snapped_pos = V2I_MAX; }
	void update(const bool p_rebuild = false);
	void destroy();

	void set_mode(const CollisionMode p_mode);
	CollisionMode get_mode() const { return _mode; }
	bool is_enabled() const { return _mode > DISABLED; }
	bool is_editor_mode() const { return _mode == DYNAMIC_EDITOR || _mode == FULL_EDITOR; }
	bool is_dynamic_mode() const { return _mode == DYNAMIC_GAME || _mode == DYNAMIC_EDITOR; }

	void set_shape_size(const uint16_t p_size);
	uint16_t get_shape_size() const { return _shape_size; }
	void set_radius(const uint16_t p_radius);
	uint16_t get_radius() const { return _radius; }
	void set_layer(const uint32_t p_layers);
	uint32_t get_layer() const { return _layer; }
	void set_mask(const uint32_t p_mask);
	uint32_t get_mask() const { return _mask; }
	void set_priority(const real_t p_priority);
	real_t get_priority() const { return _priority; }
	void set_physics_material(const Ref<PhysicsMaterial> &p_mat);
	Ref<PhysicsMaterial> get_physics_material() { return _physics_material; }
	RID get_rid() const;

protected:
	static void _bind_methods();
};

typedef WorldScape3DCollision::CollisionMode CollisionMode;
VARIANT_ENUM_CAST(WorldScape3DCollision::CollisionMode);

inline Vector2i WorldScape3DCollision::_snap_to_grid(const Vector2i &p_pos) const {
	return Vector2i(int_round_mult(p_pos.x, int32_t(_shape_size)),
			int_round_mult(p_pos.y, int32_t(_shape_size)));
}

inline Vector2i WorldScape3DCollision::_snap_to_grid(const Vector3 &p_pos) const {
	return Vector2i(Math::floor(p_pos.x / real_t(_shape_size) + 0.5f),
				   Math::floor(p_pos.z / real_t(_shape_size) + 0.5f)) *
			_shape_size;
}
