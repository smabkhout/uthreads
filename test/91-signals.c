#include "../src/thread.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #define USE_CONTEXT

#ifndef USE_CONTEXT
int main(void) {
  printf("signals: ignore sans -DUSE_CONTEXT\n");
  return EXIT_SUCCESS;
}
#else

/* test des signaux inter-threads (sans signaux systeme).
 *
 * valgrind doit etre content.
 * Les trois scenarios doivent se terminer sans blocage :
 *   - thread_sigwait bloque jusqu'a reception du signal envoye par un autre thread
 *   - un handler enregistre est appele apres thread_kill suivi d'un yield
 *   - un signal envoye avant l'appel a thread_sigwait est consomme immediatement
 *
 */

#define SIG_A 3
#define SIG_B 7

/* --- test 1: sigwait bloquant --- */

static thread_t g_receiver;

static void *receiver(void *arg __attribute__((unused))) {
  int sig;
  int ret = thread_sigwait((uint32_t)1 << SIG_A, &sig);
  assert(ret == 0);
  assert(sig == SIG_A);
  return (void *)42L;
}

static void *sender(void *arg __attribute__((unused))) {
  thread_kill(g_receiver, SIG_A);
  return NULL;
}

/* --- test 2: handler de signal --- */

static int g_handler_called = 0;

static void my_handler(int sig) {
  assert(sig == SIG_B);
  g_handler_called++;
}

static void *handler_thread(void *arg __attribute__((unused))) {
  thread_signal(SIG_B, my_handler);
  thread_kill(thread_self(), SIG_B);
  thread_yield();
  assert(g_handler_called == 1);
  return NULL;
}

/* --- test 3: signal deja en attente au moment du sigwait --- */

static thread_t g_early_receiver;

static void *early_receiver(void *arg __attribute__((unused))) {
  /* on laisse early_sender tourner en premier */
  thread_yield();
  int sig;
  int ret = thread_sigwait((uint32_t)1 << SIG_A, &sig);
  assert(ret == 0);
  assert(sig == SIG_A);
  return NULL;
}

static void *early_sender(void *arg __attribute__((unused))) {
  thread_kill(g_early_receiver, SIG_A);
  return NULL;
}

int main(void) {
  void *retval;
  thread_t s, t;

  /* sigwait bloque jusqu'a reception du signal */
  thread_create(&g_receiver, receiver, NULL);
  thread_create(&s, sender, NULL);
  thread_join(s, NULL);
  thread_join(g_receiver, &retval);
  assert((long)retval == 42L);

  /* handler appele apres kill + yield */
  thread_create(&t, handler_thread, NULL);
  thread_join(t, NULL);

  /* signal deja pending quand sigwait est appele */
  thread_create(&g_early_receiver, early_receiver, NULL);
  thread_create(&s, early_sender, NULL);
  thread_join(s, NULL);
  thread_join(g_early_receiver, NULL);

  printf("signals OK\n");
  return EXIT_SUCCESS;
}

#endif
