/**************************************************************************/
/*  camera_rig_3d.h                                                       */
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

/**
 * @file camera_rig_3d.h
 *
 * Provides target following, position smoothing & camera management.
 */

#include "core/object/object.h"
#include "scene/3d/node_3d.h"

class Camera3D;

class CameraRig3D : public Node3D {
	GDCLASS(CameraRig3D, Node3D);

	ObjectID target_id;
	ObjectID camera_id;

	Vector3 follow_offset = Vector3(0.0, 2.0, 5.0);
	bool follow_target = true;
	bool look_at_target = true;
	bool position_smoothing_enabled = true;
	real_t position_smoothing_speed = 8.0;
	bool adopt_existing_camera = true;
	bool position_initialized = false;

	Camera3D *get_managed_camera() const;
	void _setup_camera();
	void _update_process_state();

protected:
	static void _bind_methods();

public:
	void set_target(Node3D *p_target);
	Node3D *get_target() const;

	void set_follow_offset(const Vector3 &p_offset);
	Vector3 get_follow_offset() const;

	void set_follow_target(bool p_enabled);
	bool is_follow_target_enabled() const;

	void set_look_at_target(bool p_enabled);
	bool is_look_at_target_enabled() const;

	void set_position_smoothing_enabled(bool p_enabled);
	bool is_position_smoothing_enabled() const;

	void set_position_smoothing_speed(real_t p_speed);
	real_t get_position_smoothing_speed() const;

	void set_adopt_existing_camera(bool p_enabled);
	bool is_adopt_existing_camera_enabled() const;

	void _notification(int p_what);

	void _process_rig(double p_delta);
	void _ready_rig();
};
