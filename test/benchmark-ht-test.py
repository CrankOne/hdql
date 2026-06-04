#!/usr/bin/env python3

import re
import math
import subprocess
from collections import defaultdict

APP = "./hdql-ht-benchmark"
N_RUNS = 10

SIZES = [
    1, 3, 5, 10, 30, 50, 75, 100, 300, 500, 750, 1000,
    5000, 10000
]

TIME_RE = re.compile(
    r"^(?P<name>.+?)\s+(?P<kind>insert|lookup) time:\s+"
    r"(?P<time>[0-9.eE+-]+)\s+sec"
)


def sanitize_filename(name: str) -> str:
    s = name.lower()
    s = re.sub(r"[^a-z0-9]+", "_", s)
    return s.strip("_")


def mean(xs: list[float]) -> float:
    return sum(xs) / len(xs)


def stdev(xs: list[float]) -> float:
    if len(xs) < 2:
        return 0.0

    m = mean(xs)
    var = sum((x - m) * (x - m) for x in xs) / (len(xs) - 1)
    return math.sqrt(var)


def run_one(n: int) -> dict[str, dict[str, float]]:
    proc = subprocess.run(
        [APP, str(n)],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    result: dict[str, dict[str, float]] = defaultdict(dict)

    for line in proc.stdout.splitlines():
        m = TIME_RE.match(line.strip())
        if not m:
            continue

        name = m.group("name").strip()
        kind = m.group("kind")
        value = float(m.group("time"))

        result[name][kind] = value

    return result


def run_repeated(n: int, n_runs: int):
    """
    Returns:
        data[container][kind] -> list[float]
    """
    data = defaultdict(lambda: defaultdict(list))

    for _ in range(n_runs):
        parsed = run_one(n)

        for name, timings in parsed.items():
            for kind, value in timings.items():
                data[name][kind].append(value)

    return data


def write_table(name: str, rows: list[tuple[int, float, float, float, float]]) -> None:
    fname = f"{sanitize_filename(name)}.dat"

    with open(fname, "w") as f:
        f.write(f"# {name}\n")
        f.write("# columns:\n")
        f.write("# 1: N\n")
        f.write("# 2: insert_mean_sec\n")
        f.write("# 3: insert_stddev_sec\n")
        f.write("# 4: lookup_mean_sec\n")
        f.write("# 5: lookup_stddev_sec\n")
        f.write("#\n")

        for n, insert_mean, insert_sd, lookup_mean, lookup_sd in rows:
            f.write(
                f"{n:10d} "
                f"{insert_mean:.16e} "
                f"{insert_sd:.16e} "
                f"{lookup_mean:.16e} "
                f"{lookup_sd:.16e}\n"
            )

    print(f"Wrote {fname}")


def main() -> None:
    results = defaultdict(list)

    for n in SIZES:
        print(f"Running N={n} ...")

        repeated = run_repeated(n, N_RUNS)

        for name, timings in repeated.items():
            insert_samples = timings.get("insert", [])
            lookup_samples = timings.get("lookup", [])

            if not insert_samples or not lookup_samples:
                continue

            results[name].append((
                n,
                mean(insert_samples),
                stdev(insert_samples),
                mean(lookup_samples),
                stdev(lookup_samples),
            ))

    for name in sorted(results):
        write_table(name, results[name])


if __name__ == "__main__":
    main()
