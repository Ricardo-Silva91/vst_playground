#!/usr/bin/env python3
"""
vst_playground installer / updater
Fetches the latest builds from GitHub releases and installs them
to the system VST3 folder. Supports macOS and Windows.
"""

import sys
import os
import subprocess
import platform
import tempfile
import shutil
import zipfile
import json
from datetime import datetime, timezone
from pathlib import Path

# ── Auto-install requests if missing ─────────────────────────────────────────

try:
    import requests
except ImportError:
    print("Installing 'requests' package...")
    installed = False

    # Strategy 1: normal pip
    try:
        subprocess.check_call(
            [sys.executable, "-m", "pip", "install", "requests", "--quiet"],
            stderr=subprocess.DEVNULL
        )
        installed = True
    except subprocess.CalledProcessError:
        pass

    # Strategy 2: --break-system-packages (Homebrew / externally-managed envs)
    if not installed:
        try:
            subprocess.check_call(
                [sys.executable, "-m", "pip", "install", "requests",
                 "--break-system-packages", "--quiet"],
                stderr=subprocess.DEVNULL
            )
            installed = True
        except subprocess.CalledProcessError:
            pass

    # Strategy 3: --user flag
    if not installed:
        try:
            subprocess.check_call(
                [sys.executable, "-m", "pip", "install", "requests",
                 "--user", "--quiet"],
                stderr=subprocess.DEVNULL
            )
            installed = True
        except subprocess.CalledProcessError:
            pass

    # Strategy 4: create a venv, install there, and re-launch inside it
    if not installed:
        venv_dir = Path(tempfile.mkdtemp(prefix="lizard_vst_venv_"))
        print("  Creating a temporary virtual environment...")
        subprocess.check_call(
            [sys.executable, "-m", "venv", str(venv_dir)],
            stderr=subprocess.DEVNULL
        )
        venv_python = (venv_dir / "bin" / "python") if platform.system() != "Windows" \
                      else (venv_dir / "Scripts" / "python.exe")
        subprocess.check_call(
            [str(venv_python), "-m", "pip", "install", "requests", "--quiet"],
            stderr=subprocess.DEVNULL
        )
        # Re-launch the script using the venv's Python and exit this process
        os.execv(str(venv_python), [str(venv_python)] + sys.argv)

    if installed:
        import requests

# ── Config ────────────────────────────────────────────────────────────────────

GITHUB_REPO  = "Ricardo-Silva91/vst_playground"
GITHUB_API   = f"https://api.github.com/repos/{GITHUB_REPO}/releases"
LATEST_TAG_SUFFIX = "-latest"

SYSTEM = platform.system()  # "Darwin" | "Windows" | "Linux"

if SYSTEM == "Darwin":
    VST3_DIR = Path("/Library/Audio/Plug-Ins/VST3")
elif SYSTEM == "Windows":
    VST3_DIR = Path(os.environ.get("COMMONPROGRAMFILES", r"C:\Program Files\Common Files")) / "VST3"
else:
    print("Unsupported platform. This script supports macOS and Windows only.")
    sys.exit(1)

# ── Colours (ANSI — disabled on Windows unless terminal supports it) ──────────

USE_COLOUR = SYSTEM != "Windows" or os.environ.get("TERM") == "xterm"

def c(code, text):
    return f"\033[{code}m{text}\033[0m" if USE_COLOUR else text

GREEN  = lambda t: c("32", t)
YELLOW = lambda t: c("33", t)
CYAN   = lambda t: c("36", t)
BOLD   = lambda t: c("1",  t)
DIM    = lambda t: c("2",  t)
RED    = lambda t: c("31", t)

# ── Elevation ─────────────────────────────────────────────────────────────────

def is_elevated():
    if SYSTEM == "Darwin":
        return os.geteuid() == 0
    elif SYSTEM == "Windows":
        import ctypes
        try:
            return ctypes.windll.shell32.IsUserAnAdmin()
        except Exception:
            return False

