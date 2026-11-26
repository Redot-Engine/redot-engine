/**************************************************************************/
/*  unity_scene.cpp                                                       */
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

#include "unity_scene.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/templates/hash_set.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/collision_shape_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/rigid_body_3d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/animation_player.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/mesh.h"

#include "../import/engine_importer.h"
#include "unity_material.h"
#include "unity_script.h"

#ifdef MODULE_GDSCRIPT_ENABLED
#include "modules/gdscript/gdscript.h"
#endif

void _find_first_mesh(Node *p_node, MeshInstance3D *&r_found);

namespace {

constexpr int UNITY_CLASS_GAMEOBJECT = 1;
constexpr int UNITY_CLASS_TRANSFORM = 4;
constexpr int UNITY_CLASS_CAMERA = 20;
constexpr int UNITY_CLASS_MATERIAL = 21;
constexpr int UNITY_CLASS_MESHRENDERER = 23;
constexpr int UNITY_CLASS_MESHFILTER = 33;
constexpr int UNITY_CLASS_RIGIDBODY = 54;
constexpr int UNITY_CLASS_MESHCOLLIDER = 64;
constexpr int UNITY_CLASS_BOXCOLLIDER = 65;
constexpr int UNITY_CLASS_AUDIOLISTENER = 81;
constexpr int UNITY_CLASS_AUDIOSOURCE = 82;
constexpr int UNITY_CLASS_ANIMATOR = 95;
constexpr int UNITY_CLASS_LIGHT = 108;
constexpr int UNITY_CLASS_MONOBEHAVIOUR = 114;
constexpr int UNITY_CLASS_SPHERECOLLIDER = 135;
constexpr int UNITY_CLASS_CAPSULECOLLIDER = 136;
constexpr int UNITY_CLASS_SPRITERENDERER = 212;
constexpr int UNITY_CLASS_RECTTRANSFORM = 224;
constexpr int UNITY_CLASS_PREFABINSTANCE = 1001;

struct DocumentSet {
	Vector<UnityYamlDocument> documents;
	HashMap<int64_t, int> by_id;

	void build_index() {
		by_id.clear();
		for (int i = 0; i < documents.size(); i++) {
			by_id[documents[i].file_id] = i;
		}
	}

	const UnityYamlDocument *find(int64_t p_id) const {
		const int *index = by_id.getptr(p_id);
		return index ? &documents[*index] : nullptr;
	}

	const UnityYamlDocument *find_class(int64_t p_game_object, int p_class_id) const {
		for (const UnityYamlDocument &doc : documents) {
			if (doc.class_id != p_class_id) {
				continue;
			}
			const Variant go = doc.body.get("m_GameObject", Variant());
			if (UnityYaml::ref_file_id(go) == p_game_object) {
				return &doc;
			}
		}
		return nullptr;
	}
};

struct PendingReference {
	Node *node = nullptr;
	String field;
	int64_t target = 0;
};

struct BuildContext {
	Ref<UnityProject> project;
	UnitySceneOptions options;
	UnitySceneResult *result = nullptr;
	Node *root = nullptr;
	int prefab_depth = 0;

	HashMap<String, Ref<Material>> material_cache;
	HashMap<String, Ref<Mesh>> mesh_cache;
	HashMap<String, Ref<GDScript>> script_cache;
	HashMap<String, String> script_class_cache;
	HashMap<String, String> script_base_cache;
	HashSet<String> prefab_stack;
	HashSet<String> warned;

