#!/usr/bin/env python3
"""Logger und Auswertung für die Shot-Timer-Mess-Firmware (firmware/mess).

Aufzeichnen (Befehle werden an den Timer weitergereicht):
    python mess_logger.py log --port /dev/ttyUSB0          (Windows: --port COM5)

    Eingaben, die mit '!' beginnen, werden nicht gesendet, sondern als
    Markierung ins Log geschrieben, z. B.:
        !eigen 9mm 0.5m
        !nachbar 9mm bahn2
    So lassen sich die folgenden Schüsse am Stand einer Situation zuordnen.

Ohne Laptop am Stand (Standalone, Daten im Flash des Timers):
    python mess_logger.py download --port COM5               zu Hause auslesen
    python mess_logger.py download --port COM5 --erase       auslesen, danach Speicher löschen

Auswerten:
    python mess_logger.py summary messungen/*.log            Pegel je Markierung
    python mess_logger.py summary messungen/*.log --protokoll protokoll.txt
        protokoll.txt ordnet Situationsnummern Beschreibungen zu, eine Zeile je Situation:
            1 = A eigen 9mm 0.5m
            2 = A nachbar 9mm 1bahn
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


class CaptureWriter:
    """Schreibt CAP_BEGIN ... CAP_END-Blöcke der Firmware als CSV-Dateien."""

    def __init__(self, outdir, prefix):
        self.outdir = outdir
        self.prefix = prefix
        self.count = 0
        self.file = None
        self.writer = None
        self.trigger = 0

    def feed(self, line):
        """Gibt True zurück, wenn die Zeile zu einem Mitschnitt gehörte."""
        if line.startswith("CAP_BEGIN"):
            self.count += 1
            fields = line.split(",")
            self.trigger = int(fields[1])
            name = fields[3].removesuffix(".bin") if len(fields) > 3 else f"{self.count:04d}"
            self.path = self.outdir / f"cap_{self.prefix}_{name}.csv"
            self.file = open(self.path, "w", newline="", encoding="utf-8")
            self.writer = csv.writer(self.file)
            self.writer.writerow(["idx", "t_ms", "low", "high"])
            return True
        if line == "CAP_END" and self.file:
            self.file.close()
            self.file = self.writer = None
            return True
        if self.writer:
            idx, low, high = (int(v) for v in line.split(","))
            self.writer.writerow([idx, f"{(idx - self.trigger) * 0.05:.2f}", low, high])
            return True
        return False


def serial_lines(ser):
    buf = b""
    while True:
        chunk = ser.read(4096)
        if not chunk:
            yield None   # Zeitüberschreitung
            continue
        buf += chunk
        *lines, buf = buf.split(b"\n")
        for raw in lines:
            line = raw.decode("utf-8", errors="replace").strip()
            if line:
                yield line


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
        caps = CaptureWriter(outdir, stamp)
        for line in serial_lines(ser):
            if line is None:
                continue
            if caps.feed(line):
                if line.startswith("CAP_BEGIN"):
                    write_log(f"{line},{caps.path.name}")
                    print(f"# Mitschnitt -> {caps.path.name}")
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


def cmd_download(args):
    import serial  # pyserial

    outdir = pathlib.Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    ser = serial.Serial(args.port, args.baud, timeout=3)
    ser.reset_input_buffer()
    ser.write(b"F\n")
    caps = CaptureWriter(outdir, f"flash_{stamp}")
    current = None
    files = 0
    for line in serial_lines(ser):
        if line is None:
            sys.exit("Keine Antwort vom Timer (Port, Baudrate, Firmware prüfen). Speicher wurde NICHT gelöscht.")
        if line == "DUMP_END":
            break
        if caps.feed(line):
            continue
        if line.startswith("FILE_BEGIN,"):
            name = line.split(",", 1)[1].removesuffix(".txt")
            current = open(outdir / f"flash_{stamp}_{name}.log", "w", encoding="utf-8")
            files += 1
        elif line == "FILE_END" and current:
            current.close()
            current = None
        elif current:
            current.write(line + "\n")
    print(f"{files} Sitzung(en) und {caps.count} Mitschnitt(e) nach {outdir} gespeichert.")
    if args.erase:
        ser.write(b"X\n")
        print("Speicher im Timer gelöscht.")


def read_log(paths):
    """Liefert die Zeilen der Logs, mit oder ohne vorangestellten PC-Zeitstempel."""
    for path in paths:
        with open(path, encoding="utf-8") as f:
            for row in f:
                row = row.rstrip("\n")
                first, _, rest = row.partition(" ")
                if first[:1].isdigit() and "T" in first:
                    row = rest
                if row:
                    yield row


def read_protokoll(path):
    names = {}
    if path:
        with open(path, encoding="utf-8") as f:
            for row in f:
                num, sep, text = row.partition("=")
                if sep and num.strip().isdigit():
                    names[f"Situation {int(num)}"] = f"{int(num):>2} {text.strip()}"
    return names


def cmd_summary(args):
    names = read_protokoll(args.protokoll)
    groups = {}
    ratios = []
    mark = "(ohne Markierung)"
    for line in read_log(args.logs):
        if line.startswith("MARK,"):
            mark = names.get(line[5:], line[5:])
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
    ps.add_argument("--protokoll", help="Datei mit Zuordnung Situationsnummer = Beschreibung")
    pd = sub.add_parser("download", help="im Timer gespeicherte Messungen auslesen")
    pd.add_argument("--port", required=True)
    pd.add_argument("--baud", type=int, default=921600)
    pd.add_argument("--outdir", default="messungen")
    pd.add_argument("--erase", action="store_true", help="nach erfolgreichem Auslesen Speicher löschen")
    pp = sub.add_parser("plot", help="Mitschnitt anzeigen")
    pp.add_argument("csv")
    pw = sub.add_parser("sweep", help="Beep-Sweep anzeigen")
    pw.add_argument("logs", nargs="+")
    args = p.parse_args()
    {"log": cmd_log, "summary": cmd_summary, "plot": cmd_plot, "sweep": cmd_sweep,
     "download": cmd_download}[args.cmd](args)


if __name__ == "__main__":
    main()
