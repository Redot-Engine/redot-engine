/**************************************************************************/
/*  test_converter.cpp                                                    */
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

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Simplified version of the YAML parsing logic
void test_yaml_parsing() {
	std::string yaml_content = R"(%YAML 1.1
%TAG !u! tag:unity3d.com,2011:
--- !u!1 &5
GameObject:
  m_ObjectHideFlags: 0
  m_Name: Main Camera
  m_TagString: MainCamera
--- !u!4 &6
Transform:
  m_GameObject: {fileID: 5}
  m_LocalPosition: {x: 0, y: 1, z: -10}
--- !u!1 &7
GameObject:
  m_ObjectHideFlags: 0
  m_Name: Test Cube
  m_TagString: Untagged
--- !u!4 &8
Transform:
  m_GameObject: {fileID: 7}
  m_LocalPosition: {x: 0, y: 0, z: 0}
)";

	// Split into lines
	std::vector<std::string> lines;
	std::stringstream ss(yaml_content);
	std::string ss_line;
	while (std::getline(ss, ss_line)) {
		lines.push_back(ss_line);
	}

	// Parse with state machine
	std::string current_game_object_name = "GameObject";
	bool in_game_object = false;
	int game_object_count = 0;
	std::vector<std::string> found_objects;

	for (size_t i = 0; i < lines.size(); i++) {
		std::string current_line = lines[i];

		// Trim leading/trailing whitespace
		current_line.erase(0, current_line.find_first_not_of(" \t\n\r"));
		current_line.erase(current_line.find_last_not_of(" \t\n\r") + 1);

		// Detect gameObject sections (type ID 1)
		if (current_line.find("--- !u!1") != std::string::npos) {
			in_game_object = true;
			current_game_object_name = "GameObject";
			game_object_count++;
			std::cout << "Detected GameObject #" << game_object_count << std::endl;
		}

		// Extract m_Name from gameObjects
		if (in_game_object && current_line.find("m_Name:") == 0) {
			// Extract the name value
			size_t colon = current_line.find(":");
			std::string name_value = current_line.substr(colon + 1);

			// Trim
			name_value.erase(0, name_value.find_first_not_of(" \t\n\r"));
			name_value.erase(name_value.find_last_not_of(" \t\n\r") + 1);

			// Remove quotes
			if (name_value.front() == '"' && name_value.back() == '"') {
				name_value = name_value.substr(1, name_value.length() - 2);
			}

			if (!name_value.empty()) {
				current_game_object_name = name_value;
				std::cout << "  Found name: " << current_game_object_name << std::endl;
			}
		}

		// Create node when transitioning out of gameObject section
		if (in_game_object && current_line.find("---") == 0 && game_object_count > 1) {
			found_objects.push_back(current_game_object_name);
			std::cout << "  Created node: " << current_game_object_name << std::endl;
			in_game_object = false;
		}
	}

	// Add any last node
	if (in_game_object && !current_game_object_name.empty()) {
		found_objects.push_back(current_game_object_name);
		std::cout << "  Created last node: " << current_game_object_name << std::endl;
	}

	std::cout << "\nTotal nodes created: " << found_objects.size() + 1 << " (including root)" << std::endl;
	std::cout << "Objects found:" << std::endl;
	for (const auto &obj : found_objects) {
		std::cout << "  - " << obj << std::endl;
	}
}

int main() {
	std::cout << "Testing YAML parsing logic..." << std::endl;
	test_yaml_parsing();
	return 0;
}
