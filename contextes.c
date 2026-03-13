#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h> /* ne compile pas avec -std=c89 ou -std=c99 */
/*
void func(int numero)
{
  printf("j'affiche le numéro %d\n", numero);
}

int main() {
  ucontext_t uc, previous;

  getcontext(&uc); /* initialisation de uc avec valeurs coherentes
		    * (pour éviter de tout remplir a la main ci-dessous) 

  uc.uc_stack.ss_size = 64*1024;
  uc.uc_stack.ss_sp = malloc(uc.uc_stack.ss_size);
  uc.uc_link = &previous;
  makecontext(&uc, (void (*)(void)) func, 1, 34);

  printf("je suis dans le main\n");
  swapcontext(&previous, &uc);
  printf("je suis revenu dans le main\n");

  uc.uc_stack.ss_size = 64*1024;
  uc.uc_stack.ss_sp = malloc(uc.uc_stack.ss_size);
  uc.uc_link = NULL;
  makecontext(&uc, (void (*)(void)) func, 1, 57);

  printf("je suis dans le main\n");
  setcontext(&uc);
  printf("je ne reviens jamais ici\n");
  return 0;
}
*/

ucontext_t ctx_a, ctx_b, ctx_main;

void create_context(ucontext_t *uc, ucontext_t *next, void (*func)(void)) {
    getcontext(uc);
    uc->uc_stack.ss_size = 64*1024;
    uc->uc_stack.ss_sp = malloc(uc->uc_stack.ss_size);
    uc->uc_link = next;
    makecontext(uc, func, 0);
}

void task_a() {
    for (int i = 1; i <= 3; i++) {
        printf(" A : Étape %d\n", i);
        printf("A passe  à B ---\n");
        swapcontext(&ctx_a, &ctx_b); 
    }
    printf("A : Terminée\n");
}

void task_b() {
    for (int i = 1; i <= 3; i++) {
        printf("B : Étape %d\n", i);
        printf("B passe à A \n");
        swapcontext(&ctx_b, &ctx_a); 
    }
    printf(" B : Terminée\n");
}



int main() {
    create_context(&ctx_a, &ctx_main, task_a);
    create_context(&ctx_b, &ctx_main, task_b);

    printf("Main : Lancement du bal...\n");
    
    swapcontext(&ctx_main, &ctx_a);

    printf("Main : Tout le monde a fini !\n");

    free(ctx_a.uc_stack.ss_sp);
    free(ctx_b.uc_stack.ss_sp);

    return 0;
}
