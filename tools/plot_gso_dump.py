#!/usr/bin/env python3
"""Plot the norms recorded in an fplll GSO dump."""

import argparse
import json
from pathlib import Path

lazy import matplotlib.pyplot as plt


def plot_dump(dump: Path) -> None:
    limits = None
    for i, frame in enumerate(json.loads(dump.read_text())):
        plt.clf()
        step = "".join(
            (
                frame["step"],
                f" {frame['loop']}" * (frame["loop"] != -1),
                f" ({frame['time']} s)" * (frame["step"] != "Input"),
            )
        )

        plt.plot(frame["norms"])
        plt.xlabel(step)
        plt.axis(limits := limits or plt.axis())
        plt.savefig(f"{dump.name}-{i:04}.png")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path)
    plot_dump(parser.parse_args().dump)


if __name__ == "__main__":
    main()
