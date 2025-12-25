/**************************************************************************/
/*  sprite_frames.cpp                                                     */
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

#include "sprite_frames.h"

#include "scene/resources/compressed_texture.h"
#include "scene/resources/image_texture.h"
#include "scene/scene_string_names.h"

Error SpriteFrames::load(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, vformat("Unable to open file: %s.", p_path));

	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] != 'R' || header[1] != 'S' || header[2] != 'S' || header[3] != 'F') {
		ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Compressed SpriteFrames file is corrupt (Bad header).");
	}

	uint32_t version = f->get_32();

	if (version > FORMAT_VERSION) {
		ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Compressed SpriteFrames file is too new.");
	}

	f->get_32(); // file width
	f->get_32(); // file height
	uint32_t data_flags = f->get_32();
	int frame_count = f->get_32();
	float frame_speed_multiplier = f->get_float();
	bool will_loop = f->get_8();

	//reserved
	f->get_32();
	f->get_32();
	f->get_32();

	int size_limit = 0;
	if (!(data_flags & CompressedTexture2D::FORMAT_BIT_STREAM)) {
		size_limit = 0;
	}

	Ref<Image> image;

	HashMap<StringName, Anim>::Iterator E = animations.find(SceneStringName(default_));
	ERR_FAIL_COND_V_MSG(!E, ERR_BUG, "BUG: Animation '" + SceneStringName(default_) + "' doesn't exist.");

	E->value.frames.resize(frame_count);
	Frame *frame = E->value.frames.ptrw();

	for (int current_frame = 0; current_frame < frame_count; current_frame++, frame++) {
		//reserved
		f->get_32();
		f->get_32();
		f->get_32();

		float delay = MAX(SPRITE_FRAME_MINIMUM_DURATION, f->get_float());
		image = CompressedTexture2D::load_image_from_file(f, size_limit);

		if (image.is_null() || image->is_empty()) {
			return ERR_CANT_OPEN;
		}

		*frame = { ImageTexture::create_from_image(image), delay };
	}

	set_animation_loop(SceneStringName(default_), will_loop);
	set_animation_speed(SceneStringName(default_), frame_speed_multiplier);

	emit_changed();
	notify_property_list_changed();
	return OK;
}

void SpriteFrames::add_frame(const StringName &p_anim, const Ref<Texture2D> &p_texture, float p_duration, int p_at_pos) {
	HashMap<StringName, Anim>::Iterator E = animations.find(p_anim);
	ERR_FAIL_COND_MSG(!E, "Animation '" + String(p_anim) + "' doesn't exist.");

	p_duration = MAX(SPRITE_FRAME_MINIMUM_DURATION, p_duration);

	Frame frame = { p_texture, p_duration };

	if (p_at_pos >= 0 && p_at_pos < E->value.frames.size()) {
		E->value.frames.insert(p_at_pos, frame);
	} else {
		E->value.frames.push_back(frame);
	}

	emit_changed();
}

void SpriteFrames::set_frame(const StringName &p_anim, int p_idx, const Ref<Texture2D> &p_texture, float p_duration) {
	HashMap<StringName, Anim>::Iterator E = animations.find(p_anim);
	ERR_FAIL_COND_MSG(!E, "Animation '" + String(p_anim) + "' doesn't exist.");
	ERR_FAIL_COND(p_idx < 0);
	if (p_idx >= E->value.frames.size()) {
		return;
	}

	p_duration = MAX(SPRITE_FRAME_MINIMUM_DURATION, p_duration);

	Frame frame = { p_texture, p_duration };

	E->value.frames.write[p_idx] = frame;

	emit_changed();
}

int SpriteFrames::get_frame_count(const StringName &p_anim) const {
	HashMap<StringName, Anim>::ConstIterator E = animations.find(p_anim);
	ERR_FAIL_COND_V_MSG(!E, 0, "Animation '" + String(p_anim) + "' doesn't exist.");

	return E->value.frames.size();
}

void SpriteFrames::remove_frame(const StringName &p_anim, int p_idx) {
	HashMap<StringName, Anim>::Iterator E = animations.find(p_anim);
	ERR_FAIL_COND_MSG(!E, "Animation '" + String(p_anim) + "' doesn't exist.");

	E->value.frames.remove_at(p_idx);

	emit_changed();
}

