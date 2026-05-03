import os
import subprocess
import math
import matplotlib.pyplot as plt

BIN_DIR = "./install/bin"
TASKSET_CMD = ["taskset", "-c", "0"]

# (test name, extra fixed args passed after n_threads)
TESTS = [
    ("21-create-many",  []),       # mesure le coût de création/destruction
    ("31-switch-many",  ["50"]),   # mesure le coût des context switches (50 yields par thread)
]

THREADS = [50, 100, 250, 500, 750, 1000, 1500, 2000, 3000, 4000, 5000, 6000, 7000, 8000]

RUNS = 10


def run_test(binary, n, extra_args):
    try:
        result = subprocess.run(
            TASKSET_CMD + [binary, str(n)] + extra_args,
            capture_output=True,
            text=True
        )
        output = result.stdout + result.stderr
        for line in output.splitlines():
            if line.startswith("GRAPH;"):
                parts = line.strip().split(";")
                time_us = int(parts[-1])
                return time_us / 1_000  # µs → ms
        print(f"[ERROR] Pas de ligne GRAPH dans {binary} ({n})")
    except Exception as e:
        print(f"Erreur avec {binary} ({n}) :", e)
    return None


def measure(binary, n, extra_args):
    times = [run_test(binary, n, extra_args) for _ in range(RUNS)]
    times = [t for t in times if t is not None]
    if not times:
        return None, None
    mean = sum(times) / len(times)
    std = math.sqrt(sum((t - mean) ** 2 for t in times) / len(times))
    return mean, std


def main():
    os.makedirs("results", exist_ok=True)

    for test, extra_args in TESTS:
        print(f"\n=== Test: {test} ===")

        variants = {
            "setjmp/longjmp":       os.path.join(BIN_DIR, test + "-setjmp"),
            #"ucontext":             os.path.join(BIN_DIR, test + "-context"),
            #"pthreads":             os.path.join(BIN_DIR, test + "-pthread"),
            "one-malloc":           os.path.join(BIN_DIR, test + "-one-malloc"),
            "recycle":              os.path.join(BIN_DIR, test + "-recycle"),
            "one-malloc + recycle": os.path.join(BIN_DIR, test + "-one-malloc-recycle"),
        }

        missing = [name for name, path in variants.items() if not os.path.exists(path)]
        if missing:
            print(f"[SKIP] binaires manquants : {missing}")
            continue

        results = {name: {"means": [], "stds": []} for name in variants}

        for n in THREADS:
            print(f"  n={n}")
            for name, binary in variants.items():
                mean, std = measure(binary, n, extra_args)
                results[name]["means"].append(mean)
                results[name]["stds"].append(std)
                print(f"    {name}: mean={mean:.3f}ms  std={std:.3f}ms")

        plt.figure(figsize=(10, 6))
        for name, data in results.items():
            plt.errorbar(
                THREADS,
                data["means"],
                yerr=data["stds"],
                marker='o',
                capsize=4,
                label=name
            )

        plt.xlabel("Nombre de threads")
        plt.ylabel("Temps d'exécution (ms)")
        plt.title(f"Performance - {test} (moyenne ± écart-type, {RUNS} runs)")
        plt.legend()
        plt.grid()
        plt.tight_layout()
        plt.savefig(f"results/{test}.png", dpi=150)
        print(f"  -> résultats sauvegardés dans results/{test}.png")

    plt.show()


if __name__ == "__main__":
    main()
