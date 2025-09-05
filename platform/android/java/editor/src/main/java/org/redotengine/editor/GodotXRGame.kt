/**************************************************************************/
/*  GodotXRGame.kt                                                        */
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

package org.redotengine.editor

import org.redotengine.godot.GodotLib
import org.redotengine.godot.xr.XRMode

/**
 * Provide support for running XR apps / games from the editor window.
 */
open class GodotXRGame: BaseGodotGame() {

	override fun overrideOrientationRequest() = true

	override fun getCommandLine(): MutableList<String> {
		val updatedArgs = super.getCommandLine()
		if (!updatedArgs.contains(XRMode.OPENXR.cmdLineArg)) {
			updatedArgs.add(XRMode.OPENXR.cmdLineArg)
		}
		if (!updatedArgs.contains(XR_MODE_ARG)) {
			updatedArgs.add(XR_MODE_ARG)
			updatedArgs.add("on")
		}
		return updatedArgs
	}

	override fun getEditorWindowInfo() = XR_RUN_GAME_INFO

	override fun getGodotAppLayout() = R.layout.godot_xr_game_layout

	override fun getProjectPermissionsToEnable(): MutableList<String> {
		val permissionsToEnable = super.getProjectPermissionsToEnable()

		val xrRuntimePermission = getXRRuntimePermissions()
		if (xrRuntimePermission.isNotEmpty() && GodotLib.getGlobal("xr/openxr/enabled").toBoolean()) {
			// We only request permissions when the `automatically_request_runtime_permissions`
			// project setting is enabled.
			// If the project setting is not defined, we fall-back to the default behavior which is
			// to automatically request permissions.
			val automaticallyRequestPermissionsSetting = GodotLib.getGlobal("xr/openxr/extensions/automatically_request_runtime_permissions")
			val automaticPermissionsRequestEnabled = automaticallyRequestPermissionsSetting.isNullOrEmpty() ||
				automaticallyRequestPermissionsSetting.toBoolean()
			if (automaticPermissionsRequestEnabled) {
				permissionsToEnable.addAll(xrRuntimePermission)
			}
		}

		return permissionsToEnable
	}
}
