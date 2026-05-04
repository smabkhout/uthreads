# Bibliotheque de Threads Utilisateur (User-Level Threads)

Ce projet propose l'implementation complete d'une bibliotheque de gestion de threads en C. Elle permet la creation, l'ordonnancement, et la destruction de threads legers.

Outre les fonctionnalites de base, ce projet integre plusieurs objectifs avances, activables via des options de compilation : optimisations memoire (one-malloc, recycle), changement de contexte via ucontext, gestion de priorites, ordonnancement preemptif, protection contre les depassements de pile, signaux inter-threads en espace utilisateur, et detection de deadlocks.


## Architecture du Projet

```
.
├── src/            # Code source de la bibliotheque
├── test/           # Fichiers de test
├── install/        # Genere automatiquement a la compilation
│   ├── lib/        # Bibliotheques statiques compilees
│   └── bin/        # Executables de tests compiles
├── scripts/        # Scripts annexes (plot.py pour les graphes de performances)
├── results/        # Figures generees par le script Python
├── rapport/        # Fichiers source LaTeX pour le rapport intermediaire
├── rapport-final/  # Fichiers source LaTeX pour le rapport final
├── Makefile        # Fichier de regles de compilation
└── README.md
```


## Compilation et Options

La compilation s'effectue via make. Le systeme genere plusieurs versions de la bibliotheque dans install/lib/ et compile les tests correspondants dans install/bin/.

### Version par defaut (optimisee)
La version par defaut est compilee avec les deux optimisations memoire activees (-DUSE_ONE_MALLOC -DUSE_RECYCLE). C'est la version la plus performante.
* Commande : `make all` ou `make build_tests`

### setjmp/longjmp sans optimisations (reference pour benchmarks)
Version de base utilisant uniquement setjmp/longjmp, sans aucune optimisation memoire. Sert de reference pour les mesures de performances.
* Commande : `make setjmp`
* Genere les executables suffixes par -setjmp.

### One-malloc
Alloue la structure du thread et sa pile en un seul bloc memoire contigu, reduisant le nombre d'appels malloc/free de moitie.
* Commande : `make one-malloc`
* Genere les executables suffixes par -one-malloc.

### Recycle
Maintient une file de blocs liberes et les reutilise a la prochaine creation de thread, evitant les appels repetes a malloc/free.
* Commande : `make recycle`
* Genere les executables suffixes par -recycle.

### One-malloc + Recycle
Combinaison des deux optimisations. Version la plus performante pour la creation/destruction de threads.
* Commande : `make one-malloc-recycle`
* Genere les executables suffixes par -one-malloc-recycle.

### Changement de contexte (ucontext)
Remplace setjmp/longjmp par l'API ucontext (getcontext, makecontext, swapcontext).
* Commande : `make context`
* Genere les executables suffixes par -context.

### Ordonnancement Preemptif
Active la preemption via des alarmes virtuelles (SIGVTALRM via setitimer). Les threads sont interrompus automatiquement sans avoir besoin d'appeler thread_yield() explicitement. Les sections critiques sont protegees par masquage des signaux.
* Commande : `make preem`
* Genere les executables suffixes par -preem.

### Ordonnancement par Priorite
Active une file d'attente triee par priorite avec vieillissement pour eviter la famine.
* Commande : `make priority`
* Genere les executables suffixes par -priority.

### Protection de la Pile (Stack Guard)
Ajoute une page de protection sans droits d'acces a la fin de la pile de chaque thread via mprotect. En cas de debordement, un gestionnaire sur une pile alternative (sigaltstack) intercepte proprement le SIGSEGV.
* Commande : `make stackprot`
* Genere les executables suffixes par -stackprot.

### Signaux inter-threads en espace utilisateur
Implemente des signaux cooperatifs purement en espace utilisateur (sans signaux POSIX). Expose thread_kill, thread_signal et thread_sigwait. Compatible avec setjmp/longjmp et ucontext.
* Commande : `make signals`
* Genere les executables suffixes par -signals.

### Tout compiler
Pour compiler toutes les bibliotheques et toutes les declinaisons des tests :
* Commande : `make install`


## Execution des Tests

* Lancer les tests de la version standard : `make check`
* Lancer TOUTES les versions des tests : `make check_all`
* Verifier les fuites memoire avec Valgrind : `make valgrind`
* Lancer le test specifique aux signaux : `make signals`


## Performances

* Generer les graphiques de performances : `make graphs`

Compare quatre variantes (setjmp/longjmp, one-malloc, recycle, one-malloc+recycle) ainsi que ucontext et pthreads sur deux tests : creation/destruction de threads et changements de contexte. Chaque mesure est repetee 10 fois sur un seul coeur (taskset -c 0). Les resultats sont stockes dans results/.

* Compiler le rapport final : `make rapport-final`


## Nettoyage

Supprime tous les fichiers generes (objets, bibliotheques, executables, fichiers LaTeX temporaires, et figures dans results/) :
* Commande : `make clean`