void SpriteFrames::clear(const StringName &p_anim) {
	HashMap<StringName, Anim>::Iterator E = animations.find(p_anim);
	ERR_FAIL_COND_MSG(!E, "Animation '" + String(p_anim) + "' doesn't exist.");

	E->value.frames.clear();

	emit_changed();
}

void SpriteFrames::clear_all() {
	animations.clear();
	add_animation(SceneStringName(default_));
}

void SpriteFrames::add_animation(const StringName &p_anim) {
	ERR_FAIL_COND_MSG(animations.has(p_anim), "SpriteFrames already has animation '" + p_anim + "'.");

	animations[p_anim] = Anim();
}

bool SpriteFrames::has_animation(const StringName &p_anim) const {
	return animations.has(p_anim);
}

void SpriteFrames::duplicate_animation(const StringName &p_from, const StringName &p_to) {
	ERR_FAIL_COND_MSG(!animations.has(p_from), vformat("SpriteFrames doesn't have animation '%s'.", p_from));
	ERR_FAIL_COND_MSG(animations.has(p_to), vformat("Animation '%s' already exists.", p_to));
	animations[p_to] = animations[p_from];
}

void SpriteFrames::remove_animation(const StringName &p_anim) {
	animations.erase(p_anim);
}

void SpriteFrames::rename_animation(const StringName &p_prev, const StringName &p_next) {
	ERR_FAIL_COND_MSG(!animations.has(p_prev), "SpriteFrames doesn't have animation '" + String(p_prev) + "'.");
	ERR_FAIL_COND_MSG(animations.has(p_next), "Animation '" + String(p_next) + "' already exists.");

	Anim anim = animations[p_prev];
	animations.erase(p_prev);
	animations[p_next] = anim;
}

void SpriteFrames::get_animation_list(List<StringName> *r_animations) const {
	for (const KeyValue<StringName, Anim> &E : animations) {
		r_animations->push_back(E.key);
	}
}

Vector<String> SpriteFrames::get_animation_names() const {
	Vector<String> names;
	for (const KeyValue<StringName, Anim> &E : animations) {
		names.push_back(E.key);
	}
	names.sort();
	return names;
}

void SpriteFrames::set_animation_speed(const StringName &p_anim, double p_fps) {
	ERR_FAIL_COND_MSG(p_fps < 0, "Animation speed cannot be negative (" + itos(p_fps) + ").");
	HashMap<StringName, Anim>::Iterator E = animations.find(p_anim);
	ERR_FAIL_COND_MSG(!E, "Animation '" + String(p_anim) + "' doesn't exist.");
	E->value.speed = p_fps;
}

double SpriteFrames::get_animation_speed(const StringName &p_anim) const {
	HashMap<StringName, Anim>::ConstIterator E = animations.find(p_anim);
	ERR_FAIL_COND_V_MSG(!E, 0, "Animation '" + String(p_anim) + "' doesn't exist.");
	return E->value.speed;
}

void SpriteFrames::set_animation_loop(const StringName &p_anim, bool p_loop) {
	HashMap<StringName, Anim>::Iterator E = animations.find(p_anim);
	ERR_FAIL_COND_MSG(!E, "Animation '" + String(p_anim) + "' doesn't exist.");
	E->value.loop = p_loop;
}

bool SpriteFrames::get_animation_loop(const StringName &p_anim) const {
	HashMap<StringName, Anim>::ConstIterator E = animations.find(p_anim);
	ERR_FAIL_COND_V_MSG(!E, false, "Animation '" + String(p_anim) + "' doesn't exist.");
	return E->value.loop;
}

Array SpriteFrames::_get_animations() const {
	Array anims;

	List<StringName> sorted_names;
	get_animation_list(&sorted_names);
	sorted_names.sort_custom<StringName::AlphCompare>();

	for (const StringName &anim_name : sorted_names) {
		const Anim &anim = animations[anim_name];
		Dictionary d;
		d["name"] = anim_name;
		d["speed"] = anim.speed;
		d["loop"] = anim.loop;
		Array frames;
		for (int i = 0; i < anim.frames.size(); i++) {
			Dictionary f;
			f["texture"] = anim.frames[i].texture;
			f["duration"] = anim.frames[i].duration;
			frames.push_back(f);
		}
		d["frames"] = frames;
		anims.push_back(d);
	}

	return anims;
}

