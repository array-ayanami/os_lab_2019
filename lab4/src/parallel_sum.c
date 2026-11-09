#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "sum.h"
#include "utils.h"

struct ThreadArgs {
  const int *array;
  size_t begin;
  size_t end;
  int64_t sum;
};

static void *ThreadSum(void *arg) {
  struct ThreadArgs *thread_args = arg;
  thread_args->sum =
      Sum(thread_args->array, thread_args->begin, thread_args->end);
  return NULL;
}

static int ParseUint32(const char *text, uint32_t *value) {
  char *end = NULL;
  errno = 0;
  unsigned long long parsed = strtoull(text, &end, 10);
  if (errno != 0 || text[0] == '\0' || *end != '\0' ||
      parsed > UINT32_MAX) {
    return 0;
  }
  *value = (uint32_t)parsed;
  return 1;
}

static double ElapsedMilliseconds(const struct timespec *start,
                                 const struct timespec *finish) {
  return (finish->tv_sec - start->tv_sec) * 1000.0 +
         (finish->tv_nsec - start->tv_nsec) / 1000000.0;
}

int main(int argc, char **argv) {
  uint32_t threads_num = 0;
  uint32_t array_size = 0;
  uint32_t seed = 0;
  int have_threads = 0;
  int have_seed = 0;
  int have_array_size = 0;
  static const struct option options[] = {
      {"threads_num", required_argument, NULL, 't'},
      {"seed", required_argument, NULL, 's'},
      {"array_size", required_argument, NULL, 'a'},
      {NULL, 0, NULL, 0},
  };

  int option;
  while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
    uint32_t *destination = NULL;
    int *provided = NULL;
    switch (option) {
      case 't':
        destination = &threads_num;
        provided = &have_threads;
        break;
      case 's':
        destination = &seed;
        provided = &have_seed;
        break;
      case 'a':
        destination = &array_size;
        provided = &have_array_size;
        break;
      default:
        fprintf(stderr,
                "Usage: %s --threads_num num --seed num --array_size num\n",
                argv[0]);
        return 1;
    }

    if (*provided || !ParseUint32(optarg, destination) ||
        (option != 's' && *destination == 0)) {
      fprintf(stderr, "Error: invalid or repeated argument value\n");
      return 1;
    }
    *provided = 1;
  }

  if (optind != argc || !have_threads || !have_seed || !have_array_size) {
    fprintf(stderr,
            "Usage: %s --threads_num num --seed num --array_size num\n",
            argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(*array) * (size_t)array_size);
  pthread_t *threads = malloc(sizeof(*threads) * (size_t)threads_num);
  struct ThreadArgs *args = malloc(sizeof(*args) * (size_t)threads_num);
  if (array == NULL || threads == NULL || args == NULL) {
    fprintf(stderr, "Error: memory allocation failed\n");
    free(array);
    free(threads);
    free(args);
    return 1;
  }

  GenerateArray(array, array_size, seed);

  struct timespec start_time;
  struct timespec finish_time;
  if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
    perror("clock_gettime");
    free(array);
    free(threads);
    free(args);
    return 1;
  }

  size_t chunk_size = array_size / threads_num;
  size_t remainder = array_size % threads_num;
  size_t begin = 0;
  uint32_t created = 0;
  for (uint32_t i = 0; i < threads_num; i++) {
    size_t chunk_length = chunk_size + (i < remainder);
    args[i].array = array;
    args[i].begin = begin;
    args[i].end = begin + chunk_length;
    args[i].sum = 0;
    begin = args[i].end;

    int error = pthread_create(&threads[i], NULL, ThreadSum, &args[i]);
    if (error != 0) {
      fprintf(stderr, "Error: pthread_create failed: %d\n", error);
      for (uint32_t j = 0; j < created; j++) {
        pthread_join(threads[j], NULL);
      }
      free(array);
      free(threads);
      free(args);
      return 1;
    }
    created++;
  }

  int64_t total_sum = 0;
  for (uint32_t i = 0; i < threads_num; i++) {
    int error = pthread_join(threads[i], NULL);
    if (error != 0) {
      fprintf(stderr, "Error: pthread_join failed: %d\n", error);
      free(array);
      free(threads);
      free(args);
      return 1;
    }
    total_sum += args[i].sum;
  }

  if (clock_gettime(CLOCK_MONOTONIC, &finish_time) != 0) {
    perror("clock_gettime");
    free(array);
    free(threads);
    free(args);
    return 1;
  }

  printf("Total: %" PRId64 "\n", total_sum);
  printf("Elapsed time: %.3fms\n",
         ElapsedMilliseconds(&start_time, &finish_time));

  free(array);
  free(threads);
  free(args);
  return 0;
}