	void warn(const String &p_message, const String &p_path = String()) {
		const String key = p_message + "|" + p_path;
		if (warned.has(key)) {
			return;
		}
		warned.insert(key);
		result->warnings.push_back(p_path.is_empty() ? p_message : vformat("%s (%s)", p_message, p_path));
	}
};

String _sanitize_name(const String &p_name) {
	return ProjectImportUtils::sanitize_node_name(p_name);
}

bool _inherits_or_is(const String &p_class, const String &p_base) {
	return p_class == p_base || ClassDB::is_parent_class(p_class, p_base);
}

String _unique_name(Node *p_parent, const String &p_name) {
	const String base = _sanitize_name(p_name);
	if (!p_parent) {
		return base;
	}
	String candidate = base;
	int suffix = 2;
	while (p_parent->has_node(NodePath(candidate))) {
		candidate = vformat("%s%d", base, suffix++);
	}
	return candidate;
}

Vector3 _unity_position(const Dictionary &p_body, const String &p_key, const Vector3 &p_default = Vector3()) {
	Vector3 value = UnityYaml::get_vector3(p_body, p_key, p_default);
	value.x = -value.x;
	return value;
}

Quaternion _unity_rotation(const Dictionary &p_body, const String &p_key) {
	Quaternion value = UnityYaml::get_quaternion(p_body, p_key, Quaternion());
	value.y = -value.y;
	value.z = -value.z;
	return value;
}

Quaternion _local_flip() {
	return Quaternion(Vector3(0, 1, 0), Math_PI);
}

Variant _convert_serialized_value(const Variant &p_value, const DocumentSet &p_docs, const String &p_field, Node *p_node, Vector<PendingReference> &r_pending) {
	switch (p_value.get_type()) {
		case Variant::DICTIONARY: {
			const Dictionary d = p_value;
			if (d.has("fileID")) {
				const int64_t id = UnityYaml::ref_file_id(d);
				const String guid = UnityYaml::ref_guid(d);
				if (guid.is_empty() && id != 0) {
					PendingReference pending;
					pending.node = p_node;
					pending.field = p_field;
					pending.target = id;
					r_pending.push_back(pending);
				}
				return Variant();
			}
			if (d.has("r") && d.has("g") && d.has("b")) {
				return Color(d.get("r", 0.0), d.get("g", 0.0), d.get("b", 0.0), d.get("a", 1.0));
			}
			if (d.has("w")) {
				return Quaternion(d.get("x", 0.0), d.get("y", 0.0), d.get("z", 0.0), d.get("w", 0.0));
			}
			if (d.has("z")) {
				return Vector3(d.get("x", 0.0), d.get("y", 0.0), d.get("z", 0.0));
			}
			if (d.has("y")) {
				return Vector2(d.get("x", 0.0), d.get("y", 0.0));
			}
			return p_value;
		}
		case Variant::ARRAY: {
			const Array array = p_value;
			Array converted;
			for (int i = 0; i < array.size(); i++) {
				converted.push_back(_convert_serialized_value(array[i], p_docs, p_field, p_node, r_pending));
			}
			return converted;
		}
		default:
			return p_value;
	}
}

String _script_file_name(const String &p_class_name, const String &p_guid) {
	const String base = p_class_name.is_empty() ? String("UnityScript") : p_class_name;
	return vformat("%s_%s.gd", base, p_guid.substr(0, MIN(8, p_guid.length())));
}

Ref<GDScript> _get_script(BuildContext &ctx, const String &p_guid, String &r_base_type) {
	r_base_type = String();
	if (p_guid.is_empty() || ctx.project.is_null()) {
		return Ref<GDScript>();
	}

	if (const Ref<GDScript> *cached = ctx.script_cache.getptr(p_guid)) {
		const String *base = ctx.script_base_cache.getptr(p_guid);
		if (base) {
			r_base_type = *base;
		}
		return *cached;
	}

	const String path = ctx.project->get_path_for_guid(p_guid);
	if (path.is_empty() || path.get_extension().to_lower() != "cs") {
		return Ref<GDScript>();
	}
	if (ctx.options.script_dir.is_empty() || ctx.options.script_res_dir.is_empty()) {
		return Ref<GDScript>();
	}

	Error read_err = OK;
	const String source = FileAccess::get_file_as_string(path, &read_err);
	if (read_err != OK) {
		ctx.warn(vformat("Could not read C# script '%s'.", path), path);
		return Ref<GDScript>();
	}

	const UnityScriptConverter::Result converted = UnityScriptConverter::convert(source, path.get_file().get_basename());
	String code = converted.code;
	String base_type = converted.base_type;

	// Rewrite constructs that only make sense inside the original project:
	// unknown base classes become Node3D and `class_name` is dropped to avoid
	// global name collisions between imported projects.
	Vector<String> lines = code.split("\n");
	String rewritten;
	for (const String &line : lines) {
		const String trimmed = line.strip_edges();
		if (trimmed.begins_with("class_name ")) {
			continue;
		}
		if (trimmed.begins_with("extends ")) {
			const String declared = trimmed.substr(8).strip_edges();
			if (!declared.is_empty() && (!ClassDB::class_exists(declared) || !_inherits_or_is(declared, "Node")) && !declared.begins_with("res://")) {
				base_type = "Node3D";
				rewritten += "extends Node3D\n";
				continue;
			}
		}
		rewritten += line + "\n";
	}
	code = rewritten;
	if (!ClassDB::class_exists(base_type) || !_inherits_or_is(base_type, "Node")) {
		base_type = "Node3D";
	}
	r_base_type = base_type;

	Ref<GDScript> script;
#ifdef MODULE_GDSCRIPT_ENABLED
	const String file_name = _script_file_name(converted.class_name, p_guid);
	const String absolute = ctx.options.script_dir.path_join(file_name);
	if (ProjectImportUtils::write_text(absolute, code) != OK) {
		ctx.warn(vformat("Could not write converted script for '%s'.", path), path);
		return Ref<GDScript>();
	}
	const String res_path = ctx.options.script_res_dir.path_join(file_name);

	script.instantiate();
	script->set_path(res_path);
	script->set_source_code(code);
	if (script->reload(true) != OK) {
		ctx.warn(vformat("Converted script '%s' has errors; the node was created without it.", res_path), path);
		script = Ref<GDScript>();
	}
#else
	ctx.warn("GDScript support is disabled; C# scripts are not converted.");
#endif

	ctx.script_cache[p_guid] = script;
	ctx.script_class_cache[p_guid] = converted.class_name;
	ctx.script_base_cache[p_guid] = base_type;
	if (script.is_valid()) {
		ctx.result->script_count++;
	}
	return script;
}

Ref<Material> _get_material(BuildContext &ctx, const String &p_guid) {
	if (p_guid.is_empty() || ctx.project.is_null()) {
		return Ref<Material>();
	}
	if (const Ref<Material> *cached = ctx.material_cache.getptr(p_guid)) {
		return *cached;
	}
	const String path = ctx.project->get_path_for_guid(p_guid);
	Ref<Material> material;
	if (!path.is_empty() && path.get_extension().to_lower() == "mat") {
		material = UnityMaterialBuilder::build_from_file(ctx.project, path, ctx.options.target_prefix, &ctx.result->warnings);
		if (material.is_valid()) {
			material->set_path(ctx.project->localize(path));
		}
	}
	ctx.material_cache[p_guid] = material;
	return material;
}

Ref<Mesh> _builtin_mesh(int64_t p_file_id) {
	switch (p_file_id) {
		case 10202: { // Cube.
			Ref<BoxMesh> mesh;
			mesh.instantiate();
			return mesh;
		}
		case 10206: { // Capsule.
			Ref<CapsuleMesh> mesh;
			mesh.instantiate();
			mesh->set_radius(0.5);
			mesh->set_height(2.0);
			return mesh;
		}
		case 10207: { // Sphere.
			Ref<SphereMesh> mesh;
			mesh.instantiate();
			mesh->set_radius(0.5);
			mesh->set_height(1.0);
			return mesh;
		}
		case 10208: { // Cylinder.
			Ref<CylinderMesh> mesh;
			mesh.instantiate();
			mesh->set_top_radius(0.5);
			mesh->set_bottom_radius(0.5);
			mesh->set_height(2.0);
			return mesh;
		}
		case 10209: { // Plane (10x10 units).
			Ref<PlaneMesh> mesh;
			mesh.instantiate();
			mesh->set_size(Vector2(10, 10));
			return mesh;
		}
		case 10210: { // Quad (1x1 unit).
			Ref<QuadMesh> mesh;
			mesh.instantiate();
			mesh->set_size(Vector2(1, 1));
			return mesh;
		}
		default:
			return Ref<Mesh>();
	}
}

Ref<Mesh> _get_mesh(BuildContext &ctx, const Variant &p_reference) {
	const int64_t file_id = UnityYaml::ref_file_id(p_reference);
	const String guid = UnityYaml::ref_guid(p_reference);

	if (guid.is_empty() || guid == "0000000000000000e000000000000000") {
		Ref<Mesh> mesh = _builtin_mesh(file_id);
		if (mesh.is_valid()) {
			return mesh;
		}
	}

	if (guid.is_empty() || ctx.project.is_null()) {
		return Ref<Mesh>();
	}
	if (const Ref<Mesh> *cached = ctx.mesh_cache.getptr(guid)) {
		return *cached;
	}

	const String path = ctx.project->get_path_for_guid(guid);
	Ref<Mesh> mesh;
	if (!path.is_empty()) {
		const String extension = path.get_extension().to_lower();
		if (extension == "fbx" || extension == "obj" || extension == "glb" || extension == "gltf" || extension == "blend" || extension == "dae") {
			// Model files are imported by the editor's import pipeline. That
			// only works while the Unity folder is the active resource root
			// (direct-open). During baking the model is copied and imported
			// when the destination project is opened.
			const bool direct_open = ProjectSettings::get_singleton()->get_foreign_project_root() == ctx.project->get_root();
			if (!direct_open) {
				ctx.warn(vformat("Model '%s' will be imported when the destination project is opened; the scene references it by path.", path), path);
				ctx.mesh_cache[guid] = Ref<Mesh>();
				return Ref<Mesh>();
			}
			const Ref<Resource> resource = ResourceLoader::load(ctx.project->localize(path));
			if (resource.is_valid()) {
				if (Object::cast_to<Mesh>(resource.ptr())) {
					mesh = resource;
				} else if (Object::cast_to<PackedScene>(resource.ptr())) {
					Ref<PackedScene> scene = resource;
					Node *instance = scene->instantiate();
					if (instance) {
						MeshInstance3D *found = Object::cast_to<MeshInstance3D>(instance);
						if (!found) {
							_find_first_mesh(instance, found);
						}
						if (found) {
							mesh = found->get_mesh();
						}
						memdelete(instance);
					}
				}
			}
			if (mesh.is_null()) {
				ctx.warn(vformat("Mesh '%s' could not be loaded. Open the project once so the model is imported, then reload the scene.", ctx.project->localize(path)), path);
			}
		}
	}
	ctx.mesh_cache[guid] = mesh;
	return mesh;
}

} // namespace

