/**************************************************************************/
/*  test_tile_map_layer.h                                                 */
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

#include "scene/2d/tile_map_layer.h"
#include "scene/resources/2d/tile_set.h"
#include "scene/resources/image_texture.h"

#include "tests/test_macros.h"

namespace TestTileMapLayer {

// Builds a tile set with a single terrain set in the given mode, holding two
// terrains, and an atlas source with one tile per reachable terrains pattern.
static Ref<TileSet> make_terrain_tile_set(TileSet::TerrainMode p_mode) {
	Ref<TileSet> tile_set;
	tile_set.instantiate();
	tile_set->set_tile_shape(TileSet::TILE_SHAPE_SQUARE);
	tile_set->set_tile_size(Vector2i(16, 16));

	tile_set->add_terrain_set();
	tile_set->set_terrain_set_mode(0, p_mode);
	tile_set->add_terrain(0);
	tile_set->add_terrain(0);

	// One tile per combination of the terrain set's valid peering bits, so the
	// terrain solver always has a tile to pick for any constraint set.
	Vector<TileSet::CellNeighbor> valid_bits;
	for (int i = 0; i < TileSet::CELL_NEIGHBOR_MAX; i++) {
		TileSet::CellNeighbor bit = TileSet::CellNeighbor(i);
		if (tile_set->is_valid_terrain_peering_bit(0, bit)) {
			valid_bits.push_back(bit);
		}
	}
	const int combinations = 1 << valid_bits.size();

	Ref<TileSetAtlasSource> source;
	source.instantiate();
	// The atlas holds one row of tiles per terrain, so it has to be wide enough
	// for every combination (up to 2^8 with corners and sides).
	Ref<Image> image = Image::create_empty(combinations * 16, 2 * 16, false, Image::FORMAT_RGBA8);
	source->set_texture(ImageTexture::create_from_image(image));
	source->set_texture_region_size(Vector2i(16, 16));

	for (int terrain = 0; terrain < 2; terrain++) {
		for (int combination = 0; combination < combinations; combination++) {
			const Vector2i coords = Vector2i(combination, terrain);
			source->create_tile(coords);
			TileData *tile_data = source->get_tile_data(coords, 0);
			ERR_CONTINUE(tile_data == nullptr);
			tile_data->set_terrain_set(0);
			tile_data->set_terrain(terrain);
			for (int i = 0; i < valid_bits.size(); i++) {
				tile_data->set_terrain_peering_bit(valid_bits[i], (combination & (1 << i)) ? terrain : (1 - terrain));
			}
		}
	}
	tile_set->add_source(source, 0);
	return tile_set;
}

TEST_CASE("[SceneTree][TileMapLayer] Erasing a cell reconsiders every cell sharing a terrain bit") {
	// GH-1292: with "Match Corners", erasing a cell left the cells sharing only a
	// side with it untouched, even though a square cell's corners are shared with
	// all eight surrounding cells, not just the four diagonal ones.
	struct ModeCase {
		TileSet::TerrainMode mode;
		const char *name;
		bool expect_diagonals; // Only side bits are shared with orthogonal neighbors.
	};
	const ModeCase cases[] = {
		{ TileSet::TERRAIN_MODE_MATCH_CORNERS_AND_SIDES, "corners and sides", true },
		{ TileSet::TERRAIN_MODE_MATCH_CORNERS, "corners", true },
		{ TileSet::TERRAIN_MODE_MATCH_SIDES, "sides", false },
	};

	for (const ModeCase &mode_case : cases) {
		SUBCASE(mode_case.name) {
			Ref<TileSet> tile_set = make_terrain_tile_set(mode_case.mode);

			TileMapLayer *layer = memnew(TileMapLayer);
			layer->set_tile_set(tile_set);
			SceneTree::get_singleton()->get_root()->add_child(layer);

			// Fill a 5x5 block with terrain 0.
			TypedArray<Vector2i> block;
			for (int y = 0; y < 5; y++) {
				for (int x = 0; x < 5; x++) {
					block.push_back(Vector2i(x, y));
				}
			}
			layer->set_cells_terrain_connect(block, 0, 0, false);

			// Erase the center cell the way the editor's eraser does: fill it with
			// an empty terrains pattern.
			Vector<Vector2i> erased;
			erased.push_back(Vector2i(2, 2));
			const HashMap<Vector2i, TileSet::TerrainsPattern> output =
					layer->terrain_fill_pattern(erased, 0, TileSet::TerrainsPattern(*tile_set, 0), false);

			CHECK(output.has(Vector2i(2, 2)));
			// Orthogonal neighbors share side bits, and in corner modes they also
			// share two of the erased cell's corners.
			CHECK(output.has(Vector2i(1, 2)));
			CHECK(output.has(Vector2i(3, 2)));
			CHECK(output.has(Vector2i(2, 1)));
			CHECK(output.has(Vector2i(2, 3)));
			if (mode_case.expect_diagonals) {
				// Diagonal neighbors share one corner each.
				CHECK(output.has(Vector2i(1, 1)));
				CHECK(output.has(Vector2i(3, 1)));
				CHECK(output.has(Vector2i(1, 3)));
				CHECK(output.has(Vector2i(3, 3)));
			}

			memdelete(layer);
		}
	}
}

} // namespace TestTileMapLayer
