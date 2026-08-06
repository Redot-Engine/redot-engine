/**************************************************************************/
/*  worldscape_3d_editor.cpp                                              */
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

#include "worldscape_3d_editor.h"

#include "core/config/engine.h"
#include "core/os/time.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"

#include "../constants.h"
#include "../logger.h"
#include "scene/3d/navigation/navigation_region_3d.h"
#include "scene/gui/separator.h"
#include "scene/gui/subviewport_container.h"
#include "scene/main/viewport.h"
#include "scene/resources/image_texture.h"
#include "scene/theme/theme_db.h"

#include "../worldscape_3d.h"
#include "../worldscape_3d_data.h"
#include "../worldscape_3d_util.h"
#include "ui/worldscape_3d_asset_dock.h"
#include "ui/worldscape_3d_operations.h"
#include "ui/worldscape_3d_ui.h"

///////////////////////////
// Private Functions
///////////////////////////

// Sends the whole region aabb to edited_area
void WorldScape3DEditor::_send_region_aabb(const Vector2i &p_region_loc, const Vector2 &p_height_range) {
	WorldScape3D::RegionSize region_size = _terrain->get_region_size();
	AABB edited_area;
	edited_area.position = Vector3(p_region_loc.x * region_size, p_height_range.x, p_region_loc.y * region_size);
	edited_area.size = Vector3(region_size, p_height_range.y - p_height_range.x, region_size);
	edited_area.position *= _terrain->get_vertex_spacing();
	edited_area.size *= _terrain->get_vertex_spacing();
	_terrain->get_data()->add_edited_area(edited_area);
}

// Process location to add new region, mark as deleted, or just retrieve
Ref<WorldScape3DRegion> WorldScape3DEditor::_operate_region(const Vector2i &p_region_loc) {
	bool changed = false;
	Vector2 height_range;
	WorldScape3DData *data = _terrain->get_data();

	// Check if in bounds, limiting errors
	bool can_print = false;
	uint64_t ticks = Time::get_singleton()->get_ticks_msec();
	if (ticks - _last_region_bounds_error > 1000) {
		_last_region_bounds_error = ticks;
		can_print = true;
	}
	if (data->get_region_map_index(p_region_loc) < 0) {
		if (can_print) {
			print_line_rich("Location ", p_region_loc, " out of bounds. Max: ",
					-WorldScape3DData::REGION_MAP_SIZE / 2, " to ", WorldScape3DData::REGION_MAP_SIZE / 2 - 1);
		}
		return Ref<WorldScape3DRegion>();
	}

	// Get Region & dump data if debug
	Ref<WorldScape3DRegion> region = data->get_region(p_region_loc);
	if (can_print) {
		LOG(DEBUG, "Tool: ", _tool, " Op: ", _operation, " processing region ", p_region_loc, ": ", ptr_to_str(*region));
	}

	// Create new region if location is null or deleted
	if (region.is_null() || (region.is_valid() && region->is_deleted())) {
		// And tool is Add Region, or Height + auto_regions
		if ((_tool == REGION && _operation == ADD) || ((_tool == SCULPT || _tool == HEIGHT) && _brush_data["auto_regions"])) {
			LOG(DEBUG, "Adding blank region at: ", p_region_loc, ", ptr: ", ptr_to_str(*region));
			region = data->add_region_blank(p_region_loc);
			if (region.is_null()) {
				print_error("A new region cannot be created");
				return region;
			}
			_edited_regions.push_back(region); // Ensure new region is added to the redo set
			changed = true;
		}
	}

	// If removing region
	else if (region.is_valid() && _tool == REGION && _operation == SUBTRACT) {
		LOG(DEBUG, "Removing region at: ", p_region_loc, ", ptr: ", ptr_to_str(*region));
		_original_regions.push_back(region);
		height_range = region->get_height_range();
		_terrain->get_data()->remove_region(region);
		changed = true;
	}

	if (changed) {
		_added_removed_locations.push_back(p_region_loc);
		region->set_modified(true);
		_send_region_aabb(p_region_loc, height_range);
	}
	return region;
}

