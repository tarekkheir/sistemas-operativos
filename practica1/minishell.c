#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

int main() {
    char input[1024];
    char *args[2]; // pour une commande simple sans arguments, args[1] sera NULL
    pid_t pid;

    // Ignorer SIGINT dans le minishell
    signal(SIGINT, SIG_IGN);

    while (1) {
        printf("minishell> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // Sortir si EOF est reçu
        }

        // Supprimer le saut de ligne
        input[strcspn(input, "\n")] = 0;

        // Si la ligne est vide, continuer à la prochaine itération
        if (strlen(input) == 0) {
            continue;
        }

        // Préparer les arguments pour execvp
        args[0] = input;
        args[1] = NULL;

        // Créer un processus enfant pour exécuter la commande
        pid = fork();
        if (pid == 0) { // Dans le processus enfant
            signal(SIGINT, SIG_DFL); // Rétablir le comportement par défaut pour SIGINT
            execvp(args[0], args);
            perror("execvp failed"); // Si execvp échoue
            exit(1);
        } else if (pid > 0) { // Dans le processus parent
            wait(NULL); // Attendre que le processus enfant se termine
        } else {
            perror("fork failed");
        }
    }

    return 0;
}
