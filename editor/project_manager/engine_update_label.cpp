/**************************************************************************/
/*  engine_update_label.cpp                                               */
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

#include "engine_update_label.h"

#include "core/io/dir_access.h"
#include "core/io/json.h"
#include "core/os/time.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_settings.h"
#include "scene/main/http_request.h"

bool EngineUpdateLabel::_can_check_updates() const {
	return int(EDITOR_GET("network/connection/network_mode")) == EditorSettings::NETWORK_ONLINE &&
			UpdateMode(int(EDITOR_GET("network/connection/check_for_updates"))) != UpdateMode::DISABLED;
}

void EngineUpdateLabel::_check_update() {
	if (!_can_check_updates()) {
		_set_status(UpdateStatus::OFFLINE);
		return;
	}

	checked_update = true;

	if (ratelimit_remaining == UINT64_MAX && ratelimit_reset == UINT64_MAX) {
		const String gh_ratelimit_path = OS::get_singleton()->get_data_path().path_join(OS::get_singleton()->get_godot_dir_name()).path_join("gh_ratelimit");
		Ref<FileAccess> gh_ratelimit_file = FileAccess::open(gh_ratelimit_path, FileAccess::READ);
		if (gh_ratelimit_file.is_valid() && gh_ratelimit_file->is_open()) {
			ratelimit_remaining = gh_ratelimit_file->get_64();
			ratelimit_reset = gh_ratelimit_file->get_64();
		} else {
			ratelimit_remaining = UINT64_MAX;
			ratelimit_reset = 0;
		}
	}

	uint64_t current_epoch = Time::get_singleton()->get_unix_time_from_system();
	if (ratelimit_remaining <= 0 && ratelimit_reset >= current_epoch) {
		_set_status(UpdateStatus::UP_TO_DATE);
		get_tree()->create_timer(current_epoch - ratelimit_reset + 1, false, true)->connect("timeout", callable_mp(this, &EngineUpdateLabel::_check_update));
		return;
	}

	_set_status(UpdateStatus::BUSY);
	http->request("https://api.github.com/repos/Redot-Engine/redot-engine/releases", { "Accept: application/vnd.github+json", "X-GitHub-Api-Version:2022-11-28" });
}

