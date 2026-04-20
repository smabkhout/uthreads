import os import subprocess import time import matplotlib.pyplot as plt

    BIN_DIR = "./install/bin" TASKSET_CMD =["taskset", "-c", "0"]

#tests à exécuter
                                            TESTS =[
#"01-main",
#"02-switch",
#"03-equity",
#"11-join",
#"12-join-main",
#"13-join-switch",
                                                "21-create-many", "22-create-many-recursive", "23-create-many-once",
#"31-switch-many",
#"32-switch-many-join",
#"33-switch-many-cascade",
#"51-fibonacci",
                                                "61-mutex",
#"62-mutex",
#"63-mutex-equity",
#"64-mutex-join",
#"71-preemption",
#"81-deadlock",
#"example",
]

#paramètres
                                                THREADS =[100, 500, 1000, 2000]

#moyenne
                                                          RUNS = 3

                                                          def run_test(binary, n) : try :result = subprocess.run(TASKSET_CMD +[binary, str(n)], capture_output = True, text = True)

                                                                                                                                                                           output = result.stdout + result.stderr

                                                                                                                               for line in output.splitlines() : if line.startswith("GRAPH;") :parts = line.strip().split(";")

                                                                                                                                                                                                                      time_us = int(parts[- 1])

                                                                                                                                                                                                                          return time_us / 1_000_000 #µs → secondes

                                                                                                                                                                                                                  print(f "[ERROR] Pas de ligne GRAPH dans {binary} ({n})")

                                                                                                                                                                                                                      except Exception as e:print(f "Erreur avec {binary} ({n}) :", e)

                                                                                                                                                                                                                                                      return None

                                                                                                                                                                                                                                                  def average_time(binary, n) :times =[]

                                                                                                                                                                                                                                                                   for _ in range(RUNS) :t = run_test(binary, n) if t is not None:times.append(t)

                                                                                                                                                                                                                                                                                                                  if not times: return None

                                                                                                                                                                                                                                                                                                      return sum(times) / len(times)

                                                                                                                                                                                                                                                                                                          def main() :os.makedirs("results", exist_ok = True)

                                                                                                                                                                                                                                                                                                                                      for test in TESTS:

                                                                                                                                                                                                                                                                                                                                  print(f "\n=== Test: {test} ===")

                                                                                                                                                                                                                                                                                                                                            setjmp_bin = os.path.join(BIN_DIR, test) context_bin = os.path.join(BIN_DIR, test + "-context") pthread_bin = os.path.join(BIN_DIR, test + "-pthread")

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    if not os.path.exists(setjmp_bin) or not os.path.exists(pthread_bin) or not os.path.exists(context_bin) :print(f "[SKIP] {test} non trouvé") continue

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            setjmp_times =[] context_times =[] pthread_times =[]

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            for n in THREADS:print(f "Threads: {n}")

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                t1 = average_time(setjmp_bin, n) t3 = average_time(context_bin, n) t2 = average_time(pthread_bin, n)

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         print(f "  setjmp/longjmp avg: {t1}") print(f "  ucontext avg: {t3}") print(f "  Pthreads avg: {t2}")

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         setjmp_times.append(t1) context_times.append(t3) pthread_times.append(t2)

#graphe
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   plt.figure()

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  plt.plot(THREADS, setjmp_times, marker = 'o', label = "setjmp/longjmp") plt.plot(THREADS, context_times, marker = 'o', label = "ucontext") plt.plot(THREADS, pthread_times, marker = 'o', label = "Pthreads")

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                plt.xlabel("Number of threads") plt.ylabel("Execution time (s)") plt.title(f "Performance - {test}")

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               plt.legend() plt.grid()

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         plt.savefig(f "results/{test}.png")

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         plt.show()

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             if __name__ == "__main__" :main()