// Recursively searches for the first MeshInstance3D. Kept outside the anonymous
// namespace helpers above because it is used before its definition.
void _find_first_mesh(Node *p_node, MeshInstance3D *&r_found);

void _find_first_mesh(Node *p_node, MeshInstance3D *&r_found) {
	if (r_found) {
		return;
	}
	MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(p_node);
	if (mesh_instance && mesh_instance->get_mesh().is_valid()) {
		r_found = mesh_instance;
		return;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_find_first_mesh(p_node->get_child(i), r_found);
		if (r_found) {
			return;
		}
	}
}

namespace {

class SceneWalker {
	BuildContext &ctx;
	DocumentSet &docs;
	HashMap<int64_t, Node *> nodes;
	Vector<PendingReference> pending;

public:
	SceneWalker(BuildContext &p_ctx, DocumentSet &p_docs) :
			ctx(p_ctx), docs(p_docs) {}

	const HashMap<int64_t, Node *> &get_nodes() const { return nodes; }
	Vector<PendingReference> &get_pending() { return pending; }

	void build_roots(Node *p_parent) {
		// Prefer Unity's explicit root order when present.
		Vector<int64_t> root_ids;
		for (const UnityYamlDocument &doc : docs.documents) {
			if (doc.class_id != 1660057539) {
				continue;
			}
			const Variant roots_value = doc.body.get("m_Roots", Variant());
			if (roots_value.get_type() != Variant::ARRAY) {
				continue;
			}
			const Array roots = roots_value;
			for (int i = 0; i < roots.size(); i++) {
				root_ids.push_back(UnityYaml::ref_file_id(roots[i]));
			}
		}
		if (root_ids.is_empty()) {
			for (const UnityYamlDocument &doc : docs.documents) {
				if (doc.class_id != UNITY_CLASS_TRANSFORM && doc.class_id != UNITY_CLASS_RECTTRANSFORM) {
					continue;
				}
				if (UnityYaml::ref_file_id(doc.body.get("m_Father", Variant())) == 0) {
					root_ids.push_back(doc.file_id);
				}
			}
		}

		for (int64_t transform_id : root_ids) {
			const UnityYamlDocument *transform = docs.find(transform_id);
			if (!transform) {
				continue;
			}
			const int64_t game_object = UnityYaml::ref_file_id(transform->body.get("m_GameObject", Variant()));
			if (game_object == 0) {
				continue;
			}
			build_game_object(game_object, p_parent);
		}
	}

