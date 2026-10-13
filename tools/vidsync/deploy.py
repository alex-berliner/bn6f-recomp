#!/usr/bin/env python3
"""deploy.py [--pc HOST]... [--json]: put this tree's vidsync on the compute PCs (docs/vidsync/README.md).

It tars the part of the recomp tree vidsync needs there (tools/vidsync, tools/gamepaths.py, tools/paths.env,
tools/mgba_run.c, tools/Makefile; the files git tracks or would, so no build output), adds bn6f's symbols as
`syms.py --json` writes them (the PCs have no bn6f: VIDSYNC_SYMS points there) and a DEPLOYED.json saying
which commit it is, copies it and remote/launch.ps1 to C:/Users/alex on each PC, and installs it in WSL as
~/vidsync/deploys/STAMP with the link ~/vidsync/recomp pointing at it (remote/install.sh: link the PC's own
data in, build mgba_run and emu_server, move the link; running jobs keep their own tree). The PCs' data
stays where it is (the LAN is a slow WireGuard link: never copy video over it). Restarting the
driver or the worker server is separate (the vidsync-run workflow, or remote/launch.ps1).

Default PCs: 192.168.1.175 (the driver) and 192.168.1.167 (the worker server), in parallel. --json prints
one JSON object per PC: {"pc", "ok", "deployed", "output"}."""
from __future__ import annotations

import argparse
import io
import json
import os
import subprocess
import sys
import tarfile
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PARTS = ("tools/vidsync", "tools/gamepaths.py", "tools/paths.env", "tools/mgba_run.c", "tools/Makefile")
PCS = ("192.168.1.175", "192.168.1.167")
USER = "alex"
WINDOWS_HOME = "C:/Users/alex"              # where scp puts files; /mnt/c/Users/alex in WSL
TGZ = "vidsync_deploy.tgz"
LAUNCH = "vidsync_launch.ps1"
SSH = ("ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=20")


def git(*args: str) -> str:
    return subprocess.run(["git", "-C", ROOT, *args], capture_output=True, text=True, check=True).stdout


def files() -> list[str]:
    """The files of PARTS git tracks or would track (not ignored), that exist."""
    listed = git("ls-files", "-co", "--exclude-standard", "--", *PARTS).splitlines()
    return sorted(path for path in set(listed) if os.path.isfile(os.path.join(ROOT, path)))


def symbols_json() -> bytes:
    """bn6f's symbols vidsync uses (syms.py --json; with VIDSYNC_SYMS set, the file it names)."""
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "syms.json")
        subprocess.run([sys.executable, os.path.join(ROOT, "tools/vidsync/syms.py"), "--json", out], check=True)
        with open(out, "rb") as fh:
            return fh.read()


def package() -> tuple[bytes, dict[str, object]]:
    """The tarball, and what it says about itself (DEPLOYED.json)."""
    paths = files()
    deployed: dict[str, object] = {
        "commit": git("rev-parse", "HEAD").strip(),
        "uncommitted": bool(git("status", "--porcelain", "--", *PARTS).strip()),
        "time": time.strftime("%Y-%m-%d %H:%M:%S"),
        "files": len(paths),
    }
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz") as tar:
        for path in paths:
            tar.add(os.path.join(ROOT, path), arcname=path, recursive=False)
        for name, data in (("vidsync-syms.json", symbols_json()), ("DEPLOYED.json", json.dumps(deployed).encode())):
            info = tarfile.TarInfo(name)
            info.size, info.mtime = len(data), int(time.time())
            tar.addfile(info, io.BytesIO(data))
    return buffer.getvalue(), deployed


def wsl(pc: str, script: str, timeout: float = 600) -> subprocess.CompletedProcess[str]:
    """Run a bash script in the PC's WSL (given on stdin: no quoting through cmd.exe)."""
    return subprocess.run([*SSH, f"{USER}@{pc}", "wsl -d Ubuntu -- bash -s"], input=script, capture_output=True,
                          text=True, timeout=timeout)


def deploy_to(pc: str, tgz: str) -> dict[str, object]:
    for local, remote in ((tgz, TGZ), (os.path.join(ROOT, "tools/vidsync/remote/launch.ps1"), LAUNCH)):
        copied = subprocess.run(["scp", "-q", "-o", "BatchMode=yes", local, f"{USER}@{pc}:{WINDOWS_HOME}/{remote}"],
                                capture_output=True, text=True, timeout=600)
        if copied.returncode != 0:
            return {"pc": pc, "ok": False, "output": f"scp {remote}: {copied.stderr.strip()}"}
    win = "/mnt/c/" + WINDOWS_HOME[3:]
    script = (f"set -e; tmp=$(mktemp -d); tar xzf {win}/{TGZ} -C $tmp tools/vidsync/remote/install.sh; "
              f"bash $tmp/tools/vidsync/remote/install.sh {win}/{TGZ}; rm -rf $tmp\n")
    done = wsl(pc, script)
    output = (done.stdout + done.stderr).replace("\r", "").replace("\0", "").strip()
    return {"pc": pc, "ok": done.returncode == 0, "output": output[-3000:]}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--pc", action="append", help="a PC's address (default: both)")
    ap.add_argument("--json", action="store_true", help="one JSON object per PC")
    a = ap.parse_args()
    data, deployed = package()
    with tempfile.TemporaryDirectory() as tmp:
        tgz = os.path.join(tmp, TGZ)
        with open(tgz, "wb") as fh:
            fh.write(data)
        with ThreadPoolExecutor() as pool:
            results = list(pool.map(lambda pc: deploy_to(pc, tgz), a.pc or PCS))
    for result in results:
        result["deployed"] = deployed
        if a.json:
            print(json.dumps(result))
        else:
            print(f"{result['pc']}: {'ok' if result['ok'] else 'FAILED'}\n{result['output']}\n")
    sys.exit(0 if all(result["ok"] for result in results) else 1)


if __name__ == "__main__":
    main()