void WorldScape3DEditor::_operate_map(const Vector3 &p_global_position, const real_t p_camera_direction) {
	LOG(EXTREME, "Operating at ", p_global_position, " tool type ", _tool, " op ", _operation);

	MapType map_type = _get_map_type();
	if (map_type == TYPE_MAX) {
		print_error("Invalid tool selected");
		return;
	}

	int region_size = _terrain->get_region_size();
	Vector2i region_vsize = Vector2i(region_size, region_size);

	// If no region and can't add one, skip whole function. Checked again later
	WorldScape3DData *data = _terrain->get_data();
	if (!data->has_regionp(p_global_position) && (!_brush_data["auto_regions"] || (_tool != SCULPT && _tool != HEIGHT))) {
		return;
	}

	bool modifier_alt = _brush_data["modifier_alt"];
	bool modifier_ctrl = _brush_data["modifier_ctrl"];
	bool modifier_shift = _brush_data["modifier_shift"];

	Image *brush_image = cast_to<Image>(_brush_data["brush_image"]);
	if (!brush_image) {
		LOG(ERROR, "Invalid brush image. Returning");
		return;
	}
	Vector2i img_size = _brush_data["brush_image_size"];
	real_t brush_size = CLAMP(real_t(_brush_data.get("size", 10.f)), 2.f, 4096.f); // Meters

	// Typically we multiply mouse pressure & strength setting, but
	// * Mouse movement w/ button down has a pressure of 1
	// * Mouse clicks always have pressure of 0
	// * Pen movement pressure varies, sometimes lifting or clicking has a pressure of 0
	// If we're operating with a pressure of 0.001-.999 it's a pen
	// So if there's a 0 pressure operation >100ms after a pen operation, we assume it's
	// a mouse click. This occasionally catches a pen click, but avoids most pen lifts.
	real_t mouse_pressure = CLAMP(real_t(_brush_data.get("mouse_pressure", 0.f)), 0.f, 1.f);
	if (mouse_pressure > CMP_EPSILON && mouse_pressure < 1.f) {
		_last_pen_tick = Time::get_singleton()->get_ticks_msec();
	}
	uint64_t ticks = Time::get_singleton()->get_ticks_msec();
	if (mouse_pressure < CMP_EPSILON && ticks - _last_pen_tick >= 100) {
		mouse_pressure = 1.f;
	}
	real_t strength = mouse_pressure * static_cast<real_t>(_brush_data["strength"]);

	real_t height = _brush_data["height"];
	Color color = _brush_data["color"];
	real_t roughness = _brush_data["roughness"];

	bool enable_texture = _brush_data["enable_texture"];
	bool texture_filter = _brush_data["texture_filter"];
	int margin = _brush_data["margin"];
	int asset_id = _brush_data["asset_id"];

	Vector2 slope_range = _brush_data["slope"];
	bool enable_angle = _brush_data["enable_angle"];
	bool dynamic_angle = _brush_data["dynamic_angle"];
	real_t angle = _brush_data["angle"];

	bool enable_scale = _brush_data["enable_scale"];
	real_t scale = _brush_data["scale"];

	real_t gamma = _brush_data["gamma"];
	PackedVector3Array gradient_points = _brush_data["gradient_points"];

	real_t randf = VariantUtilityFunctions::randf();
	real_t rot = randf * std::numbers::pi_v<real_t> * real_t{ _brush_data["jitter"] };
	if (_brush_data["align_to_view"]) {
		rot += p_camera_direction;
	}
	// Rotate the decal to align with the brush
	if (IS_EDITOR && _terrain->get_plugin()) {
		cast_to<WorldScape3DEditorPlugin>(_terrain->get_plugin())->get_ui()->set_decal_rotation(rot);
	}
	AABB edited_area;
	edited_area.position = p_global_position - Vector3(brush_size, 0.f, brush_size) * .5f;
	edited_area.size = Vector3(brush_size, 0.f, brush_size);

	if (_tool == INSTANCER) {
		if (modifier_ctrl) {
			_terrain->get_instancer()->remove_instances(p_global_position, _brush_data);
		} else {
			_terrain->get_instancer()->add_instances(p_global_position, _brush_data);
		}
		return;
	}

	// MAP Operations
	real_t vertex_spacing = _terrain->get_vertex_spacing();

	// save region count before brush pixel loop. Any regions added will have caused an Array
	// rebuild at the end of the last _operate() call, but until painting is finished we only
	// need to track if _added_removed_locations has changed between now and the end of the loop
	int regions_added_removed = _added_removed_locations.size();

	for (real_t x = 0.f; x < brush_size; x += vertex_spacing) {
		for (real_t y = 0.f; y < brush_size; y += vertex_spacing) {
			Vector2 brush_offset = Vector2(x, y) - (Vector2(brush_size, brush_size) / 2.f);
			Vector3 brush_global_position =
					Vector3(p_global_position.x + brush_offset.x + .5f, p_global_position.y,
							p_global_position.z + brush_offset.y + .5f);

			// Get region for current brush pixel global position
			Vector2i region_loc = data->get_region_location(brush_global_position);
			Ref<WorldScape3DRegion> region = _operate_region(region_loc);
			// If no region and can't make one, skip
			if (region.is_null()) {
				continue;
			}

			// Get map for this region and tool
			Image *map = region->get_map_ptr(map_type);
			if (!map) {
				continue;
			}

			// Identify position on map image
			Vector2 uv_position = _get_uv_position(brush_global_position, region_size, vertex_spacing);
			Vector2i map_pixel_position = Vector2i(uv_position * region_size);
			if (!_is_in_bounds(map_pixel_position, region_vsize)) {
				continue;
			}

			Vector2 brush_uv = Vector2(x, y) / brush_size;
			Vector2i brush_pixel_position = Vector2i(_get_rotated_uv(brush_uv, rot) * img_size);
			if (!_is_in_bounds(brush_pixel_position, img_size)) {
				continue;
			}

			Vector3 edited_position = brush_global_position;
			edited_position.y = data->get_height(edited_position);
			edited_area = edited_area.expand(edited_position);

			// Start brushing on the map
			real_t brush_alpha = brush_image->get_pixelv(brush_pixel_position).r;
			brush_alpha = real_t(Math::pow(double(brush_alpha), double(gamma)));
			brush_alpha = std::isnan(brush_alpha) || std::isnan(brush_alpha) ? 0.f : CLAMP(brush_alpha, 0.f, 1.f);
			Color src = map->get_pixelv(map_pixel_position);
			Color dest = src;

			if (map_type == TYPE_HEIGHT) {
				real_t srcf = src.r;
				// In case data in existing map has nan or inf saved, check, and reset to real number if required.
				srcf = std::isnan(srcf) || std::isnan(srcf) ? 0.f : srcf;
				real_t destf = srcf;

				switch (_operation) {
					case ADD: {
						if (_tool == HEIGHT) {
							// Height
							destf = Math::lerp(srcf, height, CLAMP(brush_alpha * strength, 0.f, 1.f));
						} else if (modifier_alt && !std::isnan(p_global_position.y)) {
							// Lift troughs
							real_t brush_center_y = p_global_position.y + brush_alpha * strength;
							destf = Math::clamp(brush_center_y, srcf, srcf + brush_alpha * strength);
						} else {
							// Raise
							destf = srcf + (brush_alpha * strength);
						}
						break;
					}
					case SUBTRACT: {
						if (_tool == HEIGHT) {
							// Height, but GDScript has already picked height at cursor
							destf = Math::lerp(srcf, height, CLAMP(brush_alpha * strength, 0.f, 1.f));
						} else if (modifier_alt && !std::isnan(p_global_position.y)) {
							// Flatten peaks
							real_t brush_center_y = p_global_position.y - brush_alpha * strength;
							destf = Math::clamp(brush_center_y, srcf - brush_alpha * strength, srcf);
						} else {
							// Lower
							destf = srcf - (brush_alpha * strength);
						}
						break;
					}
					case AVERAGE: {
						Vector3 left_position = brush_global_position - Vector3(vertex_spacing, 0.f, 0.f);
						Vector3 right_position = brush_global_position + Vector3(vertex_spacing, 0.f, 0.f);
						Vector3 down_position = brush_global_position - Vector3(0.f, 0.f, vertex_spacing);
						Vector3 up_position = brush_global_position + Vector3(0.f, 0.f, vertex_spacing);
						real_t bg_srcf_zero = _terrain->get_material()->get_world_background() == 0u ? srcf : 0.0;
						real_t left = data->get_pixel(map_type, left_position).r;
						if (std::isnan(left)) {
							left = bg_srcf_zero;
						}
						real_t right = data->get_pixel(map_type, right_position).r;
						if (std::isnan(right)) {
							right = bg_srcf_zero;
						}
						real_t up = data->get_pixel(map_type, up_position).r;
						if (std::isnan(up)) {
							up = bg_srcf_zero;
						}
						real_t down = data->get_pixel(map_type, down_position).r;
						if (std::isnan(down)) {
							down = bg_srcf_zero;
						}
						real_t avg = (srcf + left + right + up + down) * 0.2f;
						destf = Math::lerp(srcf, avg, CLAMP(brush_alpha * strength * 2.f, .02f, 1.f));
						break;
					}
					case GRADIENT: {
						if (gradient_points.size() == 2) {
							Vector3 point_1 = gradient_points[0];
							Vector3 point_2 = gradient_points[1];

							Vector2 point_1_xz = Vector2(point_1.x, point_1.z);
							Vector2 point_2_xz = Vector2(point_2.x, point_2.z);
							Vector2 brush_xz = Vector2(brush_global_position.x, brush_global_position.z);

							if (_operation_movement.length_squared() > 0.f) {
								// Ramp up/down only in the direction of movement, to avoid giving winding
								// paths one edge higher than the other.
								Vector2 movement_xz = Vector2(_operation_movement.x, _operation_movement.z).normalized();
								Vector2 offset = movement_xz * Vector2(brush_offset).dot(movement_xz);
								brush_xz = Vector2(p_global_position.x + offset.x, p_global_position.z + offset.y);
							}

							Vector2 dir = point_2_xz - point_1_xz;
							real_t weight = dir.normalized().dot(brush_xz - point_1_xz) / dir.length();
							weight = Math::clamp(weight, (real_t)0.0f, (real_t)1.0f);
							real_t height = Math::lerp(point_1.y, point_2.y, weight);
							destf = Math::lerp(srcf, height, CLAMP(brush_alpha * strength, 0.f, 1.f));
						}
						break;
					}
					default:
						break;
				}
				dest = Color(destf, 0.f, 0.f, 1.f);
				region->update_height(destf);
				data->update_master_height(destf);
				edited_position.y = destf;
				edited_area = edited_area.expand(edited_position);

			} else if (map_type == TYPE_CONTROL) {
				// Get current bit field from pixel
				uint32_t base_id = get_base(src.r);
				uint32_t overlay_id = get_overlay(src.r);
				real_t blend = real_t(get_blend(src.r)) / 255.f;
				uint32_t uvrotation = get_uv_rotation(src.r);
				uint32_t uvscale = get_uv_scale(src.r);
				bool hole = is_hole(src.r);
				bool navigation = is_nav(src.r);
				bool autoshader = is_auto(src.r);
				// Lookup to shift values saved to control map so that 0 (default) is the first entry
				// Shader scale array is aligned to match this.
				std::array<uint32_t, 8> scale_align = { 5, 6, 7, 0, 1, 2, 3, 4 };

				switch (_tool) {
					case TEXTURE: {
						if (!data->is_in_slope(brush_global_position, slope_range, modifier_alt)) {
							continue;
						}
						switch (_operation) {
							// Base Paint
							case REPLACE: {
								if (brush_alpha > 0.5f) {
									if (enable_texture) {
										// Set base & overlay texture
										base_id = asset_id;
										overlay_id = asset_id;
										// Erase blend value
										blend = 0.f;
										autoshader = false;
									}
									// Set angle & scale
									if (base_id == asset_id && enable_angle && !autoshader) {
										if (dynamic_angle) {
											// Angle from mouse movement.
											angle = Vector2(-_operation_movement.x, _operation_movement.z).angle();
											// Avoid negative, align texture "up" with mouse direction.
											angle = real_t(Math::fmod(Math::rad_to_deg(angle) + 450.f, real_t(360.f)));
										}
										// Convert from degrees to 0 - 15 value range
										uvrotation = uint32_t(CLAMP(Math::round(angle / 22.5f), 0.f, 15.f));
									}
									if (base_id == asset_id && enable_scale && !autoshader) {
										// Offset negative and convert from percentage to 0 - 7 bit value range
										// Maintain 0 = 0, remap negatives to end.
										uvscale = scale_align[uint8_t(CLAMP(Math::round((scale + 60.f) / 20.f), 0.f, 7.f))];
									}
								}
								break;
							}

							// Add asset id, and increase weighting
							case ADD: {
								real_t spray_strength = CLAMP(strength * 0.05f, 0.004f, .25f);
								real_t brush_value = CLAMP(brush_alpha * spray_strength, 0.f, 1.f);
								if (enable_texture && brush_alpha * strength * 11.f > 0.1f) {
									// Pick lowest weighted id, and lower to zero before setting new asset id.
									if (asset_id != base_id && asset_id != overlay_id) {
										if (blend >= 0.5f) {
											blend = CLAMP(blend + brush_value, 0.f, 1.f);
										} else {
											blend = CLAMP(blend - brush_value, 0.f, 1.f);
										}
										if (blend <= 1.0f / 254.f) {
											overlay_id = asset_id;
										} else if (blend >= (1.f - 1.0f / 254.f)) {
											base_id = asset_id;
										}
									}

									if (base_id == asset_id) {
										blend = CLAMP(blend - brush_value, 0.f, 1.f);
										if (blend < 0.5f) {
											autoshader = false;
										}
									}
									if (overlay_id == asset_id) {
										blend = CLAMP(blend + brush_value, 0.f, 1.f);
										if (blend >= 0.5f) {
											autoshader = false;
										}
									}
								}

								if ((base_id == asset_id && blend < 0.5f) || (overlay_id == asset_id && blend >= 0.5f)) {
									// Set angle & scale
									if (enable_angle && !autoshader && brush_alpha > 0.5f) {
										if (dynamic_angle) {
											// Angle from mouse movement.
											angle = Vector2(-_operation_movement.x, _operation_movement.z).angle();
											// Avoid negative, align texture "up" with mouse direction.
											angle = real_t(Math::fmod(Math::rad_to_deg(angle) + 450.f, real_t(360.f)));
										}
										// Convert from degrees to 0 - 15 value range
										uvrotation = uint32_t(CLAMP(Math::round(angle / 22.5f), 0.f, 15.f));
									}
									if (enable_scale && !autoshader && brush_alpha > 0.5f) {
										// Offset negative and convert from percentage to 0 - 7 bit value range
										// Maintain 0 = 0, remap negatives to end.
										uvscale = scale_align[uint8_t(CLAMP(Math::round((scale + 60.f) / 20.f), 0.f, 7.f))];
									}
								}
								break;
							}

							// Lower weight of current asset id
							case SUBTRACT: {
								real_t spray_strength = CLAMP(strength * 0.05f, 0.004f, .25f);
								real_t brush_value = CLAMP(brush_alpha * spray_strength, 0.f, 1.f);
								if (base_id == asset_id) {
									blend = CLAMP(blend + brush_value, 0.f, 1.f);
								}
								if (overlay_id == asset_id) {
									blend = CLAMP(blend - brush_value, 0.f, 1.f);
								}
								break;
							}

							default: {
								break;
							}
						}
						break;
					}
					case AUTOSHADER: {
						if (brush_alpha > 0.5f) {
							autoshader = (_operation == ADD);
							uvscale = 0.f;
							uvrotation = 0.f;
						}
						break;
					}
					case HOLES: {
						if (brush_alpha > 0.5f) {
							hole = (_operation == ADD);
						}
						break;
					}
					case NAVIGATION: {
						if (brush_alpha > 0.5f) {
							navigation = (_operation == ADD);
						}
						break;
					}
					default: {
						break;
					}
				}

				// Convert back to bitfield
				uint32_t blend_int = uint32_t(CLAMP(Math::round(blend * 255.f), 0.f, 255.f));
				uint32_t bits = enc_base(base_id) | enc_overlay(overlay_id) |
						enc_blend(blend_int) | enc_uv_rotation(uvrotation) |
						enc_uv_scale(uvscale) | enc_hole(hole) |
						enc_nav(navigation) | enc_auto(autoshader);

				// Write back to pixel in FORMAT_RF. Must be a 32-bit float
				dest = Color(as_float(bits), 0.f, 0.f, 1.f);

			} else if (map_type == TYPE_COLOR) {
				// Filter by visible texture
				if (texture_filter) {
					Image *cmap = region->get_map_ptr(TYPE_CONTROL);
					if (!cmap) {
						continue;
					}
					float src_ctrl = cmap->get_pixelv(map_pixel_position).r; // Must be float
					int tex_id = (get_blend(src_ctrl) > 110 - margin) ? get_overlay(src_ctrl) : get_base(src_ctrl);
					if (tex_id != asset_id) {
						continue;
					}
				}
				if (!data->is_in_slope(brush_global_position, slope_range, modifier_alt)) {
					continue;
				}
				switch (_tool) {
					case COLOR:
						dest = src.lerp((_operation == ADD) ? color : COLOR_WHITE, brush_alpha * strength);
						dest.a = src.a;
						break;
					case ROUGHNESS:
						/* Roughness received from UI is -100 to 100. Changed to 0,1 before storing.
						 * To convert 0,1 back to -100,100 use: 200 * (color.a - 0.5)
						 * However Godot stores values as 8-bit ints. Roundtrip is = int(a*255)/255.0
						 * Roughness 0 is saved as 0.5, but retrieved is 0.498, or -0.4 roughness
						 * We round the final amount in tool_settings.gd:_on_picked().
						 */
						if (_operation == ADD) {
							real_t target = .5f + .5f * roughness;
							dest.a = Math::lerp(real_t(src.a), target, brush_alpha * strength);
							dest.a = float(int(dest.a * 255.f)) / 255.f; // Quantize explicitly so picked values match painted values
						} else {
							dest.a = Math::lerp(real_t(src.a), real_t(.5f), brush_alpha * strength);
							dest.a = float(int(dest.a * 255.f)) / 255.f;
						}
						break;
					default:
						break;
				}
			}
			backup_region(region);
			map->set_pixelv(map_pixel_position, dest);
		}
	}
	// Regenerate color mipmaps for edited regions
	if (map_type == TYPE_COLOR) {
		for (int i = 0; i < _edited_regions.size(); i++) {
			Ref<WorldScape3DRegion> region = _edited_regions[i];
			if (region.is_valid()) {
				region->get_map(map_type)->generate_mipmaps();
			}
		}
	}
	// If no added or removed regions, update only changed texture array layers from the edited regions in the rendering server
	if (_added_removed_locations.size() == regions_added_removed) {
		data->update_maps(map_type, false, false);
	} else {
		// If region qty was changed, must fully rebuild the maps
		data->update_maps(map_type, true, map_type == TYPE_COLOR);
	}
	data->add_edited_area(edited_area);

	if (_tool == HOLES || _tool == HEIGHT || _tool == SCULPT) {
		_terrain->get_instancer()->update_transforms(edited_area);
	}
	// Update Dynamic / Editor collision
	if (_terrain->get_collision_mode() == WorldScape3DCollision::DYNAMIC_EDITOR) {
		_terrain->get_collision()->update(true);
	}
}

