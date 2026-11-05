/**************************************************************************/
/*  physx_flow_blast_bridge_3d.h                                          */
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

#if defined(GODOT_PHYSX_FLOW) && defined(GODOT_PHYSX_BLAST)

#include "scene/3d/node_3d.h"

class PhysXFlowSimulation3D;
class PhysXDestructible3D;

// Destruction-event adapter: Blast fracture -> Flow dust/smoke/fire. The
// bridge owns NOTHING simulated -- it translates PhysXDestructible3D's
// `fractured` signal into transient emitters on the referenced
// PhysXFlowSimulation3D (see its add_transient_emitter). Flow never learns
// what Blast is; Blast never learns what Flow is; either system works
// without the other (this class only exists when BOTH are compiled in).
class PhysXFlowBlastBridge3D : public Node3D {
	GDCLASS(PhysXFlowBlastBridge3D, Node3D);

	bool enabled = true;
	NodePath destructible_path; // PhysXDestructible3D to listen to
	NodePath simulation_path; // PhysXFlowSimulation3D to inject into

	// Authored effect shape: the burst radius scales off the damage radius
	// (`radius_scale * max_radius`, clamped) and the channel amounts below
	// are the values injected at event time (decaying over lifetime).
	float radius_scale = 1.0f;
	float max_radius = 2.0f;
	float smoke = 1.2f;
	float temperature = 0.0f; // > 0 with fuel > 0 turns dust into fire
	float fuel = 0.0f;
	float velocity = 1.2f; // upward puff speed
	float lifetime = 1.5f;

	bool connected = false;
	ObjectID connected_id; // the destructible the signal is actually wired to
	void _disconnect_destructible();

	void _on_fractured(const Vector3 &p_world_position, int p_pieces, float p_damage);
	PhysXFlowSimulation3D *_resolve_simulation() const;
	PhysXDestructible3D *_resolve_destructible() const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	PackedStringArray get_configuration_warnings() const override;

	void set_enabled(bool p_enabled) { enabled = p_enabled; }
	bool get_enabled() const { return enabled; }
	void set_destructible_path(const NodePath &p_path);
	NodePath get_destructible_path() const { return destructible_path; }
	void set_simulation_path(const NodePath &p_path);
	NodePath get_simulation_path() const { return simulation_path; }
	void set_radius_scale(float p_scale) { radius_scale = MAX(p_scale, 0.01f); }
	float get_radius_scale() const { return radius_scale; }
	void set_max_radius(float p_radius) { max_radius = MAX(p_radius, 0.05f); }
	float get_max_radius() const { return max_radius; }
	void set_smoke(float p_smoke) { smoke = MAX(p_smoke, 0.0f); }
	float get_smoke() const { return smoke; }
	void set_temperature(float p_temperature) { temperature = MAX(p_temperature, 0.0f); }
	float get_temperature() const { return temperature; }
	void set_fuel(float p_fuel) { fuel = MAX(p_fuel, 0.0f); }
	float get_fuel() const { return fuel; }
	void set_velocity(float p_velocity) { velocity = p_velocity; }
	float get_velocity() const { return velocity; }
	void set_lifetime(float p_lifetime) { lifetime = MAX(p_lifetime, 0.05f); }
	float get_lifetime() const { return lifetime; }

	PhysXFlowBlastBridge3D() {}
};

#endif // GODOT_PHYSX_FLOW && GODOT_PHYSX_BLAST