def relaunch_elevated():
    """Re-launch this script with elevated privileges and exit."""
    script = os.path.abspath(__file__)
    if SYSTEM == "Darwin":
        print("Requesting sudo access to install to system VST3 folder...")
        args = ["sudo", sys.executable] + [script] + sys.argv[1:]
        os.execvp("sudo", args)  # replaces current process
    elif SYSTEM == "Windows":
        import ctypes
        params = f'"{sys.executable}" "{script}"'
        ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, f'"{script}"', None, 1)
        sys.exit(0)

# ── GitHub API ────────────────────────────────────────────────────────────────

def fetch_releases():
    """
    Return a dict of { plugin_name: { "tag": str, "published_at": datetime,
    "asset_url": str, "asset_name": str } }
    Only includes releases whose tag ends with -latest.
    """
    plugins = {}
    page = 1
    while True:
        resp = requests.get(
            GITHUB_API,
            params={"per_page": 100, "page": page},
            headers={"Accept": "application/vnd.github+json"},
            timeout=15,
        )
        resp.raise_for_status()
        data = resp.json()
        if not data:
            break

        for release in data:
            tag = release.get("tag_name", "")
            if not tag.endswith(LATEST_TAG_SUFFIX):
                continue

            plugin_name = tag[: -len(LATEST_TAG_SUFFIX)]

            # Pick the right asset for this platform
            asset = _pick_asset(release.get("assets", []), plugin_name)
            if asset is None:
                continue

            published_at = datetime.fromisoformat(
                release["published_at"].replace("Z", "+00:00")
            )

            plugins[plugin_name] = {
                "tag":          tag,
                "published_at": published_at,
                "asset_url":    asset["browser_download_url"],
                "asset_name":   asset["name"],
            }

        page += 1
        # If we got a full page there might be more; if less, we're done
        if len(data) < 100:
            break

    return plugins

def _pick_asset(assets, plugin_name):
    """Choose the zip asset that matches the current platform."""
    # Asset names in the workflow follow the pattern:
    #   plugin_name.vst3.zip  (both platforms, distinguished by release tag or asset name)
    # The workflow currently produces one zip per platform per release.
    # We prefer an asset whose name contains the platform hint; fall back to any zip.
    platform_hint = "mac" if SYSTEM == "Darwin" else "windows"
    zips = [a for a in assets if a["name"].endswith(".zip")]
    for a in zips:
        if platform_hint in a["name"].lower():
            return a
    # Fallback: first zip found
    return zips[0] if zips else None

# ── Installed plugin detection ────────────────────────────────────────────────

def find_installed(plugin_name):
    """
    Return the mtime (UTC datetime) of the installed .vst3 bundle,
    or None if not installed.
    Handles both directory bundles (Mac) and flat directories (Windows).
    """
    # Common name patterns: "Plugin Name.vst3", "plugin_name.vst3"
    candidates = [
        VST3_DIR / f"{plugin_name}.vst3",
        VST3_DIR / f"{plugin_name.replace('_', ' ').title()}.vst3",
    ]
    # Also scan the folder for anything matching loosely
    if VST3_DIR.exists():
        for entry in VST3_DIR.iterdir():
            if entry.suffix == ".vst3":
                # Mac: expect a directory bundle; Windows: expect a plain file
                is_right_type = entry.is_dir() if SYSTEM == "Darwin" else entry.is_file()
                if not is_right_type:
                    continue
                stem_normalised = entry.stem.lower().replace(" ", "_")
                if stem_normalised == plugin_name.lower():
                    candidates.insert(0, entry)

    for path in candidates:
        if path.exists():
            mtime = os.path.getmtime(path)
            return datetime.fromtimestamp(mtime, tz=timezone.utc)
    return None

# ── Install ───────────────────────────────────────────────────────────────────