void WorldScape3DEditor::_store_undo() {
	IS_INIT_COND_MESG(!_terrain->get_plugin(), "_terrain isn't initialized, returning", VOID);
	if (_tool < 0 || _tool >= TOOL_MAX) {
		return;
	}
	LOG(DEBUG, "Finalize undo & redo snapshots");
	Dictionary redo_data;
	// Store current locations; Original backed up in start_operation()
	redo_data["region_locations"] = _terrain->get_data()->get_region_locations().duplicate();
	// Store original and current backups of edited regions
	_undo_data["edited_regions"] = _original_regions;
	redo_data["edited_regions"] = _edited_regions;

	// Store regions that were removed or added
	if (_added_removed_locations.size() > 0) {
		if (_tool == REGION && _operation == SUBTRACT) {
			_undo_data["removed_regions"] = _added_removed_locations;
			redo_data["added_regions"] = _added_removed_locations;
			LOG(DEBUG, "Removed regions: ", _added_removed_locations);
		} else {
			_undo_data["added_regions"] = _added_removed_locations;
			redo_data["removed_regions"] = _added_removed_locations;
			LOG(DEBUG, "Added regions: ", _added_removed_locations);
		}
	}

	if (_terrain->get_data()->get_edited_area().has_volume()) {
		_undo_data["edited_area"] = _terrain->get_data()->get_edited_area();
		redo_data["edited_area"] = _terrain->get_data()->get_edited_area();
		LOG(DEBUG, "Adding edited area to snapshots: ", _undo_data["edited_area"]);
	}

	// Store data in Godot's Undo/Redo Manager
	EditorUndoRedoManager *undo_redo = _terrain->get_plugin()->get_undo_redo();
	LOG(INFO, "Storing undo snapshot");
	String action_name = String("WorldScape3D ") + OPNAME[_operation] + String(" ") + TOOLNAME[_tool];
	LOG(DEBUG, "Creating undo action: '", action_name, "'");
	undo_redo->create_action(action_name, UndoRedo::MERGE_DISABLE, _terrain);

	LOG(DEBUG, "Storing undo snapshot: ", _undo_data);
	undo_redo->add_undo_method(this, "apply_undo", _undo_data.duplicate());

	LOG(DEBUG, "Storing redo snapshot: ", redo_data);
	undo_redo->add_do_method(this, "apply_undo", redo_data);

	LOG(DEBUG, "Committing undo action");
	undo_redo->commit_action(false);
}

