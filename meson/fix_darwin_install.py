"""Keep installed Mach-O library references relative to the executable."""

import os
import subprocess
import sys
from pathlib import Path

prefix = Path(os.environ["MESON_INSTALL_PREFIX"])
libdir, bindir, filename, install_name_tool, codesign = sys.argv[1:]
library = prefix / libdir / filename
install_name = "@rpath/" + filename
destdir = os.environ.get("DESTDIR")


def staged(path):
    return Path(destdir) / path.relative_to(path.anchor) if destdir else path


subprocess.run([install_name_tool, "-id", install_name, str(staged(library))], check=True)
subprocess.run([codesign, "--force", "--sign", "-", str(staged(library))], check=True)
for name in ("fplll", "latticegen"):
    binary = str(staged(prefix / bindir / name))
    subprocess.run([install_name_tool, "-change", str(library), install_name, binary], check=True)
    subprocess.run([codesign, "--force", "--sign", "-", binary], check=True)
