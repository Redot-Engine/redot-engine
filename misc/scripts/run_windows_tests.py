#!/usr/bin/env python3
"""Run Windows unit tests with a watchdog and save a minidump on timeout."""

import argparse
import ctypes
import subprocess
import sys
from ctypes import wintypes
from pathlib import Path

if sys.platform == "win32":
    import msvcrt


def write_dump(pid: int, path: Path) -> None:
    if sys.platform == "win32":
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel32.OpenProcess.restype = wintypes.HANDLE
        kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel32.CloseHandle.restype = wintypes.BOOL
        dbghelp = ctypes.WinDLL("dbghelp", use_last_error=True)
        dbghelp.MiniDumpWriteDump.argtypes = [
            wintypes.HANDLE,
            wintypes.DWORD,
            wintypes.HANDLE,
            wintypes.DWORD,
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.c_void_p,
        ]
        dbghelp.MiniDumpWriteDump.restype = wintypes.BOOL
        process_handle = kernel32.OpenProcess(0x0400 | 0x0010, False, pid)  # QUERY_INFORMATION | VM_READ
        if not process_handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            with path.open("wb") as dump:
                # MiniDumpWithThreadInfo includes the thread states alongside the normal stacks.
                if not dbghelp.MiniDumpWriteDump(
                    process_handle, pid, msvcrt.get_osfhandle(dump.fileno()), 0x1000, None, None, None
                ):
                    raise ctypes.WinError(ctypes.get_last_error())
        finally:
            kernel32.CloseHandle(process_handle)
    else:
        raise OSError("Minidumps require Windows.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--timeout", type=float, default=300)
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    # Monitor the engine itself rather than the console wrapper process.
    binary = args.binary.resolve()
    binary = binary.with_name(binary.name.replace(".console.exe", ".exe"))
    process = subprocess.Popen(
        [str(binary), "--test", "--force-colors", *args.arguments], stdout=sys.stdout, stderr=sys.stderr
    )
    try:
        return process.wait(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        print(f"::error::Unit tests exceeded {args.timeout:g} seconds; saving a minidump.", flush=True)
        try:
            write_dump(process.pid, binary.parent / "unit-tests.dmp")
        except OSError as error:
            print(f"::warning::Failed to save the unit test minidump: {error}", flush=True)
        finally:
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], check=False)
            if process.poll() is None:
                process.kill()
            process.wait()
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
