import os


def can_build(env, platform):
    # The module links prebuilt PhysX SDK static libs vendored under
    # thirdparty/physx/lib/<platform>/<arch>/<config> (see SCsub's lib-path
    # table). Only build on platforms whose libs are actually vendored, so
    # CI for platforms without an SDK checkout stays green instead of dying
    # at link time; the module lights up on a platform automatically as soon
    # as its lib directory lands in the tree.
    lib_roots = {
        "windows": "thirdparty/physx/lib/windows/x86_64",
        "macos": "thirdparty/physx/lib/macos/universal",
        "linuxbsd": "thirdparty/physx/lib/linux/x86_64",
    }
    # Windows: the vendored static libs are MSVC /MT builds. MinGW and
    # llvm-mingw cannot link them (the lib's static-UCRT symbols duplicate
    # the MinGW CRT, and its embedded /DEFAULTLIB directives demand MSVC-only
    # import libs), so the module is MSVC-only on Windows -- same constraint
    # as the upstream godot_physx integration (win.x86_64.vc143.mt).
    if platform == "windows":
        return os.path.isdir(lib_roots[platform]) and env.msvc
    if platform == "android":
        return os.path.isdir("thirdparty/physx/lib/android")
    if platform in lib_roots:
        return os.path.isdir(lib_roots[platform])
    # iOS and web have no SDK wiring yet and would fail at link time.
    return False


def configure(env):
    pass


def get_doc_classes():
    return [
        "PhysXServer3D",
        "PhysXDirectBodyState3D",
        "PhysXChunkEmitter3D",
        "PhysXCloth3D",
        "PhysXParticleFluid3D",
        "PhysXGranular3D",
        "PhysXGas3D",
        "PhysXGasEmitter3D",
        "PhysXBlastAsset",
        "PhysXBlastAuthoring",
        "PhysXDestructible3D",
        "PhysXFlowSimulation3D",
        "PhysXFlowEmitter3D",
        "PhysXFlowCollider3D",
        "PhysXFlowBlastBridge3D",
        "PhysXVehicle3D",
        "PhysXVehicleWheel3D",
        "PhysXMotorcycle3D",
        "PhysXTank3D",
        "PhysXWaterSurface3D",
        "PhysXWaterWake3D",
        "PhysXWaterSpray3D",
        "PhysXBoat3D",
        "PhysXBuoyancy3D",
    ]


def get_doc_path():
    return "doc_classes"