def install_plugin(plugin_name, info):
    """Download, extract, and install a single plugin. Returns True on success."""
    asset_url  = info["asset_url"]
    asset_name = info["asset_name"]

    print(f"\n  {CYAN('→')} Downloading {asset_name} ...")

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        zip_path = tmp_path / asset_name

        # Download
        resp = requests.get(asset_url, stream=True, timeout=60)
        resp.raise_for_status()
        total = int(resp.headers.get("content-length", 0))
        downloaded = 0
        with open(zip_path, "wb") as f:
            for chunk in resp.iter_content(chunk_size=65536):
                f.write(chunk)
                downloaded += len(chunk)
                if total:
                    pct = downloaded * 100 // total
                    print(f"\r  {DIM(f'{pct:3d}%')} [{('#' * (pct // 5)).ljust(20)}]", end="", flush=True)
        print()  # newline after progress

        # Extract
        print(f"  {CYAN('→')} Extracting ...")
        with zipfile.ZipFile(zip_path, "r") as zf:
            zf.extractall(tmp_path)

        # Find the .vst3 bundle inside the extracted content
        # On Mac: copy the whole .vst3 directory bundle
        # On Windows: dig into Contents/x86_64-win/ and copy just the inner .vst3 file
        if SYSTEM == "Darwin":
            vst3_bundles = [p for p in tmp_path.glob("**/*.vst3") if p.is_dir()]
            if not vst3_bundles:
                print(f"  {RED('✗')} No .vst3 bundle found in archive. Skipping.")
                return False
            bundle_src = vst3_bundles[0]

            bundle_dst = VST3_DIR / bundle_src.name
            VST3_DIR.mkdir(parents=True, exist_ok=True)

            if bundle_dst.exists():
                shutil.rmtree(bundle_dst)

            print(f"  {CYAN('→')} Installing to {bundle_dst} ...")
            shutil.copytree(bundle_src, bundle_dst)

        elif SYSTEM == "Windows":
            # Find the outer .vst3 folder, then locate the inner .vst3 file
            outer_bundles = [p for p in tmp_path.glob("**/*.vst3") if p.is_dir()]
            if not outer_bundles:
                print(f"  {RED('✗')} No .vst3 bundle folder found in archive. Skipping.")
                return False
            outer_bundle = outer_bundles[0]

            win_dir = outer_bundle / "Contents" / "x86_64-win"
            if not win_dir.exists():
                print(f"  {RED('✗')} Expected Contents/x86_64-win/ not found in bundle. Skipping.")
                return False

            inner_files = list(win_dir.glob("*.vst3"))
            if not inner_files:
                print(f"  {RED('✗')} No .vst3 file found inside Contents/x86_64-win/. Skipping.")
                return False
            bundle_src = inner_files[0]

            bundle_dst = VST3_DIR / bundle_src.name
            VST3_DIR.mkdir(parents=True, exist_ok=True)

            if bundle_dst.exists():
                bundle_dst.unlink()

            print(f"  {CYAN('→')} Installing to {bundle_dst} ...")
            shutil.copy2(bundle_src, bundle_dst)

        # macOS: clear Gatekeeper quarantine
        if SYSTEM == "Darwin":
            print(f"  {CYAN('→')} Clearing Gatekeeper quarantine ...")
            result = subprocess.run(
                ["xattr", "-cr", str(bundle_dst)],
                capture_output=True, text=True
            )
            if result.returncode != 0:
                print(f"  {YELLOW('⚠')} xattr warning: {result.stderr.strip()}")

    print(f"  {GREEN('✓')} {plugin_name} installed successfully.")
    return True

# ── Display ───────────────────────────────────────────────────────────────────

STATUS_UP_TO_DATE    = "up to date"
STATUS_UPDATE        = "UPDATE AVAILABLE"
STATUS_NOT_INSTALLED = "not installed"

def build_status_table(releases):
    """
    Returns list of dicts with display info for each plugin.
    """
    rows = []
    for plugin_name, info in sorted(releases.items()):
        installed_mtime = find_installed(plugin_name)
        if installed_mtime is None:
            status = STATUS_NOT_INSTALLED
        elif info["published_at"] > installed_mtime:
            status = STATUS_UPDATE
        else:
            status = STATUS_UP_TO_DATE

        rows.append({
            "name":         plugin_name,
            "info":         info,
            "installed":    installed_mtime,
            "status":       status,
        })
    return rows

