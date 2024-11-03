#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

#include "chrono.c"

#define DEBUG 0
#define MAX_THREADS 64
#define LOOP_COUNT 1
#define MAX_TOTAL_ELEMENTS (long long)250e6
#define TOTAL_SEARCHABLE_ELEMENTS 100

pthread_t Thread[MAX_THREADS];
int thread_id[MAX_THREADS];
long long answers[MAX_THREADS];

int nThreads;       // numero efetivo de threads
int nTotalElements; // numero total de elementos

long long InputVector[MAX_TOTAL_ELEMENTS];
long long *InVec = InputVector;

long long SearchVector[TOTAL_SEARCHABLE_ELEMENTS];
long long *SearchVec = SearchVector;

typedef struct
{
    int thread_id;
    long long *searchVec;
} thread_args_t;

pthread_barrier_t bsearch_barrier;

int compare(const void *a, const void *b)
{
    long long int_a = *(const long long *)a;
    long long int_b = *(const long long *)b;

    if (int_a < int_b)
        return -1;
    else if (int_a > int_b)
        return 1;
    else
        return 0;
}

void *bsearch_lower_bound(void *ptr)
{
    thread_args_t *args = (thread_args_t *)ptr;
    int myIndex = args->thread_id;
    long long *searchVec = args->searchVec;

    pthread_barrier_wait(&bsearch_barrier);
    int first = 0;
    int last = nTotalElements - 1;

    printf("thread %d here! first=%d last=%d\n",
           myIndex, first, last);

    int myAnswer = last + 1;
    long long x = searchVec[myIndex];

    while (first <= last)
    {
        int m = first + (last - first) / 2;

        if (InVec[m] >= x)
        {
            myAnswer = m;
            last = m - 1;
        }
        else
        {
            first = m + 1;
        }
    }

    answers[myIndex] = myAnswer;

    pthread_barrier_wait(&bsearch_barrier);
    if (myIndex == 0)
        return NULL;
}

// NEVER HERE!
if (myIndex != 0)
    pthread_exit(NULL);

return NULL;
}

void parallel_multiple_bsearch(long long searchVec[], int nThreads)
{
    static int initialized = 0;
    thread_args_t thread_args[nThreads];

    if (!initialized)
    {
        pthread_barrier_init(&bsearch_barrier, NULL, nThreads);
        // thread 0 will be the caller thread

        // cria todas as outra threds trabalhadoras
        for (int i = 1; i < nThreads; i++)
        {
            thread_args[i].thread_id = i;
            thread_args[i].searchVec = searchVec;
            pthread_create(&Thread[i], NULL,
                           bsearch_lower_bound, &thread_args[i]);
        }

        initialized = 1;
    }

    thread_args[0].thread_id = 0;
    thread_args[0].searchVec = searchVec;

    // caller thread will be thread 0, and will start working on its chunk
    bsearch_lower_bound(&thread_args[0]);

    // a thread chamadora mostra o vetor resultado
    long long globalSum = 0;
    for (int i = 0; i < nThreads; i++)
    {
        printf("Thread %d found %lld at index %lld\n", i, SearchVec[i], answers[i]);
    }
    printf("\n\n");

    return;
}

int main(int argc, char *argv[])
{
    int i;
    chronometer_t chrono_time;

    if (argc != 3)
    {
        printf("usage: %s <nTotalElements> <nThreads>\n",
               argv[0]);
        return 0;
    }
    else
    {
        nThreads = atoi(argv[2]);
        if (nThreads == 0)
        {
            printf("usage: %s <nTotalElements> <nThreads>\n",
                   argv[0]);
            printf("<nThreads> can't be 0\n");
            return 0;
        }
        if (nThreads > MAX_THREADS)
        {
            printf("usage: %s <nTotalElements> <nThreads>\n",
                   argv[0]);
            printf("<nThreads> must be less than %d\n", MAX_THREADS);
            return 0;
        }
        nTotalElements = atoi(argv[1]);
        if (nTotalElements > MAX_TOTAL_ELEMENTS)
        {
            printf("usage: %s <nTotalElements> <nThreads>\n",
                   argv[0]);
            printf("<nTotalElements> must be up to %lld\n", MAX_TOTAL_ELEMENTS);
            return 0;
        }
    }

    printf("will use %d threads to run bsearch on %d elements\n\n", nThreads, nTotalElements);

    srand((unsigned int)time(NULL));

    for (int i = 0; i < nTotalElements; i++)
        InputVector[i] = (long long)rand() * rand();

    qsort(InputVector, nTotalElements, sizeof(long long), compare);

    for (int i = 0; i < TOTAL_SEARCHABLE_ELEMENTS; i++)
        SearchVector[i] = (long long)rand() * rand();

    chrono_reset(&chrono_time);
    chrono_start(&chrono_time);

// call it N times
#define NTIMES 10

    int start_position = 0;
    InVec = &InputVector[start_position];
    SearchVec = &SearchVector[start_position];

    long long auxSearchVec[nThreads];

    for (int i = 0; i < TOTAL_SEARCHABLE_ELEMENTS; i += nThreads)
    {
        for (int j = 0; j < nThreads && (i + j) < TOTAL_SEARCHABLE_ELEMENTS; j++)
        {
            auxSearchVec[j] = SearchVector[i + j];
        }
        parallel_multiple_bsearch(auxSearchVec, nThreads);
    }

    // Measuring time after parallel_lowerBoundBinarySearch finished...
    chrono_stop(&chrono_time);

    chrono_reportTime(&chrono_time, "time");

    // calcular e imprimir a VAZAO (numero de operacoes/s)
    double total_time_in_seconds = (double)chrono_gettotal(&chrono_time) /
                                   ((double)1000 * 1000 * 1000);
    printf("total_time_in_seconds: %lf s\n", total_time_in_seconds);

    // double OPS = ((double)nTotalElements * NTIMES) / total_time_in_seconds;
    // printf("Throughput: %lf OP/s\n", OPS);

    return 0;
}
