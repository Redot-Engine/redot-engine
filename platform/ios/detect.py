import os
import sys
from typing import TYPE_CHECKING

from methods import detect_darwin_sdk_path, detect_darwin_toolchain_path, print_error, print_warning
from platform_methods import validate_arch

if TYPE_CHECKING:
    from SCons.Script.SConscript import SConsEnvironment


def get_name():
    return "iOS"


def can_build():
    if sys.platform == "darwin" or ("OSXCROSS_IOS" in os.environ):
        return True

    return False


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        ("vulkan_sdk_path", "Path to the Vulkan SDK", ""),
        # APPLE_TOOLCHAIN_PATH Example: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain
        (("APPLE_TOOLCHAIN_PATH", "IOS_TOOLCHAIN_PATH"), "Path to the Apple toolchain", ""),
        ("IOS_SDK_PATH", "Path to the iOS SDK", ""),
        (("apple_target_triple", "ios_triple"), "Triple for the corresponding target Apple platform toolchain", ""),
        BoolVariable(("simulator", "ios_simulator"), "Build for Simulator", False),
        BoolVariable("generate_bundle", "Generate an APP bundle after building iOS/macOS binaries", False),
    ]


def get_doc_classes():
    return [
        "EditorExportPlatformIOS",
    ]


def get_doc_path():
    return "doc_classes"


def get_flags():
    return {
        "arch": "arm64",
        "target": "template_debug",
        "use_volk": False,
        "metal": True,
        "supported": ["metal", "mono"],
        "builtin_pcre2_with_jit": False,
    }


def configure(env: "SConsEnvironment"):
    # Validate arch.
    supported_arches = ["x86_64", "arm64"]
    validate_arch(env["arch"], get_name(), supported_arches)
    detect_darwin_toolchain_path(env)

    ## LTO

    if env["lto"] == "auto":  # Disable by default as it makes linking in Xcode very slow.
        env["lto"] = "none"

    if env["lto"] != "none":
        if env["lto"] == "thin":
            env.Append(CCFLAGS=["-flto=thin"])
            env.Append(LINKFLAGS=["-flto=thin"])
        else:
            env.Append(CCFLAGS=["-flto"])
            env.Append(LINKFLAGS=["-flto"])

    ## Compiler configuration

    # Save this in environment for use by other modules
    if "OSXCROSS_IOS" in os.environ:
        env["osxcross"] = True

    env["ENV"]["PATH"] = env["APPLE_TOOLCHAIN_PATH"] + "/Developer/usr/bin/:" + env["ENV"]["PATH"]

    compiler_path = "$APPLE_TOOLCHAIN_PATH/usr/bin/${apple_target_triple}"

    ccache_path = os.environ.get("CCACHE")
    if ccache_path is None:
        env["CC"] = compiler_path + "clang"
        env["CXX"] = compiler_path + "clang++"
        env["S_compiler"] = compiler_path + "clang"
    else:
        # there aren't any ccache wrappers available for iOS,
        # to enable caching we need to prepend the path to the ccache binary
        env["CC"] = ccache_path + " " + compiler_path + "clang"
        env["CXX"] = ccache_path + " " + compiler_path + "clang++"
        env["S_compiler"] = ccache_path + " " + compiler_path + "clang"
    env["AR"] = compiler_path + "ar"
    env["RANLIB"] = compiler_path + "ranlib"

    ## Compile flags

    if env["simulator"]:
        detect_darwin_sdk_path("iossimulator", env)
        env.Append(ASFLAGS=["-mios-simulator-version-min=12.0"])
        env.Append(CCFLAGS=["-mios-simulator-version-min=12.0"])
        env.Append(CPPDEFINES=["IOS_SIMULATOR"])
        env.extra_suffix = ".simulator" + env.extra_suffix
    else:
        detect_darwin_sdk_path("ios", env)
        env.Append(ASFLAGS=["-miphoneos-version-min=12.0"])
        env.Append(CCFLAGS=["-miphoneos-version-min=12.0"])

    if env["arch"] == "x86_64":
        if not env["simulator"]:
            print_error("Building for iOS with 'arch=x86_64' requires 'simulator=yes'.")
            sys.exit(255)

        env["ENV"]["MACOSX_DEPLOYMENT_TARGET"] = "10.9"
        env.Append(
            CCFLAGS=(
                "-fobjc-arc -arch x86_64"
                " -fobjc-abi-version=2 -fobjc-legacy-dispatch -fmessage-length=0 -fpascal-strings -fblocks"
                " -fasm-blocks -isysroot $IOS_SDK_PATH"
            ).split()
        )
        env.Append(ASFLAGS=["-arch", "x86_64"])
    elif env["arch"] == "arm64":
        env.Append(
            CCFLAGS=(
                "-fobjc-arc -arch arm64 -fmessage-length=0"
                " -fdiagnostics-print-source-range-info -fdiagnostics-show-category=id -fdiagnostics-parseable-fixits"
                " -fpascal-strings -fblocks -fvisibility=hidden -MMD -MT dependencies"
                " -isysroot $IOS_SDK_PATH".split()
            )
        )
        env.Append(ASFLAGS=["-arch", "arm64"])

    # Temp fix for ABS/MAX/MIN macros in iOS SDK blocking compilation
    env.Append(CCFLAGS=["-Wno-ambiguous-macro"])

    env.Prepend(
        CPPPATH=[
            "$IOS_SDK_PATH/usr/include",
            "$IOS_SDK_PATH/System/Library/Frameworks/AudioUnit.framework/Headers",
        ]
    )

    env.Prepend(CPPPATH=["#platform/ios"])
    env.Append(CPPDEFINES=["IOS_ENABLED", "APPLE_EMBEDDED_ENABLED", "UNIX_ENABLED", "COREAUDIO_ENABLED"])

    if env["metal"] and env["simulator"]:
        print_warning("iOS Simulator does not support the Metal rendering driver")
        env["metal"] = False

    if env["metal"]:
        env.AppendUnique(CPPDEFINES=["METAL_ENABLED", "RD_ENABLED"])
        env.Prepend(
            CPPPATH=[
                "$IOS_SDK_PATH/System/Library/Frameworks/Metal.framework/Headers",
                "$IOS_SDK_PATH/System/Library/Frameworks/MetalFX.framework/Headers",
                "$IOS_SDK_PATH/System/Library/Frameworks/QuartzCore.framework/Headers",
            ]
        )
        env.Prepend(CPPEXTPATH=["#thirdparty/spirv-cross"])

    if env["vulkan"] and env["simulator"]:
        print_warning("iOS Simulator does not support the Vulkan rendering driver")
        env["vulkan"] = False

    if env["vulkan"]:
        env.AppendUnique(CPPDEFINES=["VULKAN_ENABLED", "RD_ENABLED"])

    if env["opengl3"]:
        env.Append(CPPDEFINES=["GLES3_ENABLED", "GLES_SILENCE_DEPRECATION"])
        env.Append(CCFLAGS=["-Wno-module-import-in-extern-c"])
        env.Prepend(
            CPPPATH=[
                "$IOS_SDK_PATH/System/Library/Frameworks/OpenGLES.framework/Headers",
            ]
        )
