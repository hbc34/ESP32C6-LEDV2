#!/usr/bin/env python3
"""Run ESP-IDF commands through the EIM-managed installation."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


DEFAULT_TARGET = os.environ.get("ESP_IDF_DEFAULT_TARGET", "esp32c6")
PROJECT_ROOT = Path(__file__).resolve().parents[1]
CACHE_ROOT = PROJECT_ROOT / ".cache" / "eim"
LOG_FILE = CACHE_ROOT / "eim.log"


@dataclass(frozen=True)
class IdfInstall:
    version: str
    path: Path
    selected: bool


def fail(message: str) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(1)


def section(message: str) -> None:
    print(f"==> {message}")


def command_path(command: str) -> str:
    path = shutil.which(command)
    if path is None:
        fail(f"Could not find '{command}'. Install Espressif EIM CLI first.")
    return path


def run_eim(eim_path: str, *args: str, capture: bool = False) -> subprocess.CompletedProcess[str]:
    CACHE_ROOT.mkdir(parents=True, exist_ok=True)
    return subprocess.run(
        [eim_path, "--log-file", str(LOG_FILE), *args],
        check=False,
        capture_output=capture,
        text=True,
    )


def require_success(result: subprocess.CompletedProcess[str], message: str) -> None:
    if result.returncode == 0:
        return
    if result.stdout:
        print(result.stdout, end="", file=sys.stderr)
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    fail(message)


def installed_idfs(eim_path: str) -> list[IdfInstall]:
    result = run_eim(eim_path, "list", capture=True)
    require_success(result, "'eim list' failed.")

    installs: list[IdfInstall] = []
    for line in result.stdout.splitlines():
        match = re.match(r"^-\s+(\S+).*?\[([^]]+)\].*", line)
        if match is None:
            continue
        installs.append(
            IdfInstall(
                version=match.group(1),
                path=Path(match.group(2)),
                selected="(selected)" in line,
            )
        )
    return installs


def default_install_path(version: str, base: str | None = None) -> Path:
    r"""Get EIM's default install path for a version (typically {base}\{version}\esp-idf)."""
    # EIM uses base_path\{version}\esp-idf as default
    if base is None:
        base = os.environ.get("ESPRESSIF_INSTALL_BASE", r"C:\esp")
    return Path(base) / version.lstrip("v") / "esp-idf"


def resolve_idf(eim_path: str, version: str, target: str) -> IdfInstall:
    installs = installed_idfs(eim_path)
    if not installs:
        fail(
            "EIM did not report any installed ESP-IDF versions. Start with: "
            f"eim install -i v6.0.2 -t {target} -n true"
        )

    for install in installs:
        if install.version == version:
            return install

    known_versions = ", ".join(install.version for install in installs)
    fail(f"EIM does not know version '{version}'. Installed versions: {known_versions}")


def validate_idf_runtime(eim_path: str, install: IdfInstall, target: str) -> str:
    idf_py = install.path / "tools" / "idf.py"
    if not install.path.is_dir():
        fail(f"The configured ESP-IDF path does not exist: {install.path}")
    if not idf_py.is_file():
        fail(f"Could not find tools/idf.py under {install.path}")

    result = run_eim(eim_path, "run", "idf.py --version", install.version, capture=True)
    if result.returncode == 0:
        return result.stdout.strip()

    print(
        "EIM found ESP-IDF, but the runtime validation failed.\n\n"
        f"Version : {install.version}\n"
        f"Path    : {install.path}\n\n"
        "Try fixing the installation first:\n"
        f'  eim fix -p "{install.path}" -i {install.version} -t {target}\n\n'
        "If that still fails, reinstall it:\n"
        f"  eim install -i {install.version} -t {target} -n true\n\n"
        "Original output:",
        file=sys.stderr,
    )
    if result.stdout:
        print(result.stdout, end="", file=sys.stderr)
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    raise SystemExit(1)


def powershell_quote(value: str) -> str:
    if not value:
        return "''"
    if re.fullmatch(r"[A-Za-z0-9_./:\\-]+", value):
        return value
    return "'" + value.replace("'", "''") + "'"


def idf_command(action: str, target: str, port: str, trailing_args: list[str]) -> str:
    args = ["idf.py", "-C", str(PROJECT_ROOT)]
    port_actions = {"flash", "monitor", "flash-monitor", "erase-flash"}
    if port and action in port_actions:
        args.extend(["-p", port])

    action_args = {
        "build": ["build"],
        "flash": ["flash"],
        "monitor": ["monitor"],
        "flash-monitor": ["flash", "monitor"],
        "menuconfig": ["menuconfig"],
        "clean": ["clean"],
        "fullclean": ["fullclean"],
        "set-target": ["set-target", target],
        "erase-flash": ["erase-flash"],
        "size": ["size"],
    }
    if action not in action_args:
        fail(f"Unsupported action: {action}")
    args.extend(action_args[action])
    args.extend(trailing_args)
    return " ".join(powershell_quote(arg) for arg in args)