void EngineUpdateLabel::_http_request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	for (const String &header_text : p_headers) {
		const String header = header_text.to_lower();
		if (header.begins_with("x-ratelimit-remaining")) {
			ratelimit_remaining = header.get_slice(":", 1).to_int();
		} else if (header.begins_with("x-ratelimit-reset") || header.begins_with("retry-after")) {
			ratelimit_reset = header.get_slice(":", 1).to_int();
		}
	}

	const String gh_ratelimit_path = OS::get_singleton()->get_data_path().path_join(OS::get_singleton()->get_godot_dir_name()).path_join("gh_ratelimit");
	if (ratelimit_remaining == 0) {
		Ref<FileAccess> gh_ratelimit_file = FileAccess::open(gh_ratelimit_path, FileAccess::WRITE);
		if (gh_ratelimit_file.is_valid() && gh_ratelimit_file->is_open()) {
			gh_ratelimit_file->store_64(ratelimit_remaining);
			gh_ratelimit_file->store_64(ratelimit_reset);
		}
	} else if (FileAccess::exists(gh_ratelimit_path)) {
		DirAccess::remove_absolute(gh_ratelimit_path);
	}

	if (p_result != OK) {
		_set_status(UpdateStatus::ERROR);
		_set_message(vformat(TTR("Failed to check for updates. Error: %d."), p_result), theme_cache.error_color);
		return;
	}

	if (p_response_code != 200) {
		_set_status(UpdateStatus::ERROR);
		_set_message(vformat(TTR("Failed to check for updates. Response code: %d."), p_response_code), theme_cache.error_color);
		return;
	}

	Array version_array;
	{
		const uint8_t *r = p_body.ptr();
		String s = String::utf8((const char *)r, p_body.size());

		Variant result = JSON::parse_string(s);
		if (result == Variant()) {
			_set_status(UpdateStatus::ERROR);
			_set_message(TTR("Failed to parse version JSON."), theme_cache.error_color);
			return;
		}
		if (result.get_type() != Variant::ARRAY) {
			_set_status(UpdateStatus::ERROR);
			_set_message(TTR("Received JSON data is not a valid version array."), theme_cache.error_color);
			return;
		}
		version_array = result;
	}

	UpdateMode update_mode = UpdateMode(int(EDITOR_GET("network/connection/check_for_updates")));
	bool stable_only = update_mode == UpdateMode::NEWEST_STABLE || update_mode == UpdateMode::NEWEST_PATCH;

	const Dictionary current_version_info = Engine::get_singleton()->get_version_info();
	int current_major = current_version_info.get("major", 0);
	int current_minor = current_version_info.get("minor", 0);
	int current_patch = current_version_info.get("patch", 0);

	for (const Variant &data_bit : version_array) {
		const Dictionary version_info = data_bit;

		const String bare_tag_name = (String)version_info.get("tag_name", "");
		const PackedStringArray tag_bits = bare_tag_name.split("-");

		if (tag_bits.size() < 3) {
			continue;
		}

		const String base_version_string = tag_bits[1];
		const PackedStringArray version_bits = base_version_string.split(".");

		if (version_bits.size() < 2) {
			continue;
		}

		int minor = version_bits[1].to_int();
		if (version_bits[0].to_int() != current_major || minor < current_minor) {
			continue;
		}

		int patch = 0;
		if (version_bits.size() >= 3) {
			patch = version_bits[2].to_int();
		}

		if (minor == current_minor && patch < current_patch) {
			continue;
		}

		if (update_mode == UpdateMode::NEWEST_PATCH && minor > current_minor) {
			continue;
		}

		const String release_string = tag_bits[2];

		int release_index = DEV_VERSION;
		VersionType release_type = _get_version_type(release_string, &release_index);

		if (minor > current_minor || patch > current_patch) {
			if (stable_only && release_type != VersionType::STABLE) {
				continue;
			}

			available_newer_version = vformat("%s-%s", base_version_string, release_string);
			break;
		}

		int current_version_index = current_version_info.get("status_version", 0);
		current_version_index = current_version_index > 0 ? current_version_index : DEV_VERSION;
		VersionType current_version_type = _get_version_type(current_version_info.get("status", "unknown"), &current_version_index);

		if (int(release_type) > int(current_version_type)) {
			break;
		}

		if (int(release_type) == int(current_version_type) && release_index <= current_version_index) {
			break;
		}

		available_newer_version = vformat("%s-%s", base_version_string, release_string);
		break;
	}

	if (!available_newer_version.is_empty()) {
		_set_status(UpdateStatus::UPDATE_AVAILABLE);
		_set_message(vformat(TTR("Update available: %s."), available_newer_version), theme_cache.update_color);
	} else if (available_newer_version.is_empty()) {
		_set_status(UpdateStatus::UP_TO_DATE);
	}
}

void EngineUpdateLabel::_set_message(const String &p_message, const Color &p_color) {
	if (is_disabled()) {
		add_theme_color_override("font_disabled_color", p_color);
	} else {
		add_theme_color_override(SceneStringName(font_color), p_color);
	}
	set_text(p_message);
}

