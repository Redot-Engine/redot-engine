#!/usr/bin/env python3
"""
Simple Unity YAML to Godot Scene Converter
Tests the YAML parsing logic without needing the full Redot import system
"""

import hashlib
import os
from pathlib import Path


def parse_unity_scene(yaml_content):
    """Parse Unity scene YAML and extract GameObjects"""
    lines = yaml_content.split("\n")

    game_objects = []
    current_name = "GameObject"
    in_game_object = False
    game_object_count = 0

    for line in lines:
        stripped = line.strip()

        # Detect GameObject sections
        if "--- !u!1" in stripped:
            in_game_object = True
            current_name = "GameObject"
            game_object_count += 1
            print(f"[Line] Detected GameObject #{game_object_count}")

        # Extract m_Name field
        if in_game_object and stripped.startswith("m_Name:"):
            # Extract name value after the colon
            parts = stripped.split(":", 1)
            if len(parts) == 2:
                name_value = parts[1].strip()

                # Handle both quoted ("name") and unquoted (name) formats
                if name_value.startswith('"') and name_value.endswith('"'):
                    name_value = name_value[1:-1]

                if name_value:  # Only use non-empty names
                    current_name = name_value
                    print(f"[Line] Found name: {current_name}")

        # Transition between sections
        if in_game_object and stripped.startswith("---") and game_object_count > 1:
            game_objects.append(current_name)
            print(f"[Line] Added object: {current_name}")
            in_game_object = False

    # Add last object
    if in_game_object and current_name:
        game_objects.append(current_name)
        print(f"[Line] Added last object: {current_name}")

    return game_objects


def create_tscn_content(root_name, game_objects):
    """Create a Godot .tscn file content with the parsed objects"""
    lines = ["RSRC", 'script_class = ""', '[node name="%s" type="Node3D"]' % root_name, ""]

    for i, obj_name in enumerate(game_objects):
        # Add child node
        lines.append(f'[node name="{obj_name}" parent="." instance=SubResource( 1 )]')
        lines.append("")

    return "\n".join(lines)


def convert_unity_file(unity_path, output_dir):
    """Convert a Unity .unity file to Godot .tscn"""

    # Read the unity file
    with open(unity_path, "r", encoding="utf-8") as f:
        yaml_content = f.read()

    # Parse objects
    print(f"\n=== Converting {os.path.basename(unity_path)} ===")
    game_objects = parse_unity_scene(yaml_content)

    # Generate output filename with hash (matching Redot's behavior)
    file_hash = hashlib.md5(yaml_content.encode()).hexdigest()
    base_name = Path(unity_path).stem
    output_filename = f"{base_name}.unity-{file_hash}.tscn"
    output_path = os.path.join(output_dir, output_filename)

    # Create directory if needed
    os.makedirs(output_dir, exist_ok=True)

    # Create tscn content
    tscn_content = create_tscn_content(base_name, game_objects)

    # Write the file
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(tscn_content)

    print(f"\n✓ Converted to: {output_filename}")
    print(f"✓ Total nodes: {len(game_objects) + 1} (including root)")
    print(f"✓ Output path: {output_path}")

    return output_path


# Main
if __name__ == "__main__":
    base_path = r"C:\Users\charl\Documents\redot_test_simple"
    unity_file = os.path.join(base_path, "Assets", "TestScene.unity")
    output_dir = os.path.join(base_path, ".godot", "imported", "Assets")

    if os.path.exists(unity_file):
        result = convert_unity_file(unity_file, output_dir)
        print("\n=== SUCCESS ===")
    else:
        print(f"ERROR: {unity_file} not found!")