	void build_prefab_instances(Node *p_parent) {
		for (const UnityYamlDocument &doc : docs.documents) {
			if (doc.class_id != UNITY_CLASS_PREFABINSTANCE) {
				continue;
			}
			const Dictionary modification = doc.body.get("m_Modification", Dictionary());
			const int64_t parent_transform = UnityYaml::ref_file_id(modification.get("m_TransformParent", Variant()));
			Node *parent = p_parent;
			if (parent_transform != 0) {
				Node **found = nodes.getptr(parent_transform);
				if (found) {
					parent = *found;
				} else {
					ctx.warn("Prefab instance parent could not be resolved; the prefab was attached to the scene root.");
				}
			}
			const String source_guid = UnityYaml::ref_guid(doc.body.get("m_SourcePrefab", Variant()));
			build_prefab(source_guid, parent, modification);
		}
	}

	void resolve_pending() {
		for (const PendingReference &reference : pending) {
			if (!reference.node) {
				continue;
			}
			Node **target = nodes.getptr(reference.target);
			if (!target || !*target) {
				continue;
			}
			if (reference.node == *target) {
				continue;
			}
			reference.node->set(reference.field, reference.node->get_path_to(*target));
		}
		pending.clear();
	}

	Node *build_game_object(int64_t p_game_object_id, Node *p_parent) {
		const UnityYamlDocument *game_object = docs.find(p_game_object_id);
		if (!game_object) {
			return nullptr;
		}

		const Dictionary body = game_object->body;
		const String object_name = body.get("m_Name", String("GameObject"));
		const bool active = UnityYaml::get_bool(body, "m_IsActive", true);

		// Discover the Transform and components first: they decide the node type.
		const UnityYamlDocument *transform = docs.find_class(p_game_object_id, UNITY_CLASS_TRANSFORM);
		if (!transform) {
			transform = docs.find_class(p_game_object_id, UNITY_CLASS_RECTTRANSFORM);
		}

		Vector<const UnityYamlDocument *> components;
		const Variant component_value = body.get("m_Component", Variant());
		if (component_value.get_type() == Variant::ARRAY) {
			const Array component_array = component_value;
			for (int i = 0; i < component_array.size(); i++) {
				if (component_array[i].get_type() != Variant::DICTIONARY) {
					continue;
				}
				const Dictionary entry = component_array[i];
				const int64_t id = UnityYaml::ref_file_id(entry.get("component", Variant()));
				const UnityYamlDocument *component = docs.find(id);
				if (component) {
					components.push_back(component);
				}
			}
		}

		String node_type = transform && transform->class_id == UNITY_CLASS_RECTTRANSFORM ? "Control" : "Node3D";
		bool is_ui = node_type == "Control";
		bool has_rigidbody = false;
		Ref<GDScript> attached_script;
		String script_base;

		for (const UnityYamlDocument *component : components) {
			if (component->class_id == UNITY_CLASS_RIGIDBODY) {
				has_rigidbody = true;
			} else if (component->class_id == UNITY_CLASS_MONOBEHAVIOUR) {
				const String guid = UnityYaml::ref_guid(component->body.get("m_Script", Variant()));
				if (guid.is_empty()) {
					continue;
				}
				String base;
				Ref<GDScript> script = _get_script(ctx, guid, base);
				if (script.is_valid()) {
					attached_script = script;
					script_base = base;
					if (ClassDB::class_exists(base) && _inherits_or_is(base, "Node")) {
						node_type = base;
						is_ui = _inherits_or_is(base, "Control");
					}
				}
			}
		}

		if (has_rigidbody) {
			node_type = "RigidBody3D";
		}
		if (!ClassDB::class_exists(node_type)) {
			node_type = is_ui ? "Control" : "Node3D";
		}

		Object *instance = ClassDB::instantiate(node_type);
		Node *node = Object::cast_to<Node>(instance);
		if (!node) {
			if (instance) {
				memdelete(instance);
			}
			node = is_ui ? static_cast<Node *>(memnew(Control)) : static_cast<Node *>(memnew(Node3D));
		}

		node->set_name(_unique_name(p_parent, object_name));
		p_parent->add_child(node);
		node->set_owner(ctx.root);
		ctx.result->node_count++;

		nodes[p_game_object_id] = node;
		if (transform) {
			nodes[transform->file_id] = node;
			apply_transform(node, transform->body, is_ui);
		}

		if (!active && ctx.options.include_inactive) {
			Node3D *node_3d = Object::cast_to<Node3D>(node);
			if (node_3d) {
				node_3d->set_visible(false);
			} else {
				Control *control = Object::cast_to<Control>(node);
				if (control) {
					control->set_visible(false);
				}
			}
		}

		if (attached_script.is_valid()) {
			node->set_script(attached_script);
		}

		// Components.
		const UnityYamlDocument *mesh_filter = nullptr;
		const UnityYamlDocument *mesh_renderer = nullptr;
		for (const UnityYamlDocument *component : components) {
			switch (component->class_id) {
				case UNITY_CLASS_MESHFILTER:
					mesh_filter = component;
					break;
				case UNITY_CLASS_MESHRENDERER:
					mesh_renderer = component;
					break;
				case UNITY_CLASS_MONOBEHAVIOUR:
					apply_monobehaviour(ctx, node, component, pending);
					break;
				case UNITY_CLASS_CAMERA:
					add_camera(ctx, node, component);
					break;
				case UNITY_CLASS_LIGHT:
					add_light(ctx, node, component);
					break;
				case UNITY_CLASS_BOXCOLLIDER:
				case UNITY_CLASS_SPHERECOLLIDER:
				case UNITY_CLASS_CAPSULECOLLIDER:
					add_collider(ctx, node, component);
					break;
				case UNITY_CLASS_MESHCOLLIDER:
					ctx.warn(vformat("MeshCollider on '%s' is not supported yet; add a collision shape manually.", object_name));
					break;
				case UNITY_CLASS_RIGIDBODY:
					configure_rigidbody(node, component);
					break;
				case UNITY_CLASS_AUDIOSOURCE:
					add_audio_source(ctx, node, component);
					break;
				case UNITY_CLASS_SPRITERENDERER:
					add_sprite(ctx, node, component);
					break;
				case UNITY_CLASS_ANIMATOR:
					add_animator(ctx, node, component);
					break;
				case UNITY_CLASS_AUDIOLISTENER:
					break; // The active Camera3D acts as the listener.
				case UNITY_CLASS_TRANSFORM:
				case UNITY_CLASS_RECTTRANSFORM:
					break;
				default:
					break;
			}
		}

		if (mesh_filter || mesh_renderer) {
			add_mesh(ctx, node, mesh_filter, mesh_renderer, object_name);
		}

		// Children.
		if (transform) {
			const Variant children_value = transform->body.get("m_Children", Variant());
			if (children_value.get_type() == Variant::ARRAY) {
				const Array children = children_value;
				for (int i = 0; i < children.size(); i++) {
					const int64_t child_transform = UnityYaml::ref_file_id(children[i]);
					const UnityYamlDocument *child_doc = docs.find(child_transform);
					if (!child_doc) {
						continue;
					}
					const int64_t child_game_object = UnityYaml::ref_file_id(child_doc->body.get("m_GameObject", Variant()));
					if (child_game_object != 0) {
						build_game_object(child_game_object, node);
					}
				}
			}
		}

		return node;
	}

private:
	void apply_transform(Node *p_node, const Dictionary &p_body, bool p_is_ui) {
		if (p_is_ui) {
			Control *control = Object::cast_to<Control>(p_node);
			if (!control) {
				return;
			}
			Vector2 anchored = Vector2(
					UnityYaml::get_vector3(p_body, "m_AnchoredPosition", Vector3()).x,
					UnityYaml::get_vector3(p_body, "m_AnchoredPosition", Vector3()).y);
			const Vector2 size = Vector2(
					UnityYaml::get_vector3(p_body, "m_SizeDelta", Vector3()).x,
					UnityYaml::get_vector3(p_body, "m_SizeDelta", Vector3()).y);
			control->set_position(Vector2(-anchored.x, -anchored.y));
			control->set_size(size.abs());
			const Vector3 scale = UnityYaml::get_vector3(p_body, "m_LocalScale", Vector3.ONE);
			control->set_scale(Vector2(scale.x, scale.y));
			return;
		}

		Node3D *node = Object::cast_to<Node3D>(p_node);
		if (!node) {
			return;
		}
		node->set_position(_unity_position(p_body, "m_LocalPosition"));
		node->set_quaternion(_unity_rotation(p_body, "m_LocalRotation"));
		node->set_scale(UnityYaml::get_vector3(p_body, "m_LocalScale", Vector3.ONE));
	}