void WorldScape3DEditor::_apply_undo(const Dictionary &p_data) {
	IS_INIT_COND_MESG(!_terrain->get_plugin(), "_terrain isn't initialized, returning", VOID);
	LOG(INFO, "Applying Undo/Redo data");

	WorldScape3DData *data = _terrain->get_data();

	if (p_data.has("edited_regions")) {
		Util::print_arr("Edited regions", p_data["edited_regions"]);
		TypedArray<WorldScape3DRegion> undo_regions = p_data["edited_regions"];
		LOG(DEBUG, "Backup has ", undo_regions.size(), " edited regions");
		for (int i = 0; i < undo_regions.size(); i++) {
			Ref<WorldScape3DRegion> region = undo_regions[i];
			if (region.is_null()) {
				LOG(ERROR, "Null region saved in undo data. Please report this error.");
				continue;
			}
			region->sanitize_maps(); // Live data may not have some maps so must be sanitized
			Dictionary regions = data->get_regions_all();
			regions[region->get_location()] = region;
			region->set_modified(true); // Tell update_maps() this region has layers that can be individually updated
			region->set_edited(true);
			region->set_deleted(false); // Ensure region not marked for deletion
		}
	}

	if (p_data.has("edited_area")) {
		LOG(DEBUG, "Edited area: ", p_data["edited_area"]);
		data->add_edited_area(p_data["edited_area"]);
	}

	if (p_data.has("added_regions")) {
		LOG(DEBUG, "Added regions: ", p_data["added_regions"]);
		TypedArray<Vector2i> region_locs = p_data["added_regions"];
		for (int i = 0; i < region_locs.size(); i++) {
			Ref<WorldScape3DRegion> region = data->get_region(region_locs[i]);
			if (region.is_valid()) {
				LOG(DEBUG, "Marking region: ", region_locs[i], " +deleted, +modified, ", ptr_to_str(*region));
				region->set_deleted(true);
				region->set_modified(true);
			}
		}
	}
	if (p_data.has("removed_regions")) {
		LOG(DEBUG, "Removed regions: ", p_data["removed_regions"]);
		TypedArray<Vector2i> region_locs = p_data["removed_regions"];
		for (int i = 0; i < region_locs.size(); i++) {
			Ref<WorldScape3DRegion> region = data->get_region(region_locs[i]);
			if (region.is_valid()) {
				LOG(DEBUG, "Marking region: ", region_locs[i], " -deleted, +modified, ", ptr_to_str(*region));
				region->set_deleted(false);
				region->set_modified(true);
				_send_region_aabb(region_locs[i], region->get_height_range());
			}
		}
	}

	// After all regions are in place, reset the region map, which also calls update_maps
	if (p_data.has("region_locations")) {
		// Load w/ duplicate or it gets a bit wonky undoing removed regions w/ saves
		_terrain->get_data()->set_region_locations(p_data["region_locations"].duplicate());
		Array locations = data->get_region_locations();
		LOG(DEBUG, "Locations(", locations.size(), "): ", locations);
	}
	// If this undo set modifies the region qty, we must rebuild the arrays. Otherwise we can update individual layers
	if (p_data.has("added_regions") || p_data.has("removed_regions")) {
		data->update_maps(TYPE_MAX, true, false);
	} else {
		data->update_maps(TYPE_MAX, false, false);
	}
	// After TextureArray updates clear edited regions flag.
	if (p_data.has("edited_regions")) {
		TypedArray<WorldScape3DRegion> undo_regions = p_data["edited_regions"];
		for (int i = 0; i < undo_regions.size(); i++) {
			Ref<WorldScape3DRegion> region = undo_regions[i];
			if (region.is_valid()) {
				region->set_edited(false);
			}
		}
	}
	_terrain->get_instancer()->update_mmis(true);
	if (_terrain->get_plugin()->has_method("update_grid")) {
		LOG(DEBUG, "Calling GDScript update_grid()");
		_terrain->get_plugin()->call("update_grid");
	}
}

///////////////////////////
// Public Functions
///////////////////////////

