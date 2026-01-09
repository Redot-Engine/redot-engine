/**************************************************************************/
/*  mcp_tools.cpp                                                         */
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

#include "mcp_tools.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/version.h"
#include "scene/resources/packed_scene.h"

#include <functional>

MCPTools::MCPTools() {
}

MCPTools::~MCPTools() {
}

void MCPTools::_bind_methods() {
	// Tools are called internally, no need to bind to GDScript
}

// ============================================================================
// Path Utilities
// ============================================================================

String MCPTools::normalize_path(const String &p_path) {
	if (p_path.begins_with("res://")) {
		return p_path;
	}
	// Handle absolute paths within project
	String project_path = ProjectSettings::get_singleton()->get_resource_path();
	if (p_path.begins_with(project_path)) {
		return "res://" + p_path.substr(project_path.length()).lstrip("/");
	}
	// Assume relative to project root
	return "res://" + p_path.lstrip("/");
}

bool MCPTools::validate_path(const String &p_path) {
	String normalized = normalize_path(p_path);
	// Prevent path traversal attacks
	if (normalized.contains("..")) {
		return false;
	}
	if (!normalized.begins_with("res://")) {
		return false;
	}
	return true;
}

String MCPTools::get_absolute_path(const String &p_path) {
	String normalized = normalize_path(p_path);
	return ProjectSettings::get_singleton()->globalize_path(normalized);
}

// ============================================================================
// Tool Definitions
// ============================================================================

