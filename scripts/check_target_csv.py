#!/usr/bin/env python3
"""Check recorded target motion against the independent analytical trajectory."""

import argparse
import csv
import math
import sys
from collections import OrderedDict
from pathlib import Path


def check_file(args):
    columns = ("run", "time_s", "x_m", "y_m", "z_m")
    runs = OrderedDict()
    with args.csv_path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != list(columns):
            raise ValueError("Expected CSV heading: " + ",".join(columns))
        previous_run = None
        for line_number, row in enumerate(reader, start=2):
            if None in row or any(row.get(key) in (None, "") for key in columns):
                raise ValueError(f"Line {line_number}: incomplete or extra fields")
            run = int(row["run"])
            values = tuple(float(row[key]) for key in columns[1:])
            if not all(math.isfinite(value) for value in values):
                raise ValueError(f"Line {line_number}: non-finite number")
            if run != previous_run and run in runs:
                raise ValueError(f"Line {line_number}: run {run} appears in separate groups")
            runs.setdefault(run, []).append(values)
            previous_run = run

    if not runs:
        raise ValueError("No recorded data rows")

    intervals = round(args.duration / args.interval)
    if abs(intervals * args.interval - args.duration) > args.tolerance:
        raise ValueError("Duration must be a whole number of recording intervals")
    expected_count = intervals + 1
    complete_count = 0
    partial_count = 0
    worst_position_error = 0.0

    for run, rows in runs.items():
        if len(rows) > expected_count:
            raise ValueError(f"Run {run}: too many rows ({len(rows)} > {expected_count})")
        for index, (time_s, x, y, z) in enumerate(rows):
            scheduled_time = index * args.interval
            if abs(time_s - scheduled_time) > args.tolerance:
                raise ValueError(
                    f"Run {run}, row {index}: time {time_s} differs from {scheduled_time}"
                )
            expected = (args.x0 + args.vx * time_s,
                        args.y0 + args.vy * time_s,
                        args.z)
            error = max(abs(actual - wanted)
                        for actual, wanted in zip((x, y, z), expected))
            worst_position_error = max(worst_position_error, error)
            if error > args.tolerance:
                raise ValueError(
                    f"Run {run}, t={time_s}: position {(x, y, z)} "
                    f"does not match {expected}"
                )

        if len(rows) == expected_count:
            complete_count += 1
            print(f"PASS run {run}: {len(rows)} rows, final position {rows[-1][1:]}")
        else:
            partial_count += 1
            print(f"PARTIAL run {run}: {len(rows)} valid rows; ended before completion")

    if not complete_count:
        raise ValueError("No complete run found; finish at least one trial and close the app")
    print(f"PASS: {complete_count} complete run(s), {partial_count} partial run(s)")
    print(f"Largest coordinate error: {worst_position_error:.3g} m")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path)
    parser.add_argument("--vx", type=float, default=0.26)
    parser.add_argument("--vy", type=float, default=0.12)
    parser.add_argument("--x0", type=float, default=-0.10)
    parser.add_argument("--y0", type=float, default=0.0)
    parser.add_argument("--z", type=float, default=0.05)
    parser.add_argument("--duration", type=float, default=5.0)
    parser.add_argument("--interval", type=float, default=0.1)
    parser.add_argument("--tolerance", type=float, default=1e-6)
    args = parser.parse_args()
    settings = (args.vx, args.vy, args.x0, args.y0, args.z,
                args.duration, args.interval, args.tolerance)
    if (not all(math.isfinite(value) for value in settings)
            or args.duration <= 0 or args.interval <= 0 or args.tolerance <= 0):
        parser.error("Settings must be finite; duration, interval and tolerance must be positive")
    try:
        check_file(args)
    except (OSError, ValueError, csv.Error) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())