void SpriteFrames::_set_animations(const Array &p_animations) {
	animations.clear();
	for (int i = 0; i < p_animations.size(); i++) {
		Dictionary d = p_animations[i];

		ERR_CONTINUE(!d.has("name"));
		ERR_CONTINUE(!d.has("speed"));
		ERR_CONTINUE(!d.has("loop"));
		ERR_CONTINUE(!d.has("frames"));

		Anim anim;
		anim.speed = d["speed"];
		anim.loop = d["loop"];
		Array frames = d["frames"];
		for (int j = 0; j < frames.size(); j++) {
#ifndef DISABLE_DEPRECATED
			// For compatibility.
			Ref<Resource> res = frames[j];
			if (res.is_valid()) {
				Frame frame = { res, 1.0 };
				anim.frames.push_back(frame);
				continue;
			}
#endif

			Dictionary f = frames[j];

			ERR_CONTINUE(!f.has("texture"));
			ERR_CONTINUE(!f.has("duration"));

			Frame frame = { f["texture"], MAX(SPRITE_FRAME_MINIMUM_DURATION, (float)f["duration"]) };
			anim.frames.push_back(frame);
		}

		animations[d["name"]] = anim;
	}
}

#ifdef TOOLS_ENABLED
void SpriteFrames::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0) {
		if (pf == "has_animation" || pf == "remove_animation" || pf == "rename_animation" ||
				pf == "set_animation_speed" || pf == "get_animation_speed" ||
				pf == "set_animation_loop" || pf == "get_animation_loop" ||
				pf == "add_frame" || pf == "set_frame" || pf == "remove_frame" ||
				pf == "get_frame_count" || pf == "get_frame_texture" || pf == "get_frame_duration" ||
				pf == "clear") {
			for (const String &E : get_animation_names()) {
				r_options->push_back(E.quote());
			}
		}
	}
	Resource::get_argument_options(p_function, p_idx, r_options);
}
#endif

void SpriteFrames::set_from_image_frames(const Ref<ImageFrames> &p_image_frames, const StringName &p_anim) {
	ERR_FAIL_COND_MSG(p_image_frames.is_null(), "Invalid image frames");

	HashMap<StringName, Anim>::Iterator E;
	if (!animations.has(p_anim)) {
		E = animations.insert(p_anim, Anim());
	} else {
		E = animations.find(p_anim);
	}

	Ref<Image> image;

	const int frame_count = p_image_frames->get_frame_count();

	E->value.frames.resize(frame_count);
	Frame *frame = E->value.frames.ptrw();

	for (int current_frame = 0; current_frame < frame_count; current_frame++, frame++) {
		float delay = MAX(SPRITE_FRAME_MINIMUM_DURATION, p_image_frames->get_frame_delay(current_frame));

		image = p_image_frames->get_frame_image(current_frame);
		*frame = { ImageTexture::create_from_image(image), delay };
	}

	int loop_count = p_image_frames->get_loop_count();
	set_animation_loop(p_anim, loop_count > 1 || loop_count == 0);

	emit_changed();
	notify_property_list_changed();
}

Ref<SpriteFrames> SpriteFrames::create_from_image_frames(const Ref<ImageFrames> &p_image_frames) {
	ERR_FAIL_COND_V_MSG(p_image_frames.is_null(), Ref<SpriteFrames>(), "Invalid image frames: null");

	Ref<SpriteFrames> sprite_frames;
	sprite_frames.instantiate();
	sprite_frames->set_from_image_frames(p_image_frames);
	return sprite_frames;
}

