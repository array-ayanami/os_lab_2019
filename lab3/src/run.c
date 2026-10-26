#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    printf("Родитель: Запускаю sequential_min_max в отдельном процессе...\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return 1;
    } 
    else if (pid == 0) {

        char *args[] = {
            "./sequential_min_max", 
            "42", 
            "10", 
            NULL
        };

        execv(args[0], args);

        perror("Execv failed");
        exit(1);
    } 
    else {

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("Родитель: Дочерний процесс завершился с кодом %d\n", WEXITSTATUS(status));
        } else {
            printf("Родитель: Дочерний процесс завершился аварийно\n");
        }
    }

    return 0;
}
