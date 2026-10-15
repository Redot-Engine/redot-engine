/**************************************************************************/
/*  physx_rid_owner.h                                                     */
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
 * @file physx_rid_owner.h
 * @brief RID ownership mixin for PhysX-side objects.
 *
 * The RID is assigned exactly once at registration time; the DEV_ASSERT in
 * set_rid() fails a second assignment, which would orphan the previous entry
 * in the RID owner table.
 */

#ifndef PHYSX_RID_OWNER_H
#define PHYSX_RID_OWNER_H

#include "core/templates/rid.h"
#include <core/error/error_macros.h>

class PhysXRIDOwner {
	RID rid;

public:
	PhysXRIDOwner() = default;

	void set_rid(const RID &p_rid) {
		DEV_ASSERT(!rid.is_valid());
		rid = p_rid;
	}

	_FORCE_INLINE_ const RID &get_rid() const { return rid; }

	_FORCE_INLINE_ bool is_valid() const { return rid.is_valid(); }
};
#endif // PHYSX_RID_OWNER_H