void EngineUpdateLabel::_set_status(UpdateStatus p_status) {
	status = p_status;
	if (status == UpdateStatus::BUSY || status == UpdateStatus::UP_TO_DATE) {
		// Hide the label to prevent unnecessary distraction.
		hide();
		return;
	} else {
		show();
	}

	switch (status) {
		case UpdateStatus::OFFLINE: {
			set_disabled(false);
			if (int(EDITOR_GET("network/connection/network_mode")) == EditorSettings::NETWORK_OFFLINE) {
				_set_message(TTR("Offline mode, update checks disabled."), theme_cache.disabled_color);
			} else {
				_set_message(TTR("Update checks disabled."), theme_cache.disabled_color);
			}
			set_accessibility_live(DisplayServer::AccessibilityLiveMode::LIVE_OFF);
			set_tooltip_text("");
			break;
		}

		case UpdateStatus::ERROR: {
			set_disabled(false);
			set_accessibility_live(DisplayServer::AccessibilityLiveMode::LIVE_POLITE);
			set_tooltip_text(TTR("An error has occurred. Click to try again."));
		} break;

		case UpdateStatus::UPDATE_AVAILABLE: {
			set_disabled(false);
			set_accessibility_live(DisplayServer::AccessibilityLiveMode::LIVE_POLITE);
			set_tooltip_text(TTR("Click to open download page."));
		} break;

		default: {
		}
	}
}

EngineUpdateLabel::VersionType EngineUpdateLabel::_get_version_type(const String &p_string, int *r_index) const {
	VersionType type = VersionType::UNKNOWN;
	String index_string;

	static HashMap<String, VersionType> type_map;
	if (type_map.is_empty()) {
		type_map["stable"] = VersionType::STABLE;
		type_map["rc"] = VersionType::RC;
		type_map["beta"] = VersionType::BETA;
		type_map["alpha"] = VersionType::ALPHA;
		type_map["dev"] = VersionType::DEV;
	}

	for (const KeyValue<String, VersionType> &kv : type_map) {
		if (p_string.begins_with(kv.key)) {
			index_string = p_string.trim_prefix(kv.key);
			type = kv.value;
			break;
		}
	}

	if (r_index && *r_index == DEV_VERSION) {
		if (index_string.is_empty()) {
			*r_index = DEV_VERSION;
		} else {
			*r_index = index_string.trim_prefix(".").to_int();
		}
	}
	return type;
}

String EngineUpdateLabel::_extract_sub_string(const String &p_line) const {
	int j = p_line.find_char('"') + 1;
	return p_line.substr(j, p_line.find_char('"', j) - j);
}

void EngineUpdateLabel::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorSettings::get_singleton()->check_changed_settings_in_group("network/connection")) {
				break;
			}

			if (_can_check_updates()) {
				_check_update();
			} else {
				_set_status(UpdateStatus::OFFLINE);
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			theme_cache.default_color = get_theme_color(SceneStringName(font_color), "Button");
			theme_cache.disabled_color = get_theme_color("font_disabled_color", "Button");
			theme_cache.error_color = get_theme_color("error_color", EditorStringName(Editor));
			theme_cache.update_color = get_theme_color("warning_color", EditorStringName(Editor));
		} break;

		case NOTIFICATION_READY: {
			_check_update();
		} break;
	}
}

void EngineUpdateLabel::_bind_methods() {
	ADD_SIGNAL(MethodInfo("offline_clicked"));
}

void EngineUpdateLabel::pressed() {
	switch (status) {
		case UpdateStatus::OFFLINE: {
			emit_signal("offline_clicked");
		} break;

		case UpdateStatus::ERROR: {
			_check_update();
		} break;

		case UpdateStatus::UPDATE_AVAILABLE: {
			OS::get_singleton()->shell_open("https://github.com/Redot-Engine/redot-engine/releases/tag/redot-" + available_newer_version);
		} break;

		default: {
		}
	}
}

EngineUpdateLabel::EngineUpdateLabel() {
	set_underline_mode(UNDERLINE_MODE_ON_HOVER);

	http = memnew(HTTPRequest);
	http->set_https_proxy(EDITOR_GET("network/http_proxy/host"), EDITOR_GET("network/http_proxy/port"));
	http->set_timeout(10.0);
	add_child(http);
	http->connect("request_completed", callable_mp(this, &EngineUpdateLabel::_http_request_completed));
}
