/**************************************************************************/
/*  unity_material.cpp                                                    */
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

#include "unity_material.h"

#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "scene/resources/shader.h"

#include "unity_shader_converter.h"

namespace {

// Unity stores the saved properties as a sequence of one-entry maps:
//   m_Floats:
//   - _Metallic: 0
//   - _Glossiness: 0.5
// Flatten them into a single dictionary for easy lookup.
Dictionary _flatten_property_list(const Variant &p_value) {
	Dictionary result;
	if (p_value.get_type() != Variant::ARRAY) {
		return result;
	}
	const Array entries = p_value;
	for (int i = 0; i < entries.size(); i++) {
		if (entries[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary entry = entries[i];
		const Array keys = entry.keys();
		for (int k = 0; k < keys.size(); k++) {
			result[keys[k]] = entry[keys[k]];
		}
	}
	return result;
}

// Unity's built-in shaders are referenced by well-known GUIDs. They cannot be
// resolved through `.meta` files because they live in Unity's installation.
String _builtin_shader_kind(const String &p_guid, const String &p_name) {
	static const char *builtin_guids[] = {
		// Standard, Standard (Specular setup), Sprites/Default, UI/Default,
		// Unlit/Color, Unlit/Texture, Skybox/Procedural, Legacy Shaders/Diffuse.
		"46e2941b3c97d5644a54db28cbffd3d8",
		"47f1c1c3c8b0b4b4d9d4e2b1c0e5c3a7",
		"9d6b1b0e0e5b7e94dbf3f5b1e0e6a5e8",
		"0d5f1e2e3b4c5a6d7e8f901234567890",
	};
	for (const char *guid : builtin_guids) {
		if (p_guid == guid) {
			return "standard";
		}
	}
	if (p_name.ends_with("Sprites/Default") || p_name.ends_with("UI/Default") || p_name.ends_with("Unlit/Texture") || p_name.ends_with("Unlit/Color")) {
		return "unlit";
	}
	return "standard";
}

Ref<Texture2D> _texture_from_absolute(const String &p_absolute, const String &p_res_path) {
	Error err = OK;
	const Ref<Image> image = Image::load_from_file(p_absolute, &err);
	if (err != OK || image.is_null()) {
		return Ref<Texture2D>();
	}
	Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
	// Give the texture the path it will have in the destination project so
	// that saving a material references it instead of embedding the pixels.
	texture->set_path(p_res_path);
	return texture;
}

Color _color_from(const Variant &p_value, const Color &p_default = Color(1, 1, 1, 1)) {
	if (p_value.get_type() != Variant::DICTIONARY) {
		return p_default;
	}
	const Dictionary d = p_value;
	return Color(d.get("r", p_default.r), d.get("g", p_default.g), d.get("b", p_default.b), d.get("a", p_default.a));
}

} // namespace

Ref<Texture2D> UnityMaterialBuilder::load_texture(Ref<UnityProject> p_project, const String &p_guid, const String &p_target_prefix) {
	if (p_guid.is_empty() || p_project.is_null()) {
		return Ref<Texture2D>();
	}
	const String absolute = p_project->get_path_for_guid(p_guid);
	if (absolute.is_empty()) {
		return Ref<Texture2D>();
	}

	const String relative = p_project->localize(absolute);
	String res_path = relative;
	if (res_path.begins_with("res://")) {
		res_path = p_target_prefix + res_path.substr(6);
	}

	// In direct-open mode the Unity folder is the resource root, so the asset
	// loads through the regular importers. When baking, the source folder is
	// outside `res://`, so fall back to loading the image by absolute path.
	if (relative.begins_with("res://")) {
		const Ref<Resource> resource = ResourceLoader::load(relative);
		if (resource.is_valid() && Object::cast_to<Texture2D>(resource.ptr())) {
			return resource;
		}
	}
	return _texture_from_absolute(absolute, res_path);
}

Error UnityMaterialBuilder::convert_shader(Ref<UnityProject> p_project, const String &p_shader_path, String &r_code) {
	Error err = OK;
	const String source = FileAccess::get_file_as_string(p_shader_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot read Unity shader '%s'.", p_shader_path));
	return UnityShaderConverter::convert_shaderlab_to_godot(source, r_code);
}

Ref<Material> UnityMaterialBuilder::build_from_document(Ref<UnityProject> p_project, const String &p_material_path, const UnityYamlDocument &p_document, const String &p_target_prefix, Vector<String> *r_warnings) {
	const Dictionary body = p_document.body;
	const Dictionary saved = body.get("m_SavedProperties", Dictionary());

	const Dictionary floats = _flatten_property_list(saved.get("m_Floats", Variant()));
	const Dictionary colors = _flatten_property_list(saved.get("m_Colors", Variant()));
	const Dictionary tex_envs = _flatten_property_list(saved.get("m_TexEnvs", Variant()));

	const String shader_guid = UnityYaml::ref_guid(body.get("m_Shader", Variant()));
	const String shader_path = p_project.is_valid() ? p_project->get_path_for_guid(shader_guid) : String();
	String shader_name;
	const Variant shader_ref = body.get("m_Shader", Variant());
	if (shader_ref.get_type() == Variant::DICTIONARY) {
		const Dictionary shader_dict = shader_ref;
		shader_name = shader_dict.get("name", String());
	}

	String shader_code;
	const bool has_project_shader = !shader_path.is_empty() && shader_path.get_extension().to_lower() == "shader" &&
			convert_shader(p_project, shader_path, shader_code) == OK;

	if (has_project_shader) {
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code(shader_code);

		Ref<ShaderMaterial> material;
		material.instantiate();
		material->set_shader(shader);
		material->set_path(p_material_path);

		const Array float_keys = floats.keys();
		for (int i = 0; i < float_keys.size(); i++) {
			material->set_shader_parameter(float_keys[i], floats[float_keys[i]]);
		}
		const Array color_keys = colors.keys();
		for (int i = 0; i < color_keys.size(); i++) {
			material->set_shader_parameter(color_keys[i], _color_from(colors[color_keys[i]]));
		}
		const Array texture_keys = tex_envs.keys();
		for (int i = 0; i < texture_keys.size(); i++) {
			const Dictionary env = tex_envs[texture_keys[i]];
			const String guid = UnityYaml::ref_guid(env.get("m_Texture", Variant()));
			const Ref<Texture2D> texture = load_texture(p_project, guid, p_target_prefix);
			if (texture.is_valid()) {
				material->set_shader_parameter(texture_keys[i], texture);
			}
		}
		return material;
	}

	const String kind = _builtin_shader_kind(shader_guid, shader_name);
	const bool unlit = kind == "unlit";

	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_path(p_material_path);
	if (unlit) {
		material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	}

	// Albedo colour: the Standard shader uses `_Color`, URP/HDRP `_BaseColor`.
	Color albedo = Color(1, 1, 1, 1);
	if (colors.has("_BaseColor")) {
		albedo = _color_from(colors["_BaseColor"]);
	} else if (colors.has("_Color")) {
		albedo = _color_from(colors["_Color"]);
	}
	material->set_albedo(albedo);

	// Main texture (with tiling/offset).
	const String main_tex_keys[] = { "_BaseMap", "_MainTex" };
	for (const String &key : main_tex_keys) {
		if (!tex_envs.has(key)) {
			continue;
		}
		const Dictionary env = tex_envs[key];
		const String guid = UnityYaml::ref_guid(env.get("m_Texture", Variant()));
		const Ref<Texture2D> texture = load_texture(p_project, guid, p_target_prefix);
		if (texture.is_valid()) {
			material->set_albedo_texture(texture);
			const Dictionary scale = env.get("m_Scale", Dictionary());
			const Dictionary offset = env.get("m_Offset", Dictionary());
			if (!scale.is_empty()) {
				material->set_uv1_scale(Vector3(scale.get("x", 1.0), scale.get("y", 1.0), 1.0));
			}
			if (!offset.is_empty()) {
				material->set_uv1_offset(Vector3(offset.get("x", 0.0), offset.get("y", 0.0), 0.0));
			}
		}
		break;
	}

	if (floats.has("_Metallic")) {
		material->set_metallic(floats["_Metallic"]);
	} else if (floats.has("_MetallicValue")) {
		material->set_metallic(floats["_MetallicValue"]);
	}

	// Unity's `_Glossiness`/`_Smoothness` is the inverse of Godot roughness.
	if (floats.has("_Glossiness")) {
		material->set_roughness(1.0 - (double)floats["_Glossiness"]);
	} else if (floats.has("_Smoothness")) {
		material->set_roughness(1.0 - (double)floats["_Smoothness"]);
	}

	// Normal map.
	if (tex_envs.has("_BumpMap") || tex_envs.has("_NormalMap")) {
		const Variant bump_value = tex_envs.has("_BumpMap") ? tex_envs["_BumpMap"] : tex_envs["_NormalMap"];
		Dictionary env;
		if (bump_value.get_type() == Variant::DICTIONARY) {
			env = bump_value;
		}
		const Ref<Texture2D> texture = load_texture(p_project, UnityYaml::ref_guid(env.get("m_Texture", Variant())), p_target_prefix);
		if (texture.is_valid()) {
			material->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
			material->set_normal_texture(texture);
			if (floats.has("_BumpScale")) {
				material->set_normal_scale(floats["_BumpScale"]);
			}
		}
	}

	// Emission.
	const String keywords = body.get("m_ShaderKeywords", String());
	const bool emission_enabled = keywords.contains("_EMISSION") || (colors.has("_EmissionColor") && _color_from(colors["_EmissionColor"]).get_luminance() > 0.001);
	if (emission_enabled) {
		material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		if (colors.has("_EmissionColor")) {
			const Color emission = _color_from(colors["_EmissionColor"]);
			material->set_emission(emission);
			material->set_emission_energy_multiplier(MAX(1.0, emission.get_luminance() * 2.0));
		}
		if (tex_envs.has("_EmissionMap")) {
			const Dictionary env = tex_envs["_EmissionMap"];
			const Ref<Texture2D> texture = load_texture(p_project, UnityYaml::ref_guid(env.get("m_Texture", Variant())), p_target_prefix);
			if (texture.is_valid()) {
				material->set_emission_texture(texture);
			}
		}
	}

	// Rendering mode: `_Mode` 0 opaque, 1 cutout, 2 fade, 3 transparent.
	const int mode = floats.has("_Mode") ? (int)floats["_Mode"] : 0;
	if (mode == 1) {
		material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
		material->set_alpha_scissor_threshold(floats.has("_Cutoff") ? (float)floats["_Cutoff"] : 0.5f);
	} else if (mode == 2 || mode == 3) {
		material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	}
	if (albedo.a < 0.999) {
		material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	}

	// Culling: Unity `_Cull` 0 = off, 1 = front, 2 = back.
	if (floats.has("_Cull")) {
		const int cull = (int)floats["_Cull"];
		if (cull == 0) {
			material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		} else if (cull == 1) {
			material->set_cull_mode(BaseMaterial3D::CULL_FRONT);
		}
	}

	if (unlit && r_warnings) {
		r_warnings->push_back(vformat("Unlit shader '%s' was approximated with an unshaded StandardMaterial3D.", shader_name.is_empty() ? p_material_path : shader_name));
	}

	return material;
}

Ref<Material> UnityMaterialBuilder::build_from_file(Ref<UnityProject> p_project, const String &p_material_path, const String &p_target_prefix, Vector<String> *r_warnings) {
	Vector<UnityYamlDocument> documents;
	if (p_project.is_valid()) {
		p_project->load_documents(p_material_path, documents);
	} else {
		UnityYaml::parse_file(p_material_path, documents);
	}
	for (const UnityYamlDocument &document : documents) {
		if (document.type_name == "Material" || document.class_id == 21) {
			return build_from_document(p_project, p_material_path, document, p_target_prefix, r_warnings);
		}
	}
	if (r_warnings) {
		r_warnings->push_back(vformat("No material document found in '%s'.", p_material_path));
	}
	return Ref<Material>();
}

void UnityMaterialBuilder::clear_cache() {
	// Textures are cached by the ResourceLoader; nothing else is retained here.
}
