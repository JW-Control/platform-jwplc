#!/usr/bin/env python3
"""
JWPLC package release helper.

This script prepares a JWPLC Arduino package release by:

1. Reusing or generating the ZIP archive.
2. Calculating SHA-256 and size.
3. Updating package_jwplc_index_dev.json.
4. Updating package_jwplc_index.json only when requested or when channel is stable.
5. Emitting GitHub Actions outputs.

It intentionally does NOT create the GitHub Release. The workflow does that after
this script has produced the archive and updated the package indexes.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import re
import sys
import zipfile
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


REPO_DEFAULT = "JW-Control/platform-jwplc"
PACKAGE_NAME = "jwplc"
PLATFORM_NAME = "JW Control ESP32 Boards"
ARCHITECTURE = "esp32"


class ReleaseError(RuntimeError):
    pass


def log(msg: str) -> None:
    print(f"[jwplc-release] {msg}")


def fail(msg: str) -> None:
    raise ReleaseError(msg)


def detect_channel(version: str, channel: str) -> str:
    if channel != "auto":
        return channel
    v = version.lower()
    if "-alpha" in v:
        return "alpha"
    if "-beta" in v:
        return "beta"
    return "stable"


def is_prerelease_channel(channel: str) -> bool:
    return channel in {"alpha", "beta", "dev", "rc"}


def parse_version_for_sort(version: str) -> Tuple[Tuple[int, int, int], int, int, str]:
    """Return sortable key for semver-like JWPLC versions.

    Order intended for descending sort:
      2.1.0 > 2.1.0-beta.1 > 2.1.0-alpha.2 > 2.1.0-alpha.1
    """
    match = re.match(r"^(\d+)\.(\d+)\.(\d+)(?:-([A-Za-z]+)\.(\d+))?$", version)
    if not match:
        return ((0, 0, 0), -1, -1, version)

    major, minor, patch = map(int, match.group(1, 2, 3))
    pre_name = match.group(4)
    pre_num = int(match.group(5) or 0)

    rank = {
        None: 4,
        "rc": 3,
        "beta": 2,
        "alpha": 1,
        "dev": 0,
    }.get(pre_name.lower() if pre_name else None, 0)

    return ((major, minor, patch), rank, pre_num, version)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def should_skip_in_zip(path: Path) -> bool:
    parts = set(path.parts)
    if ".git" in parts:
        return True
    if "__pycache__" in parts:
        return True
    if path.name.lower() in {"thumbs.db", ".ds_store"}:
        return True
    return False


def create_zip_from_folder(source_folder: Path, zip_file: Path, archive_root_mode: str) -> None:
    if not source_folder.exists() or not source_folder.is_dir():
        fail(f"Source folder does not exist or is not a directory: {source_folder}")

    zip_file.parent.mkdir(parents=True, exist_ok=True)

    if archive_root_mode not in {"contents", "folder"}:
        fail("archive_root_mode must be 'contents' or 'folder'")

    log(f"Generating ZIP: {zip_file}")
    log(f"Source folder : {source_folder}")
    log(f"Archive mode  : {archive_root_mode}")

    with zipfile.ZipFile(zip_file, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for file_path in sorted(source_folder.rglob("*")):
            if not file_path.is_file():
                continue
            if should_skip_in_zip(file_path):
                continue

            if archive_root_mode == "contents":
                arcname = file_path.relative_to(source_folder)
            else:
                arcname = Path(source_folder.name) / file_path.relative_to(source_folder)

            zf.write(file_path, arcname.as_posix())


def resolve_zip(version: str, source_folder: Path, zip_file_input: str, recreate_zip: bool, archive_root_mode: str) -> Path:
    zip_name = f"jwplc-esp32-{version}.zip"
    repo_default_zip = Path("JWPLC") / zip_name
    dist_zip = Path("dist") / zip_name

    explicit_zip = Path(zip_file_input) if zip_file_input else None

    if explicit_zip is not None:
        zip_file = explicit_zip
        if zip_file.exists() and not recreate_zip:
            log(f"Existing ZIP found and will be reused: {zip_file}")
            return zip_file
        if zip_file.exists() and recreate_zip:
            log(f"Existing ZIP will be overwritten because recreate_zip=true: {zip_file}")
            zip_file.unlink()
        create_zip_from_folder(source_folder, zip_file, archive_root_mode)
        return zip_file

    # No explicit zip path.
    # Safe default: reuse JWPLC/<zip> if present, otherwise generate into dist/.
    if repo_default_zip.exists() and not recreate_zip:
        log(f"Existing repository ZIP found and will be reused: {repo_default_zip}")
        return repo_default_zip

    if repo_default_zip.exists() and recreate_zip:
        log(f"Repository ZIP exists but will NOT be overwritten by default: {repo_default_zip}")
        log(f"A fresh ZIP will be generated in: {dist_zip}")

    create_zip_from_folder(source_folder, dist_zip, archive_root_mode)
    return dist_zip


def load_json(path: Path) -> Dict[str, Any]:
    if not path.exists():
        fail(f"JSON file not found: {path}")
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def save_json(path: Path, data: Dict[str, Any]) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")


def get_package(data: Dict[str, Any]) -> Dict[str, Any]:
    packages = data.get("packages")
    if not isinstance(packages, list) or not packages:
        fail("Invalid package index: missing packages[]")

    for package in packages:
        if package.get("name") == PACKAGE_NAME:
            return package

    fail(f"Package '{PACKAGE_NAME}' not found in index")


def get_platforms(data: Dict[str, Any]) -> List[Dict[str, Any]]:
    package = get_package(data)
    platforms = package.get("platforms")
    if not isinstance(platforms, list):
        fail("Invalid package index: missing platforms[]")
    return platforms


def find_template_platform(dev_data: Dict[str, Any], public_data: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    for data in [dev_data, public_data]:
        if not data:
            continue
        for platform in get_platforms(data):
            if platform.get("name") == PLATFORM_NAME and platform.get("architecture") == ARCHITECTURE:
                return copy.deepcopy(platform)

    fail("Could not find a platform template to clone")


def build_platform_entry(
    template: Dict[str, Any],
    version: str,
    repo_full_name: str,
    zip_file: Path,
    checksum: str,
    size: int,
) -> Dict[str, Any]:
    zip_name = zip_file.name
    tag = f"v{version}"
    url = f"https://github.com/{repo_full_name}/releases/download/{tag}/{zip_name}"

    entry = copy.deepcopy(template)
    entry["name"] = PLATFORM_NAME
    entry["architecture"] = ARCHITECTURE
    entry["version"] = version
    entry["category"] = entry.get("category", "Contributed")
    entry["url"] = url
    entry["archiveFileName"] = zip_name
    entry["checksum"] = f"SHA-256:{checksum}"
    entry["size"] = str(size)

    return entry


def upsert_platform(
    data: Dict[str, Any],
    entry: Dict[str, Any],
    replace_existing: bool,
    fail_if_exists: bool,
    latest_only: bool = False,
) -> bool:
    platforms = get_platforms(data)
    version = entry["version"]

    existing_indexes = [i for i, platform in enumerate(platforms) if platform.get("version") == version]

    if existing_indexes and fail_if_exists and not replace_existing:
        fail(f"Version already exists in index: {version}. Use replace_existing_index_entry=true to update it.")

    changed = False

    if latest_only:
        if platforms != [entry]:
            package = get_package(data)
            package["platforms"] = [entry]
            changed = True
        return changed

    if existing_indexes:
        for i in reversed(existing_indexes):
            del platforms[i]
        platforms.append(entry)
        changed = True
    else:
        platforms.append(entry)
        changed = True

    platforms.sort(key=lambda p: parse_version_for_sort(str(p.get("version", "0.0.0"))), reverse=True)
    return changed


def determine_update_public(channel: str, update_public_index: str) -> bool:
    if update_public_index == "yes":
        return True
    if update_public_index == "no":
        return False
    return channel == "stable"


def write_github_output(outputs: Dict[str, str]) -> None:
    output_path = os.environ.get("GITHUB_OUTPUT")
    if not output_path:
        return

    with open(output_path, "a", encoding="utf-8") as f:
        for key, value in outputs.items():
            f.write(f"{key}={value}\n")


def prepare_release(args: argparse.Namespace) -> int:
    version = args.version.strip().lstrip("v")
    if not re.match(r"^\d+\.\d+\.\d+(?:-[A-Za-z]+\.\d+)?$", version):
        fail(f"Invalid version format: {args.version}. Expected 2.1.0, 2.1.0-alpha.1 or 2.1.0-beta.1")

    channel = detect_channel(version, args.channel)
    if channel not in {"alpha", "beta", "stable", "dev", "rc"}:
        fail(f"Invalid channel: {channel}")

    update_public = determine_update_public(channel, args.update_public_index)

    log(f"Version          : {version}")
    log(f"Channel          : {channel}")
    log(f"Update dev index : yes")
    log(f"Update public    : {'yes' if update_public else 'no'}")

    source_folder = Path(args.source_folder)
    zip_file = resolve_zip(
        version=version,
        source_folder=source_folder,
        zip_file_input=args.zip_file,
        recreate_zip=args.recreate_zip,
        archive_root_mode=args.archive_root_mode,
    )

    if not zip_file.exists():
        fail(f"ZIP file was not created/found: {zip_file}")

    checksum = sha256_file(zip_file)
    size = zip_file.stat().st_size

    log(f"ZIP              : {zip_file}")
    log(f"Archive name     : {zip_file.name}")
    log(f"SHA-256          : {checksum}")
    log(f"Size             : {size}")

    dev_index_path = Path(args.dev_index)
    public_index_path = Path(args.public_index)

    dev_data = load_json(dev_index_path)
    public_data = load_json(public_index_path) if public_index_path.exists() else None

    template = find_template_platform(dev_data, public_data)
    entry = build_platform_entry(
        template=template,
        version=version,
        repo_full_name=args.repo_full_name,
        zip_file=zip_file,
        checksum=checksum,
        size=size,
    )

    replace_existing = args.replace_existing_index_entry
    fail_if_exists = args.fail_if_version_exists

    log(f"Updating dev index: {dev_index_path}")
    upsert_platform(
        dev_data,
        entry,
        replace_existing=replace_existing,
        fail_if_exists=fail_if_exists,
        latest_only=False,
    )
    save_json(dev_index_path, dev_data)

    if update_public:
        if public_data is None:
            fail(f"Public index not found: {public_index_path}")
        if channel != "stable" and args.update_public_index != "yes":
            fail("Refusing to update public index for non-stable channel unless update_public_index=yes")

        log(f"Updating public index: {public_index_path}")
        upsert_platform(
            public_data,
            entry,
            replace_existing=replace_existing,
            fail_if_exists=fail_if_exists,
            latest_only=(args.public_strategy == "latest-only"),
        )
        save_json(public_index_path, public_data)
    else:
        log("Public index skipped")

    prerelease = is_prerelease_channel(channel)
    title = args.release_title.strip() if args.release_title.strip() else f"v{version} - JWPLC Arduino package"

    outputs = {
        "version": version,
        "tag": f"v{version}",
        "channel": channel,
        "zip_file": zip_file.as_posix(),
        "zip_name": zip_file.name,
        "checksum": f"SHA-256:{checksum}",
        "size": str(size),
        "prerelease": "true" if prerelease else "false",
        "update_public": "true" if update_public else "false",
        "release_title": title,
    }
    write_github_output(outputs)

    log("Preparation complete")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Prepare JWPLC package release")
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("prepare", help="Prepare ZIP and package indexes")
    p.add_argument("--version", required=True, help="Version without leading v. Example: 2.1.0-alpha.1")
    p.add_argument("--channel", default="auto", choices=["auto", "alpha", "beta", "stable", "dev", "rc"])
    p.add_argument("--source-folder", default="JWPLC/2.1.0", help="Folder to zip when a ZIP must be generated")
    p.add_argument("--zip-file", default="", help="Optional ZIP path. If omitted, JWPLC/jwplc-esp32-<version>.zip is reused if present; otherwise dist/<zip> is generated")
    p.add_argument("--recreate-zip", action="store_true", help="Regenerate the ZIP. With no explicit --zip-file, output goes to dist/ and does not overwrite repository ZIPs")
    p.add_argument("--archive-root-mode", default="contents", choices=["contents", "folder"], help="Zip source folder contents or include the folder itself")
    p.add_argument("--repo-full-name", default=REPO_DEFAULT)
    p.add_argument("--dev-index", default="JWPLC/package_jwplc_index_dev.json")
    p.add_argument("--public-index", default="JWPLC/package_jwplc_index.json")
    p.add_argument("--update-public-index", default="auto", choices=["auto", "yes", "no"], help="auto updates public index only for stable releases")
    p.add_argument("--public-strategy", default="latest-only", choices=["latest-only", "append"], help="For public index, latest-only keeps one visible stable platform entry")
    p.add_argument("--fail-if-version-exists", action="store_true", help="Fail if the version already exists in a package index")
    p.add_argument("--replace-existing-index-entry", action="store_true", help="Replace existing platform entry for the same version")
    p.add_argument("--release-title", default="", help="Optional release title")

    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        if args.command == "prepare":
            return prepare_release(args)
        fail(f"Unknown command: {args.command}")
    except ReleaseError as exc:
        print(f"[jwplc-release][ERROR] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
