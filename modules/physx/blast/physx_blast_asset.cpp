/**************************************************************************/
/*  physx_blast_asset.cpp                                                 */
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
/*  physx_blast_asset.cpp                                               */
//////////////////////////////////////////////////////////////////////////
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/* Copyright (c) 2024-present Redot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */

#include "physx_blast_asset.h"

#include "core/object/class_db.h"

void PhysXBlastAsset::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_asset_bytes", "bytes"), &PhysXBlastAsset::set_asset_bytes);
	ClassDB::bind_method(D_METHOD("get_asset_bytes"), &PhysXBlastAsset::get_asset_bytes);
	ClassDB::bind_method(D_METHOD("set_chunk_points", "points"), &PhysXBlastAsset::set_chunk_points);
	ClassDB::bind_method(D_METHOD("get_chunk_points"), &PhysXBlastAsset::get_chunk_points);
	ClassDB::bind_method(D_METHOD("get_chunk_count"), &PhysXBlastAsset::get_chunk_count);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "asset_bytes"), "set_asset_bytes", "get_asset_bytes");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "chunk_points"), "set_chunk_points", "get_chunk_points");
}
