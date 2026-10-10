/**************************************************************************/
/*  camera_rig_3d.cpp                                                     */
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

/**
 * @file camera_rig_3d.cpp
 *
 * Provides target following, position smoothing & camera management for CameraRig3D.
 */

#include "camera_rig_3d.h"

#include "core/math/math_funcs.h"
#include "core/templates/list.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/viewport.h"

bool CameraRig3D::_is_valid_target(Node3D *p_target) const {
	return !p_target || (p_target != this && !is_ancestor_of(p_target));
}

Camera3D *CameraRig3D::get_managed_camera() const {
	return ObjectDB::get_instance<Camera3D>(camera_id);
}

void CameraRig3D::_setup_camera() {
	if (adopt_existing_camera) {
		for (int i = 0; i < get_child_count(); i++) {
			Camera3D *child_camera = Object::cast_to<Camera3D>(get_child(i));

			if (child_camera) {
				camera_id = child_camera->get_instance_id();
				child_camera->make_current();
				return;
			}
		}

		List<Node *> pending_nodes;

		for (int i = 0; i < get_child_count(); i++) {
			pending_nodes.push_back(get_child(i));
		}

		while (!pending_nodes.is_empty()) {
			Node *node = pending_nodes.front()->get();
			pending_nodes.pop_front();

			// Don't adopt cameras from nested viewports since they belong to a different viewport
			if (Object::cast_to<Viewport>(node)) {
				continue;
			}

			Camera3D *camera = Object::cast_to<Camera3D>(node);

			if (camera) {
				camera_id = camera->get_instance_id();
				camera->make_current();
				return;
			}

			for (int i = 0; i < node->get_child_count(); i++) {
				pending_nodes.push_back(node->get_child(i));
			}
		}
	}

	// Create a camera when adoption is disabled or no direct child camera exists
	Camera3D *camera = memnew(Camera3D);
	camera->set_name("Camera3D");
	add_child(camera);
	camera_id = camera->get_instance_id();
	camera->make_current();
}

void CameraRig3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			_ready_rig();
			break;
		case NOTIFICATION_PROCESS:
			_process_rig(get_process_delta_time());
			break;
	}
}

void CameraRig3D::_update_process_state() {
	// Avoid processing every frame when no target-dependent behavior is enabled
	set_process(ObjectDB::get_instance<Node3D>(target_id) != nullptr && (follow_target || look_at_target));
}

void CameraRig3D::set_target(Node3D *p_target) {
	// A target inside the rig's hierarchy would move with the rig, feeding its
	// own movement back into the next desired position
	ERR_FAIL_COND_MSG(!_is_valid_target(p_target), "CameraRig3D can't target itself or one of its descendants.");

	target_id = p_target ? p_target->get_instance_id() : ObjectID();
	_update_process_state();
}

Node3D *CameraRig3D::get_target() const {
	return ObjectDB::get_instance<Node3D>(target_id);
}

void CameraRig3D::set_follow_offset(const Vector3 &p_offset) {
	follow_offset = p_offset;
}

Vector3 CameraRig3D::get_follow_offset() const {
	return follow_offset;
}

void CameraRig3D::set_follow_target(bool p_enabled) {
	follow_target = p_enabled;
	_update_process_state();
}

bool CameraRig3D::is_follow_target_enabled() const {
	return follow_target;
}

void CameraRig3D::set_look_at_target(bool p_enabled) {
	look_at_target = p_enabled;
	_update_process_state();
}

bool CameraRig3D::is_look_at_target_enabled() const {
	return look_at_target;
}

void CameraRig3D::set_position_smoothing_enabled(bool p_enabled) {
	position_smoothing_enabled = p_enabled;
}

bool CameraRig3D::is_position_smoothing_enabled() const {
	return position_smoothing_enabled;
}

void CameraRig3D::set_position_smoothing_speed(real_t p_speed) {
	// Keep the interpolation speed positive to avoid a stationary or divergent exponential interpolation
	position_smoothing_speed = MAX(real_t(0.01), p_speed);
}

real_t CameraRig3D::get_position_smoothing_speed() const {
	return position_smoothing_speed;
}

void CameraRig3D::set_adopt_existing_camera(bool p_enabled) {
	adopt_existing_camera = p_enabled;
}

bool CameraRig3D::is_adopt_existing_camera_enabled() const {
	return adopt_existing_camera;
}