Array MCPTools::get_tool_definitions() {
	Array tools;

	// === File System Tools ===

	// read_file
	{
		Dictionary props;
		props["path"] = MCPSchemaBuilder::make_string_property("Path to file (e.g., 'res://scripts/player.gd' or 'scripts/player.gd')");

		Array required;
		required.push_back("path");

		Dictionary tool;
		tool["name"] = "read_file";
		tool["description"] = "Read the contents of a file in the project";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// write_file
	{
		Dictionary props;
		props["path"] = MCPSchemaBuilder::make_string_property("Path to file (e.g., 'res://scripts/player.gd')");
		props["content"] = MCPSchemaBuilder::make_string_property("Content to write to the file");

		Array required;
		required.push_back("path");
		required.push_back("content");

		Dictionary tool;
		tool["name"] = "write_file";
		tool["description"] = "Write content to a file in the project. Creates the file if it doesn't exist.";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// list_directory
	{
		Dictionary props;
		props["path"] = MCPSchemaBuilder::make_string_property("Path to directory (e.g., 'res://scripts' or 'scenes')");
		props["recursive"] = MCPSchemaBuilder::make_boolean_property("Whether to list subdirectories recursively (default: false)");

		Array required;
		required.push_back("path");

		Dictionary tool;
		tool["name"] = "list_directory";
		tool["description"] = "List contents of a directory in the project";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// file_exists
	{
		Dictionary props;
		props["path"] = MCPSchemaBuilder::make_string_property("Path to check (e.g., 'res://project.godot')");

		Array required;
		required.push_back("path");

		Dictionary tool;
		tool["name"] = "file_exists";
		tool["description"] = "Check if a file or directory exists in the project";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// === Scene Tools ===

	// create_scene
	{
		Dictionary props;
		props["path"] = MCPSchemaBuilder::make_string_property("Path for new scene file (e.g., 'scenes/player.tscn')");
		props["root_type"] = MCPSchemaBuilder::make_string_property("Node type for root (e.g., 'CharacterBody2D', 'Node3D', 'Control')");
		props["root_name"] = MCPSchemaBuilder::make_string_property("Name for root node (default: derived from filename)");

		Array required;
		required.push_back("path");
		required.push_back("root_type");

		Dictionary tool;
		tool["name"] = "create_scene";
		tool["description"] = "Create a new scene file with a specified root node type";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// get_scene_tree
	{
		Dictionary props;
		props["path"] = MCPSchemaBuilder::make_string_property("Path to scene file (e.g., 'res://scenes/player.tscn')");

		Array required;
		required.push_back("path");

		Dictionary tool;
		tool["name"] = "get_scene_tree";
		tool["description"] = "Get the node tree structure of a scene as JSON";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// add_node
	{
		Dictionary props;
		props["scene_path"] = MCPSchemaBuilder::make_string_property("Path to scene file");
		props["parent_path"] = MCPSchemaBuilder::make_string_property("Path to parent node (e.g., '.' for root, 'Player/Sprite2D')");
		props["node_type"] = MCPSchemaBuilder::make_string_property("Type of node to add (e.g., 'Sprite2D', 'CollisionShape2D')");
		props["node_name"] = MCPSchemaBuilder::make_string_property("Name for the new node");
		props["properties"] = MCPSchemaBuilder::make_object_property("Properties to set on the node (optional)");

		Array required;
		required.push_back("scene_path");
		required.push_back("node_type");
		required.push_back("node_name");

		Dictionary tool;
		tool["name"] = "add_node";
		tool["description"] = "Add a child node to an existing scene";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// remove_node
	{
		Dictionary props;
		props["scene_path"] = MCPSchemaBuilder::make_string_property("Path to scene file");
		props["node_path"] = MCPSchemaBuilder::make_string_property("Path to node to remove (e.g., 'Player/OldSprite')");

		Array required;
		required.push_back("scene_path");
		required.push_back("node_path");

		Dictionary tool;
		tool["name"] = "remove_node";
		tool["description"] = "Remove a node from a scene";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// set_node_property
	{
		Dictionary props;
		props["scene_path"] = MCPSchemaBuilder::make_string_property("Path to scene file");
		props["node_path"] = MCPSchemaBuilder::make_string_property("Path to node (e.g., '.' for root, 'Player/Sprite2D')");
		props["property"] = MCPSchemaBuilder::make_string_property("Property name to set");
		props["value"] = MCPSchemaBuilder::make_string_property("Value to set (will be parsed as appropriate type)");

		Array required;
		required.push_back("scene_path");
		required.push_back("node_path");
		required.push_back("property");
		required.push_back("value");

		Dictionary tool;
		tool["name"] = "set_node_property";
		tool["description"] = "Set a property on a node in a scene";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// === Project Tools ===

	// get_project_info
	{
		Dictionary props;
		// No required parameters

		Dictionary tool;
		tool["name"] = "get_project_info";
		tool["description"] = "Get information about the current Redot project including name, version, main scene, and structure";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props);
		tools.push_back(tool);
	}

	return tools;
}

// ============================================================================
// Tool Execution
// ============================================================================

MCPTools::ToolResult MCPTools::execute_tool(const String &p_name, const Dictionary &p_arguments) {
	if (p_name == "read_file") {
		return tool_read_file(p_arguments);
	} else if (p_name == "write_file") {
		return tool_write_file(p_arguments);
	} else if (p_name == "list_directory") {
		return tool_list_directory(p_arguments);
	} else if (p_name == "file_exists") {
		return tool_file_exists(p_arguments);
	} else if (p_name == "create_scene") {
		return tool_create_scene(p_arguments);
	} else if (p_name == "get_scene_tree") {
		return tool_get_scene_tree(p_arguments);
	} else if (p_name == "add_node") {
		return tool_add_node(p_arguments);
	} else if (p_name == "remove_node") {
		return tool_remove_node(p_arguments);
	} else if (p_name == "set_node_property") {
		return tool_set_node_property(p_arguments);
	} else if (p_name == "get_project_info") {
		return tool_get_project_info(p_arguments);
	}

	ToolResult result;
	result.set_error("Unknown tool: " + p_name);
	return result;
}

// ============================================================================
// File System Tools Implementation
// ============================================================================

MCPTools::ToolResult MCPTools::tool_read_file(const Dictionary &p_args) {
	ToolResult result;

	String path = p_args.get("path", "");
	if (path.is_empty()) {
		result.set_error("Missing required parameter: path");
		return result;
	}

	if (!validate_path(path)) {
		result.set_error("Invalid path: " + path);
		return result;
	}

	String normalized = normalize_path(path);

	Error err;
	Ref<FileAccess> file = FileAccess::open(normalized, FileAccess::READ, &err);
	if (err != OK || file.is_null()) {
		result.set_error("Failed to open file: " + normalized);
		return result;
	}

	String content = file->get_as_text();
	result.add_text(content);
	return result;
}

MCPTools::ToolResult MCPTools::tool_write_file(const Dictionary &p_args) {
	ToolResult result;

	String path = p_args.get("path", "");
	String content = p_args.get("content", "");

	if (path.is_empty()) {
		result.set_error("Missing required parameter: path");
		return result;
	}

	if (!validate_path(path)) {
		result.set_error("Invalid path: " + path);
		return result;
	}

	String normalized = normalize_path(path);

	// Ensure parent directory exists
	String dir_path = normalized.get_base_dir();
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (!dir->dir_exists(dir_path)) {
		Error mkdir_err = dir->make_dir_recursive(dir_path);
		if (mkdir_err != OK) {
			result.set_error("Failed to create directory: " + dir_path);
			return result;
		}
	}

	Error err;
	Ref<FileAccess> file = FileAccess::open(normalized, FileAccess::WRITE, &err);
	if (err != OK || file.is_null()) {
		result.set_error("Failed to open file for writing: " + normalized);
		return result;
	}

	file->store_string(content);
	file->flush();

	result.add_text("Successfully wrote to: " + normalized);
	return result;
}

MCPTools::ToolResult MCPTools::tool_list_directory(const Dictionary &p_args) {
	ToolResult result;

	String path = p_args.get("path", "res://");
	bool recursive = p_args.get("recursive", false);

	if (!validate_path(path)) {
		result.set_error("Invalid path: " + path);
		return result;
	}

	String normalized = normalize_path(path);

	Error err;
	Ref<DirAccess> dir = DirAccess::open(normalized, &err);
	if (err != OK || dir.is_null()) {
		result.set_error("Failed to open directory: " + normalized);
		return result;
	}

	Array entries;

	dir->list_dir_begin();
	String entry = dir->get_next();
	while (!entry.is_empty()) {
		if (entry != "." && entry != "..") {
			Dictionary entry_info;
			entry_info["name"] = entry;
			entry_info["is_directory"] = dir->current_is_dir();
			entry_info["path"] = normalized.path_join(entry);
			entries.push_back(entry_info);
		}
		entry = dir->get_next();
	}
	dir->list_dir_end();

	// Convert to JSON string for output
	String json_output = JSON::stringify(entries, "  ");
	result.add_text(json_output);
	return result;
}

MCPTools::ToolResult MCPTools::tool_file_exists(const Dictionary &p_args) {
	ToolResult result;

	String path = p_args.get("path", "");
	if (path.is_empty()) {
		result.set_error("Missing required parameter: path");
		return result;
	}

	if (!validate_path(path)) {
		result.set_error("Invalid path: " + path);
		return result;
	}

	String normalized = normalize_path(path);

	bool file_exists = FileAccess::exists(normalized);
	bool dir_exists = DirAccess::exists(normalized);

	Dictionary response;
	response["path"] = normalized;
	response["exists"] = file_exists || dir_exists;
	response["is_file"] = file_exists && !dir_exists;
	response["is_directory"] = dir_exists;

	result.add_text(JSON::stringify(response, "  "));
	return result;
}

// ============================================================================
// Scene Tools Implementation
// ============================================================================

MCPTools::ToolResult MCPTools::tool_create_scene(const Dictionary &p_args) {
	ToolResult result;

	String path = p_args.get("path", "");
	String root_type = p_args.get("root_type", "");
	String root_name = p_args.get("root_name", "");

	if (path.is_empty()) {
		result.set_error("Missing required parameter: path");
		return result;
	}
	if (root_type.is_empty()) {
		result.set_error("Missing required parameter: root_type");
		return result;
	}

	if (!validate_path(path)) {
		result.set_error("Invalid path: " + path);
		return result;
	}

	// Ensure path ends with .tscn
	String normalized = normalize_path(path);
	if (!normalized.ends_with(".tscn") && !normalized.ends_with(".scn")) {
		normalized += ".tscn";
	}

	// Check if class exists
	if (!ClassDB::class_exists(root_type)) {
		result.set_error("Unknown node type: " + root_type);
		return result;
	}

	// Check if it's a Node type
	if (!ClassDB::is_parent_class(root_type, "Node")) {
		result.set_error("Type must be a Node subclass: " + root_type);
		return result;
	}

	// Create root node
	Object *obj = ClassDB::instantiate(root_type);
	if (!obj) {
		result.set_error("Failed to instantiate node type: " + root_type);
		return result;
	}

	Node *root = Object::cast_to<Node>(obj);
	if (!root) {
		memdelete(obj);
		result.set_error("Failed to cast to Node: " + root_type);
		return result;
	}

	// Set root name
	if (root_name.is_empty()) {
		root_name = normalized.get_file().get_basename();
		// Convert to PascalCase if needed
		root_name = root_name.capitalize().replace(" ", "");
	}
	root->set_name(root_name);

	// Ensure parent directory exists
	String dir_path = normalized.get_base_dir();
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (!dir->dir_exists(dir_path)) {
		Error mkdir_err = dir->make_dir_recursive(dir_path);
		if (mkdir_err != OK) {
			memdelete(root);
			result.set_error("Failed to create directory: " + dir_path);
			return result;
		}
	}

	// Pack into scene
	Ref<PackedScene> scene;
	scene.instantiate();
	Error pack_err = scene->pack(root);
	memdelete(root);

	if (pack_err != OK) {
		result.set_error("Failed to pack scene");
		return result;
	}

	// Save scene
	Error save_err = ResourceSaver::save(scene, normalized);
	if (save_err != OK) {
		result.set_error("Failed to save scene to: " + normalized);
		return result;
	}

	result.add_text("Scene created successfully: " + normalized);
	return result;
}

MCPTools::ToolResult MCPTools::tool_get_scene_tree(const Dictionary &p_args) {
	ToolResult result;

	String path = p_args.get("path", "");
	if (path.is_empty()) {
		result.set_error("Missing required parameter: path");
		return result;
	}

	if (!validate_path(path)) {
		result.set_error("Invalid path: " + path);
		return result;
	}

	String normalized = normalize_path(path);

	// Load the scene
	Ref<PackedScene> scene = ResourceLoader::load(normalized, "PackedScene");
	if (scene.is_null()) {
		result.set_error("Failed to load scene: " + normalized);
		return result;
	}

	// Instantiate to get the tree structure
	Node *root = scene->instantiate();
	if (!root) {
		result.set_error("Failed to instantiate scene: " + normalized);
		return result;
	}

	// Build tree structure recursively
	std::function<Dictionary(Node *)> build_tree = [&build_tree](Node *node) -> Dictionary {
		Dictionary node_info;
		node_info["name"] = node->get_name();
		node_info["type"] = node->get_class();

		Array children;
		for (int i = 0; i < node->get_child_count(); i++) {
			children.push_back(build_tree(node->get_child(i)));
		}
		if (!children.is_empty()) {
			node_info["children"] = children;
		}

		return node_info;
	};

	Dictionary tree = build_tree(root);
	memdelete(root);

	result.add_text(JSON::stringify(tree, "  "));
	return result;
}

MCPTools::ToolResult MCPTools::tool_add_node(const Dictionary &p_args) {
	ToolResult result;

	String scene_path = p_args.get("scene_path", "");
	String parent_path = p_args.get("parent_path", ".");
	String node_type = p_args.get("node_type", "");
	String node_name = p_args.get("node_name", "");
	Dictionary properties = p_args.get("properties", Dictionary());

	if (scene_path.is_empty()) {
		result.set_error("Missing required parameter: scene_path");
		return result;
	}
	if (node_type.is_empty()) {
		result.set_error("Missing required parameter: node_type");
		return result;
	}
	if (node_name.is_empty()) {
		result.set_error("Missing required parameter: node_name");
		return result;
	}

	if (!validate_path(scene_path)) {
		result.set_error("Invalid path: " + scene_path);
		return result;
	}

	String normalized = normalize_path(scene_path);

	// Check if node type exists
	if (!ClassDB::class_exists(node_type)) {
		result.set_error("Unknown node type: " + node_type);
		return result;
	}

	// Load the scene
	Ref<PackedScene> scene = ResourceLoader::load(normalized, "PackedScene");
	if (scene.is_null()) {
		result.set_error("Failed to load scene: " + normalized);
		return result;
	}

	// Instantiate
	Node *root = scene->instantiate();
	if (!root) {
		result.set_error("Failed to instantiate scene: " + normalized);
		return result;
	}

	// Find parent node
	Node *parent = root;
	if (parent_path != "." && !parent_path.is_empty()) {
		parent = root->get_node_or_null(NodePath(parent_path));
		if (!parent) {
			memdelete(root);
			result.set_error("Parent node not found: " + parent_path);
			return result;
		}
	}

	// Create new node
	Object *obj = ClassDB::instantiate(node_type);
	if (!obj) {
		memdelete(root);
		result.set_error("Failed to instantiate node type: " + node_type);
		return result;
	}

	Node *new_node = Object::cast_to<Node>(obj);
	if (!new_node) {
		memdelete(obj);
		memdelete(root);
		result.set_error("Failed to cast to Node: " + node_type);
		return result;
	}

	new_node->set_name(node_name);

	// Set properties
	for (const Variant *key = properties.next(nullptr); key; key = properties.next(key)) {
		String prop_name = *key;
		Variant prop_value = properties[*key];
		new_node->set(prop_name, prop_value);
	}

	// Add to parent
	parent->add_child(new_node);
	new_node->set_owner(root);

	// Re-pack and save
	Ref<PackedScene> new_scene;
	new_scene.instantiate();
	Error pack_err = new_scene->pack(root);
	memdelete(root);

	if (pack_err != OK) {
		result.set_error("Failed to pack modified scene");
		return result;
	}

	Error save_err = ResourceSaver::save(new_scene, normalized);
	if (save_err != OK) {
		result.set_error("Failed to save scene: " + normalized);
		return result;
	}

	result.add_text("Added node '" + node_name + "' of type '" + node_type + "' to scene: " + normalized);
	return result;
}

MCPTools::ToolResult MCPTools::tool_remove_node(const Dictionary &p_args) {
	ToolResult result;

	String scene_path = p_args.get("scene_path", "");
	String node_path = p_args.get("node_path", "");

	if (scene_path.is_empty()) {
		result.set_error("Missing required parameter: scene_path");
		return result;
	}
	if (node_path.is_empty()) {
		result.set_error("Missing required parameter: node_path");
		return result;
	}

	if (!validate_path(scene_path)) {
		result.set_error("Invalid path: " + scene_path);
		return result;
	}

	String normalized = normalize_path(scene_path);

	// Load the scene
	Ref<PackedScene> scene = ResourceLoader::load(normalized, "PackedScene");
	if (scene.is_null()) {
		result.set_error("Failed to load scene: " + normalized);
		return result;
	}

	// Instantiate
	Node *root = scene->instantiate();
	if (!root) {
		result.set_error("Failed to instantiate scene: " + normalized);
		return result;
	}

	// Find node to remove
	Node *node_to_remove = root->get_node_or_null(NodePath(node_path));
	if (!node_to_remove) {
		memdelete(root);
		result.set_error("Node not found: " + node_path);
		return result;
	}

	// Can't remove root
	if (node_to_remove == root) {
		memdelete(root);
		result.set_error("Cannot remove root node");
		return result;
	}

	// Remove node
	node_to_remove->get_parent()->remove_child(node_to_remove);
	memdelete(node_to_remove);

	// Re-pack and save
	Ref<PackedScene> new_scene;
	new_scene.instantiate();
	Error pack_err = new_scene->pack(root);
	memdelete(root);

	if (pack_err != OK) {
		result.set_error("Failed to pack modified scene");
		return result;
	}

	Error save_err = ResourceSaver::save(new_scene, normalized);
	if (save_err != OK) {
		result.set_error("Failed to save scene: " + normalized);
		return result;
	}

	result.add_text("Removed node '" + node_path + "' from scene: " + normalized);
	return result;
}

MCPTools::ToolResult MCPTools::tool_set_node_property(const Dictionary &p_args) {
	ToolResult result;

	String scene_path = p_args.get("scene_path", "");
	String node_path = p_args.get("node_path", ".");
	String property = p_args.get("property", "");
	Variant value = p_args.get("value", Variant());

	if (scene_path.is_empty()) {
		result.set_error("Missing required parameter: scene_path");
		return result;
	}
	if (property.is_empty()) {
		result.set_error("Missing required parameter: property");
		return result;
	}

	if (!validate_path(scene_path)) {
		result.set_error("Invalid path: " + scene_path);
		return result;
	}

	String normalized = normalize_path(scene_path);

	// Load the scene
	Ref<PackedScene> scene = ResourceLoader::load(normalized, "PackedScene");
	if (scene.is_null()) {
		result.set_error("Failed to load scene: " + normalized);
		return result;
	}

	// Instantiate
	Node *root = scene->instantiate();
	if (!root) {
		result.set_error("Failed to instantiate scene: " + normalized);
		return result;
	}

	// Find node
	Node *target = root;
	if (node_path != "." && !node_path.is_empty()) {
		target = root->get_node_or_null(NodePath(node_path));
		if (!target) {
			memdelete(root);
			result.set_error("Node not found: " + node_path);
			return result;
		}
	}

	// Set property
	target->set(property, value);

	// Re-pack and save
	Ref<PackedScene> new_scene;
	new_scene.instantiate();
	Error pack_err = new_scene->pack(root);
	memdelete(root);

	if (pack_err != OK) {
		result.set_error("Failed to pack modified scene");
		return result;
	}

	Error save_err = ResourceSaver::save(new_scene, normalized);
	if (save_err != OK) {
		result.set_error("Failed to save scene: " + normalized);
		return result;
	}

	result.add_text("Set property '" + property + "' on node '" + node_path + "' in scene: " + normalized);
	return result;
}

// ============================================================================
// Project Tools Implementation
// ============================================================================

MCPTools::ToolResult MCPTools::tool_get_project_info(const Dictionary &p_args) {
	ToolResult result;

	Dictionary info;

	// Basic project info
	info["name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "Unnamed Project");
	info["description"] = ProjectSettings::get_singleton()->get_setting("application/config/description", "");
	info["main_scene"] = ProjectSettings::get_singleton()->get_setting("application/run/main_scene", "");

	// Version info
	Dictionary version;
	version["engine"] = "Redot Engine";
	version["major"] = VERSION_MAJOR;
	version["minor"] = VERSION_MINOR;
	version["patch"] = VERSION_PATCH;
	version["status"] = VERSION_STATUS;
	info["version"] = version;

	// Project path
	info["project_path"] = ProjectSettings::get_singleton()->get_resource_path();

	// Get autoloads
	Array autoloads;
	List<PropertyInfo> props;
	ProjectSettings::get_singleton()->get_property_list(&props);
	for (const PropertyInfo &prop : props) {
		if (prop.name.begins_with("autoload/")) {
			Dictionary autoload;
			autoload["name"] = prop.name.substr(9); // Remove "autoload/" prefix
			autoload["path"] = ProjectSettings::get_singleton()->get_setting(prop.name, "");
			autoloads.push_back(autoload);
		}
	}
	info["autoloads"] = autoloads;

	// Get input actions
	Array input_actions;
	for (const PropertyInfo &prop : props) {
		if (prop.name.begins_with("input/")) {
			input_actions.push_back(prop.name.substr(6)); // Remove "input/" prefix
		}
	}
	info["input_actions"] = input_actions;

	result.add_text(JSON::stringify(info, "  "));
	return result;
}