// Sanitize and set incoming brush data w/ defaults and clamps
// Only sanitizes data needed for the editor, other parameters (eg instancer) untouched here
void WorldScape3DEditor::set_brush_data(const Dictionary &p_data) {
	_brush_data = p_data; // Same instance. Anything could be inserted after this, eg mouse_pressure

	// Sanitize image and textures
	Array brush_images = p_data["brush"];
	bool error = false;
	if (brush_images.size() == 2) {
		Ref<Image> img = brush_images[0];
		if (img.is_valid() && !img->is_empty()) {
			_brush_data["brush_image"] = img;
			_brush_data["brush_image_size"] = img->get_size();
		} else {
			print_error("Brush data doesn't contain a valid image");
		}
		Ref<Texture2D> tex = brush_images[1];
		if (tex.is_valid() && tex->get_width() > 0 && tex->get_height() > 0) {
			_brush_data["brush_texture"] = tex;
		} else {
			print_error("Brush data doesn't contain a valid texture");
		}
	} else {
		print_error("Brush data doesn't contain an image and texture");
	}

	// Sanitize settings
	// size is redundantly clamped differently in _operate_map and instancer::add_transforms
	_brush_data["size"] = CLAMP(real_t(p_data.get("size", 10.f)), 0.1f, 4096.f); // Diameter in meters
	_brush_data["strength"] = CLAMP(real_t(p_data.get("strength", .1f)) * .01f, .01f, 1000.f); // 1-100k% (max of 1000m per click)
	// mouse_pressure injected in editor.gd and sanitized in _operate_map()
	Vector2 slope = p_data.get("slope", Vector2(0.f, 90.f));
	slope.x = CLAMP(slope.x, 0.f, 90.f);
	slope.y = CLAMP(slope.y, 0.f, 90.f);
	_brush_data["slope"] = slope; // 0-90 (degrees)
	_brush_data["height"] = CLAMP(real_t(p_data.get("height", 0.f)), -65536.f, 65536.f); // Meters
	Color col = p_data.get("color", COLOR_ROUGHNESS);
	col.r = CLAMP(col.r, 0.f, 5.f);
	col.g = CLAMP(col.g, 0.f, 5.f);
	col.b = CLAMP(col.b, 0.f, 5.f);
	col.a = CLAMP(col.a, 0.f, 1.f);
	_brush_data["color"] = col;
	_brush_data["roughness"] = CLAMP(real_t(p_data.get("roughness", 0.f)), -100.f, 100.f) * .01f; // Percentage

	_brush_data["enable_texture"] = p_data.get("enable_texture", true);
	_brush_data["texture_filter"] = p_data.get("texture_filter", false);
	_brush_data["asset_id"] = CLAMP(int(p_data.get("asset_id", 0)), 0, ((_tool == INSTANCER) ? WorldScape3DAssets::MAX_MESHES : WorldScape3DAssets::MAX_TEXTURES) - 1);
	_brush_data["margin"] = CLAMP(int(p_data.get("margin", 0)), -100, 100);

	_brush_data["enable_angle"] = p_data.get("enable_angle", true);
	_brush_data["dynamic_angle"] = p_data.get("dynamic_angle", false);
	_brush_data["angle"] = CLAMP(real_t(p_data.get("angle", 0.f)), 0.f, 337.5f);

	_brush_data["enable_scale"] = p_data.get("enable_scale", true);
	_brush_data["scale"] = CLAMP(real_t(p_data.get("scale", 0.f)), -60.f, 80.f);

	_brush_data["auto_regions"] = bool(p_data.get("auto_regions", true));
	_brush_data["align_to_view"] = bool(p_data.get("align_to_view", true));
	_brush_data["gamma"] = CLAMP(real_t(p_data.get("gamma", 1.f)), 0.1f, 2.f);
	_brush_data["jitter"] = CLAMP(real_t(p_data.get("jitter", 0.f)), 0.f, 1.f);
	_brush_data["gradient_points"] = p_data.get("gradient_points", PackedVector3Array());

	Util::print_dict("set_brush_data() Sanitized brush data:", _brush_data, EXTREME);
}

void WorldScape3DEditor::set_tool(const Tool p_tool) {
	if (_terrain && _tool != p_tool && (_tool == Tool::NAVIGATION || p_tool == Tool::NAVIGATION)) {
		_tool = CLAMP(p_tool, Tool(0), TOOL_MAX);
		_terrain->get_material()->update();
	} else {
		_tool = p_tool;
	}
}

// Called on mouse click
void WorldScape3DEditor::start_operation(const Vector3 &p_global_position) {
	IS_DATA_INIT_MESG("Terrain isn't initialized", VOID);
	LOG(INFO, "Setting up undo snapshot");
	_undo_data.clear();
	_undo_data["region_locations"] = _terrain->get_data()->get_region_locations().duplicate();
	_is_operating = true;
	_original_regions = TypedArray<WorldScape3DRegion>(); // New pointers instead of clear
	_edited_regions = TypedArray<WorldScape3DRegion>();
	_added_removed_locations = TypedArray<Vector2i>();
	// Reset counter at start to ensure first click places an instance
	_terrain->get_instancer()->reset_density_counter();
	_terrain->get_data()->clear_edited_area();
	_operation_position = p_global_position;
	_operation_movement = Vector3();
}

// Called on mouse movement with left mouse button down
void WorldScape3DEditor::operate(const Vector3 &p_global_position, const real_t p_camera_direction) {
	IS_DATA_INIT_MESG("Terrain isn't initialized", VOID);
	if (!_is_operating) {
		LOG(ERROR, "Run start_operation() before operating");
		return;
	}
	_operation_movement = p_global_position - _operation_position;
	_operation_position = p_global_position;

	// Convolve the last 8 movement events, we dont clear on mouse release
	// so as to make repeated mouse strokes in the same direction consistent
	_operation_movement_history.push_back(_operation_movement);
	if (_operation_movement_history.size() > 8) {
		_operation_movement_history.pop_front();
	}
	// size -1, dont add the last appended entry
	for (int i = 0; i < _operation_movement_history.size() - 1; i++) {
		_operation_movement += _operation_movement_history[i];
	}
	_operation_movement *= 0.125f; // 1/8th

	if (_tool == REGION) {
		_operate_region(_terrain->get_data()->get_region_location(p_global_position));
	} else if (_tool >= 0 && _tool < TOOL_MAX) {
		_operate_map(p_global_position, p_camera_direction);
	}
}

void WorldScape3DEditor::backup_region(const Ref<WorldScape3DRegion> &p_region) {
	// Backup region once at the start of an operation. Once Edited is set, this is skipped
	if (_is_operating && p_region.is_valid() && !p_region->is_edited()) {
		LOG(DEBUG, "Storing original copy of region: ", p_region->get_location());
		_original_regions.push_back(p_region->duplicate(true));
		_edited_regions.push_back(p_region);
		p_region->set_edited(true);
		p_region->set_modified(true);
	}
}

// Called on left mouse button released
void WorldScape3DEditor::stop_operation() {
	IS_DATA_INIT_MESG("Terrain isn't initialized", VOID);
	// If undo was created and terrain actually modified, store it
	LOG(DEBUG, "Backed up regions: ", _original_regions.size(), ", Edited regions: ", _edited_regions.size(),
			", Added/Removed regions: ", _added_removed_locations.size());
	if (_is_operating && (!_added_removed_locations.is_empty() || !_edited_regions.is_empty())) {
		for (int i = 0; i < _edited_regions.size(); i++) {
			Ref<WorldScape3DRegion> region = _edited_regions[i];
			region->set_edited(false);
			// Make duplicate for redo backup
			_edited_regions[i] = region->duplicate(true);
		}
		_store_undo();
	}
	_undo_data.clear();
	_original_regions = TypedArray<WorldScape3DRegion>(); //New pointers instead of clear
	_edited_regions = TypedArray<WorldScape3DRegion>();
	_added_removed_locations = TypedArray<Vector2i>();
	_terrain->get_data()->clear_edited_area();
	_is_operating = false;
}

void WorldScape3DEditorPlugin::init() {
	_editor_settings = EditorSettings::get_singleton();

	// Get the ReX Editor window. Structure is root:Window/EditorNode/Base Control
	_rex_editor_window = cast_to<Window>(EditorInterface::get_singleton()->get_base_control()->get_parent()->get_parent());
	_rex_editor_window->connect("focus_entered", callable_mp(this, &WorldScape3DEditorPlugin::on_focus_entered));

	_ui = memnew(WorldScape3DUI(this));
	add_child(_ui);

	_asset_dock = memnew(WorldScape3DAssetDock(this));

	EditorPlugin::connect("scene_changed", callable_mp(this, &WorldScape3DEditorPlugin::on_scene_changed));

	_use_meta = OS::get_singleton()->get_name() == "macOS";

	set_process_input(true);
}

WorldScape3DEditorPlugin::WorldScape3DEditorPlugin() :
		_editor(std::make_unique<WorldScape3DEditor>()), _region_gizmo(memnew(WorldScape3DRegionGizmo)) {
}

WorldScape3DEditorPlugin::~WorldScape3DEditorPlugin() {
	_asset_dock->queue_free();
	_ui->queue_free();

	if (_rex_editor_window && _rex_editor_window->is_connected("focus_entered", callable_mp(this, &WorldScape3DEditorPlugin::on_focus_entered))) {
		_rex_editor_window->disconnect("focus_entered", callable_mp(this, &WorldScape3DEditorPlugin::on_focus_entered));
	}
	EditorPlugin::disconnect("scene_changed", callable_mp(this, &WorldScape3DEditorPlugin::on_scene_changed));
}