void CameraRig3D::_ready_rig() {
	_setup_camera();

	Node3D *target = get_target();

	// The hierarchy may have changed after set_target() was called
	if (!_is_valid_target(target)) {
		target_id = ObjectID();
		target = nullptr;
		ERR_PRINT("CameraRig3D target became the rig itself or one of its descendants.");
	}

	if (target && follow_target) {
		// Initialize directly so the camera starts at the expected position
		set_global_position(target->to_global(follow_offset));
		position_initialized = true;
	}

	Camera3D *camera = get_managed_camera();

	if (camera) {
		if (target && look_at_target) {
			const Vector3 direction = target->get_global_position() - camera->get_global_position();

			if (direction.length_squared() > CMP_EPSILON) {
				Vector3 up(0.0, 1.0, 0.0);

				// look_at() can't use an up vector parallel to the viewing direction
				if (Math::abs(direction.normalized().dot(up)) > 0.999) {
					up = Vector3(0.0, 0.0, 1.0);
				}

				camera->look_at(target->get_global_position(), up);
			}
		}

		camera->make_current();
	}

	_update_process_state();
}

void CameraRig3D::_process_rig(double p_delta) {
	Node3D *target = get_target();

	if (!target) {
		_update_process_state();
		return;
	}

	// Reparenting can make a previously valid target dependent on the rig's transform,
	// so validate the relationship again while processing
	if (!_is_valid_target(target)) {
		target_id = ObjectID();
		_update_process_state();
		ERR_PRINT("CameraRig3D target became the rig itself or one of its descendants.");
		return;
	}

	Camera3D *camera = get_managed_camera();

	if (follow_target) {
		// Transform the offset through the target so it follows the target's orientation
		const Vector3 desired_position = target->to_global(follow_offset);

		if (!position_initialized || !position_smoothing_enabled) {
			set_global_position(desired_position);
			position_initialized = true;
		} else {
			// Exponential interpolation gives consistent smoothing across frame rates
			const real_t weight = 1.0 - Math::exp(-position_smoothing_speed * p_delta);
			set_global_position(get_global_position().lerp(desired_position, weight));
		}
	}

	if (look_at_target && camera) {
		const Vector3 target_position = target->get_global_position();
		const Vector3 direction = target_position - camera->get_global_position();

		if (direction.length_squared() > CMP_EPSILON) {
			Vector3 up(0.0, 1.0, 0.0);

			// Avoid a degenerate look-at basis when looking almost vertically
			if (Math::abs(direction.normalized().dot(up)) > 0.999) {
				up = Vector3(0.0, 0.0, 1.0);
			}

			camera->look_at(target_position, up);
		}
	}
}

void CameraRig3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target", "target"), &CameraRig3D::set_target);
	ClassDB::bind_method(D_METHOD("get_target"), &CameraRig3D::get_target);
	ClassDB::bind_method(D_METHOD("set_follow_offset", "offset"), &CameraRig3D::set_follow_offset);
	ClassDB::bind_method(D_METHOD("get_follow_offset"), &CameraRig3D::get_follow_offset);
	ClassDB::bind_method(D_METHOD("set_follow_target", "enabled"), &CameraRig3D::set_follow_target);
	ClassDB::bind_method(D_METHOD("is_follow_target_enabled"), &CameraRig3D::is_follow_target_enabled);
	ClassDB::bind_method(D_METHOD("set_look_at_target", "enabled"), &CameraRig3D::set_look_at_target);
	ClassDB::bind_method(D_METHOD("is_look_at_target_enabled"), &CameraRig3D::is_look_at_target_enabled);
	ClassDB::bind_method(D_METHOD("set_position_smoothing_enabled", "enabled"), &CameraRig3D::set_position_smoothing_enabled);
	ClassDB::bind_method(D_METHOD("is_position_smoothing_enabled"), &CameraRig3D::is_position_smoothing_enabled);
	ClassDB::bind_method(D_METHOD("set_position_smoothing_speed", "speed"), &CameraRig3D::set_position_smoothing_speed);
	ClassDB::bind_method(D_METHOD("get_position_smoothing_speed"), &CameraRig3D::get_position_smoothing_speed);
	ClassDB::bind_method(D_METHOD("set_adopt_existing_camera", "enabled"), &CameraRig3D::set_adopt_existing_camera);
	ClassDB::bind_method(D_METHOD("is_adopt_existing_camera_enabled"), &CameraRig3D::is_adopt_existing_camera_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "target", PROPERTY_HINT_NODE_TYPE, "Node3D"), "set_target", "get_target");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "follow_offset"), "set_follow_offset", "get_follow_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "follow_target"), "set_follow_target", "is_follow_target_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "look_at_target"), "set_look_at_target", "is_look_at_target_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "position_smoothing_enabled"), "set_position_smoothing_enabled", "is_position_smoothing_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "position_smoothing_speed", PROPERTY_HINT_RANGE, "0.01,30.0,0.01,or_greater"), "set_position_smoothing_speed", "get_position_smoothing_speed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "adopt_existing_camera"), "set_adopt_existing_camera", "is_adopt_existing_camera_enabled");
}
