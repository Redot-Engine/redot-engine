#!/usr/bin/env python3
"""
Add missing transform extraction code to Unity importer.
"""


def add_transform_extraction():
    file_path = r"c:\Users\charl\Documents\redot-engine\editor\plugins\unity_package_importer.cpp"

    # Read the file
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()

    # Transform extraction code to insert
    transform_code_scene = """		}
		// Extract Transform position
		else if (in_transform && trimmed.begins_with("m_LocalPosition:")) {
			int colon = trimmed.find(":");
			String pos_yaml = trimmed.substr(colon + 1).strip_edges();
			current_position = _parse_vector3_from_yaml(pos_yaml);
			fileID_to_position[current_file_id] = current_position;
			print_verbose(vformat("Scene Transform: Position for '%s': (%f, %f, %f)", current_game_object_name, current_position.x, current_position.y, current_position.z));
		}
		// Extract Transform rotation
		else if (in_transform && trimmed.begins_with("m_LocalRotation:")) {
			int colon = trimmed.find(":");
			String rot_yaml = trimmed.substr(colon + 1).strip_edges();
			current_rotation = _parse_quaternion_from_yaml(rot_yaml);
			fileID_to_rotation[current_file_id] = current_rotation;
			print_verbose(vformat("Scene Transform: Rotation for '%s': (%f, %f, %f, %f)", current_game_object_name, current_rotation.x, current_rotation.y, current_rotation.z, current_rotation.w));
		}
		// Extract Transform scale
		else if (in_transform && trimmed.begins_with("m_LocalScale:")) {
			int colon = trimmed.find(":");
			String scale_yaml = trimmed.substr(colon + 1).strip_edges();
			current_scale = _parse_vector3_from_yaml(scale_yaml);
			fileID_to_scale[current_file_id] = current_scale;
			print_verbose(vformat("Scene Transform: Scale for '%s': (%f, %f, %f)", current_game_object_name, current_scale.x, current_scale.y, current_scale.z));
		}
		// Extract prefab GUID reference
		else if (trimmed.begins_with("guid:") && (in_game_object || in_transform)) {"""

    # Search pattern for convert_scene function
    search_pattern_scene = """		}
		// Extract prefab GUID reference
		else if (trimmed.begins_with("guid:") && (in_game_object || in_transform)) {"""

    # Count occurrences
    count = content.count(search_pattern_scene)
    print(f"Found {count} occurrences of the search pattern")

    # Only replace if we find exactly what we expect
    if count >= 2:
        # Replace first occurrence (convert_scene)
        new_content = content.replace(search_pattern_scene, transform_code_scene, 1)

        # For convert_prefab, we need different insertion point
        search_pattern_prefab = """		}
	}

	// Save last object
	if (in_game_object && !current_file_id.is_empty() && !current_game_object_name.is_empty()) {"""

        # Count how many times this appears after our first replacement
        parts = new_content.split(search_pattern_prefab)
        if len(parts) >= 2:
            # Insert transform code before the second occurrence's "Save last object"
            # But first we need to add it in the loop
            # Let's find the second m_Father pattern instead

            # Simpler approach: find both functions and patch separately
            lines = new_content.split("\n")
            output_lines = []
            in_convert_prefab = False
            added_to_prefab = False

            for i, line in enumerate(lines):
                output_lines.append(line)

                # Detect convert_prefab function
                if "Error UnityAssetConverter::convert_prefab" in line:
                    in_convert_prefab = True
                    added_to_prefab = False

                # Detect next function (end of convert_prefab)
                if (
                    in_convert_prefab
                    and i > 0
                    and lines[i - 1].strip().startswith("Error UnityAssetConverter::")
                    and "convert_prefab" not in line
                ):
                    in_convert_prefab = False

                # Add transform extraction in convert_prefab loop
                if in_convert_prefab and not added_to_prefab:
                    if 'm_Father:")) {' in line and i + 5 < len(lines):
                        # Check if we already added it
                        next_lines = "\n".join(lines[i + 1 : i + 10])
                        if "m_LocalPosition:" not in next_lines:
                            # Find the closing brace and add our code
                            j = i + 1
                            brace_count = 1
                            while j < len(lines) and brace_count > 0:
                                if "{" in lines[j]:
                                    brace_count += 1
                                if "}" in lines[j]:
                                    brace_count -= 1
                                    if brace_count == 0:
                                        # Found the closing brace of m_Father block
                                        output_lines.append(lines[j])
                                        # Now add transform extraction
                                        output_lines.extend(
                                            [
                                                "\t\t}",
                                                "\t\t// Extract Transform position",
                                                '\t\telse if (in_transform && trimmed.begins_with("m_LocalPosition:")) {',
                                                '\t\t\tint colon = trimmed.find(":");',
                                                "\t\t\tString pos_yaml = trimmed.substr(colon + 1).strip_edges();",
                                                "\t\t\tcurrent_position = _parse_vector3_from_yaml(pos_yaml);",
                                                "\t\t\tfileID_to_position[current_file_id] = current_position;",
                                                "\t\t\tprint_verbose(vformat(\"Prefab Transform: Position for '%s': (%f, %f, %f)\", current_game_object_name, current_position.x, current_position.y, current_position.z));",
                                                "\t\t}",
                                                "\t\t// Extract Transform rotation",
                                                '\t\telse if (in_transform && trimmed.begins_with("m_LocalRotation:")) {',
                                                '\t\t\tint colon = trimmed.find(":");',
                                                "\t\t\tString rot_yaml = trimmed.substr(colon + 1).strip_edges();",
                                                "\t\t\tcurrent_rotation = _parse_quaternion_from_yaml(rot_yaml);",
                                                "\t\t\tfileID_to_rotation[current_file_id] = current_rotation;",
                                                "\t\t\tprint_verbose(vformat(\"Prefab Transform: Rotation for '%s': (%f, %f, %f, %f)\", current_game_object_name, current_rotation.x, current_rotation.y, current_rotation.z, current_rotation.w));",
                                                "\t\t}",
                                                "\t\t// Extract Transform scale",
                                                '\t\telse if (in_transform && trimmed.begins_with("m_LocalScale:")) {',
                                                '\t\t\tint colon = trimmed.find(":");',
                                                "\t\t\tString scale_yaml = trimmed.substr(colon + 1).strip_edges();",
                                                "\t\t\tcurrent_scale = _parse_vector3_from_yaml(scale_yaml);",
                                                "\t\t\tfileID_to_scale[current_file_id] = current_scale;",
                                                "\t\t\tprint_verbose(vformat(\"Prefab Transform: Scale for '%s': (%f, %f, %f)\", current_game_object_name, current_scale.x, current_scale.y, current_scale.z));",
                                            ]
                                        )
                                        added_to_prefab = True
                                        break
                                else:
                                    output_lines.append(lines[j])
                                j += 1

            final_content = "\n".join(output_lines)

            # Write back
            with open(file_path, "w", encoding="utf-8") as f:
                f.write(final_content)

            print("✓ Successfully added transform extraction to convert_scene")
            if added_to_prefab:
                print("✓ Successfully added transform extraction to convert_prefab")
            else:
                print("✗ Could not add transform extraction to convert_prefab")
        else:
            print(f"Error: Expected 2+ parts after split, got {len(parts)}")
    else:
        print(f"Error: Expected 2+ occurrences, found {count}")


if __name__ == "__main__":
    add_transform_extraction()