	void apply_monobehaviour(BuildContext &p_ctx, Node *p_node, const UnityYamlDocument *p_component, Vector<PendingReference> &r_pending) {
		const Dictionary body = p_component->body;
		const Array keys = body.keys();
		for (int i = 0; i < keys.size(); i++) {
			const String key = keys[i];
			if (key.begins_with("m_") || key == "serializedVersion") {
				continue;
			}
			Variant value = _convert_serialized_value(body[key], docs, key, p_node, r_pending);
			if (value.get_type() != Variant::NIL) {
				p_node->set(key, value);
			}
		}
	}

	void add_camera(BuildContext &p_ctx, Node *p_parent, const UnityYamlDocument *p_component) {
		const Dictionary body = p_component->body;
		Camera3D *camera = memnew(Camera3D);
		camera->set_name(_unique_name(p_parent, "Camera"));
		p_parent->add_child(camera);
		camera->set_owner(ctx.root);
		camera->set_quaternion(_local_flip());

		camera->set_fov(UnityYaml::get_number(body, "m_FieldOfView", 60.0));
		camera->set_near(UnityYaml::get_number(body, "m_NearClipPlane", 0.3));
		camera->set_far(UnityYaml::get_number(body, "m_FarClipPlane", 1000.0));
		const bool orthographic = UnityYaml::get_bool(body, "m_Orthographic", false);
		camera->set_orthogonal(orthographic);
		if (orthographic) {
			camera->set_size(UnityYaml::get_number(body, "m_OrthographicSize", 5.0) * 2.0);
		}
		if (UnityYaml::get_bool(body, "m_Enabled", true)) {
			camera->set_current(true);
		}
		ctx.result->node_count++;
	}

