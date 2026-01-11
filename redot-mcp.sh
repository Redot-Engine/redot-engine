#!/usr/bin/env bash
# Wrapper to run Redot MCP server inside Nix environment

ENGINE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="$ENGINE_DIR/bin/redot.linuxbsd.editor.x86_64"
# Project path should be provided as an argument
PROJECT_PATH="$1"

if [ -z "$PROJECT_PATH" ]; then
    echo "Usage: $0 <path_to_project>"
    exit 1
fi

if ! command -v nix &> /dev/null; then
    echo "Error: nix command not found. Please install Nix."
    exit 1
fi

nix develop "$ENGINE_DIR" -c "$BINARY" --headless --mcp-server --path "$PROJECT_PATH"
