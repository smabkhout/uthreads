# Bibliotheque de Threads Utilisateur (User-Level Threads)

Ce projet propose l'implementation complete d'une bibliotheque de gestion de threads en C. Elle permet la creation, l'ordonnancement, et la destruction de threads legers. 

Outre les fonctionnalites de base, ce projet integre plusieurs objectifs avances, activables via des options de compilation : l'usage de setjmp/longjmp, gestion de priorite, ordonnancement preemptif, protection contre les depassements de pile, et gestion des signaux.


## Architecture du Projet

Le projet suit une arborescence suivante :

```
.
├── src/            # Code source de la bibliotheque
├── test/           # Fichiers de test
├── install/        # Genere automatiquement a la compilation
│   ├── lib/        # Bibliotheques statiques compilees
│   └── bin/        # Executables de tests compiles
├── scripts/        # Scripts annexes (ex: plot.py pour les graphes)
├── rapport/        # Fichiers source LaTeX pour le rapport intermediaire
├── rapport-final/  # Fichiers source LaTeX pour le rapport final
├── Makefile        # Fichier de regles de compilation
└── README.md
```


## Compilation et Options Avancees

La compilation s'effectue via make. Le systeme genere plusieurs versions de la bibliotheque (dans install/lib/) et compile les tests correspondants (dans install/bin/).

### 1. Version Standard (Par defaut)
Utilise setjmp / longjmp avec de l'assembleur inline pour le changement de contexte.
* Commande : make all ou make build_tests

### 2. Changement de contexte (ucontext)
Remplace setjmp/longjmp par l'API ucontext (getcontext, makecontext, swapcontext). 
* Commande : make context
* Genere les executables suffixes par -context.

### 3. Ordonnancement Preemptif
Active la preemption via des alarmes virtuelles (SIGVTALRM via setitimer). Les threads sont interrompus automatiquement pour passer la main, sans avoir besoin d'appeler thread_yield() explicitement. Les sections critiques (comme thread_mutex_lock ou thread_create) sont protegees par le masquage des signaux.
* Commande : make preem
* Genere les executables suffixes par -preem.

### 4. Protection de la Pile (Stack Guard)
Ajoute une page de protection (guard page) sans droits d'acces a la fin de la pile de chaque thread via mprotect. En cas de debordement (Stack Overflow), un gestionnaire de signal sur une pile alternative (sigaltstack) intercepte proprement le SIGSEGV au lieu d'un simple plantage silencieux.
* Commande : make stackprot
* Genere les executables suffixes par -stackprot.

### 5. L'astuce Fibonacci
Une macro speciale TRICHER_FIBO permet d'intercepter les creations de threads massives et recursives (typiquement le calcul naif de Fibonacci). Elle met en place un cache de memoisation (bottom-up) qui transforme un temps d'execution exponentiel O(2^n) en temps lineaire O(n). 
(Activee via make install/lib/libthread-fibo.a, le bloc dans le Makefile peut etre decommente pour tester le binaire 51-fibonacci).

### 6. Tout compiler
Pour compiler toutes les bibliotheques (.a) et toutes les declinaisons des tests.
* Commande : make install



## Execution des Tests

De nombreuses commandes sont prevues pour verifier la robustesse de l'implementation :

* Lancer les tests de la version standard : make check
* Lancer TOUTES les versions des tests (standard, context, preem, stackprot) : make check_all
* Verifier les fuites memoire avec Valgrind : make valgrind
    (Le code inclut les macros Valgrind VALGRIND_STACK_REGISTER / DEREGISTER pour eviter les faux positifs lors du changement de contexte).
* Lancer le test specifique aux signaux (necessite ucontext) : make signals


## Performances et Rapports

* Generer des graphiques : make graphs
    Compare les performances de cette implementation face a la librairie standard systeme (pthreads). Les resultats sont stockes dans le dossier results/.
* Compiler les rapports LaTeX : make rapport ou make rapport-final


## Nettoyage

Pour supprimer tous les fichiers generes (fichiers objets, bibliotheques, executables, et fichiers LaTeX temporaires) :
* Commande : make clean