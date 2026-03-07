
## AI Integration (Model Context Protocol)

Redot includes a native **MCP (Model Context Protocol)** server, allowing AI coding assistants (like OpenCode, Claude Desktop, Cursor, or Zed) to interact directly with your game project.

### Tools Overview
The MCP server provides 5 master controllers:

*   **`redot_scene_action`**: Manage `.tscn` files (add nodes, set properties, instance scenes, and wire signals with automatic callback generation).
*   **`redot_resource_action`**: Manage `.tres` files and assets (create/modify materials, themes, inspect `.import` metadata).
*   **`redot_code_intel`**: Deep script analysis (GDScript syntax validation, symbol extraction, and engine documentation lookup).
*   **`redot_project_config`**: Project-level control (configure Input Map, Autoloads, run/stop the game, read logs, and res:// I/O).
*   **`redot_game_control`**: Vision & Interaction (capture screenshots, click UI elements with high-precision, and inspect the live scene tree recursively).

### Running the MCP Server

#### 1. Using the compiled binary (Standard)
```json
{
  "mcp": {
    "redot": {
      "type": "local",
      "command": [
        "/path/to/redot.editor.binary",
        "--headless",
        "--mcp-server",
        "--path",
        "/path/to/your/project"
      ],
      "enabled": true
    }
  }
}
```

#### 2. Running Unit Tests
You can execute automated tests headlessly using the `--run-tests` flag:
```bash
./bin/redot.<platform> --headless --run-tests=res://tests/my_test.gd
```
*Note: Your test script should have a `func run():` method.*


#### 3. Using Nix (For Development)
If you are developing inside a Nix environment, use the provided wrapper to ensure all libraries are correctly linked:

1. Locate the `redot-mcp.sh` script in the engine root.
2. In your `opencode.json` or MCP client config, use:
```json
{
  "mcp": {
    "redot": {
      "type": "local",
      "command": ["/path/to/redot-engine/redot-mcp.sh", "/path/to/your/project"],
      "enabled": true
    }
  }
}
```

### AI Agent Best Practices
To get the most out of Redot's MCP server, agents should follow these guidelines:

1.  **Scene Editing**: Use `redot_scene_action` for all `.tscn` modifications. Avoid editing TSCN files as raw text to prevent breaking node UIDs and internal references.
2.  **Scripting**: For existing `.gd` files, use **native text editing tools** (like `edit`) for precise logic changes. Use `redot_project_config(action="create_file_res")` only when creating new scripts from scratch.
3.  **Live Interaction**: Always `wait` 3-5 seconds after `run` before attempting vision or input actions to allow the bridge to initialize.
4.  **Spatial Awareness**: Use `redot_game_control(action="inspect_live", recursive=true)` to discover UI paths and their pre-calculated screen coordinates for 100% accurate clicking.
5.  **Debugging**: Use `redot_project_config(action="output")` to read real-time logs and `redot_code_intel(action="validate")` to syntax-check fixes before running the game.
6.  **Script Editing Policy**: The MCP tool `create_file_res` is restricted to creating **new** files. To edit existing `.gd` scripts, agents **must** use native text editing tools (like `edit`). This ensures precision and prevents accidental overwrites of complex logic.

### MCP Server Configuration Examples
#### 1. Antigravity IDE
```
{
  "mcpServers": {
    "redot": {
      "command": "/path/to/redot.editor.binary",
      "args": [
        "--headless",
        "--mcp-server",
        "--path",
        "/path/to/your/project"
      ],
      "disabled": false
    }
  }
}
```