	void add_light(BuildContext &p_ctx, Node *p_parent, const UnityYamlDocument *p_component) {
		const Dictionary body = p_component->body;
		const int type = (int)UnityYaml::get_int(body, "m_Type", 1);

		Light3D *light = nullptr;
		if (type == 0) {
			SpotLight3D *spot = memnew(SpotLight3D);
			spot->set_param(Light3D::PARAM_SPOT_ANGLE, UnityYaml::get_number(body, "m_SpotAngle", 30.0) * 0.5);
			spot->set_param(Light3D::PARAM_SPOT_ATTENUATION, 0.5);
			light = spot;
		} else if (type == 1) {
			light = memnew(DirectionalLight3D);
		} else {
			light = memnew(OmniLight3D);
			if (type == 3) {
				ctx.warn("Unity area lights are approximated with an OmniLight3D.");
			}
		}

		light->set_name(_unique_name(p_parent, "Light"));
		p_parent->add_child(light);
		light->set_owner(ctx.root);
		light->set_quaternion(_local_flip());

		light->set_color(UnityYaml::get_color(body, "m_Color", Color(1, 1, 1)));
		light->set_param(Light3D::PARAM_ENERGY, MAX(0.0, UnityYaml::get_number(body, "m_Intensity", 1.0)));
		light->set_param(Light3D::PARAM_RANGE, UnityYaml::get_number(body, "m_Range", 10.0));
		const Variant shadows = body.get("m_Shadows", Variant());
		bool shadow_enabled = true;
		if (shadows.get_type() == Variant::DICTIONARY) {
			const Dictionary shadow_dict = shadows;
			shadow_enabled = UnityYaml::get_int(shadow_dict, "m_Type", 1) != 0;
		}
		light->set_shadow(shadow_enabled);
		ctx.result->node_count++;
	}

	void add_collider(BuildContext &p_ctx, Node *p_parent, const UnityYamlDocument *p_component) {
		const Dictionary body = p_component->body;
		CollisionShape3D *shape_node = memnew(CollisionShape3D);
		shape_node->set_name(_unique_name(p_parent, "Collider"));
		p_parent->add_child(shape_node);
		shape_node->set_owner(ctx.root);
		shape_node->set_position(_unity_position(body, "m_Center"));

		if (p_component->class_id == UNITY_CLASS_BOXCOLLIDER) {
			Ref<BoxShape3D> shape;
			shape.instantiate();
			shape->set_size(UnityYaml::get_vector3(body, "m_Size", Vector3::ONE));
			shape_node->set_shape(shape);
		} else if (p_component->class_id == UNITY_CLASS_SPHERECOLLIDER) {
			Ref<SphereShape3D> shape;
			shape.instantiate();
			shape->set_radius(UnityYaml::get_number(body, "m_Radius", 0.5));
			shape_node->set_shape(shape);
		} else {
			Ref<CapsuleShape3D> shape;
			shape.instantiate();
			shape->set_radius(UnityYaml::get_number(body, "m_Radius", 0.5));
			shape->set_height(UnityYaml::get_number(body, "m_Height", 2.0));
			shape_node->set_shape(shape);
			const int direction = (int)UnityYaml::get_int(body, "m_Direction", 1);
			if (direction == 0) {
				shape_node->set_rotation(Vector3(0, 0, Math_PI * 0.5));
			} else if (direction == 2) {
				shape_node->set_rotation(Vector3(Math_PI * 0.5, 0, 0));
			}
		}
		ctx.result->node_count++;
	}

	void configure_rigidbody(Node *p_node, const UnityYamlDocument *p_component) {
		RigidBody3D *body = Object::cast_to<RigidBody3D>(p_node);
		if (!body) {
			return;
		}
		const Dictionary data = p_component->body;
		body->set_mass(UnityYaml::get_number(data, "m_Mass", 1.0));
		body->set_linear_damp(UnityYaml::get_number(data, "m_Drag", 0.0));
		body->set_angular_damp(UnityYaml::get_number(data, "m_AngularDrag", 0.05));
		body->set_gravity_scale(UnityYaml::get_bool(data, "m_UseGravity", true) ? 1.0 : 0.0);
		if (UnityYaml::get_bool(data, "m_IsKinematic", false)) {
			body->set_freeze_enabled(true);
			body->set_freeze_mode(RigidBody3D::FREEZE_MODE_KINEMATIC);
		}
	}

	void add_mesh(BuildContext &p_ctx, Node *p_parent, const UnityYamlDocument *p_filter, const UnityYamlDocument *p_renderer, const String &p_object_name) {
		Ref<Mesh> mesh;
		if (p_filter) {
			mesh = _get_mesh(p_ctx, p_filter->body.get("m_Mesh", Variant()));
		}

		MeshInstance3D *instance = memnew(MeshInstance3D);
		instance->set_name(_unique_name(p_parent, "Mesh"));
		p_parent->add_child(instance);
		instance->set_owner(ctx.root);
		instance->set_mesh(mesh);

		if (p_renderer) {
			const Variant materials_value = p_renderer->body.get("m_Materials", Variant());
			if (materials_value.get_type() == Variant::ARRAY) {
				const Array materials = materials_value;
				for (int i = 0; i < materials.size(); i++) {
					const String guid = UnityYaml::ref_guid(materials[i]);
					const Ref<Material> material = _get_material(p_ctx, guid);
					if (material.is_valid()) {
						instance->set_surface_override_material(i, material);
					}
				}
			}
			if (!UnityYaml::get_bool(p_renderer->body, "m_Enabled", true)) {
				instance->set_visible(false);
			}
		}
		if (mesh.is_null()) {
			const Variant mesh_reference = p_filter ? p_filter->body.get("m_Mesh", Variant()) : Variant();
			const String mesh_guid = UnityYaml::ref_guid(mesh_reference);
			if (!mesh_guid.is_empty() && p_ctx.project.is_valid()) {
				const String mesh_path = p_ctx.project->get_path_for_guid(mesh_guid);
				if (!mesh_path.is_empty()) {
					instance->set_meta("unity_mesh_path", p_ctx.project->localize(mesh_path));
				}
			}
			ctx.warn(vformat("'%s' has a MeshRenderer but no loadable mesh.", p_object_name));
		}
		ctx.result->node_count++;
	}

