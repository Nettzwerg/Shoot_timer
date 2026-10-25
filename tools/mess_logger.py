#!/usr/bin/env python3
"""Logger und Auswertung für die Shot-Timer-Mess-Firmware (firmware/mess).

Aufzeichnen (Befehle werden an den Timer weitergereicht):
    python mess_logger.py log --port /dev/ttyUSB0          (Windows: --port COM5)

    Eingaben, die mit '!' beginnen, werden nicht gesendet, sondern als
    Markierung ins Log geschrieben, z. B.:
        !eigen 9mm 0.5m
        !nachbar 9mm bahn2
    So lassen sich die folgenden Schüsse am Stand einer Situation zuordnen.

Auswerten:
    python mess_logger.py summary messung_*.log     Pegel je Markierung
    python mess_logger.py plot cap_0001.csv          Mitschnitt anzeigen (matplotlib)
    python mess_logger.py sweep messung_*.log        Beep-Frequenzgang anzeigen
"""

import argparse
import csv
import datetime as dt
import pathlib
import statistics
import sys
import threading


def cmd_log(args):
    import serial  # pyserial

    outdir = pathlib.Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = outdir / f"messung_{stamp}.log"
    ser = serial.Serial(args.port, args.baud, timeout=0.2)
    log = open(log_path, "w", encoding="utf-8", buffering=1)
    print(f"# Log: {log_path}  (Beenden mit Strg+C)")

    def write_log(line):
        log.write(f"{dt.datetime.now().isoformat(timespec='milliseconds')} {line}\n")

    def reader():
        cap_file = None
        cap_writer = None
        cap_no = 0
        buf = b""
        while True:
            buf += ser.read(4096)
            *lines, buf = buf.split(b"\n")
            for raw in lines:
                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                if line.startswith("CAP_BEGIN"):
                    cap_no += 1
                    path = outdir / f"cap_{stamp}_{cap_no:04d}.csv"
                    cap_file = open(path, "w", newline="", encoding="utf-8")
                    cap_writer = csv.writer(cap_file)
                    cap_writer.writerow(["idx", "t_ms", "low", "high"])
                    trigger = int(line.split(",")[1])
                    write_log(f"{line},{path.name}")
                    print(f"# Mitschnitt -> {path.name}")
                    continue
                if line == "CAP_END" and cap_file:
                    cap_file.close()
                    cap_file = cap_writer = None
                    continue
                if cap_writer:
                    idx, low, high = (int(v) for v in line.split(","))
                    cap_writer.writerow([idx, f"{(idx - trigger) * 0.05:.2f}", low, high])
                    continue
                write_log(line)
                print(line)

    threading.Thread(target=reader, daemon=True).start()
    try:
        for line in sys.stdin:
            line = line.strip()
            if line.startswith("!"):
                write_log(f"MARK,{line[1:].strip()}")
                print(f"# Markierung: {line[1:].strip()}")
            else:
                ser.write((line + "\n").encode())
    except KeyboardInterrupt:
        pass


def read_log(paths):
    for path in paths:
        with open(path, encoding="utf-8") as f:
            for row in f:
                parts = row.rstrip("\n").split(" ", 1)
                if len(parts) == 2:
                    yield parts[1]


def cmd_summary(args):
    groups = {}
    ratios = []
    mark = "(ohne Markierung)"
    for line in read_log(args.logs):
        if line.startswith("MARK,"):
            mark = line[5:]
        elif line.startswith("SHOT,"):
            f = line.split(",")
            groups.setdefault(mark, []).append((float(f[4]), float(f[5]), f[8] == "1"))
            low, high = int(f[6]), int(f[7])
            if f[8] == "0" and low >= 50:
                ratios.append(high / low)
    if not groups:
        print("Keine Schüsse gefunden.")
        return
    print(f"{'Markierung':30} {'Anz.':>5} {'min':>7} {'Median':>7} {'max':>7}  {'Anstieg ms':>10}  HIGH übersteuert")
    for mark, shots in groups.items():
        peaks = [s[0] for s in shots]
        rises = [s[1] for s in shots]
        clipped = sum(s[2] for s in shots)
        print(f"{mark[:30]:30} {len(peaks):5d} {min(peaks):7.1f} {statistics.median(peaks):7.1f} "
              f"{max(peaks):7.1f}  {statistics.median(rises):10.2f}  {clipped}/{len(peaks)}")
    print("\nPegel in dBFS. Ziel: Median 'eigen' mindestens 8 dB über Maximum 'nachbar'.")
    if ratios:
        print(f"Gemessenes Verhältnis HIGH/LOW (nicht übersteuerte Schüsse, n={len(ratios)}): "
              f"{statistics.median(ratios):.2f}  -> mit Befehl 'k' übernehmen")


def cmd_plot(args):
    import matplotlib.pyplot as plt

    t, low, high = [], [], []
    with open(args.csv, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            t.append(float(row["t_ms"]))
            low.append(int(row["low"]))
            high.append(int(row["high"]))
    plt.plot(t, high, label="HIGH")
    plt.plot(t, low, label="LOW")
    plt.axvline(0, color="grey", linestyle="--", linewidth=0.8)
    plt.xlabel("Zeit relativ zur Schwellenüberschreitung [ms]")
    plt.ylabel("ADC-Rohwert")
    plt.title(pathlib.Path(args.csv).name)
    plt.legend()
    plt.grid(alpha=0.3)
    plt.show()


def cmd_sweep(args):
    import matplotlib.pyplot as plt

    freq, db = [], []
    for line in read_log(args.logs):
        if line.startswith("SWEEP,"):
            f = line.split(",")
            freq.append(int(f[1]))
            db.append(float(f[2]))
    if not freq:
        print("Keine SWEEP-Zeilen gefunden.")
        return
    best = max(range(len(db)), key=db.__getitem__)
    print(f"Lautester Punkt: {freq[best]} Hz, {db[best]:.1f} dBFS")
    plt.plot(freq, db, marker="o")
    plt.xlabel("Frequenz [Hz]")
    plt.ylabel("Pegel am Mikrofon [dBFS]")
    plt.grid(alpha=0.3)
    plt.show()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    pl = sub.add_parser("log", help="seriell aufzeichnen")
    pl.add_argument("--port", required=True)
    pl.add_argument("--baud", type=int, default=921600)
    pl.add_argument("--outdir", default="messungen")
    ps = sub.add_parser("summary", help="Pegel je Markierung auswerten")
    ps.add_argument("logs", nargs="+")
    pp = sub.add_parser("plot", help="Mitschnitt anzeigen")
    pp.add_argument("csv")
    pw = sub.add_parser("sweep", help="Beep-Sweep anzeigen")
    pw.add_argument("logs", nargs="+")
    args = p.parse_args()
    {"log": cmd_log, "summary": cmd_summary, "plot": cmd_plot, "sweep": cmd_sweep}[args.cmd](args)


if __name__ == "__main__":
    main()
