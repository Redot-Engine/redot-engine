"""
Verify Redot MCP Server end-to-end workflow

Usage:
    python3 verify_workflow.py --binary <path_to_redot_binary> --project <path_to_project_dir> [options]

Arguments:
    --binary <path>         Path to Redot editor binary (e.g. ./bin/redot.linuxbsd.editor.x86_64)
    --project <path>        Path to Redot project directory
    --click-node <path>     Node path to click after launch (default: MainMenu/MenuPanel/VBoxContainer/StartButton)
    --trigger-action <name> Input action to trigger (e.g. ui_cancel for Pause) (default: ui_cancel)
    --wait-load <seconds>   Seconds to wait for game load (default: 15.0)
    --wait-gameplay <seconds> Seconds to wait after click (default: 5.0)
    --screenshot-file <path> Output path for screenshot (default: mcp_verify_capture.png)

Example:
    python3 modules/mcp/tests/verify_workflow.py \
        --binary ./bin/redot.linuxbsd.editor.x86_64 \
        --project /home/user/my_game \
        --click-node "StartButton" \
        --trigger-action "pause"

    # For Nix users:
    nix develop -c python3 modules/mcp/tests/verify_workflow.py --binary ./bin/redot.linuxbsd.editor.x86_64 --project ...
"""

import json
import sys
import subprocess
import time
import base64
import argparse
import os


def send_request(proc, method, params, req_id=1):
    req = {"jsonrpc": "2.0", "id": req_id, "method": method, "params": params}
    msg = json.dumps(req)
    proc.stdin.write(msg + "\n")
    proc.stdin.flush()


def read_response(proc, timeout=5.0):
    start_time = time.time()
    while time.time() - start_time < timeout:
        line = proc.stdout.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            continue
        try:
            return json.loads(line)
        except json.JSONDecodeError:
            print(f"[MCP LOG] {line}", file=sys.stderr)
            continue
    return None


def main():
    parser = argparse.ArgumentParser(description="Verify Redot MCP Server end-to-end workflow")
    parser.add_argument(
        "--binary", required=True, help="Path to Redot editor binary (e.g. ./bin/redot.linuxbsd.editor.x86_64)"
    )
    parser.add_argument("--project", required=True, help="Path to Redot project directory")
    parser.add_argument(
        "--click-node", default="MainMenu/MenuPanel/VBoxContainer/StartButton", help="Node path to click after launch"
    )
    parser.add_argument(
        "--trigger-action", default="ui_cancel", help="Input action to trigger (e.g. ui_cancel for Pause)"
    )
    parser.add_argument("--wait-load", type=float, default=15.0, help="Seconds to wait for game load")
    parser.add_argument("--wait-gameplay", type=float, default=5.0, help="Seconds to wait after click")
    parser.add_argument("--screenshot-file", default="mcp_verify_capture.png", help="Output path for screenshot")

    args = parser.parse_args()

    if not os.path.exists(args.binary):
        print(f"Error: Binary not found at {args.binary}")
        sys.exit(1)
    if not os.path.exists(args.project):
        print(f"Error: Project path not found at {args.project}")
        sys.exit(1)

    cmd = [os.path.abspath(args.binary), "--headless", "--mcp-server", "--path", os.path.abspath(args.project)]

    print(f"Starting MCP server: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        # 1. Initialize
        print("\n--- Sending Initialize ---")
        send_request(
            proc,
            "initialize",
            {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "VerifyScript", "version": "1.0"},
            },
            req_id=1,
        )
        resp = read_response(proc)
        print(f"Response: {json.dumps(resp, indent=2)}")

        # 2. Run Game
        print("\n--- Launching Game ---")
        send_request(proc, "tools/call", {"name": "project_config", "arguments": {"action": "run"}}, req_id=2)
        resp = read_response(proc)
        print(f"Response: {json.dumps(resp, indent=2)}")

        print(f"Waiting {args.wait_load}s for game to load...")
        time.sleep(args.wait_load)

        # 3. Click Start Game
        if args.click_node:
            print(f"\n--- Clicking Node: {args.click_node} ---")
            send_request(
                proc,
                "tools/call",
                {"name": "game_control", "arguments": {"action": "click", "node_path": args.click_node}},
                req_id=3,
            )
            resp = read_response(proc)
            print(f"Response: {json.dumps(resp, indent=2)}")

            print(f"Waiting {args.wait_gameplay}s for gameplay...")
            time.sleep(args.wait_gameplay)

        # 4. Trigger Action
        if args.trigger_action:
            print(f"\n--- Triggering Action: {args.trigger_action} ---")
            send_request(
                proc,
                "tools/call",
                {"name": "game_control", "arguments": {"action": "trigger_action", "action_name": args.trigger_action}},
                req_id=4,
            )
            resp = read_response(proc)
            print(f"Response: {json.dumps(resp, indent=2)}")

            time.sleep(2)

        # 5. Capture Screenshot
        print(f"\n--- Capturing Screenshot ---")
        send_request(
            proc, "tools/call", {"name": "game_control", "arguments": {"action": "capture", "scale": 0.5}}, req_id=5
        )
        resp = read_response(proc, timeout=10.0)  # Larger timeout for image transfer

        if resp and "result" in resp:
            content = resp["result"].get("content", [])
            has_image = False
            for item in content:
                if item.get("type") == "image":
                    data = base64.b64decode(item["data"])
                    with open(args.screenshot_file, "wb") as f:
                        f.write(data)
                    print(f"Screenshot saved to {args.screenshot_file}")
                    has_image = True
                    break

            if not has_image:
                print(f"Warning: Capture returned no image content.")
                print(json.dumps(resp, indent=2))
        else:
            print("Capture failed or timed out.")

    except KeyboardInterrupt:
        print("\nInterrupted by user.")
    except Exception as e:
        print(f"\nError: {e}")
    finally:
        print("\nTerminating MCP server...")
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    main()
