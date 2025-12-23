# Unity Terminology Mapping for Redot Engine

This document provides a comprehensive mapping between Godot/Redot terminology and Unity equivalents. This can be used for implementing a "Unity-friendly mode" in the editor or for documentation purposes.

## Overview

The mapping is organized into the following categories:

### 1. Core Types
Fundamental object types in the engine:
- **Node** → **GameObject** - The basic building block
- **PackedScene** → **Prefab** - Reusable scene templates
- **Resource** → **Asset** - Serializable data containers
- **Script** → **Script** (same, but .gd → .cs)
- **Material**, **Shader**, **Texture** → Same names

### 2. Editor Windows & Panels
UI panels and docks in the editor:
- **Scene** / **SceneTree** → **Hierarchy** - Object tree view
- **Inspector** → **Inspector** - Property editor (same name!)
- **FileSystem** → **Project** - Asset browser
- **Import** → **Import Settings** 
- **Output** / **Debugger** → **Console**
- **Animation** → **Animation** window
- **History** → **History** panel

### 3. Common Operations
Frequently used operations:
- **instantiate** / **instance** → **Instantiate** - Create instance from prefab
- **add_child** → **SetParent** / **AddChild** - Parent an object
- **queue_free** → **Destroy** - Delete object
- **get_node** → **Find** / **GetComponent** - Find objects/components
- **duplicate** → **Duplicate** / **Instantiate**
- **reparent** → **SetParent** - Change parent
- **attach_script** → **Add Component** - Attach script to object

### 4. File Extensions
Project file types:
- **.tscn** → **.prefab** / **.unity** - Scene files
- **.tres** → **.asset** - Resource files
- **.res** → **.asset** - Binary resource files
- **.gd** → **.cs** - Script files (GDScript → C#)
- **.gdshader** → **.shader** - Shader files
- **.import** → **.meta** - Import metadata
- **project.godot** → **ProjectSettings.asset** - Project config

### 5. UI Strings & Actions
Common editor UI strings:
- **"Add Child Node"** → **"Create Empty"** / **"Add Component"**
- **"Instantiate Scene"** → **"Instantiate Prefab"**
- **"Attach Script"** → **"Add Script Component"**
- **"Editable Children"** → **"Unpack Prefab"**
- **"Make Unique"** → **"Break Prefab Instance"**
- **"Save Branch as Scene"** → **"Create Prefab"**
- **"Copy Node Path"** → **"Copy Reference"**

### 6. Lifecycle Methods
Script lifecycle callbacks:
- **_ready()** → **Start()** - Called when node enters tree
- **_process(delta)** → **Update()** - Called every frame
- **_physics_process(delta)** → **FixedUpdate()** - Called every physics frame
- **_input(event)** → **Input system** - Handle input
- **_enter_tree()** → **Awake()** - Node added to tree
- **_exit_tree()** → **OnDestroy()** - Node removed from tree

### 7. Properties & Concepts
Programming concepts:
- **@export** → **[SerializeField]** / **public** - Expose to inspector
- **@tool** → **[ExecuteInEditMode]** - Run in editor
- **signals** → **Events** / **UnityEvents** - Event system
- **groups** → **Tags** / **Layers** - Object categorization
- **owner** → **Prefab root** - Scene ownership
- **preload** → **Resources.Load** (compile-time) - Asset loading
- **class_name** → **Class declaration** - Named classes

### 8. Common Node Types
Frequently used node types:

#### 3D Nodes
- **Node3D** → **GameObject** (3D empty)
- **Camera3D** → **Camera**
- **DirectionalLight3D** → **Directional Light**
- **MeshInstance3D** → **Mesh Filter + Mesh Renderer**
- **CollisionShape3D** → **Collider**
- **RigidBody3D** → **Rigidbody**
- **CharacterBody3D** → **Character Controller**
- **Area3D** → **Trigger**

#### 2D Nodes
- **Node2D** → **GameObject** (2D empty)
- **Sprite2D** → **Sprite Renderer**
- **Camera2D** → **Camera** (2D)
- **CollisionShape2D** → **Collider2D**
- **RigidBody2D** → **Rigidbody2D**
- **TileMap** → **Tilemap**

#### UI Nodes
- **Control** → **UI GameObject** (RectTransform)
- **Label** → **Text** / **TextMeshPro**
- **Button** → **Button**
- **LineEdit** → **Input Field**
- **TextureRect** → **Image**
- **Panel** → **Panel**
- **ScrollContainer** → **Scroll View**

### 9. Resource Handling
Resource/Asset management:
- **ResourceLoader.load()** → **Resources.Load()**
- **ResourceSaver.save()** → **AssetDatabase.CreateAsset()**
- **Resource.duplicate()** → **Instantiate** (for ScriptableObjects)
- **make_unique** → **Break prefab connection**

### 10. Project Structure
File organization:
- **res://** → **Assets/** - Project resource root
- **user://** → **Application.persistentDataPath** - User data
- **addons/** → **Plugins/** or **Packages/** - Extensions
- **.godot/** → **Library/** - Build cache (hidden)

## Usage Examples

### For UI Replacement
```cpp
// When displaying UI strings in "Unity mode":
String term = "Add Child Node";
if (unity_mode_enabled) {
    term = terminology_map.get("Add Child Node", "Create Empty");
}
button->set_text(term);
```

### For Documentation
When generating help text or tooltips for Unity developers:
```cpp
String help = "Click to instantiate a scene";
if (unity_mode_enabled) {
    help = "Click to instantiate a prefab";
}
```

### Context-Sensitive Mapping
Some terms have different meanings in different contexts:
- **Scene** can mean:
  - Scene file (.tscn) → Unity Scene (.unity) or Prefab (.prefab)
  - Scene dock → Hierarchy window
  - Scene in memory → Active scene

## Implementation Notes

### Priority Mappings
The most important UI elements to translate:
1. **Main editor panels** (Scene → Hierarchy, FileSystem → Project)
2. **Common operations** (Instantiate, Add Child, Destroy)
3. **File extensions** (shown in file dialogs)
4. **Menu items** (Scene menu, GameObject menu)
5. **Tooltips** (for common actions)

### Context Preservation
Some mappings should preserve Godot terminology in parentheses:
- "Hierarchy (Scene Tree)"
- "Instantiate Prefab (Scene)"
- "Assets (Resources)"

This helps users learn both systems.

### Limitations
Not all terms have direct equivalents:
- **Signals** are more powerful than Unity Events
- **Node** architecture differs from Component architecture
- **PackedScene** is not exactly the same as Prefab

## File Location

The complete JSON mapping is stored in:
```
editor/plugins/unity_terminology_mapping.json
```

## Integration Points

This mapping can be integrated at several levels:

### 1. Editor UI Layer
Replace strings in:
- Menu items
- Button labels
- Dock titles
- Context menus
- Tooltips

### 2. Documentation Layer
- Help tooltips
- Error messages
- Status messages
- Tutorial text

### 3. Code Completion
- Autocomplete suggestions
- Function documentation
- Parameter hints

### 4. Import/Export
- File dialog filters
- Import dialogs for Unity assets
- Export templates

## See Also

- `unity_importer_plugin.cpp` - Unity asset import functionality
- `unity_package_importer.cpp` - Unity package (.unitypackage) import
- Editor internationalization system (l10n/i18n)
- Editor theme system (for icon replacements)

## Version History

- **v1.0** - Initial comprehensive mapping (December 2025)
  - Core types, editor panels, operations
  - File extensions and UI strings
  - Lifecycle methods and common nodes
  - Resource handling and project structure