WorldScape3DEditor *WorldScape3DEditorPlugin::get_editor() const {
	return _editor.get();
}

WorldScape3D *WorldScape3DEditorPlugin::get_terrain() const {
	auto terrain = _editor->get_terrain();
	return is_terrain_valid(terrain) ? terrain : nullptr;
}

WorldScape3D *WorldScape3DEditorPlugin::get_last_terrain() const {
	return is_terrain_valid(_last_terrain) ? _last_terrain : nullptr;
}

bool WorldScape3DEditorPlugin::is_selected() const {
	TypedArray<Node> selected = EditorInterface::get_singleton()->get_selection()->get_selected_nodes();
	for (auto &node : selected) {
		if (cast_to<WorldScape3D>(node) != nullptr) {
			return true;
		}
	}
	return false;
}

void WorldScape3DEditorPlugin::select_terrain() {
	if (is_terrain_valid(_last_terrain) && !is_selected()) {
		auto editor_sel = EditorInterface::get_singleton()->get_selection();
		editor_sel->clear();
		editor_sel->add_node(_last_terrain);
	}
}

void WorldScape3DEditorPlugin::make_visible(const bool visible) {
	if (visible && is_selected()) {
		_ui->set_visible(true);
	} else {
		_ui->set_visible(false);
	}
	if (_asset_dock->is_visible() != visible) {
		_asset_dock->set_visible(visible);
		_asset_dock->update_dock();
	}
}

void WorldScape3DEditorPlugin::edit(Object *object) {
	if (!object) {
		clear();
	}

	auto terrain = cast_to<WorldScape3D>(object);
	auto nav3d = cast_to<NavigationRegion3D>(object);
	if (nav3d) {
		for (auto child : nav3d->get_children()) {
			if (auto t_child = cast_to<WorldScape3D>(child); is_terrain_valid(t_child)) {
				terrain = t_child;
				break;
			}
		}
	}

	if (is_terrain_valid(terrain)) {
		_last_terrain = terrain;
		terrain->set_plugin(this);
		terrain->set_editor(_editor.get());
		_editor->set_terrain(terrain);
		if (!terrain->get_gizmos().has(_region_gizmo)) {
			_region_gizmo->set_node_3d(terrain);
			terrain->add_gizmo(_region_gizmo);
		}
		terrain->set_meta("_edit_lock_", true);
		_ui->set_visible(true);
		if (nav3d) {
			_nav_region = nav3d;
		}

		// Get alerted when a new asset list is loaded
		if (!terrain->is_connected("assets_changed", callable_mp(_asset_dock, &WorldScape3DAssetDock::update_assets))) {
			terrain->connect("assets_changed", callable_mp(_asset_dock, &WorldScape3DAssetDock::update_assets));
		}
		_asset_dock->update_assets();
		// Get alerted when the region map changes
		auto data = terrain->get_data();
		if (data && !data->is_connected("region_map_changed", callable_mp(this, &WorldScape3DEditorPlugin::update_region_grid))) {
			data->connect("region_map_changed", callable_mp(this, &WorldScape3DEditorPlugin::update_region_grid));
		}
		update_region_grid();
	} else if (is_terrain_valid(_last_terrain) && nav3d) {
		_ui->set_visible(true, true);
	} else {
		clear();
	}
}

// EditorPlugin selection function call chain isn't consistent. Here's the map of calls:
// Assume we handle WorldScape3D and NavigationRegion3D
// Click WorldScape3D: 					handles(WorldScape3D), make_visible(true), edit(WorldScape3D)
// Deselect:							make_visible(false), edit(null)
// Click other node:					handles(OtherNode)
// Click NavRegion3D:					handles(NavReg3D), make_visible(true), edit(NavReg3D)
// Click NavRegion3D, WorldScape3D:		handles(WorldScape3D), edit(WorldScape3D)
// Click WorldScape3D, NavRegion3D:		handles(NavReg3D), edit(NavReg3D)
bool WorldScape3DEditorPlugin::handles(Object *object) const {
	if (auto terrain = cast_to<WorldScape3D>(object); terrain) {
		return true;
	}
	if (auto nav3d = cast_to<NavigationRegion3D>(object); nav3d) {
		for (auto child : nav3d->get_children()) {
			if (cast_to<WorldScape3D>(child) != nullptr) {
				return true;
			}
		}
	}

	// TODO: WorldScape3DObjects class that locks children to the terrain surface
	// WorldScape3DObjects requires access to EditorUndoRedoManager. The only way to make sure it
	// always has it, is to pass it in here. _edit is NOT called if the node is cut and pasted.

	return false;
}

void WorldScape3DEditorPlugin::clear() {
	if (is_terrain_valid()) {
		auto terrain = _editor->get_terrain();
		if (terrain) {
			auto data = terrain->get_data();
			if (data->is_connected("region_map_changed", callable_mp(this, &WorldScape3DEditorPlugin::update_region_grid))) {
				data->disconnect("region_map_changed", callable_mp(this, &WorldScape3DEditorPlugin::update_region_grid));
			}
			terrain->clear_gizmos();
		}
		_editor->set_terrain(nullptr);
		_ui->clear_picking();
	}
	_region_gizmo->clear();
}

void WorldScape3DEditorPlugin::on_scene_changed(Node *scene_root) {
	if (!scene_root) {
		return;
	}

	// TODO: WorldScape3DObjects
	// for (auto child : scene_root->find_children("WorldScape3D")) {
	// 	auto t3dobj = cast_to<WorldScape3DObjects>(child);
	// 	t3dobj->editor_setup(this);
	// }

	_asset_dock->update_assets();
	_scene_change_timer = get_tree()->create_timer(2.);
	_scene_change_timer->connect("timeout", callable_mp(this, &WorldScape3DEditorPlugin::on_scene_change_timeout));
}

void WorldScape3DEditorPlugin::on_scene_change_timeout() {
	if (_asset_dock) {
		_asset_dock->update_thumbnails();
	}
}

void WorldScape3DEditorPlugin::on_focus_entered() {
	read_input({});
	_ui->update_decal();
}

void WorldScape3DEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_POST_ENTER_TREE) {
		init();
		set_input_event_forwarding_always_enabled();
	}
}

