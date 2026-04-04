import os
import subprocess
import time
import matplotlib.pyplot as plt

BIN_DIR = "./install/bin"

# tests à exécuter
TESTS = [
    "01-main",
    "02-switch",
    "03-equity",
    "11-join",
    "12-join-main",
    "13-join-switch",
    "21-create-many",
    "22-create-many-recursive",
    "23-create-many-once",
    "31-switch-many",
    "32-switch-many-join",
    "33-switch-many-cascade",
    # "51-fibonacci",
    "61-mutex",
    "62-mutex",
    "63-mutex-equity",
    "64-mutex-join",
    # "71-preemption",
    # "81-deadlock",
    # "example",
]

# paramètres
THREADS = [100, 500, 1000, 2000]

# moyenne
RUNS = 3


def run_test(binary, n):
    try:
        start = time.perf_counter()

        subprocess.run(
            [binary, str(n)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )

        end = time.perf_counter()

        return end - start

    except Exception as e:
        print(f"Erreur avec {binary} ({n}) :", e)

    return None


def average_time(binary, n):
    times = []

    for _ in range(RUNS):
        t = run_test(binary, n)
        if t is not None:
            times.append(t)

    if not times:
        return None

    return sum(times) / len(times)


def main():
    os.makedirs("results", exist_ok=True)

    for test in TESTS:

        print(f"\n=== Test: {test} ===")

        user_bin = os.path.join(BIN_DIR, test)
        pthread_bin = os.path.join(BIN_DIR, test + "-pthread")

        if not os.path.exists(user_bin) or not os.path.exists(pthread_bin):
            print(f"[SKIP] {test} non trouvé")
            continue

        user_times = []
        pthread_times = []

        for n in THREADS:
            print(f"Threads: {n}")

            t1 = average_time(user_bin, n)
            t2 = average_time(pthread_bin, n)

            print(f"  user avg: {t1}")
            print(f"  pthread avg: {t2}")

            user_times.append(t1)
            pthread_times.append(t2)

        # graphe
        plt.figure()

        plt.plot(THREADS, user_times, marker='o', label="User Threads")
        plt.plot(THREADS, pthread_times, marker='o', label="Pthreads")

        plt.xlabel("Number of threads")
        plt.ylabel("Execution time (s)")
        plt.title(f"Performance - {test}")

        plt.legend()
        plt.grid()

        plt.savefig(f"results/{test}.png")

    plt.show()


if __name__ == "__main__":
    main()