	void add_audio_source(BuildContext &p_ctx, Node *p_parent, const UnityYamlDocument *p_component) {
		const Dictionary body = p_component->body;
		const String guid = UnityYaml::ref_guid(body.get("m_audioClip", Variant()));
		Ref<AudioStream> stream;
		if (!guid.is_empty() && ctx.project.is_valid()) {
			const String path = ctx.project->get_path_for_guid(guid);
			if (!path.is_empty()) {
				const Ref<Resource> resource = ResourceLoader::load(ctx.project->localize(path));
				if (resource.is_valid()) {
					stream = resource;
				}
			}
		}

		const double spatial = UnityYaml::get_number(body, "m_SpatialBlend", 1.0);
		Node *player = nullptr;
		if (spatial < 0.5) {
			AudioStreamPlayer *audio = memnew(AudioStreamPlayer);
			audio->set_stream(stream);
			audio->set_volume_db(Math::linear_to_db(UnityYaml::get_number(body, "m_Volume", 1.0)));
			audio->set_autoplay(UnityYaml::get_bool(body, "m_PlayOnAwake", true));
			player = audio;
		} else {
			AudioStreamPlayer3D *audio = memnew(AudioStreamPlayer3D);
			audio->set_stream(stream);
			audio->set_volume_db(Math::linear_to_db(UnityYaml::get_number(body, "m_Volume", 1.0)));
			audio->set_max_distance(UnityYaml::get_number(body, "m_MaxDistance", 500.0));
			audio->set_autoplay(UnityYaml::get_bool(body, "m_PlayOnAwake", true));
			player = audio;
		}
		player->set_name(_unique_name(p_parent, "Audio"));
		p_parent->add_child(player);
		player->set_owner(ctx.root);
		if (stream.is_null() && !guid.is_empty()) {
			ctx.warn("An AudioSource clip could not be loaded.");
		}
		ctx.result->node_count++;
	}

	void add_sprite(BuildContext &p_ctx, Node *p_parent, const UnityYamlDocument *p_component) {
		const Dictionary body = p_component->body;
		Sprite3D *sprite = memnew(Sprite3D);
		sprite->set_name(_unique_name(p_parent, "Sprite"));
		p_parent->add_child(sprite);
		sprite->set_owner(ctx.root);

		const String guid = UnityYaml::ref_guid(body.get("m_Sprite", Variant()));
		if (!guid.is_empty() && ctx.project.is_valid()) {
			const String path = ctx.project->get_path_for_guid(guid);
			if (!path.is_empty()) {
				const Ref<Resource> resource = ResourceLoader::load(ctx.project->localize(path));
				if (resource.is_valid() && Object::cast_to<Texture2D>(resource.ptr())) {
					sprite->set_texture(resource);
				}
			}
		}
		sprite->set_modulate(UnityYaml::get_color(body, "m_Color", Color(1, 1, 1)));
		sprite->set_flip_h(UnityYaml::get_bool(body, "m_FlipX", false));
		sprite->set_flip_v(UnityYaml::get_bool(body, "m_FlipY", false));
		sprite->set_pixel_size(0.01);
		ctx.result->node_count++;
	}

	void add_animator(BuildContext &p_ctx, Node *p_parent, const UnityYamlDocument *p_component) {
		const String guid = UnityYaml::ref_guid(p_component->body.get("m_Controller", Variant()));
		if (!guid.is_empty()) {
			ctx.warn("Unity Animator controllers are not converted; add an AnimationPlayer and clips manually.");
		}
		AnimationPlayer *player = memnew(AnimationPlayer);
		player->set_name(_unique_name(p_parent, "AnimationPlayer"));
		p_parent->add_child(player);
		player->set_owner(ctx.root);
		ctx.result->node_count++;
	}

	void build_prefab(const String &p_guid, Node *p_parent, const Dictionary &p_modification) {
		if (p_guid.is_empty() || ctx.project.is_null()) {
			ctx.warn("Prefab instance without a source prefab was skipped.");
			return;
		}
		if (ctx.prefab_depth >= ctx.options.max_prefab_depth || ctx.prefab_stack.has(p_guid)) {
			ctx.warn("Recursive prefab reference was cut off.");
			return;
		}
		const String path = ctx.project->get_path_for_guid(p_guid);
		if (path.is_empty()) {
			ctx.warn("Prefab source could not be resolved.", p_guid);
			return;
		}

		DocumentSet prefab_docs;
		if (ctx.project->load_documents(path, prefab_docs.documents) != OK || prefab_docs.documents.is_empty()) {
			ctx.warn(vformat("Prefab '%s' could not be parsed.", path), path);
			return;
		}
		prefab_docs.build_index();

		ctx.prefab_stack.insert(p_guid);
		ctx.prefab_depth++;

		SceneWalker walker(ctx, prefab_docs);
		walker.build_roots(p_parent);
		walker.build_prefab_instances(p_parent);
		walker.resolve_pending();

		apply_modifications(walker.get_nodes(), p_modification);

		ctx.prefab_depth--;
		ctx.prefab_stack.erase(p_guid);
	}