EditorPlugin::AfterGUIInput WorldScape3DEditorPlugin::forward_3d_gui_input(Camera3D *camera, const Ref<InputEvent> &event) {
	if (!_editor) {
		return AfterGUIInput::AFTER_GUI_INPUT_PASS;
	}

	auto terrain = _editor->get_terrain();
	if (!terrain) {
		return AfterGUIInput::AFTER_GUI_INPUT_PASS;
	}

	if (const auto continue_input = read_input(event); continue_input != AFTER_GUI_INPUT_CUSTOM) {
		return continue_input;
	}
	_ui->update_decal();

	auto brush_data = _ui->get_tool_settings()->get_brush_data(); // Dictionaries are ref counted

	/* Setup active camera & viewport
	 * Always update this for all inputs, as the mouse position can move without
	 * necessarily being a InputEventMouseMotion object. get_intersection() also
	 * returns the last frame position, and should be updated more frequently.
	 */

	// Snap terrain to current camera
	terrain->set_camera(camera);

	// Detect if viewport is set to half_resolution
	// Structure is: Node3DEditorViewportContainer/Node3DEditorViewport(4)/SubViewportContainer/SubViewport/Camera3D
	auto editor_vpc = Object::cast_to<SubViewportContainer>(camera->get_parent()->get_parent());
	if (!editor_vpc) {
		return AfterGUIInput::AFTER_GUI_INPUT_PASS;
	}
	const bool full_res = editor_vpc->get_stretch_shrink() != 2;

	// Get mouse location on terrain
	// Project 2D mouse position to 3D position and direction
	auto vp_mouse_pos = editor_vpc->get_local_mouse_position();
	auto mouse_pos = full_res ? vp_mouse_pos : vp_mouse_pos / 2;
	auto camera_pos = camera->project_ray_origin(mouse_pos);
	auto camera_dir = camera->project_ray_normal(mouse_pos);

	// If region tool, grab mouse position without considering height
	if (_editor->get_tool() == WorldScape3DEditor::REGION) {
		const auto t = -Vector3{ 0, 1, 0 }.dot(camera_pos) / Vector3{ 0., 1, 0 }.dot(camera_dir);
		_mouse_global_position = camera_pos + t * camera_dir;
	} else {
		// otherwise, look for intersection with terrain
		const auto intersection_point = terrain->get_intersection(camera_pos, camera_dir, true);
		if (intersection_point.z > 3.4e38 || std::isnan(intersection_point.y)) {
			return AfterGUIInput::AFTER_GUI_INPUT_PASS;
		}
		_mouse_global_position = intersection_point;
	}

	Ref<InputEventMouseMotion> mouse_motion_event = event;
	if (mouse_motion_event.is_valid()) {
		// Handle mouse movement
		if (_mouse_mode != CameraMove) {
			// Update region highlight
			const Vector2 region_pos = (Vector2{ _mouse_global_position.x, _mouse_global_position.z } / (terrain->get_region_size() * terrain->get_vertex_spacing())).floor();
			if (_current_region_pos != region_pos) {
				_current_region_pos = region_pos;
				update_region_grid();
			}
		}
		if (_mouse_mode == Operating && _editor->is_operating()) {
			// Inject pressure
			brush_data["mouse_pressure"] = mouse_motion_event->get_pressure();

			_editor->operate(_mouse_global_position, camera->get_rotation().y);
			return AfterGUIInput::AFTER_GUI_INPUT_STOP;
		}
		return AfterGUIInput::AFTER_GUI_INPUT_PASS;
	}

	Ref<InputEventMouseButton> mouse_button = event;
	if (_mouse_mode == Operating && mouse_button.is_valid()) {
		if (mouse_button->is_pressed()) {
			if (_ui->is_picking()) {
				_ui->pick(_mouse_global_position);
				if (auto builder = _ui->get_operation_builder(); !builder || !builder->is_ready()) {
					return AFTER_GUI_INPUT_STOP;
				}
			}

			if (_mod_ctrl && _editor->get_tool() == WorldScape3DEditor::HEIGHT) {
				const real_t height = terrain->get_data()->get_height(_mouse_global_position);
				brush_data["height"] = height;
				_ui->get_tool_settings()->set_setting("height", height);
			}

			// If adjusting regions
			if (_editor->get_tool() == WorldScape3DEditor::REGION) {
				// skip if region already exists
				const bool has_region = terrain->get_data()->has_regionp(_mouse_global_position);
				auto op = _editor->get_operation();
				if ((has_region && op == WorldScape3DEditor::Operation::ADD) ||
						(!has_region && op == WorldScape3DEditor::Operation::SUBTRACT)) {
					return AfterGUIInput::AFTER_GUI_INPUT_STOP;
				}
			}

			// If an automatic operation is ready to go (e.g. gradient)
			auto builder = _ui->get_operation_builder();
			if (builder && builder->is_ready()) {
				builder->apply_operation(_editor.get(), _mouse_global_position, camera->get_rotation().y);
				return AfterGUIInput::AFTER_GUI_INPUT_STOP;
			}

			// Mouse clicked, start editing
			_editor->start_operation(_mouse_global_position);
			_editor->operate(_mouse_global_position, camera->get_rotation().y);
			return AfterGUIInput::AFTER_GUI_INPUT_STOP;
		} else if (_editor->is_operating()) {
			// stop editing, store undo data
			_editor->stop_operation();
			return AfterGUIInput::AFTER_GUI_INPUT_STOP;
		}
	}

	return AfterGUIInput::AFTER_GUI_INPUT_PASS;
}

void WorldScape3DEditorPlugin::update_region_grid() {
	if (_region_gizmo.is_null()) {
		return;
	}
	_region_gizmo->set_hidden(!_ui || !_ui->is_visible());

	auto terrain = _editor->get_terrain();
	if (!terrain) {
		return;
	}

	if (is_terrain_valid()) {
		_region_gizmo->update(_current_region_pos, _editor.get());
		terrain->update_gizmos();
		return;
	}
	_region_gizmo->update();
}

EditorPlugin::AfterGUIInput WorldScape3DEditorPlugin::read_input(const Ref<InputEvent> &event) {
	auto input = Input::get_singleton();
	auto time = Time::get_singleton();

	// Determine if user is moving camera or applying
	Ref<InputEventMouseButton> mbevent = event;
	if (input->is_mouse_button_pressed(MouseButton::LEFT) ||
			mbevent.is_valid() && event->is_released() && mbevent->get_button_index() == MouseButton::LEFT) {
		_mouse_mode = Operating;
	} else {
		_mouse_mode = None;
	}

	int nav_scheme = get_setting("editors/3d/navigation/navigation_scheme", 0);
	switch (nav_scheme) {
		case 2:
		case 1: { // Modo, Maya
			if (input->is_mouse_button_pressed(MouseButton::RIGHT) ||
					input->is_key_pressed(Key::ALT) && input->is_mouse_button_pressed(MouseButton::LEFT)) {
				_mouse_mode = CameraMove;
			}
			if (mbevent.is_valid() && mbevent->is_released() &&
					(mbevent->get_button_index() == MouseButton::RIGHT ||
							input->is_key_pressed(Key::ALT) && mbevent->get_button_index() == MouseButton::LEFT)) {
				_rmb_release_time = time->get_ticks_msec();
			}
		} break;
		default:
		case 0: { // Godot
			if (input->is_mouse_button_pressed(MouseButton::RIGHT) ||
					input->is_mouse_button_pressed(MouseButton::MIDDLE)) {
				_mouse_mode = CameraMove;
			}
			if (mbevent.is_valid() && mbevent->is_released() &&
					(mbevent->get_button_index() == MouseButton::RIGHT ||
							mbevent->get_button_index() == MouseButton::MIDDLE)) {
				_rmb_release_time = time->get_ticks_msec();
			}
		} break;
	}

	if (_mouse_mode == CameraMove) {
		return AFTER_GUI_INPUT_PASS;
	}

	// Determine modifiers pressed
	_mod_shift = input->is_key_pressed(Key::SHIFT);

	// Editor responds to modifier_ctrl so we must register touchscreen Invert
	_mod_ctrl = _ui->input_inverted() || (_use_meta ? input->is_key_pressed(Key::META) : input->is_key_pressed(Key::CTRL));

	Key alt_key;
	switch (int alt_key_bind = get_setting("terrain3d/config/alt_key_bind", 0); alt_key_bind) {
		case 3:
			alt_key = Key::CAPSLOCK;
			break;
		case 2:
			alt_key = Key::META;
			break;
		case 1:
			alt_key = Key::SPACE;
			break;
		default:
		case 0:
			alt_key = Key::ALT;
			break;
	}
	_mod_alt = input->is_key_pressed(alt_key);
	int curr_mods = static_cast<int>(_mod_shift) | static_cast<int>(_mod_ctrl) << 1 | static_cast<int>(_mod_alt) << 2;

	// Process hotkeys
	if (Ref<InputEventKey> kevent = event;
			kevent.is_valid() && curr_mods == 0 &&
			event->is_pressed() && !event->is_echo() &&
			consume_hotkey(kevent->get_keycode())) {
		EditorInterface::get_singleton()->get_editor_viewport_3d()->set_input_as_handled();
		return AFTER_GUI_INPUT_STOP;
	}

	// Brush data is cleared on set_tool, or clicking textures in the asset dock
	// Update modifiers if changed or missing
	auto brush_data = _editor->get_brush_data();
	if (_last_mods != curr_mods || !brush_data.has("modifier_shift")) {
		_last_mods = curr_mods;
		brush_data["modifier_shift"] = _mod_shift;
		brush_data["modifier_ctrl"] = _mod_ctrl;
		brush_data["modifier_alt"] = _mod_alt;
		_ui->set_active_operation();
	}

	// Continue processing input
	return AFTER_GUI_INPUT_CUSTOM;
}