def print_table(rows):
    name_w = max(len(r["name"]) for r in rows) + 2
    print()
    print(f"  {'#':<4} {'Plugin':<{name_w}} {'Released':<22} Status")
    print(f"  {'─'*4} {'─'*name_w} {'─'*22} {'─'*20}")
    for i, row in enumerate(rows):
        num     = f"[{i+1}]"
        name    = row["name"]
        pub     = row["info"]["published_at"].strftime("%Y-%m-%d %H:%M UTC")
        status  = row["status"]

        if status == STATUS_UP_TO_DATE:
            status_str = GREEN(f"✓ {status}")
        elif status == STATUS_UPDATE:
            status_str = YELLOW(f"↑ {status}")
        else:
            status_str = DIM(f"  {status}")

        print(f"  {BOLD(num):<4} {name:<{name_w}} {DIM(pub):<22} {status_str}")
    print()

# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    print()
    print(BOLD("  vst_playground installer"))
    print(DIM(f"  github.com/{GITHUB_REPO}"))
    print()

    # Elevation check
    if not is_elevated():
        relaunch_elevated()
        return  # unreachable on Mac (execvp replaces process), reached on Windows

    # Fetch releases
    print("  Fetching plugin list from GitHub releases ...")
    try:
        releases = fetch_releases()
    except requests.RequestException as e:
        print(f"\n  {RED('✗')} Could not reach GitHub API: {e}")
        sys.exit(1)

    if not releases:
        print(f"\n  {YELLOW('⚠')} No plugins found. Make sure builds have run at least once.")
        sys.exit(0)

    # Build and display status table
    rows = build_status_table(releases)
    print_table(rows)

    # Selection prompt
    updates_available = [r for r in rows if r["status"] == STATUS_UPDATE]
    not_installed     = [r for r in rows if r["status"] == STATUS_NOT_INSTALLED]

    print("  Select plugins to install / update:")
    print(f"  {BOLD('1–' + str(len(rows)))}  individual plugin number(s), comma-separated")
    print(f"  {BOLD('a')}    all plugins")
    if updates_available:
        print(f"  {BOLD('u')}    all plugins with updates ({len(updates_available)} available)")
    print(f"  {BOLD('q')}    quit")
    print()

    choice = input("  > ").strip().lower()
    print()

    if choice == "q" or choice == "":
        print("  Goodbye.\n")
        sys.exit(0)
    elif choice == "a":
        selected = rows
    elif choice == "u":
        selected = updates_available
        if not selected:
            print(f"  {GREEN('✓')} Everything is up to date.\n")
            sys.exit(0)
    else:
        indices = []
        for part in choice.split(","):
            part = part.strip()
            if part.isdigit():
                idx = int(part) - 1
                if 0 <= idx < len(rows):
                    indices.append(idx)
                else:
                    print(f"  {YELLOW('⚠')} Skipping invalid number: {part}")
            else:
                print(f"  {YELLOW('⚠')} Skipping unrecognised input: {part}")
        if not indices:
            print(f"  {RED('✗')} No valid selection. Exiting.\n")
            sys.exit(1)
        selected = [rows[i] for i in indices]

    # Install
    succeeded = []
    failed    = []
    for row in selected:
        ok = install_plugin(row["name"], row["info"])
        (succeeded if ok else failed).append(row["name"])

    # Summary
    print()
    print(BOLD("  Summary"))
    print(f"  {'─'*40}")
    if succeeded:
        print(f"  {GREEN('✓')} Installed: {', '.join(succeeded)}")
    if failed:
        print(f"  {RED('✗')} Failed:    {', '.join(failed)}")
    print()
    print(DIM("  Remember to rescan plugins in your DAW."))
    if SYSTEM == "Windows":
        print(DIM("  FL Studio: Options → Manage plugins → Find more plugins"))
    print()

if __name__ == "__main__":
    main()