def can_build(env, platform):
    env.module_add_dependencies("gdscript", ["jsonrpc", "websocket"], True)
    return True


def configure(env):
    env.Append(CPPDEFINES=["MODULE_GDSCRIPT_ENABLED"])


def get_doc_classes():
    return [
        "@GDScript",
        "GDScript",
        "GDScriptSyntaxHighlighter",
    ]


def get_doc_path():
    return "doc_classes"