	void apply_modifications(const HashMap<int64_t, Node *> &p_nodes, const Dictionary &p_modification) {
		const Variant modifications_value = p_modification.get("m_Modifications", Variant());
		if (modifications_value.get_type() != Variant::ARRAY) {
			return;
		}
		const Array modifications = modifications_value;
		for (int i = 0; i < modifications.size(); i++) {
			if (modifications[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary entry = modifications[i];
			const int64_t target_id = UnityYaml::ref_file_id(entry.get("target", Variant()));
			Node **found = p_nodes.getptr(target_id);
			if (!found || !*found) {
				continue;
			}
			Node *node = *found;
			const String property = entry.get("propertyPath", String());
			const Variant value = entry.get("value", Variant());
			const Variant object_reference = entry.get("objectReference", Variant());

			if (property == "m_Name") {
				node->set_name(_unique_name(node->get_parent(), value));
			} else if (property == "m_IsActive" || property == "m_Enabled") {
				Node3D *node_3d = Object::cast_to<Node3D>(node);
				if (node_3d) {
					node_3d->set_visible(value.get_type() == Variant::BOOL ? (bool)value : ((String)value).to_int() != 0);
				}
			} else if (property.begins_with("m_LocalPosition.")) {
				Node3D *node_3d = Object::cast_to<Node3D>(node);
				if (node_3d) {
					Vector3 position = node_3d->get_position();
					const String axis = property.substr(property.length() - 1);
					const double component = value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT ? (double)value : (String)value.to_float();
					if (axis == "x") {
						position.x = -component;
					} else if (axis == "y") {
						position.y = component;
					} else if (axis == "z") {
						position.z = component;
					}
					node_3d->set_position(position);
				}
			} else if (property.begins_with("m_LocalScale.")) {
				Node3D *node_3d = Object::cast_to<Node3D>(node);
				if (node_3d) {
					Vector3 scale = node_3d->get_scale();
					const String axis = property.substr(property.length() - 1);
					const double component = value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT ? (double)value : (String)value.to_float();
					if (axis == "x") {
						scale.x = component;
					} else if (axis == "y") {
						scale.y = component;
					} else if (axis == "z") {
						scale.z = component;
					}
					node_3d->set_scale(scale);
				}
			} else if (property.begins_with("m_LocalRotation.")) {
				Node3D *node_3d = Object::cast_to<Node3D>(node);
				if (node_3d) {
					Quaternion rotation = node_3d->get_quaternion();
					const String axis = property.substr(property.length() - 1);
					double component = value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT ? (double)value : (String)value.to_float();
					if (axis == "y" || axis == "z") {
						component = -component;
					}
					if (axis == "x") {
						rotation.x = component;
					} else if (axis == "y") {
						rotation.y = component;
					} else if (axis == "z") {
						rotation.z = component;
					} else if (axis == "w") {
						rotation.w = component;
					}
					node_3d->set_quaternion(rotation);
				}
			} else if (property.begins_with("m_Materials.Array.data[")) {
				MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(node);
				if (mesh_instance) {
					const int begin = property.find("[") + 1;
					const int end = property.find("]", begin);
					const int index = property.substr(begin, end - begin).to_int();
					const String guid = UnityYaml::ref_guid(object_reference);
					const Ref<Material> material = _get_material(ctx, guid);
					if (material.is_valid()) {
						mesh_instance->set_surface_override_material(index, material);
					}
				}
			}
		}
	}
};

} // namespace

String UnitySceneBuilder::convert_script_source(const String &p_csharp, const String &p_expected_class, String *r_class_name) {
	const UnityScriptConverter::Result converted = UnityScriptConverter::convert(p_csharp, p_expected_class);
	if (r_class_name) {
		*r_class_name = converted.class_name;
	}
	Vector<String> lines = converted.code.split("\n");
	String code;
	for (const String &line : lines) {
		if (line.strip_edges().begins_with("class_name ")) {
			continue;
		}
		code += line + "\n";
	}
	return code;
}

Error UnitySceneBuilder::build(Ref<UnityProject> p_project, const String &p_scene_path, const UnitySceneOptions &p_options, UnitySceneResult &r_result) {
	ERR_FAIL_COND_V(p_project.is_null(), ERR_INVALID_PARAMETER);
	r_result = UnitySceneResult();

	DocumentSet docs;
	const Error parse_error = p_project->load_documents(p_scene_path, docs.documents);
	ERR_FAIL_COND_V_MSG(parse_error != OK || docs.documents.is_empty(), ERR_FILE_CORRUPT, vformat("Cannot parse Unity scene '%s'.", p_scene_path));
	docs.build_index();

	BuildContext ctx;
	ctx.project = p_project;
	ctx.options = p_options;
	ctx.result = &r_result;

	Node3D *root = memnew(Node3D);
	root->set_name(_sanitize_name(p_scene_path.get_file().get_basename()));
	ctx.root = root;

	SceneWalker walker(ctx, docs);
	walker.build_roots(root);
	walker.build_prefab_instances(root);
	walker.resolve_pending();

	if (root->get_child_count() == 0) {
		ctx.warn(vformat("Scene '%s' contains no GameObjects.", p_scene_path), p_scene_path);
	}

	Ref<PackedScene> packed;
	packed.instantiate();
	const Error pack_error = packed->pack(root);
	memdelete(root);
	ERR_FAIL_COND_V_MSG(pack_error != OK, pack_error, vformat("Cannot pack converted scene '%s'.", p_scene_path));

	r_result.scene = packed;
	return OK;
}

void UnitySceneBuilder::clear_cache() {
	// Caches live in BuildContext, which is per-scene; nothing global to clear.
}
