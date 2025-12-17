"""Functions used to generate source files during build time"""

import os
import os.path
import subprocess
import tempfile
import uuid

import methods


def doc_data_class_path_builder(target, source, env):
    paths = dict(sorted(source[0].read().items()))
    data = "\n".join([f'\t{{"{key}", "{value}"}},' for key, value in paths.items()])
    with methods.generated_wrapper(str(target[0])) as file:
        file.write(
            f"""\
struct _DocDataClassPath {{
	const char *name;
	const char *path;
}};

inline constexpr int _doc_data_class_path_count = {len(paths)};
inline constexpr _DocDataClassPath _doc_data_class_paths[{len(paths) + 1}] = {{
	{data}
	{{nullptr, nullptr}},
}};
"""
        )


def register_exporters_builder(target, source, env):
    platforms = source[0].read()
    exp_inc = "\n".join([f'#include "platform/{p}/export/export.h"' for p in platforms])
    exp_reg = "\n\t".join([f"register_{p}_exporter();" for p in platforms])
    exp_type = "\n\t".join([f"register_{p}_exporter_types();" for p in platforms])
    with methods.generated_wrapper(str(target[0])) as file:
        file.write(
            f"""\
#include "register_exporters.h"

{exp_inc}

void register_exporters() {{
	{exp_reg}
}}

void register_exporter_types() {{
	{exp_type}
}}
"""
        )


def make_doc_header(target, source, env):
    buffer = b"".join([methods.get_buffer(src) for src in map(str, source)])
    decomp_size = len(buffer)
    buffer = methods.compress_buffer(buffer)

    with methods.generated_wrapper(str(target[0])) as file:
        file.write(f"""\
inline constexpr const char *_doc_data_hash = "{hash(buffer)}";
inline constexpr int _doc_data_compressed_size = {len(buffer)};
inline constexpr int _doc_data_uncompressed_size = {decomp_size};
inline constexpr const unsigned char _doc_data_compressed[] = {{
	{methods.format_buffer(buffer, 1)}
}};
""")


def make_translations_header(target, source, env):
    category = os.path.basename(str(target[0])).split("_")[0]
    sorted_paths = sorted([src.abspath for src in source], key=lambda path: os.path.splitext(os.path.basename(path))[0])

    xl_names = []
    msgfmt = env.Detect("msgfmt")
    if not msgfmt:
        methods.print_warning("msgfmt not found, using .po files instead of .mo")

    with methods.generated_wrapper(str(target[0])) as file:
        for path in sorted_paths:
            name = os.path.splitext(os.path.basename(path))[0]
            # msgfmt erases non-translated messages, so avoid using it if exporting the POT.
            if msgfmt and name != category:
                mo_path = os.path.join(tempfile.gettempdir(), uuid.uuid4().hex + ".mo")
                cmd = f"{msgfmt} {path} --no-hash -o {mo_path}"
                try:
                    subprocess.Popen(cmd, shell=True, stderr=subprocess.PIPE).communicate()
                    buffer = methods.get_buffer(mo_path)
                except OSError as e:
                    methods.print_warning(
                        "msgfmt execution failed, using .po file instead of .mo: path=%r; [%s] %s"
                        % (path, e.__class__.__name__, e)
                    )
                    buffer = methods.get_buffer(path)
                finally:
                    try:
                        if os.path.exists(mo_path):
                            os.remove(mo_path)
                    except OSError as e:
                        # Do not fail the entire build if it cannot delete a temporary file.
                        methods.print_warning(
                            "Could not delete temporary .mo file: path=%r; [%s] %s" % (mo_path, e.__class__.__name__, e)
                        )
            else:
                buffer = methods.get_buffer(path)
                if name == category:
                    name = "source"

            decomp_size = len(buffer)
            buffer = methods.compress_buffer(buffer)

            file.write(f"""\
inline constexpr const unsigned char _{category}_translation_{name}_compressed[] = {{
	{methods.format_buffer(buffer, 1)}
}};

""")

            xl_names.append([name, len(buffer), decomp_size])

        file.write(f"""\
struct {category.capitalize()}TranslationList {{
	const char* lang;
	int comp_size;
	int uncomp_size;
	const unsigned char* data;
}};

inline constexpr {category.capitalize()}TranslationList _{category}_translations[] = {{
""")

        for x in xl_names:
            file.write(f'\t{{ "{x[0]}", {x[1]}, {x[2]}, _{category}_translation_{x[0]}_compressed }},\n')

        file.write("""\
	{ nullptr, 0, 0, nullptr },
};
""")


def unity_vendor_builder(target, source, env):
    # Source contains two directories to scan: unidot_importer and UnityToGodot
    src_dirs = [str(s) for s in source]
    vendor = {
        "unidot_importer": [],
        "UnityToGodot": [],
    }

    def _scan_dir(dir_path):
        files = []
        for root, _, filenames in os.walk(dir_path):
            for fname in filenames:
                # Include all text and script files and images as raw bytes.
                rel = os.path.relpath(os.path.join(root, fname), dir_path).replace("\\", "/")
                abspath = os.path.join(root, fname)
                try:
                    buf = methods.get_buffer(abspath)
                    files.append((rel, buf))
                except Exception:
                    continue
        return files

    for d in src_dirs:
        name = os.path.basename(d)
        if name in vendor and os.path.isdir(d):
            vendor[name] = _scan_dir(d)

    with methods.generated_wrapper(str(target[0])) as file:
        file.write("""
#pragma once
#include <stdint.h>

namespace UnityVendor {
    struct File { const char* path; const uint8_t* data; unsigned int size; };
""")

        def _write_bundle(bundle_name, items):
            # Write data blobs
            for idx, (path, buf) in enumerate(items):
                file.write(f"inline constexpr unsigned char _{bundle_name}_data_{idx}[] = {{\n\t{methods.format_buffer(buf, 1)}\n}};\n\n")
            # Write table
            file.write(f"inline constexpr File {bundle_name.upper()}[] = {{\n")
            for idx, (path, buf) in enumerate(items):
                file.write(f"\t{{ \"{path}\", _{bundle_name}_data_{idx}, {len(buf)} }},\n")
            file.write("\t{ nullptr, nullptr, 0 },\n};\n")
            file.write(f"inline constexpr unsigned int {bundle_name.upper()}_COUNT = {len(items)};\n\n")

        _write_bundle("unidot_importer", vendor["unidot_importer"])
        _write_bundle("unitytogodot", vendor["UnityToGodot"])

        file.write("""
} // namespace UnityVendor
""")
