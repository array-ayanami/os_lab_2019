#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>
#include <signal.h>

#include "find_min_max.h"
#include "utils.h"

pid_t *child_pids = NULL;
int total_pids = 0;

void handle_alarm(int sig) {
    printf("\nTimeout reached! Killing all child processes...\n");
    for (int i = 0; i < total_pids; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGKILL);
        }
    }
    exit(1);
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  int timeout = -1;
  bool with_files = false;
  
  // Указатель для динамического массива pipe-каналов
  int **pipes = NULL;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {"timeout", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed < 0) {
              printf("Error: seed must be >= 0\n");
              return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              printf("Error: array_size must be > 0\n");
              return 1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) {
              printf("Error: pnum must be > 0\n");
              return 1;
            }
            
            break;
          case 3:
            with_files = true;
            break;
          case 4:
            timeout = atoi(optarg);
            if (timeout <= 0) {
              printf("Error: timeout must be > 0\n");
              return 1;
            }
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--timeout \"num\"]\n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  // Выделяем память и инициализируем pipes до fork
  pipes = malloc(sizeof(int*) * pnum);
  for (int i = 0; i < pnum; i++) {
    pipes[i] = malloc(sizeof(int) * 2);
    if (pipe(pipes[i]) < 0) {
      printf("Pipe failed!\n");
      return 1;
    }
  }

  child_pids = malloc(sizeof(pid_t) * pnum);
  total_pids = pnum;
  
  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  if (timeout > 0) {
      signal(SIGALRM, handle_alarm);
      alarm(timeout);
  }

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      // successful fork
      active_child_processes += 1;
      if (child_pid == 0) {
        // child process

        // Рассчитываем поддиапазон массива для текущего процесса
        int chunk_size = array_size / pnum;
        int start = i * chunk_size;
        int end = (i == pnum - 1) ? array_size : (i + 1) * chunk_size;
        struct MinMax local_min_max = GetMinMax(array, start, end);

        if (with_files) {
          char filename[64];
          sprintf(filename, "child_%d.txt", i);
          FILE *f = fopen(filename, "w");
          if (f != NULL) {
            fprintf(f, "%d %d\n", local_min_max.min, local_min_max.max);
            fclose(f);
          }
        } else {
          close(pipes[i][0]); // Закрываем чтение в дочернем процессе
          write(pipes[i][1], &local_min_max, sizeof(struct MinMax));
          close(pipes[i][1]); // Закрываем запись после отправки
        }
        
        // Освобождаем ресурсы в дочернем процессе перед выходом
        if (pipes != NULL) {
          for (int j = 0; j < pnum; j++) free(pipes[j]);
          free(pipes);
        }
        free(array);
        free(child_pids);
        return 0;
      } else {
        child_pids[i] = child_pid;
      }

    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  while (active_child_processes > 0) {
    int status;
    pid_t wpid = waitpid(-1, &status, WNOHANG);
    
    if (wpid > 0) {
        active_child_processes -= 1;
    } else if (wpid == 0) {
        usleep(10000);
    } else {
        if (active_child_processes > 0 && timeout > 0) {
            break;
        }
    }
  }

  if (active_child_processes > 0) {
      printf("Error: Execution stopped due to timeout. Total active children left: %d\n", active_child_processes);
      if (pipes != NULL) {
        for (int i = 0; i < pnum; i++) free(pipes[i]);
        free(pipes);
      }
      free(array);
      free(child_pids);
      return 1;
  }

  if (timeout > 0) {
      alarm(0);
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      char filename[64];
      sprintf(filename, "child_%d.txt", i);
      FILE *f = fopen(filename, "r");
      if (f != NULL) {
        if (fscanf(f, "%d %d", &min, &max) == 2) {
            if (min < min_max.min) min_max.min = min;
            if (max > min_max.max) min_max.max = max;
        }
        fclose(f);
        remove(filename); // Удаляем за собой временный файл
      }
    } else {
      struct MinMax local_min_max;
      close(pipes[i][1]); // Закрываем запись в родительском процессе
      if (read(pipes[i][0], &local_min_max, sizeof(struct MinMax)) > 0) {
          min = local_min_max.min;
          max = local_min_max.max;
          if (min < min_max.min) min_max.min = min;
          if (max > min_max.max) min_max.max = max;
      }
      close(pipes[i][0]); // Закрываем чтение после получения данных
    }
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  // Полное освобождение динамической памяти родителя
  if (pipes != NULL) {
    for (int i = 0; i < pnum; i++) free(pipes[i]);
    free(pipes);
  }
  free(array);
  free(child_pids);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}