Ref<ImageFrames> SpriteFrames::make_image_frames(const StringName &p_anim) const {
	ERR_FAIL_COND_V_MSG(!animations.has(p_anim), Ref<ImageFrames>(), vformat("SpriteFrames doesn't have animation '%s'.", p_anim));

	Anim const &anim = animations.find(p_anim)->value;
	int frame_count = anim.frames.size();

	Ref<ImageFrames> image_frames;
	image_frames.instantiate();
	image_frames->set_frame_count(frame_count);
	image_frames->set_loop_count(anim.loop ? 0 : 1);

	const Frame *frame = anim.frames.ptr();

	for (int frame_index = 0; frame_index < frame_count; frame_index++, frame++) {
		ERR_CONTINUE(frame->texture.is_null());
		image_frames->set_frame_image(frame_index, frame->texture->get_image());
		image_frames->set_frame_delay(frame_index, frame->duration);
	}
	return image_frames;
}

void SpriteFrames::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_animation", "anim"), &SpriteFrames::add_animation);
	ClassDB::bind_method(D_METHOD("has_animation", "anim"), &SpriteFrames::has_animation);
	ClassDB::bind_method(D_METHOD("duplicate_animation", "anim_from", "anim_to"), &SpriteFrames::duplicate_animation);
	ClassDB::bind_method(D_METHOD("remove_animation", "anim"), &SpriteFrames::remove_animation);
	ClassDB::bind_method(D_METHOD("rename_animation", "anim", "newname"), &SpriteFrames::rename_animation);

	ClassDB::bind_method(D_METHOD("get_animation_names"), &SpriteFrames::get_animation_names);

	ClassDB::bind_method(D_METHOD("set_animation_speed", "anim", "fps"), &SpriteFrames::set_animation_speed);
	ClassDB::bind_method(D_METHOD("get_animation_speed", "anim"), &SpriteFrames::get_animation_speed);

	ClassDB::bind_method(D_METHOD("set_animation_loop", "anim", "loop"), &SpriteFrames::set_animation_loop);
	ClassDB::bind_method(D_METHOD("get_animation_loop", "anim"), &SpriteFrames::get_animation_loop);

	ClassDB::bind_method(D_METHOD("add_frame", "anim", "texture", "duration", "at_position"), &SpriteFrames::add_frame, DEFVAL(1.0), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("set_frame", "anim", "idx", "texture", "duration"), &SpriteFrames::set_frame, DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("remove_frame", "anim", "idx"), &SpriteFrames::remove_frame);

	ClassDB::bind_method(D_METHOD("get_frame_count", "anim"), &SpriteFrames::get_frame_count);
	ClassDB::bind_method(D_METHOD("get_frame_texture", "anim", "idx"), &SpriteFrames::get_frame_texture);
	ClassDB::bind_method(D_METHOD("get_frame_duration", "anim", "idx"), &SpriteFrames::get_frame_duration);

	ClassDB::bind_method(D_METHOD("clear", "anim"), &SpriteFrames::clear);
	ClassDB::bind_method(D_METHOD("clear_all"), &SpriteFrames::clear_all);

	ClassDB::bind_static_method("SpriteFrames", D_METHOD("create_from_image_frames", "image_frames"), &SpriteFrames::create_from_image_frames);
	ClassDB::bind_method(D_METHOD("set_from_image_frames", "image_frames", "anim"), &SpriteFrames::set_from_image_frames, DEFVAL(SceneStringName(default_)));
	ClassDB::bind_method(D_METHOD("make_image_frames", "anim"), &SpriteFrames::make_image_frames, DEFVAL(SceneStringName(default_)));

	// `animations` property is for serialization.

	ClassDB::bind_method(D_METHOD("_set_animations", "animations"), &SpriteFrames::_set_animations);
	ClassDB::bind_method(D_METHOD("_get_animations"), &SpriteFrames::_get_animations);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "animations", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_animations", "_get_animations");
}

SpriteFrames::SpriteFrames() {
	add_animation(SceneStringName(default_));
}

Ref<Resource> ResourceFormatLoaderSpriteFrames::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Ref<SpriteFrames> st;
	st.instantiate();
	Error err = st->load(p_path);
	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		return Ref<Resource>();
	}

	return st;
}

void ResourceFormatLoaderSpriteFrames::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("csfm");
}

bool ResourceFormatLoaderSpriteFrames::handles_type(const String &p_type) const {
	return p_type == "SpriteFrames";
}

String ResourceFormatLoaderSpriteFrames::get_resource_type(const String &p_path) const {
	return p_path.get_extension().to_lower() == "csfm" ? "SpriteFrames" : String();
}
