extends RefCounted

# Shared machine-readable report writer for the standalone GDScript suites
# (physics smoke, Blast, vehicle, Flow, GPU). run_suite.py treats a suite run
# as finished only when a JSON report with "final": true lands at the
# --json=<path> given in the user args (after "--" on the command line); a
# missing report is reported as CRASH even when the process exits 0. The
# process exit code stays the secondary signal (0 = pass/skip, 1 = fail).

static func json_path_from_args() -> String:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--json="):
			return arg.substr("--json=".length())
	return ""

static func write(json_path: String, suite: String, test_id: String, description: String, status: String, assertions: int, messages: Array) -> void:
	if json_path.is_empty():
		return
	var result := {
		"suite": suite,
		"final": true,
		"tests": [{
			"id": test_id,
			"category": suite,
			"description": description,
			"status": status,
			"assertions": assertions,
			"failures": messages.size(),
			"messages": messages,
		}],
	}
	var f := FileAccess.open(json_path, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(result, "  "))
		f.close()
	else:
		push_error("[%s] could not write JSON report to %s" % [suite, json_path])
