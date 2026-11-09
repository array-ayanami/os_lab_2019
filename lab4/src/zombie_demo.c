#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main() {
    pid_t child_pid = fork();

    if (child_pid < 0) {
        perror("Fork failed");
        return 1;
    }

    if (child_pid == 0) {
        // Дочерний процесс
        printf("[Потомок] Я завершаю работу прямо сейчас (PID: %d)\n", getpid());
        exit(0); 
    } else {
        // Родительский процесс
        printf("[Родитель] Мой потомок (PID: %d) завершился.\n", child_pid);
        printf("[Родитель] Я засыпаю на 20 секунд и НЕ вызываю wait().\n");
        printf("[Родитель] Посмотрите список процессов в другом терминале: ps -ef | grep zombie\n");
        
        sleep(20); // Период, пока потомок гарантированно будет зомби
        
        printf("[Родитель] Я проснулся и завершаю работу.\n");
    }

    return 0;
}
