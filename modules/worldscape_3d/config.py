def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_path():
    return "doc_classes"


def get_doc_classes():
    return [
        "WorldScape3D",
        "WorldScape3DAssets",
        "WorldScape3DMeshAsset",
        "WorldScape3DTextureAsset",
        "WorldScape3DCollision",
        "WorldScape3DData",
        "WorldScape3DEditor",
        "WorldScape3DInstancer",
        "WorldScape3DMaterial",
        "WorldScape3DRegion",
    ]