def doctor(eim_path: str, version: str, target: str, port: str) -> None:
    section("Checking EIM")
    print(f"eim             : {eim_path}")
    print(f"eim log         : {LOG_FILE}")
    install = resolve_idf(eim_path, version, target)
    print(f"idf version     : {install.version}")
    print(f"idf path        : {install.path}")
    runtime_version = validate_idf_runtime(eim_path, install, target).splitlines()[-1]
    print(f"idf.py          : {runtime_version}")
    print(f"target          : {target}")
    print(f"port            : {port or '<auto>'}")


def list_idfs(eim_path: str) -> None:
    installs = installed_idfs(eim_path)
    if not installs:
        section("Installed ESP-IDF versions")
        print("<none>")
        return

    section("Installed ESP-IDF versions")
    for install in installs:
        selected = " (selected)" if install.selected else ""
        print(f"- {install.version}{selected}")
        print(f"  [{install.path}]")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "action",
        nargs="?",
        default="doctor",
        choices=(
            "doctor",
            "build",
            "flash",
            "monitor",
            "flash-monitor",
            "menuconfig",
            "set-target",
            "clean",
            "fullclean",
            "erase-flash",
            "size",
            "eim-list",
            "eim-install",
            "eim-fix",
            "eim-select",
        ),
    )
    parser.add_argument("--version", help="ESP-IDF version managed by EIM")
    parser.add_argument("--target", default=DEFAULT_TARGET)
    parser.add_argument("--port", nargs="?", default=os.environ.get("ESPPORT", ""), const="")
    parser.add_argument("--install-path", type=Path, help="Full installation path (overrides --install-base)")
    parser.add_argument("--install-base", help="Base directory for ESP-IDF installations (e.g. D:/esp)")
    args, idf_args = parser.parse_known_args()
    args.idf_args = idf_args[1:] if idf_args[:1] == ["--"] else idf_args
    return args


def main() -> None:
    args = parse_args()
    eim_path = command_path("eim")
    version_required_actions = {
        "doctor",
        "build",
        "flash",
        "monitor",
        "flash-monitor",
        "menuconfig",
        "set-target",
        "clean",
        "fullclean",
        "erase-flash",
        "size",
        "eim-install",
        "eim-fix",
        "eim-select",
    }

    if args.action in version_required_actions and not args.version:
        fail(f"--version is required for '{args.action}'.")

    if args.action == "eim-list":
        list_idfs(eim_path)
        return

    if args.action == "doctor":
        doctor(eim_path, args.version, args.target, args.port)
        return

    if args.action == "eim-install":
        # Check if already installed and valid before running install
        installs = installed_idfs(eim_path)
        existing = next((i for i in installs if i.version == args.version), None)

        install_path = args.install_path
        install_base = args.install_base

        if existing is not None:
            # Validate existing installation
            idf_py = existing.path / "tools" / "idf.py"
            if idf_py.is_file():
                result = run_eim(eim_path, "run", "idf.py --version", args.version, capture=True)
                if result.returncode == 0:
                    section(f"ESP-IDF {args.version} already installed and valid")
                    print(f"Path: {existing.path}")
                    print(f"Version: {result.stdout.strip()}")
                    raise SystemExit(0)

            # Installation exists but is broken (idf.py missing or runtime validation failed)
            section(f"Existing installation at {existing.path} is broken")
            print("Cleaning and reinstalling...", file=sys.stderr)
            install_path = install_path or existing.path
            if install_path.exists():
                shutil.rmtree(install_path)
        else:
            # Version not in eim list, but path might exist from a failed install
            if install_path is None and install_base is not None:
                install_path = default_install_path(args.version, base=install_base)

            if install_path is not None and install_path.exists():
                section(f"Cleaning existing directory at {install_path}")
                shutil.rmtree(install_path)
                # Don't pass -p to EIM - let it use its default path logic
                install_path = None

        # Install (first time or reinstall due to broken installation)
        section(f"Installing ESP-IDF {args.version}")
        command = ["install", "-i", args.version, "-t", args.target, "-n", "true"]
        # Only pass -p if explicitly provided by user (not auto-detected default)
        if args.install_path is not None:
            command.extend(["-p", str(args.install_path)])
        elif args.install_base is not None:
            # EIM expects base path, not full path
            command.extend(["-p", str(Path(args.install_base) / args.version.lstrip("v"))])
        raise SystemExit(run_eim(eim_path, *command).returncode)

    if args.action == "eim-select":
        raise SystemExit(run_eim(eim_path, "select", args.version).returncode)

    install = resolve_idf(eim_path, args.version, args.target)
    if args.action == "eim-fix":
        path = args.install_path or install.path
        raise SystemExit(
            run_eim(eim_path, "fix", "-p", str(path), "-i", install.version, "-t", args.target).returncode
        )

    validate_idf_runtime(eim_path, install, args.target)
    command = idf_command(args.action, args.target, args.port, args.idf_args)
    section(f"Using ESP-IDF {install.version} from {install.path}")
    print(command)
    raise SystemExit(run_eim(eim_path, "run", command, install.version).returncode)


if __name__ == "__main__":
    main()
