#!/usr/bin/env bash
# Wrapper to run Redot MCP server inside Nix environment

ENGINE_DIR="/home/micqdf/github/redot-engine"
BINARY="$ENGINE_DIR/bin/redot.linuxbsd.editor.x86_64"
DEFAULT_PROJECT_PATH="/home/micqdf/opencode-test"

# Use provided path or default
PROJECT_PATH="${1:-$DEFAULT_PROJECT_PATH}"

nix develop "$ENGINE_DIR" -c "$BINARY" --headless --mcp-server --path "$PROJECT_PATH"