bool WorldScape3DEditorPlugin::consume_hotkey(Key code) {
	auto terrain = get_terrain();
	auto material = terrain ? terrain->get_material() : nullptr;
	switch (code) {
		case Key::KEY_1:
			if (material.is_valid()) {
				material->set_show_region_grid(!material->get_show_region_grid());
			}
			break;
		case Key::KEY_2:
			if (material.is_valid()) {
				material->set_show_instancer_grid(!material->get_show_instancer_grid());
			}
			break;
		case Key::KEY_3:
			if (material.is_valid()) {
				material->set_show_vertex_grid(!material->get_show_vertex_grid());
			}
			break;
		case Key::KEY_4:
			if (material.is_valid()) {
				material->set_show_contours(!material->get_show_contours());
			}
			break;
		case Key::E: // Region
			_editor->set_tool(WorldScape3DEditor::REGION);
			_editor->set_operation(_mod_ctrl ? WorldScape3DEditor::SUBTRACT : WorldScape3DEditor::ADD);
			break;
		case Key::R: // Raise/Lower
			_editor->set_tool(WorldScape3DEditor::SCULPT);
			_editor->set_operation(_mod_ctrl ? WorldScape3DEditor::SUBTRACT : WorldScape3DEditor::ADD);
			break;
		case Key::H: // Height
			_editor->set_tool(WorldScape3DEditor::HEIGHT);
			_editor->set_operation(_mod_ctrl ? WorldScape3DEditor::SUBTRACT : WorldScape3DEditor::ADD);
			break;
		case Key::S: // Slope
			_editor->set_tool(WorldScape3DEditor::SCULPT);
			_editor->set_operation(WorldScape3DEditor::GRADIENT);
			break;
		case Key::C: // Color
			_editor->set_tool(WorldScape3DEditor::COLOR);
			_editor->set_operation(_mod_ctrl ? WorldScape3DEditor::SUBTRACT : WorldScape3DEditor::ADD);
			break;
		case Key::N: // Navigable Area
			_editor->set_tool(WorldScape3DEditor::NAVIGATION);
			_editor->set_operation(_mod_ctrl ? WorldScape3DEditor::SUBTRACT : WorldScape3DEditor::ADD);
			break;
		case Key::I: // Instance Meshes
			_editor->set_tool(WorldScape3DEditor::INSTANCER);
			_editor->set_operation(_mod_ctrl ? WorldScape3DEditor::SUBTRACT : WorldScape3DEditor::ADD);
			break;
		case Key::X: // Holes
			_editor->set_tool(WorldScape3DEditor::HEIGHT);
			_editor->set_operation(_mod_ctrl ? WorldScape3DEditor::SUBTRACT : WorldScape3DEditor::ADD);
			break;
		case Key::W: // Wetness
			_editor->set_tool(WorldScape3DEditor::ROUGHNESS);
			_editor->set_operation(_mod_ctrl ? WorldScape3DEditor::SUBTRACT : WorldScape3DEditor::ADD);
			break;
		case Key::B: // Paint Texture
			_editor->set_tool(WorldScape3DEditor::TEXTURE);
			_editor->set_operation(_mod_ctrl ? WorldScape3DEditor::SUBTRACT : WorldScape3DEditor::ADD);
			break;
		case Key::V: // Spray Texture
			_editor->set_tool(WorldScape3DEditor::TEXTURE);
			_editor->set_operation(WorldScape3DEditor::REPLACE);
			break;
		case Key::A: // Autoshader
			_editor->set_tool(WorldScape3DEditor::AUTOSHADER);
			_editor->set_operation(_mod_ctrl ? WorldScape3DEditor::SUBTRACT : WorldScape3DEditor::ADD);
			break;
		default:
			return false;
	}
	return true;
}

void WorldScape3DEditorPlugin::setup_editor_settings() {
	_editor_settings = EditorInterface::get_singleton()->get_editor_settings();
	if (!_editor_settings->has_setting("terrain3d/config/alt_key_bindings")) {
		_editor_settings->set("terrain3d/config/alt_key_bind", 0);
	}
	const PropertyInfo info{
		Variant::Type::INT,
		"terrain3d/config/alt_key_bind",
		PROPERTY_HINT_ENUM,
		"Alt,Space,Meta,Capslock"
	};
	_editor_settings->add_property_hint(info);
}

void WorldScape3DEditorPlugin::set_setting(const String &str, Variant value) {
	_editor_settings->set_setting(str, value);
}

Variant WorldScape3DEditorPlugin::get_setting(const String &str, Variant default_value) const {
	return _editor_settings->has_setting(str) ? _editor_settings->get_setting(str) : default_value;
}

bool WorldScape3DEditorPlugin::has_settings(const String &str) const {
	return _editor_settings->has_setting(str);
}

void WorldScape3DEditorPlugin::erase_setting(const String &str) {
	_editor_settings->erase(str);
}

///////////////////////////
// Protected Functions
///////////////////////////

bool WorldScape3DEditorPlugin::is_terrain_valid(WorldScape3D *terrain) const {
	WorldScape3D *t = terrain ? terrain : _editor->get_terrain();
	return t && t->is_inside_tree() && t->get_data();
}

void WorldScape3DEditor::_bind_methods() {
	BIND_ENUM_CONSTANT(ADD);
	BIND_ENUM_CONSTANT(SUBTRACT);
	BIND_ENUM_CONSTANT(REPLACE);
	BIND_ENUM_CONSTANT(AVERAGE);
	BIND_ENUM_CONSTANT(GRADIENT);
	BIND_ENUM_CONSTANT(OP_MAX);

	BIND_ENUM_CONSTANT(SCULPT);
	BIND_ENUM_CONSTANT(HEIGHT);
	BIND_ENUM_CONSTANT(TEXTURE);
	BIND_ENUM_CONSTANT(COLOR);
	BIND_ENUM_CONSTANT(ROUGHNESS);
	BIND_ENUM_CONSTANT(ANGLE);
	BIND_ENUM_CONSTANT(SCALE);
	BIND_ENUM_CONSTANT(AUTOSHADER);
	BIND_ENUM_CONSTANT(HOLES);
	BIND_ENUM_CONSTANT(NAVIGATION);
	BIND_ENUM_CONSTANT(INSTANCER);
	BIND_ENUM_CONSTANT(REGION);
	BIND_ENUM_CONSTANT(TOOL_MAX);

	ClassDB::bind_method(D_METHOD("set_terrain", "terrain"), &WorldScape3DEditor::set_terrain);
	ClassDB::bind_method(D_METHOD("get_terrain"), &WorldScape3DEditor::get_terrain);

	ClassDB::bind_method(D_METHOD("set_brush_data", "data"), &WorldScape3DEditor::set_brush_data);
	ClassDB::bind_method(D_METHOD("set_tool", "tool"), &WorldScape3DEditor::set_tool);
	ClassDB::bind_method(D_METHOD("get_tool"), &WorldScape3DEditor::get_tool);
	ClassDB::bind_method(D_METHOD("set_operation", "operation"), &WorldScape3DEditor::set_operation);
	ClassDB::bind_method(D_METHOD("get_operation"), &WorldScape3DEditor::get_operation);
	ClassDB::bind_method(D_METHOD("start_operation", "position"), &WorldScape3DEditor::start_operation);
	ClassDB::bind_method(D_METHOD("is_operating"), &WorldScape3DEditor::is_operating);
	ClassDB::bind_method(D_METHOD("operate", "position", "camera_direction"), &WorldScape3DEditor::operate);
	ClassDB::bind_method(D_METHOD("backup_region", "region"), &WorldScape3DEditor::backup_region);
	ClassDB::bind_method(D_METHOD("stop_operation"), &WorldScape3DEditor::stop_operation);

	ClassDB::bind_method(D_METHOD("apply_undo", "data"), &WorldScape3DEditor::_apply_undo);
}
