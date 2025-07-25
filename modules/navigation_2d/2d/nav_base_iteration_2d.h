/**************************************************************************/
/*  nav_base_iteration_2d.h                                               */
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

#include "../nav_utils_2d.h"

#include "core/object/ref_counted.h"
#include "servers/navigation/navigation_utilities.h"

class NavBaseIteration2D : public RefCounted {
	GDCLASS(NavBaseIteration2D, RefCounted);

public:
	bool enabled = true;
	uint32_t navigation_layers = 1;
	real_t enter_cost = 0.0;
	real_t travel_cost = 1.0;
	NavigationUtilities::PathSegmentType owner_type;
	ObjectID owner_object_id;
	RID owner_rid;
	bool owner_use_edge_connections = false;
	LocalVector<Nav2D::Polygon> navmesh_polygons;
	LocalVector<LocalVector<Nav2D::Connection>> internal_connections;

	bool get_enabled() const { return enabled; }
	NavigationUtilities::PathSegmentType get_type() const { return owner_type; }
	RID get_self() const { return owner_rid; }
	ObjectID get_owner_id() const { return owner_object_id; }
	uint32_t get_navigation_layers() const { return navigation_layers; }
	real_t get_enter_cost() const { return enter_cost; }
	real_t get_travel_cost() const { return travel_cost; }
	bool get_use_edge_connections() const { return owner_use_edge_connections; }
	const LocalVector<Nav2D::Polygon> &get_navmesh_polygons() const { return navmesh_polygons; }
	const LocalVector<LocalVector<Nav2D::Connection>> &get_internal_connections() const { return internal_connections; }

	virtual ~NavBaseIteration2D() {
		navmesh_polygons.clear();
		internal_connections.clear();
	}
};
